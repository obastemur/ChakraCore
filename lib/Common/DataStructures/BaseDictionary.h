//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//////////////////////////////////////////////////////////////////////////
// Template implementation of dictionary based on .NET BCL implementation.
//
// Buckets and entries are allocated as contiguous arrays to maintain good locality of reference.
//
// COLLISION STRATEGY
// This dictionary uses a chaining collision resolution strategy. Chains are implemented using indexes to the 'buckets' array.
//
// STORAGE (TAllocator)
// This dictionary works for both arena and recycler based allocation using TAllocator template parameter.
// It supports storing of both value and pointer types. Using template specialization, value types (built-in fundamental
// types and structs) are stored as leaf nodes by default.
//
// INITIAL SIZE and BUCKET MAPPING (SizePolicy)
// This can be specified using TSizePolicy template parameter. There are 2 implementations:
//         - PrimeSizePolicy (Better distribution): Initial size is a prime number. Mapping to bucket is done using modulus operation (costlier).
//         - PowerOf2SizePolicy (faster): Initial size is a power of 2. Mapping to bucket is done by a fast truncating the MSB bits up to the size of the table.
//
// COMPARISONS AND HASHCODE (Comparer)
// Enables custom comparisons for TKey and TValue. For example, for strings we use string comparison instead of comparing pointers.
//

#if PROFILE_DICTIONARY
#include "DictionaryStats.h"
#endif

namespace Js
{
    template <class TDictionary>
    class RemoteDictionary;
}

namespace JsDiag
{
    template <class TDictionary>
    struct RemoteDictionary;
}

namespace JsUtil
{
    class NoResizeLock
    {
    public:
        void BeginResize() {TRACE_IT(20594);}
        void EndResize() {TRACE_IT(20595);}
    };

    class AsymetricResizeLock
    {
    public:
        void BeginResize() {TRACE_IT(20596); cs.Enter(); }
        void EndResize() {TRACE_IT(20597); cs.Leave(); }
        void LockResize() {TRACE_IT(20598); cs.Enter(); }
        void UnlockResize() {TRACE_IT(20599); cs.Leave(); }
    private:
        CriticalSection cs;
    };

    template <class TKey, class TValue> class SimpleDictionaryEntry;

    template <
        class TKey,
        class TValue,
        class TAllocator,
        class SizePolicy = PowerOf2SizePolicy,
        template <typename ValueOrKey> class Comparer = DefaultComparer,
        template <typename K, typename V> class Entry = SimpleDictionaryEntry,
        typename Lock = NoResizeLock
    >
    class BaseDictionary : protected Lock
    {
    public:
        typedef TKey KeyType;
        typedef TValue ValueType;
        typedef typename AllocatorInfo<TAllocator, TValue>::AllocatorType AllocatorType;
        typedef SizePolicy CurrentSizePolicy;
        typedef Entry<
                    Field(TKey, TAllocator),
                    Field(TValue, TAllocator)> EntryType;

        template<class TDictionary> class EntryIterator;
        template<class TDictionary> class BucketEntryIterator;

    protected:
        typedef typename AllocatorInfo<TAllocator, TValue>::AllocatorFunc EntryAllocatorFuncType;
        friend class Js::RemoteDictionary<BaseDictionary>;
        template <typename ValueOrKey> struct ComparerType { typedef Comparer<ValueOrKey> Type; }; // Used by diagnostics to access Comparer type

        Field(int*, TAllocator) buckets;
        Field(EntryType*, TAllocator) entries;
        FieldNoBarrier(AllocatorType*) alloc;
        Field(int) size;
        Field(uint) bucketCount;
        Field(int) count;
        Field(int) freeList;
        Field(int) freeCount;

#if PROFILE_DICTIONARY
        FieldNoBarrier(DictionaryStats*) stats;
#endif
        enum InsertOperations
        {
            Insert_Add   ,          // FatalInternalError if the item already exist in debug build
            Insert_AddNew,          // Ignore add if the item already exist
            Insert_Item             // Replace the item if it already exist
        };

        class AutoDoResize
        {
        public:
            AutoDoResize(Lock& lock) : lock(lock) {TRACE_IT(20600); lock.BeginResize(); };
            ~AutoDoResize() {TRACE_IT(20601); lock.EndResize(); };
        private:
            Lock& lock;
        };
    public:
        BaseDictionary(AllocatorType* allocator, int capacity = 0)
            : buckets (nullptr),
            size(0),
            bucketCount(0),
            entries(nullptr),
            count(0),
            freeCount(0),
            alloc(allocator)
        {TRACE_IT(20602);
            Assert(allocator);
#if PROFILE_DICTIONARY
            stats = nullptr;
#endif
            // If initial capacity is negative or 0, lazy initialization on
            // the first insert operation is performed.
            if (capacity > 0)
            {TRACE_IT(20603);
                Initialize(capacity);
            }
        }

        BaseDictionary(const BaseDictionary &other) : alloc(other.alloc)
        {TRACE_IT(20604);
            if(other.Count() == 0)
            {TRACE_IT(20605);
                size = 0;
                bucketCount = 0;
                buckets = nullptr;
                entries = nullptr;
                count = 0;
                freeCount = 0;

#if PROFILE_DICTIONARY
                stats = nullptr;
#endif
                return;
            }

            Assert(other.bucketCount != 0);
            Assert(other.size != 0);

            buckets = AllocateBuckets(other.bucketCount);
            Assert(buckets); // no-throw allocators are currently not supported

            try
            {TRACE_IT(20606);
                entries = AllocateEntries(other.size, false /* zeroAllocate */);
                Assert(entries); // no-throw allocators are currently not supported
            }
            catch(...)
            {
                DeleteBuckets(buckets, other.bucketCount);
                throw;
            }

            size = other.size;
            bucketCount = other.bucketCount;
            count = other.count;
            freeList = other.freeList;
            freeCount = other.freeCount;

            CopyArray(buckets, bucketCount, other.buckets, bucketCount);
            CopyArray<EntryType, Field(ValueType, TAllocator), TAllocator>(
                entries, size, other.entries, size);

#if PROFILE_DICTIONARY
            stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
        }

        ~BaseDictionary()
        {TRACE_IT(20607);
            if (buckets)
            {
                DeleteBuckets(buckets, bucketCount);
            }

            if (entries)
            {
                DeleteEntries(entries, size);
            }
        }

        AllocatorType *GetAllocator() const
        {TRACE_IT(20608);
            return alloc;
        }

        inline int Capacity() const
        {TRACE_IT(20609);
            return size;
        }

        inline int Count() const
        {TRACE_IT(20610);
            return count - freeCount;
        }

        TValue Item(const TKey& key)
        {TRACE_IT(20611);
            int i = FindEntry(key);
            Assert(i >= 0);
            return entries[i].Value();
        }

        const TValue Item(const TKey& key) const
        {TRACE_IT(20612);
            int i = FindEntry(key);
            Assert(i >= 0);
            return entries[i].Value();
        }

        int Add(const TKey& key, const TValue& value)
        {TRACE_IT(20613);
            return Insert<Insert_Add>(key, value);
        }

        int AddNew(const TKey& key, const TValue& value)
        {TRACE_IT(20614);
            return Insert<Insert_AddNew>(key, value);
        }

        int Item(const TKey& key, const TValue& value)
        {TRACE_IT(20615);
            return Insert<Insert_Item>(key, value);
        }

        bool Contains(KeyValuePair<TKey, TValue> keyValuePair)
        {TRACE_IT(20616);
            int i = FindEntry(keyValuePair.Key());
            if( i >= 0 && Comparer<TValue>::Equals(entries[i].Value(), keyValuePair.Value()))
            {TRACE_IT(20617);
                return true;
            }
            return false;
        }

        bool Remove(KeyValuePair<TKey, TValue> keyValuePair)
        {TRACE_IT(20618);
            int i, last;
            uint targetBucket;
            if(FindEntryWithKey(keyValuePair.Key(), &i, &last, &targetBucket))
            {TRACE_IT(20619);
                const TValue &value = entries[i].Value();
                if(Comparer<TValue>::Equals(value, keyValuePair.Value()))
                {
                    RemoveAt(i, last, targetBucket);
                    return true;
                }
            }
            return false;
        }

        void Clear()
        {TRACE_IT(20620);
            if (count > 0)
            {
                memset(buckets, -1, bucketCount * sizeof(buckets[0]));
                memset(entries, 0, sizeof(EntryType) * size);
                count = 0;
                freeCount = 0;
#if PROFILE_DICTIONARY
                // To not loose previously collected data, we will treat cleared dictionary as a separate instance for stats tracking purpose
                stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
            }
        }

        void ResetNoDelete()
        {TRACE_IT(20621);
            this->size = 0;
            this->bucketCount = 0;
            this->buckets = nullptr;
            this->entries = nullptr;
            this->count = 0;
            this->freeCount = 0;
        }

        void Reset()
        {TRACE_IT(20622);
            if(bucketCount != 0)
            {
                DeleteBuckets(buckets, bucketCount);
                buckets = nullptr;
                bucketCount = 0;
            }
            else
            {TRACE_IT(20623);
                Assert(!buckets);
            }
            if(size != 0)
            {
                DeleteEntries(entries, size);
                entries = nullptr;
                freeCount = count = size = 0;
            }
            else
            {TRACE_IT(20624);
                Assert(!entries);
                Assert(count == 0);
                Assert(freeCount == 0);
            }
        }

        bool ContainsKey(const TKey& key) const
        {TRACE_IT(20625);
            return FindEntry(key) >= 0;
        }

        template <typename TLookup>
        inline const TValue& LookupWithKey(const TLookup& key, const TValue& defaultValue) const
        {TRACE_IT(20626);
            int i = FindEntryWithKey(key);
            if (i >= 0)
            {TRACE_IT(20627);
                return entries[i].Value();
            }
            return defaultValue;
        }

        inline const TValue& Lookup(const TKey& key, const TValue& defaultValue)
        {TRACE_IT(20628);
            return LookupWithKey<TKey>(key, defaultValue);
        }

        template <typename TLookup>
        bool TryGetValue(const TLookup& key, TValue* value) const
        {TRACE_IT(20629);
            int i = FindEntryWithKey(key);
            if (i >= 0)
            {TRACE_IT(20630);
                *value = entries[i].Value();
                return true;
            }
            return false;
        }

        bool TryGetValueAndRemove(const TKey& key, TValue* value)
        {TRACE_IT(20631);
            int i, last;
            uint targetBucket;
            if (FindEntryWithKey(key, &i, &last, &targetBucket))
            {TRACE_IT(20632);
                *value = entries[i].Value();
                RemoveAt(i, last, targetBucket);
                return true;
            }
            return false;
        }

        template <typename TLookup>
        bool TryGetReference(const TLookup& key, const TValue** value) const
        {TRACE_IT(20633);
            int i;
            return TryGetReference(key, value, &i);
        }

        template <typename TLookup>
        bool TryGetReference(const TLookup& key, TValue** value) const
        {TRACE_IT(20634);
            int i;
            return TryGetReference(key, value, &i);
        }

        template <typename TLookup>
        bool TryGetReference(const TLookup& key, const TValue** value, int* index) const
        {TRACE_IT(20635);
            int i = FindEntryWithKey(key);
            if (i >= 0)
            {TRACE_IT(20636);
                *value = AddressOf(entries[i].Value());
                *index = i;
                return true;
            }
            return false;
        }

        template <typename TLookup>
        bool TryGetReference(const TLookup& key, TValue** value, int* index) const
        {TRACE_IT(20637);
            int i = FindEntryWithKey(key);
            if (i >= 0)
            {TRACE_IT(20638);
                *value = &entries[i].Value();
                *index = i;
                return true;
            }
            return false;
        }

        const TValue& GetValueAt(const int index) const
        {TRACE_IT(20639);
            Assert(index >= 0);
            Assert(index < count);

            return entries[index].Value();
        }

        TValue* GetReferenceAt(const int index) const
        {TRACE_IT(20640);
            Assert(index >= 0);
            Assert(index < count);

            return &entries[index].Value();
        }

        TKey const& GetKeyAt(const int index) const
        {TRACE_IT(20641);
            Assert(index >= 0);
            Assert(index < count);

            return entries[index].Key();
        }

        bool TryGetValueAt(const int index, TValue const ** value) const
        {TRACE_IT(20642);
            if (index >= 0 && index < count)
            {TRACE_IT(20643);
                *value = &entries[index].Value();
                return true;
            }
            return false;
        }

        bool TryGetValueAt(int index, TValue * value) const
        {TRACE_IT(20644);
            if (index >= 0 && index < count)
            {TRACE_IT(20645);
                *value = entries[index].Value();
                return true;
            }
            return false;
        }

        bool Remove(const TKey& key)
        {TRACE_IT(20646);
            int i, last;
            uint targetBucket;
            if(FindEntryWithKey(key, &i, &last, &targetBucket))
            {
                RemoveAt(i, last, targetBucket);
                return true;
            }
            return false;
        }

        EntryIterator<const BaseDictionary> GetIterator() const
        {TRACE_IT(20647);
            return EntryIterator<const BaseDictionary>(*this);
        }

        EntryIterator<BaseDictionary> GetIterator()
        {TRACE_IT(20648);
            return EntryIterator<BaseDictionary>(*this);
        }

        BucketEntryIterator<BaseDictionary> GetIteratorWithRemovalSupport()
        {TRACE_IT(20649);
            return BucketEntryIterator<BaseDictionary>(*this);
        }

        template<class Fn>
        bool AnyValue(Fn fn) const
        {TRACE_IT(20650);
            for (uint i = 0; i < bucketCount; i++)
            {TRACE_IT(20651);
                if(buckets[i] != -1)
                {TRACE_IT(20652);
                    for (int currentIndex = buckets[i] ; currentIndex != -1 ; currentIndex = entries[currentIndex].next)
                    {TRACE_IT(20653);
                        if (fn(entries[currentIndex].Value()))
                        {TRACE_IT(20654);
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        template<class Fn>
        void EachValue(Fn fn) const
        {TRACE_IT(20655);
            for (uint i = 0; i < bucketCount; i++)
            {TRACE_IT(20656);
                if(buckets[i] != -1)
                {TRACE_IT(20657);
                    for (int currentIndex = buckets[i] ; currentIndex != -1 ; currentIndex = entries[currentIndex].next)
                    {TRACE_IT(20658);
                        fn(entries[currentIndex].Value());
                    }
                }
            }
        }

        template<class Fn>
        void MapReference(Fn fn)
        {TRACE_IT(20659);
            MapUntilReference([fn](TKey const& key, TValue& value)
            {
                fn(key, value);
                return false;
            });
        }

        template<class Fn>
        bool MapUntilReference(Fn fn) const
        {TRACE_IT(20660);
            return MapEntryUntil([fn](EntryType &entry) -> bool
            {
                return fn(entry.Key(), entry.Value());
            });
        }

        template<class Fn>
        void MapAddress(Fn fn) const
        {TRACE_IT(20661);
            MapUntilAddress([fn](TKey const& key, TValue * value) -> bool
            {
                fn(key, value);
                return false;
            });
        }

        template<class Fn>
        bool MapUntilAddress(Fn fn) const
        {TRACE_IT(20662);
            return MapEntryUntil([fn](EntryType &entry) -> bool
            {
                return fn(entry.Key(), &entry.Value());
            });
        }

        template<class Fn>
        void Map(Fn fn) const
        {TRACE_IT(20663);
            MapUntil([fn](TKey const& key, TValue const& value) -> bool
            {
                fn(key, value);
                return false;
            });
        }

        template<class Fn>
        bool MapUntil(Fn fn) const
        {TRACE_IT(20664);
            return MapEntryUntil([fn](EntryType const& entry) -> bool
            {
                return fn(entry.Key(), entry.Value());
            });
        }

        template<class Fn>
        void MapAndRemoveIf(Fn fn)
        {TRACE_IT(20665);
            for (uint i = 0; i < bucketCount; i++)
            {TRACE_IT(20666);
                if (buckets[i] != -1)
                {TRACE_IT(20667);
                    for (int currentIndex = buckets[i], lastIndex = -1; currentIndex != -1;)
                    {TRACE_IT(20668);
                        // If the predicate says we should remove this item
                        if (fn(entries[currentIndex]) == true)
                        {TRACE_IT(20669);
                            const int nextIndex = entries[currentIndex].next;
                            RemoveAt(currentIndex, lastIndex, i);
                            currentIndex = nextIndex;
                        }
                        else
                        {TRACE_IT(20670);
                            lastIndex = currentIndex;
                            currentIndex = entries[currentIndex].next;
                        }
                    }
                }
            }
        }

        template <class Fn>
        bool RemoveIf(TKey const& key, Fn fn)
        {TRACE_IT(20671);
            return RemoveIfWithKey<TKey>(key, fn);
        }

        template <typename LookupType, class Fn>
        bool RemoveIfWithKey(LookupType const& lookupKey, Fn fn)
        {TRACE_IT(20672);
            int i, last;
            uint targetBucket;
            if (FindEntryWithKey<LookupType>(lookupKey, &i, &last, &targetBucket))
            {TRACE_IT(20673);
                if (fn(entries[i].Key(), entries[i].Value()))
                {
                    RemoveAt(i, last, targetBucket);
                    return true;
                }
            }
            return false;
        }

        // Returns whether the dictionary was resized or not
        bool EnsureCapacity()
        {TRACE_IT(20674);
            if (freeCount == 0 && count == size)
            {TRACE_IT(20675);
                Resize();
                return true;
            }

            return false;
        }

        int GetNextIndex()
        {TRACE_IT(20676);
            if (freeCount != 0)
            {TRACE_IT(20677);
                Assert(freeCount > 0);
                Assert(freeList >= 0);
                Assert(freeList < count);
                return freeList;
            }

            return count;
        }

        int GetLastIndex()
        {TRACE_IT(20678);
            return count - 1;
        }

        BaseDictionary *Clone()
        {TRACE_IT(20679);
            return AllocatorNew(AllocatorType, alloc, BaseDictionary, *this);
        }

        void Copy(const BaseDictionary *const other)
        {TRACE_IT(20680);
            DoCopy(other);
        }

        void LockResize()
        {TRACE_IT(20681);
            __super::LockResize();
        }

        void UnlockResize()
        {TRACE_IT(20682);
            __super::UnlockResize();
        }
    protected:
        template<class T>
        void DoCopy(const T *const other)
        {TRACE_IT(20683);
            Assert(size == 0);
            Assert(bucketCount == 0);
            Assert(!buckets);
            Assert(!entries);
            Assert(count == 0);
            Assert(freeCount == 0);
#if PROFILE_DICTIONARY
            Assert(!stats);
#endif

            if(other->Count() == 0)
            {TRACE_IT(20684);
                return;
            }

            Assert(other->bucketCount != 0);
            Assert(other->size != 0);

            buckets = AllocateBuckets(other->bucketCount);
            Assert(buckets); // no-throw allocators are currently not supported

            try
            {TRACE_IT(20685);
                entries = AllocateEntries(other->size, false /* zeroAllocate */);
                Assert(entries); // no-throw allocators are currently not supported
            }
            catch(...)
            {
                DeleteBuckets(buckets, other->bucketCount);
                buckets = nullptr;
                throw;
            }

            size = other->size;
            bucketCount = other->bucketCount;
            count = other->count;
            freeList = other->freeList;
            freeCount = other->freeCount;

            CopyArray(buckets, bucketCount, other->buckets, bucketCount);
            CopyArray<EntryType, Field(ValueType, TAllocator), TAllocator>(
                entries, size, other->entries, size);

#if PROFILE_DICTIONARY
            stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
        }

    protected:
        template<class Fn>
        bool MapEntryUntil(Fn fn) const
        {TRACE_IT(20686);
            for (uint i = 0; i < bucketCount; i++)
            {TRACE_IT(20687);
                if(buckets[i] != -1)
                {TRACE_IT(20688);
                    int nextIndex = -1;
                    for (int currentIndex = buckets[i] ; currentIndex != -1 ; currentIndex = nextIndex)
                    {TRACE_IT(20689);
                        nextIndex = entries[currentIndex].next;
                        if (fn(entries[currentIndex]))
                        {TRACE_IT(20690);
                            return true; // fn condition succeeds
                        }
                    }
                }
            }

            return false;
        }

    private:
        template <typename TLookup>
        static hash_t GetHashCodeWithKey(const TLookup& key)
        {TRACE_IT(20691);
            // set last bit to 1 to avoid false positive to make hash appears to be a valid recycler address.
            // In the same line, 0 should be use to indicate a non-existing entry.
            return TAGHASH(Comparer<TLookup>::GetHashCode(key));
        }

        static hash_t GetHashCode(const TKey& key)
        {TRACE_IT(20692);
            return GetHashCodeWithKey<TKey>(key);
        }

        static uint GetBucket(hash_t hashCode, int bucketCount)
        {TRACE_IT(20693);
            return SizePolicy::GetBucket(UNTAGHASH(hashCode), bucketCount);
        }

        uint GetBucket(uint hashCode) const
        {TRACE_IT(20694);
            return GetBucket(hashCode, this->bucketCount);
        }

        static bool IsFreeEntry(const EntryType &entry)
        {TRACE_IT(20695);
            // A free entry's next index will be (-2 - nextIndex), such that it is always <= -2, for fast entry iteration
            // allowing for skipping over free entries. -1 is reserved for the end-of-chain marker for a used entry.
            return entry.next <= -2;
        }

        void SetNextFreeEntryIndex(EntryType &freeEntry, const int nextFreeEntryIndex)
        {TRACE_IT(20696);
            Assert(!IsFreeEntry(freeEntry));
            Assert(nextFreeEntryIndex >= -1);
            Assert(nextFreeEntryIndex < count);

            // The last entry in the free list chain will have a next of -2 to indicate that it is a free entry. The end of the
            // free list chain is identified using freeCount.
            freeEntry.next = nextFreeEntryIndex >= 0 ? -2 - nextFreeEntryIndex : -2;
        }

        static int GetNextFreeEntryIndex(const EntryType &freeEntry)
        {TRACE_IT(20697);
            Assert(IsFreeEntry(freeEntry));
            return -2 - freeEntry.next;
        }

        template <typename LookupType>
        inline int FindEntryWithKey(const LookupType& key) const
        {TRACE_IT(20698);
#if PROFILE_DICTIONARY
            uint depth = 0;
#endif
            int * localBuckets = buckets;
            if (localBuckets != nullptr)
            {TRACE_IT(20699);
                hash_t hashCode = GetHashCodeWithKey<LookupType>(key);
                uint targetBucket = this->GetBucket(hashCode);
                EntryType * localEntries = entries;
                for (int i = localBuckets[targetBucket]; i >= 0; i = localEntries[i].next)
                {TRACE_IT(20700);
                    if (localEntries[i].template KeyEquals<Comparer<TKey>>(key, hashCode))
                    {TRACE_IT(20701);
#if PROFILE_DICTIONARY
                        if (stats)
                            stats->Lookup(depth);
#endif
                        return i;
                    }

#if PROFILE_DICTIONARY
                    depth += 1;
#endif
                }
            }

#if PROFILE_DICTIONARY
            if (stats)
                stats->Lookup(depth);
#endif
            return -1;
        }

        inline int FindEntry(const TKey& key) const
        {TRACE_IT(20702);
            return FindEntryWithKey<TKey>(key);
        }

        template <typename LookupType>
        inline bool FindEntryWithKey(const LookupType& key, int *const i, int *const last, uint *const targetBucket)
        {TRACE_IT(20703);
#if PROFILE_DICTIONARY
            uint depth = 0;
#endif
            int * localBuckets = buckets;
            if (localBuckets != nullptr)
            {TRACE_IT(20704);
                uint hashCode = GetHashCodeWithKey<LookupType>(key);
                *targetBucket = this->GetBucket(hashCode);
                *last = -1;
                EntryType * localEntries = entries;
                for (*i = localBuckets[*targetBucket]; *i >= 0; *last = *i, *i = localEntries[*i].next)
                {TRACE_IT(20705);
                    if (localEntries[*i].template KeyEquals<Comparer<TKey>>(key, hashCode))
                    {TRACE_IT(20706);
#if PROFILE_DICTIONARY
                        if (stats)
                            stats->Lookup(depth);
#endif
                        return true;
                    }
#if PROFILE_DICTIONARY
                    depth += 1;
#endif
                }
            }
#if PROFILE_DICTIONARY
            if (stats)
                stats->Lookup(depth);
#endif
            return false;
        }

        void Initialize(int capacity)
        {TRACE_IT(20707);
            // minimum capacity is 4
            int initSize = max(capacity, 4);
            uint initBucketCount = SizePolicy::GetBucketSize(initSize);
            AssertMsg(initBucketCount > 0, "Size returned by policy should be greater than 0");

            int* newBuckets = nullptr;
            EntryType* newEntries = nullptr;
            Allocate(&newBuckets, &newEntries, initBucketCount, initSize);

            // Allocation can throw - assign only after allocation has succeeded.
            this->buckets = newBuckets;
            this->entries = newEntries;
            this->bucketCount = initBucketCount;
            this->size = initSize;
            Assert(this->freeCount == 0);
#if PROFILE_DICTIONARY
            stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
        }

        template <InsertOperations op>
        int Insert(const TKey& key, const TValue& value)
        {TRACE_IT(20708);
            int * localBuckets = buckets;
            if (localBuckets == nullptr)
            {TRACE_IT(20709);
                Initialize(0);
                localBuckets = buckets;
            }

#if DBG || PROFILE_DICTIONARY
            // Always search and verify
            const bool needSearch = true;
#else
            const bool needSearch = (op != Insert_Add);
#endif
            hash_t hashCode = GetHashCode(key);
            uint targetBucket = this->GetBucket(hashCode);
            if (needSearch)
            {TRACE_IT(20710);
#if PROFILE_DICTIONARY
                uint depth = 0;
#endif
                EntryType * localEntries = entries;
                for (int i = localBuckets[targetBucket]; i >= 0; i = localEntries[i].next)
                {TRACE_IT(20711);
                    if (localEntries[i].template KeyEquals<Comparer<TKey>>(key, hashCode))
                    {TRACE_IT(20712);
#if PROFILE_DICTIONARY
                        if (stats)
                            stats->Lookup(depth);
#endif
                        Assert(op != Insert_Add);
                        if (op == Insert_Item)
                        {TRACE_IT(20713);
                            localEntries[i].SetValue(value);
                            return i;
                        }
                        return -1;
                    }
#if PROFILE_DICTIONARY
                    depth += 1;
#endif
                }

#if PROFILE_DICTIONARY
                if (stats)
                    stats->Lookup(depth);
#endif
            }

            // Ideally we'd do cleanup only if weak references have been collected since the last resize
            // but that would require us to use an additional field to store the last recycler cleanup id
            // that we saw
            // We can add that optimization later if we have to.
            if (EntryType::SupportsCleanup() && freeCount == 0 && count == size)
            {TRACE_IT(20714);
                this->MapAndRemoveIf([](EntryType& entry)
                {
                    return EntryType::NeedsCleanup(entry);
                });
            }

            int index;
            if (freeCount != 0)
            {TRACE_IT(20715);
                Assert(freeCount > 0);
                Assert(freeList >= 0);
                Assert(freeList < count);
                index = freeList;
                freeCount--;
                if(freeCount != 0)
                {TRACE_IT(20716);
                    freeList = GetNextFreeEntryIndex(entries[index]);
                }
            }
            else
            {TRACE_IT(20717);
                // If there's nothing free, then in general, we set index to count, and increment count
                // If we resize, we also need to recalculate the target
                // However, if cleanup is supported, then before resize, we should try and clean up and see
                // if something got freed, and if it did, reuse that index
                if (count == size)
                {TRACE_IT(20718);
                    Resize();
                    targetBucket = this->GetBucket(hashCode);
                    index = count;
                    count++;
                }
                else
                {TRACE_IT(20719);
                    index = count;
                    count++;
                }

                Assert(count <= size);
                Assert(index < size);
            }

            entries[index].Set(key, value, hashCode);
            entries[index].next = buckets[targetBucket];
            buckets[targetBucket] = index;

#if PROFILE_DICTIONARY
            int profileIndex = index;
            uint depth = 1;  // need to recalculate depth in case there was a resize (also 1-based for stats->Insert)
            while(entries[profileIndex].next != -1)
            {TRACE_IT(20720);
                profileIndex = entries[profileIndex].next;
                ++depth;
            }
            if (stats)
                stats->Insert(depth);
#endif
            return index;
        }

        void Resize()
        {TRACE_IT(20721);
            AutoDoResize autoDoResize(*this);

            int newSize = SizePolicy::GetNextSize(count);
            uint newBucketCount = SizePolicy::GetBucketSize(newSize);

            __analysis_assume(newSize > count);
            int* newBuckets = nullptr;
            EntryType* newEntries = nullptr;
            if (newBucketCount == bucketCount)
            {TRACE_IT(20722);
                // no need to rehash
                newEntries = AllocateEntries(newSize);
                CopyArray<EntryType, Field(ValueType, TAllocator), TAllocator>(
                    newEntries, newSize, entries, count);

                DeleteEntries(entries, size);

                this->entries = newEntries;
                this->size = newSize;
                return;
            }

            Allocate(&newBuckets, &newEntries, newBucketCount, newSize);
            CopyArray<EntryType, Field(ValueType, TAllocator), TAllocator>(
                newEntries, newSize, entries, count);

            // When TAllocator is of type Recycler, it is possible that the Allocate above causes a collection, which
            // in turn can cause entries in the dictionary to be removed - i.e. the dictionary contains weak references
            // that remove themselves when no longer valid. This means the free list might not be empty anymore.
            for (int i = 0; i < count; i++)
            {TRACE_IT(20723);
                __analysis_assume(i < newSize);

                if (!IsFreeEntry(newEntries[i]))
                {TRACE_IT(20724);
                    uint hashCode = newEntries[i].template GetHashCode<Comparer<TKey>>();
                    int bucket = GetBucket(hashCode, newBucketCount);
                    newEntries[i].next = newBuckets[bucket];
                    newBuckets[bucket] = i;
                }
            }

            DeleteBuckets(buckets, bucketCount);
            DeleteEntries(entries, size);

#if PROFILE_DICTIONARY
            if (stats)
                stats->Resize(newSize, /*emptyBuckets=*/ newSize - size);
#endif
            this->buckets = newBuckets;
            this->entries = newEntries;
            bucketCount = newBucketCount;
            size = newSize;
        }

        __ecount(bucketCount) int *AllocateBuckets(DECLSPEC_GUARD_OVERFLOW const uint bucketCount)
        {TRACE_IT(20725);
            return
                AllocateArray<AllocatorType, int, false>(
                    TRACK_ALLOC_INFO(alloc, int, AllocatorType, 0, bucketCount),
                    TypeAllocatorFunc<AllocatorType, int>::GetAllocFunc(),
                    bucketCount);
        }

        __ecount(size) EntryType * AllocateEntries(DECLSPEC_GUARD_OVERFLOW int size, const bool zeroAllocate = true)
        {TRACE_IT(20726);
            // Note that the choice of leaf/non-leaf node is decided for the EntryType on the basis of TValue. By default, if
            // TValue is a pointer, a non-leaf allocation is done. This behavior can be overridden by specializing
            // TypeAllocatorFunc for TValue.
            return
                AllocateArray<AllocatorType, EntryType, false>(
                    TRACK_ALLOC_INFO(alloc, EntryType, AllocatorType, 0, size),
                    zeroAllocate ? EntryAllocatorFuncType::GetAllocZeroFunc() : EntryAllocatorFuncType::GetAllocFunc(),
                    size);
        }

        void DeleteBuckets(__in_ecount(bucketCount) int *const buckets, const uint bucketCount)
        {TRACE_IT(20727);
            Assert(buckets);
            Assert(bucketCount != 0);

            AllocatorFree(alloc, (TypeAllocatorFunc<AllocatorType, int>::GetFreeFunc()), buckets, bucketCount * sizeof(int));
        }

        void DeleteEntries(__in_ecount(size) EntryType *const entries, const int size)
        {TRACE_IT(20728);
            Assert(entries);
            Assert(size != 0);

            AllocatorFree(alloc, EntryAllocatorFuncType::GetFreeFunc(), entries, size * sizeof(EntryType));
        }

        void Allocate(__deref_out_ecount(bucketCount) int** ppBuckets, __deref_out_ecount(size) EntryType** ppEntries, DECLSPEC_GUARD_OVERFLOW uint bucketCount, DECLSPEC_GUARD_OVERFLOW int size)
        {TRACE_IT(20729);
            int *const buckets = AllocateBuckets(bucketCount);
            Assert(buckets); // no-throw allocators are currently not supported

            EntryType *entries;
            try
            {TRACE_IT(20730);
                entries = AllocateEntries(size);
                Assert(entries); // no-throw allocators are currently not supported
            }
            catch(...)
            {
                DeleteBuckets(buckets, bucketCount);
                throw;
            }

            memset(buckets, -1, bucketCount * sizeof(buckets[0]));

            *ppBuckets = buckets;
            *ppEntries = entries;
        }

        inline void RemoveAt(const int i, const int last, const uint targetBucket)
        {TRACE_IT(20731);
            if (last < 0)
            {TRACE_IT(20732);
                buckets[targetBucket] = entries[i].next;
            }
            else
            {TRACE_IT(20733);
                entries[last].next = entries[i].next;
            }
            entries[i].Clear();
            SetNextFreeEntryIndex(entries[i], freeCount == 0 ? -1 : freeList);
            freeList = i;
            freeCount++;
#if PROFILE_DICTIONARY
            if (stats)
                stats->Remove(buckets[targetBucket] == -1);
#endif
        }

#if DBG_DUMP
    public:
        void Dump()
        {TRACE_IT(20734);
            printf("Dumping Dictionary\n");
            printf("-------------------\n");
            for (uint i = 0; i < bucketCount; i++)
            {TRACE_IT(20735);
                printf("Bucket value: %d\n", buckets[i]);
                for (int j = buckets[i]; j >= 0; j = entries[j].next)
                {TRACE_IT(20736);
                    printf("%d  => %d  Next: %d\n", entries[j].Key(), entries[j].Value(), entries[j].next);
                }
            }
        }
#endif

    protected:
        template<class TDictionary, class Leaf>
        class IteratorBase _ABSTRACT
        {
        protected:
            EntryType *const entries;
            int entryIndex;

        #if DBG
        protected:
            TDictionary &dictionary;
        private:
            int usedEntryCount;
        #endif

        protected:
            IteratorBase(TDictionary &dictionary, const int entryIndex)
                : entries(dictionary.entries),
                entryIndex(entryIndex)
            #if DBG
                ,
                dictionary(dictionary),
                usedEntryCount(dictionary.Count())
            #endif
            {
            }

        protected:
            void OnEntryRemoved()
            {TRACE_IT(20737);
                DebugOnly(--usedEntryCount);
            }

        private:
            bool IsValid_Virtual() const
            {TRACE_IT(20738);
                return static_cast<const Leaf *>(this)->IsValid();
            }

        protected:
            bool IsValid() const
            {TRACE_IT(20739);
                Assert(dictionary.entries == entries);
                Assert(dictionary.Count() == usedEntryCount);

                return true;
            }

        public:
            EntryType &Current() const
            {TRACE_IT(20740);
                Assert(IsValid_Virtual());
                Assert(!IsFreeEntry(entries[entryIndex]));

                return entries[entryIndex];
            }

            TKey CurrentKey() const
            {TRACE_IT(20741);
                return Current().Key();
            }

            const TValue &CurrentValue() const
            {TRACE_IT(20742);
                return Current().Value();
            }

            TValue &CurrentValueReference() const
            {TRACE_IT(20743);
                return Current().Value();
            }

            void SetCurrentValue(const TValue &value) const
            {TRACE_IT(20744);
            #if DBG
                // For BaseHashSet, save the key before changing the value to verify that the key does not change
                const TKey previousKey = CurrentKey();
                const hash_t previousHashCode = GetHashCode(previousKey);
            #endif

                Current().SetValue(value);

                Assert(Current().KeyEquals<Comparer<TKey>>(previousKey, previousHashCode));
            }
        };

    public:
        template<class TDictionary>
        class EntryIterator sealed : public IteratorBase<TDictionary, EntryIterator<TDictionary>>
        {
        private:
            typedef IteratorBase<TDictionary, EntryIterator<TDictionary>> Base;

        private:
            const int entryCount;

        public:
            EntryIterator(TDictionary &dictionary) : Base(dictionary, 0), entryCount(dictionary.count)
            {TRACE_IT(20745);
                if(IsValid() && IsFreeEntry(this->entries[this->entryIndex]))
                {TRACE_IT(20746);
                    MoveNext();
                }
            }

        public:
            bool IsValid() const
            {TRACE_IT(20747);
                Assert(this->dictionary.count == this->entryCount);
                Assert(this->entryIndex >= 0);
                Assert(this->entryIndex <= entryCount);

                return Base::IsValid() && this->entryIndex < this->entryCount;
            }

        public:
            void MoveNext()
            {TRACE_IT(20748);
                Assert(IsValid());

                do
                {TRACE_IT(20749);
                    ++(this->entryIndex);
                } while(IsValid() && IsFreeEntry(this->entries[this->entryIndex]));
            }
        };

    public:
        template<class TDictionary>
        class BucketEntryIterator sealed : public IteratorBase<TDictionary, BucketEntryIterator<TDictionary>>
        {
        private:
            typedef IteratorBase<TDictionary, BucketEntryIterator<TDictionary>> Base;

        private:
            TDictionary &dictionary;
            int *const buckets;
            const uint bucketCount;
            uint bucketIndex;
            int previousEntryIndexInBucket;
            int indexOfEntryAfterRemovedEntry;

        public:
            BucketEntryIterator(TDictionary &dictionary)
                : Base(dictionary, -1),
                dictionary(dictionary),
                buckets(dictionary.buckets),
                bucketCount(dictionary.bucketCount),
                bucketIndex(0u - 1)
            #if DBG
                ,
                previousEntryIndexInBucket(-2),
                indexOfEntryAfterRemovedEntry(-2)
            #endif
            {
                if(dictionary.Count() != 0)
                {TRACE_IT(20750);
                    MoveNextBucket();
                }
            }

        public:
            bool IsValid() const
            {TRACE_IT(20751);
                Assert(dictionary.buckets == buckets);
                Assert(dictionary.bucketCount == bucketCount);
                Assert(this->entryIndex >= -1);
                Assert(this->entryIndex < dictionary.count);
                Assert(bucketIndex == 0u - 1 || bucketIndex <= bucketCount);
                Assert(previousEntryIndexInBucket >= -2);
                Assert(previousEntryIndexInBucket < dictionary.count);
                Assert(indexOfEntryAfterRemovedEntry >= -2);
                Assert(indexOfEntryAfterRemovedEntry < dictionary.count);

                return Base::IsValid() && this->entryIndex >= 0;
            }

        public:
            void MoveNext()
            {TRACE_IT(20752);
                if(IsValid())
                {TRACE_IT(20753);
                    previousEntryIndexInBucket = this->entryIndex;
                    this->entryIndex = this->Current().next;
                }
                else
                {TRACE_IT(20754);
                    Assert(indexOfEntryAfterRemovedEntry >= -1);
                    this->entryIndex = indexOfEntryAfterRemovedEntry;
                }

                if(!IsValid())
                {TRACE_IT(20755);
                    MoveNextBucket();
                }
            }

        private:
            void MoveNextBucket()
            {TRACE_IT(20756);
                Assert(!IsValid());

                while(++bucketIndex < bucketCount)
                {TRACE_IT(20757);
                    this->entryIndex = buckets[bucketIndex];
                    if(IsValid())
                    {TRACE_IT(20758);
                        previousEntryIndexInBucket = -1;
                        break;
                    }
                }
            }

        public:
            void RemoveCurrent()
            {TRACE_IT(20759);
                Assert(previousEntryIndexInBucket >= -1);

                indexOfEntryAfterRemovedEntry = this->Current().next;
                dictionary.RemoveAt(this->entryIndex, previousEntryIndexInBucket, bucketIndex);
                this->OnEntryRemoved();
                this->entryIndex = -1;
            }
        };

        template<class TDictionary, class Leaf> friend class IteratorBase;
        template<class TDictionary> friend class EntryIterator;
        template<class TDictionary> friend class BucketEntryIterator;

        PREVENT_ASSIGN(BaseDictionary);
    };

    template <class TKey, class TValue> class SimpleHashedEntry;

    template <
        class TElement,
        class TAllocator,
        class SizePolicy = PowerOf2SizePolicy,
        class TKey = TElement,
        template <typename ValueOrKey> class Comparer = DefaultComparer,
        template <typename, typename> class Entry = SimpleHashedEntry,
        typename Lock = NoResizeLock
    >
    class BaseHashSet : protected BaseDictionary<TKey, TElement, TAllocator, SizePolicy, Comparer, Entry, Lock>
    {
        typedef BaseDictionary<TKey, TElement, TAllocator, SizePolicy, Comparer, Entry, Lock> Base;
        typedef typename Base::EntryType EntryType;
        typedef typename Base::AllocatorType AllocatorType;
        friend struct JsDiag::RemoteDictionary<BaseHashSet<TElement, TAllocator, SizePolicy, TKey, Comparer, Entry, Lock>>;

    public:
        BaseHashSet(AllocatorType * allocator, int capacity = 0) : Base(allocator, capacity) {TRACE_IT(20760);}

        using Base::GetAllocator;

        int Count() const
        {TRACE_IT(20761);
            return __super::Count();
        }

        int Add(TElement const& element)
        {TRACE_IT(20762);
            return __super::Add(ValueToKey<TKey, TElement>::ToKey(element), element);
        }

        // Add only if there isn't an existing element
        int AddNew(TElement const& element)
        {TRACE_IT(20763);
            return __super::AddNew(ValueToKey<TKey, TElement>::ToKey(element), element);
        }

        int Item(TElement const& element)
        {TRACE_IT(20764);
            return __super::Item(ValueToKey<TKey, TElement>::ToKey(element), element);
        }

        void Clear()
        {TRACE_IT(20765);
            __super::Clear();
        }

        using Base::Reset;

        TElement const& Lookup(TKey const& key)
        {TRACE_IT(20766);
            // Use a static to pass the null default value, since the
            // default value may get returned out of the current scope by ref.
            static const TElement nullElement = nullptr;
            return __super::Lookup(key, nullElement);
        }

        template <typename KeyType>
        TElement const& LookupWithKey(KeyType const& key)
        {TRACE_IT(20767);
            static const TElement nullElement = nullptr;

            return __super::LookupWithKey(key, nullElement);
        }

        bool Contains(TElement const& element) const
        {TRACE_IT(20768);
            return ContainsKey(ValueToKey<TKey, TElement>::ToKey(element));
        }

        using Base::ContainsKey;
        using Base::TryGetValue;
        using Base::TryGetReference;

        bool RemoveKey(const TKey &key)
        {TRACE_IT(20769);
            return Base::Remove(key);
        }

        bool Remove(TElement const& element)
        {TRACE_IT(20770);
            return __super::Remove(ValueToKey<TKey, TElement>::ToKey(element));
        }

        typename Base::template EntryIterator<const BaseHashSet> GetIterator() const
        {TRACE_IT(20771);
            return typename Base::template EntryIterator<const BaseHashSet>(*this);
        }

        typename Base::template EntryIterator<BaseHashSet> GetIterator()
        {TRACE_IT(20772);
            return typename Base::template EntryIterator<BaseHashSet>(*this);
        }

        typename Base::template BucketEntryIterator<BaseHashSet> GetIteratorWithRemovalSupport()
        {TRACE_IT(20773);
            return typename Base::template BucketEntryIterator<BaseHashSet>(*this);
        }

        template<class Fn>
        void Map(Fn fn)
        {TRACE_IT(20774);
            MapUntil([fn](TElement const& value) -> bool
            {
                fn(value);
                return false;
            });
        }

        template<class Fn>
        void MapAndRemoveIf(Fn fn)
        {TRACE_IT(20775);
            __super::MapAndRemoveIf([fn](EntryType const& entry) -> bool
            {
                return fn(entry.Value());
            });
        }

        template<class Fn>
        bool MapUntil(Fn fn)
        {TRACE_IT(20776);
            return __super::MapEntryUntil([fn](EntryType const& entry) -> bool
            {
                return fn(entry.Value());
            });
        }

        bool EnsureCapacity()
        {TRACE_IT(20777);
            return __super::EnsureCapacity();
        }

        int GetNextIndex()
        {TRACE_IT(20778);
            return __super::GetNextIndex();
        }

        int GetLastIndex()
        {TRACE_IT(20779);
            return __super::GetLastIndex();
        }

        using Base::GetValueAt;

        bool TryGetValueAt(int index, TElement * value) const
        {TRACE_IT(20780);
            return __super::TryGetValueAt(index, value);
        }

        BaseHashSet *Clone()
        {TRACE_IT(20781);
            return AllocatorNew(AllocatorType, this->alloc, BaseHashSet, *this);
        }

        void Copy(const BaseHashSet *const other)
        {TRACE_IT(20782);
            this->DoCopy(other);
        }

        void LockResize()
        {TRACE_IT(20783);
            __super::LockResize();
        }

        void UnlockResize()
        {TRACE_IT(20784);
            __super::UnlockResize();
        }

        friend Base;
        PREVENT_ASSIGN(BaseHashSet);
    };

    template <
        class TKey,
        class TValue,
        class TAllocator,
        class SizePolicy = PowerOf2SizePolicy,
        template <typename ValueOrKey> class Comparer = DefaultComparer,
        template <typename K, typename V> class Entry = SimpleDictionaryEntry,
        class LockPolicy = Js::DefaultContainerLockPolicy,   // Controls lock policy for read/map/write/add/remove items
        class SyncObject = CriticalSection
    >
    class SynchronizedDictionary: protected BaseDictionary<TKey, TValue, TAllocator, SizePolicy, Comparer, Entry>
    {
    private:
        FieldNoBarrier(SyncObject*) syncObj;

        typedef BaseDictionary<TKey, TValue, TAllocator, SizePolicy, Comparer, Entry> Base;
    public:
        typedef TKey KeyType;
        typedef TValue ValueType;
        typedef typename Base::AllocatorType AllocatorType;
        typedef typename Base::EntryType EntryType;
        typedef SynchronizedDictionary<TKey, TValue, TAllocator, SizePolicy, Comparer, Entry, LockPolicy, SyncObject> DictionaryType;
    private:
        friend class Js::RemoteDictionary<DictionaryType>;

    public:
        SynchronizedDictionary(AllocatorType * allocator, int capacity, SyncObject* syncObject):
            Base(allocator, capacity),
            syncObj(syncObject)
        {TRACE_IT(20785);}

#ifdef DBG
        void Dump()
        {TRACE_IT(20786);
            typename LockPolicy::ReadLock autoLock(syncObj);

            __super::Dump();
        }
#endif

        TAllocator *GetAllocator() const
        {TRACE_IT(20787);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::GetAllocator();
        }

        inline int Count() const
        {TRACE_IT(20788);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::Count();
        }

        inline int Capacity() const
        {TRACE_IT(20789);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::Capacity();
        }

        TValue Item(const TKey& key)
        {TRACE_IT(20790);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::Item(key);
        }

        bool IsInAdd()
        {TRACE_IT(20791);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::IsInAdd();
        }

        int Add(const TKey& key, const TValue& value)
        {TRACE_IT(20792);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Add(key, value);
        }

        int AddNew(const TKey& key, const TValue& value)
        {TRACE_IT(20793);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::AddNew(key, value);
        }

        int Item(const TKey& key, const TValue& value)
        {TRACE_IT(20794);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Item(key, value);
        }

        bool Contains(KeyValuePair<TKey, TValue> keyValuePair)
        {TRACE_IT(20795);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::Contains(keyValuePair);
        }

        bool Remove(KeyValuePair<TKey, TValue> keyValuePair)
        {TRACE_IT(20796);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Remove(keyValuePair);
        }

        void Clear()
        {TRACE_IT(20797);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Clear();
        }

        void Reset()
        {TRACE_IT(20798);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Reset();
        }

        bool ContainsKey(const TKey& key)
        {TRACE_IT(20799);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::ContainsKey(key);
        }

        template <typename TLookup>
        inline const TValue& LookupWithKey(const TLookup& key, const TValue& defaultValue)
        {TRACE_IT(20800);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::LookupWithKey(key, defaultValue);
        }

        inline const TValue& Lookup(const TKey& key, const TValue& defaultValue)
        {TRACE_IT(20801);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::Lookup(key, defaultValue);
        }

        template <typename TLookup>
        bool TryGetValue(const TLookup& key, TValue* value)
        {TRACE_IT(20802);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::TryGetValue(key, value);
        }

        bool TryGetValueAndRemove(const TKey& key, TValue* value)
        {TRACE_IT(20803);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::TryGetValueAndRemove(key, value);
        }

        template <typename TLookup>
        inline bool TryGetReference(const TLookup& key, TValue** value)
        {TRACE_IT(20804);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::TryGetReference(key, value);
        }

        template <typename TLookup>
        inline bool TryGetReference(const TLookup& key, TValue** value, int* index)
        {TRACE_IT(20805);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::TryGetReference(key, value, index);
        }

        const TValue& GetValueAt(const int& index) const
        {TRACE_IT(20806);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::GetValueAt(index);
        }

        TValue* GetReferenceAt(const int& index)
        {TRACE_IT(20807);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::GetReferenceAt(index);
        }

        TKey const& GetKeyAt(const int& index)
        {TRACE_IT(20808);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::GetKeyAt(index);
        }

        bool Remove(const TKey& key)
        {TRACE_IT(20809);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::Remove(key);
        }

        template<class Fn>
        void MapReference(Fn fn)
        {TRACE_IT(20810);
            // TODO: Verify that Map doesn't actually modify the list
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapReference(fn);
        }

        template<class Fn>
        bool MapUntilReference(Fn fn) const
        {TRACE_IT(20811);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapUntilReference(fn);
        }

        template<class Fn>
        void MapAddress(Fn fn) const
        {TRACE_IT(20812);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapAddress(fn);
        }

        template<class Fn>
        bool MapUntilAddress(Fn fn) const
        {TRACE_IT(20813);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapUntilAddress(fn);
        }

        template<class Fn>
        void Map(Fn fn) const
        {TRACE_IT(20814);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::Map(fn);
        }

        template<class Fn>
        bool MapUntil(Fn fn) const
        {TRACE_IT(20815);
            typename LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapUntil(fn);
        }

        template<class Fn>
        void MapAndRemoveIf(Fn fn)
        {TRACE_IT(20816);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::MapAndRemoveIf(fn);
        }

        PREVENT_COPY(SynchronizedDictionary);
    };
}
