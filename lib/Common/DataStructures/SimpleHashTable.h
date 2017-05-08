//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template<typename TKey, typename TData>
struct SimpleHashEntry {
    TKey key;
    TData value;
    SimpleHashEntry *next;
};

// Size should be a power of 2 for optimal performance
template<
    typename TKey,
    typename TData,
    typename TAllocator = ArenaAllocator,
    template <typename DataOrKey> class Comparer = DefaultComparer,
    bool resize = false,
    typename SizePolicy = PowerOf2Policy>
class SimpleHashTable
{
    typedef SimpleHashEntry<TKey, TData> EntryType;

    // REVIEW: Consider 5 or 7 as multiplication of these might be faster.
    static const int MaxAverageChainLength = 6;

    TAllocator *allocator;
    EntryType **table;
    EntryType *free;
    uint count;
    uint size;
    uint freecount;
    bool disableResize;
#if PROFILE_DICTIONARY
    DictionaryStats *stats;
#endif
public:
    SimpleHashTable(TAllocator *allocator) :
        allocator(allocator),
        count(0),
        freecount(0)
    {TRACE_IT(22113);
        this->size = SizePolicy::GetSize(64);
        Initialize();
    }

    SimpleHashTable(uint size, TAllocator* allocator) :
        allocator(allocator),
        count(0),
        freecount(0)
    {TRACE_IT(22114);
        this->size = SizePolicy::GetSize(size);
        Initialize();
    }

    void Initialize()
    {TRACE_IT(22115);
        disableResize = false;
        free = nullptr;
        table = AllocatorNewArrayZ(TAllocator, allocator, EntryType*, size);
#if PROFILE_DICTIONARY
        stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
    }

    ~SimpleHashTable()
    {TRACE_IT(22116);
        for (uint i = 0; i < size; i++)
        {TRACE_IT(22117);
            EntryType * entry = table[i];
            while (entry != nullptr)
            {TRACE_IT(22118);
                EntryType * next = entry->next;
                AllocatorDelete(TAllocator, allocator, entry);
                entry = next;
            }
        }

        while(free)
        {TRACE_IT(22119);
            EntryType* current = free;
            free = current->next;
            AllocatorDelete(TAllocator, allocator, current);
        }
        AllocatorDeleteArray(TAllocator, allocator,  size, table);
    }

    void DisableResize()
    {TRACE_IT(22120);
        Assert(!resize || !disableResize);
        disableResize = true;
    }

    void EnableResize()
    {TRACE_IT(22121);
        Assert(!resize || disableResize);
        disableResize = false;
    }

    void Set(TKey key, TData data)
    {TRACE_IT(22122);
        EntryType* entry = FindOrAddEntry(key);
        entry->value = data;
    }

    bool Add(TKey key, TData data)
    {TRACE_IT(22123);
        uint targetBucket = HashKeyToBucket(key);

        if(FindEntry(key, targetBucket) != nullptr)
        {TRACE_IT(22124);
            return false;
        }

        AddInternal(key, data, targetBucket);
        return true;
    }

    void ReplaceValue(TKey key,TData data)
    {TRACE_IT(22125);
        EntryType *current = FindEntry(key);
        if (current != nullptr)
        {TRACE_IT(22126);
            current->value = data;
        }
    }

    void Remove(TKey key)
    {
        Remove(key, nullptr);
    }

    void Remove(TKey key, TData* pOut)
    {TRACE_IT(22127);
        uint val = HashKeyToBucket(key);
        EntryType **prev=&table[val];
        for (EntryType * current = *prev ; current != nullptr; current = current->next)
        {TRACE_IT(22128);
            if (Comparer<TKey>::Equals(key, current->key))
            {TRACE_IT(22129);
                *prev = current->next;
                if (pOut != nullptr)
                {TRACE_IT(22130);
                    (*pOut) = current->value;
                }

                count--;
                FreeEntry(current);
#if PROFILE_DICTIONARY
                if (stats)
                    stats->Remove(table[val] == nullptr);
#endif
                break;
            }
            prev = &current->next;
        }
    }

    BOOL HasEntry(TKey key)
    {TRACE_IT(22131);
        return (FindEntry(key) != nullptr);
    }

    uint Count() const
    {TRACE_IT(22132);
        return(count);
    }

    // If density is a compile-time constant, then we can optimize (avoids division)
    // Sometimes the compiler can also make this optimization, but this way it is guaranteed.
    template< uint density > bool IsDenserThan() const
    {TRACE_IT(22133);
        return count > (size * density);
    }

    TData Lookup(TKey key)
    {TRACE_IT(22134);
        EntryType *current = FindEntry(key);
        if (current != nullptr)
        {TRACE_IT(22135);
            return current->value;
        }
        return TData();
    }

    TData LookupIndex(int index)
    {TRACE_IT(22136);
        EntryType *current;
        int j=0;
        for (uint i=0; i < size; i++)
        {TRACE_IT(22137);
            for (current = table[i] ; current != nullptr; current = current->next)
            {TRACE_IT(22138);
                if (j==index)
                {TRACE_IT(22139);
                    return current->value;
                }
                j++;
            }
        }
        return nullptr;
    }

    bool TryGetValue(TKey key, TData *dataReference)
    {TRACE_IT(22140);
        EntryType *current = FindEntry(key);
        if (current != nullptr)
        {TRACE_IT(22141);
            *dataReference = current->value;
            return true;
        }
        return false;
    }

    TData& GetReference(TKey key)
    {TRACE_IT(22142);
        EntryType * current = FindOrAddEntry(key);
        return current->value;
    }

    TData * TryGetReference(TKey key)
    {TRACE_IT(22143);
        EntryType * current = FindEntry(key);
        if (current != nullptr)
        {TRACE_IT(22144);
            return &current->value;
        }
        return nullptr;
    }


    template <class Fn>
    void Map(Fn fn)
    {TRACE_IT(22145);
        EntryType *current;
        for (uint i=0;i<size;i++) {TRACE_IT(22146);
            for (current = table[i] ; current != nullptr; current = current->next) {TRACE_IT(22147);
                fn(current->key,current->value);
            }
        }
    }

    template <class Fn>
    void MapAndRemoveIf(Fn fn)
    {TRACE_IT(22148);
        for (uint i=0; i<size; i++)
        {TRACE_IT(22149);
            EntryType ** prev = &table[i];
            while (EntryType * current = *prev)
            {TRACE_IT(22150);
                if (fn(current->key,current->value))
                {TRACE_IT(22151);
                    *prev = current->next;
                    FreeEntry(current);
                }
                else
                {TRACE_IT(22152);
                    prev = &current->next;
                }
            }
        }
    }
private:
    uint HashKeyToBucket(TKey hashKey)
    {TRACE_IT(22153);
        return HashKeyToBucket(hashKey, size);
    }

    uint HashKeyToBucket(TKey hashKey, int size)
    {TRACE_IT(22154);
        uint hashCode = Comparer<TKey>::GetHashCode(hashKey);
        return SizePolicy::GetBucket(hashCode, size);
    }

    EntryType * FindEntry(TKey key)
    {TRACE_IT(22155);
        uint targetBucket = HashKeyToBucket(key);
        return FindEntry(key, targetBucket);
    }

    EntryType * FindEntry(TKey key, uint targetBucket)
    {TRACE_IT(22156);
        for (EntryType * current = table[targetBucket] ; current != nullptr; current = current->next)
        {TRACE_IT(22157);
            if (Comparer<TKey>::Equals(key, current->key))
            {TRACE_IT(22158);
                return current;
            }
        }
        return nullptr;
    }

    EntryType * FindOrAddEntry(TKey key)
    {TRACE_IT(22159);
         uint targetBucket = HashKeyToBucket(key);
         EntryType * entry = FindEntry(key, targetBucket);
         if (entry == nullptr)
         {TRACE_IT(22160);
            entry = AddInternal(key, TData(), targetBucket);
         }
         return entry;
    }

    void FreeEntry(EntryType* current)
    {TRACE_IT(22161);
        if ( freecount < 10 )
        {TRACE_IT(22162);
            current->key = nullptr;
            current->value = NULL;
            current->next = free;
            free = current;
            freecount++;
        }
        else
        {
            AllocatorDelete(TAllocator, allocator, current);
        }
    }

    EntryType* GetFreeEntry()
    {TRACE_IT(22163);
        EntryType* retFree = free;
        if (nullptr == retFree )
        {TRACE_IT(22164);
            retFree = AllocatorNewStruct(TAllocator, allocator, EntryType);
        }
        else
        {TRACE_IT(22165);
            free = retFree->next;
            freecount--;
        }
        return retFree;
    }

    EntryType* AddInternal(TKey key, TData data, uint targetBucket)
    {TRACE_IT(22166);
        if(resize && !disableResize && IsDenserThan<MaxAverageChainLength>())
        {TRACE_IT(22167);
            Resize(SizePolicy::GetSize(size*2));
            // After resize - we will need to recalculate the bucket
            targetBucket = HashKeyToBucket(key);
        }

        EntryType* entry = GetFreeEntry();
        entry->key = key;
        entry->value = data;
        entry->next = table[targetBucket];
        table[targetBucket] = entry;
        count++;

#if PROFILE_DICTIONARY
        uint depth = 0;
        for (EntryType * current = table[targetBucket] ; current != nullptr; current = current->next)
        {TRACE_IT(22168);
            ++depth;
        }
        if (stats)
            stats->Insert(depth);
#endif
        return entry;
    }

    void Resize(int newSize)
    {TRACE_IT(22169);
        Assert(!this->disableResize);
        EntryType** newTable = AllocatorNewArrayZ(TAllocator, allocator, EntryType*, newSize);

        for (uint i=0; i < size; i++)
        {TRACE_IT(22170);
            EntryType* current = table[i];
            while (current != nullptr)
            {TRACE_IT(22171);
                int targetBucket = HashKeyToBucket(current->key, newSize);
                EntryType* next = current->next; // Cache the next pointer
                current->next = newTable[targetBucket];
                newTable[targetBucket] = current;
                current = next;
            }
        }

        AllocatorDeleteArray(TAllocator, allocator, this->size, this->table);
        this->size = newSize;
        this->table = newTable;
#if PROFILE_DICTIONARY
        if (stats)
        {TRACE_IT(22172);
            uint emptyBuckets  = 0 ;
            for (uint i=0; i < size; i++)
            {TRACE_IT(22173);
                if(table[i] == nullptr)
                {TRACE_IT(22174);
                    emptyBuckets++;
                }
            }
            stats->Resize(newSize, emptyBuckets);
        }
#endif

    }
};
