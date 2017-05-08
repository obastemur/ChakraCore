//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <class TKey, class TValue> class WeakRefDictionaryEntry
    {
    public:
        static const int INVALID_HASH_VALUE = 0;
        hash_t hash;    // Lower 31 bits of hash code << 1 | 1, 0 if unused
        int next;        // Index of next entry, -1 if last
        Field(const RecyclerWeakReference<TKey>*) key;  // Key of entry- this entry holds a weak reference to the key
        TValue value;    // Value of entry
    };

    // TODO: convert to BaseDictionary- easier now to have custom dictionary since this does compacting
    // and weak reference resolution
    template <class TKey, class TValue, class KeyComparer = DefaultComparer<const TKey*>, bool cleanOnInsert = true> class WeaklyReferencedKeyDictionary
    {
    public:
        typedef WeakRefDictionaryEntry<TKey, Field(TValue)> EntryType;
        typedef TKey KeyType;
        typedef TValue ValueType;
        typedef void (*EntryRemovalCallbackMethodType)(const EntryType& e, void* cookie);

        struct EntryRemovalCallback
        {
            FieldNoBarrier(EntryRemovalCallbackMethodType) fnCallback;
            Field(void*) cookie;
        };


    private:
        Field(int) size;
        Field(int*) buckets;
        Field(EntryType *) entries;
        Field(int) count;
        Field(int) version;
        Field(int) freeList;
        Field(int) freeCount;
        FieldNoBarrier(Recycler*) recycler;
        FieldNoBarrier(EntryRemovalCallback) entryRemovalCallback;
        Field(uint) lastWeakReferenceCleanupId;
        Field(bool) disableCleanup;

    public:
        // Allow WeaklyReferencedKeyDictionary field to be inlined in classes with DEFINE_VTABLE_CTOR_MEMBER_INIT
        WeaklyReferencedKeyDictionary(VirtualTableInfoCtorEnum) {TRACE_IT(21132); }

        WeaklyReferencedKeyDictionary(Recycler* recycler, int capacity = 0, EntryRemovalCallback* pEntryRemovalCallback = nullptr):
            buckets(nullptr),
            size(0),
            entries(nullptr),
            count(0),
            version(0),
            freeList(0),
            freeCount(0),
            recycler(recycler),
            lastWeakReferenceCleanupId(recycler->GetWeakReferenceCleanupId()),
            disableCleanup(false)
        {TRACE_IT(21133);
            if (pEntryRemovalCallback != nullptr)
            {TRACE_IT(21134);
                this->entryRemovalCallback.fnCallback = pEntryRemovalCallback->fnCallback;
                this->entryRemovalCallback.cookie = pEntryRemovalCallback->cookie;
            }
            else
            {TRACE_IT(21135);
                this->entryRemovalCallback.fnCallback = nullptr;
            }

            if (capacity > 0) {TRACE_IT(21136); Initialize(capacity); }
        }

        ~WeaklyReferencedKeyDictionary()
        {TRACE_IT(21137);
        }

        inline int Count()
        {TRACE_IT(21138);
            return count - freeCount;
        }

        TValue Item(TKey* key)
        {TRACE_IT(21139);
            int i = FindEntry(key);
            if (i >= 0) return entries[i].value;
            Js::Throw::FatalInternalError();
        }

        void Item(TKey* key, const TValue value)
        {
            Insert(key, value, false);
        }

        const TValue& GetValueAt(const int& index) const
        {TRACE_IT(21140);
            if (index >= 0 && index < count)
            {TRACE_IT(21141);
                return entries[index].value;
            }
            Js::Throw::FatalInternalError();
        }

        bool TryGetValue(const TKey* key, TValue* value)
        {TRACE_IT(21142);
            int i = FindEntry<TKey>(key);
            if (i >= 0)
            {TRACE_IT(21143);
                *value = entries[i].value;
                return true;
            }
            return false;
        }

        bool TryGetValueAndRemove(const TKey* key, TValue* value)
        {TRACE_IT(21144);
            if (buckets == nullptr) return false;

            hash_t hash = GetHashCode(key);
            uint targetBucket = hash % size;
            int last = -1;
            int i = 0;

            if ((i = FindEntry<TKey>(key, hash, targetBucket, last)) != -1)
            {TRACE_IT(21145);
                *value = entries[i].value;
                RemoveEntry(i, last, targetBucket);
                return true;
            }

            return false;
        }

        template <typename TLookup>
        inline TValue Lookup(const TLookup* key, TValue defaultValue, __out TKey const** pKeyOut)
        {TRACE_IT(21146);
            int i = FindEntry(key);
            if (i >= 0)
            {TRACE_IT(21147);
                (*pKeyOut) = entries[i].key->Get();
                return entries[i].value;
            }
            (*pKeyOut) = nullptr;
            return defaultValue;
        }

        inline TValue Lookup(const TKey* key, TValue defaultValue)
        {TRACE_IT(21148);
            int i = FindEntry(key);
            if (i >= 0)
            {TRACE_IT(21149);
                return entries[i].value;
            }
            return defaultValue;
        }

        const RecyclerWeakReference<TKey>* Add(TKey* key, TValue value)
        {TRACE_IT(21150);
            return Insert(key, value, true);
        }

        const RecyclerWeakReference<TKey>* UncheckedAdd(TKey* key, TValue value)
        {TRACE_IT(21151);
            return Insert(key, value, true, false);
        }

        const RecyclerWeakReference<TKey>* UncheckedAdd(const RecyclerWeakReference<TKey>* weakRef, TValue value)
        {TRACE_IT(21152);
            return UncheckedInsert(weakRef, value);
        }

        template<class Fn>
        void Map(Fn fn)
        {TRACE_IT(21153);
            for(int i = 0; i < size; i++)
            {TRACE_IT(21154);
                if(buckets[i] != -1)
                {TRACE_IT(21155);
                    for(int previousIndex = -1, currentIndex = buckets[i]; currentIndex != -1;)
                    {TRACE_IT(21156);
                        EntryType &currentEntry = entries[currentIndex];
                        TKey * key = currentEntry.key->Get();
                        if(key != nullptr)
                        {
                            fn(key, currentEntry.value, currentEntry.key);

                            // Keep the entry
                            previousIndex = currentIndex;
                            currentIndex = currentEntry.next;
                        }
                        else
                        {TRACE_IT(21157);
                            // Remove the entry
                            const int nextIndex = currentEntry.next;
                            RemoveEntry(currentIndex, previousIndex, i);
                            currentIndex = nextIndex;
                        }
                    }
                }
            }
        }

        void SetDisableCleanup(bool disableCleanup)
        {TRACE_IT(21158);
            this->disableCleanup = disableCleanup;
        }

        bool GetDisableCleanup()
        {TRACE_IT(21159);
            return this->disableCleanup;
        }

        bool Clean()
        {TRACE_IT(21160);
            if (!disableCleanup && recycler->GetWeakReferenceCleanupId() != this->lastWeakReferenceCleanupId)
            {TRACE_IT(21161);
                Map([](TKey * key, TValue value, const RecyclerWeakReference<TKey>* weakRef) {});
                this->lastWeakReferenceCleanupId = recycler->GetWeakReferenceCleanupId();
            }

            return freeCount > 0;
        }

        void Clear()
        {TRACE_IT(21162);
            if (count > 0)
            {TRACE_IT(21163);
                for (int i = 0; i < size; i++) buckets[i] = -1;
                ClearArray(entries, size);
                freeList = -1;
                count = 0;
                freeCount = 0;
            }
        }

        void EnsureCapacity()
        {TRACE_IT(21164);
            if (freeCount == 0 && count == size)
            {TRACE_IT(21165);
                if (cleanOnInsert && Clean())
                {TRACE_IT(21166);
                    Assert(freeCount > 0);
                }
                else
                {TRACE_IT(21167);
                    Resize();
                }
            }
        }

    private:
        const RecyclerWeakReference<TKey>* UncheckedInsert(const RecyclerWeakReference<TKey>* weakRef, TValue value)
        {TRACE_IT(21168);
            if (buckets == nullptr) Initialize(0);

            int hash = GetHashCode(weakRef->FastGet());
            uint bucket = (uint)hash % size;

            Assert(FindEntry(weakRef->FastGet()) == -1);
            return Insert(weakRef, value, hash, bucket);
        }

        const RecyclerWeakReference<TKey>* Insert(TKey* key, TValue value, bool add, bool checkForExisting = true)
        {TRACE_IT(21169);
            if (buckets == nullptr) Initialize(0);

            hash_t hash = GetHashCode(key);
            uint bucket = hash % size;

            if (checkForExisting)
            {TRACE_IT(21170);
                int previous = -1;
                int i = FindEntry(key, hash, bucket, previous);

                if (i != -1)
                {TRACE_IT(21171);
                    if (add)
                    {TRACE_IT(21172);
                        Js::Throw::FatalInternalError();
                    }

                    entries[i].value = value;
                    version++;
                    return entries[i].key;
                }
            }

            // We know we need to insert- so first try creating the weak reference, before adding it to
            // the dictionary. If we OOM here, we still leave the dictionary as we found it.
            const RecyclerWeakReference<TKey>* weakRef = recycler->CreateWeakReferenceHandle<TKey>(key);

            return Insert(weakRef, value, hash, bucket);
        }

        const RecyclerWeakReference<TKey>* Insert(const RecyclerWeakReference<TKey>* weakRef, TValue value, int hash, uint bucket)
        {TRACE_IT(21173);
            int index;
            if (freeCount > 0)
            {TRACE_IT(21174);
                index = freeList;
                freeList = entries[index].next;
                freeCount--;
            }
            else
            {TRACE_IT(21175);
                if (count == size)
                {TRACE_IT(21176);
                    if (cleanOnInsert && Clean())
                    {TRACE_IT(21177);
                        index = freeList;
                        freeList = entries[index].next;
                        freeCount--;
                    }
                    else
                    {TRACE_IT(21178);
                        Resize();
                        bucket = (uint)hash % size;
                        index = count;
                        count++;
                    }
                }
                else
                {TRACE_IT(21179);
                    index = count;
                    count++;
                }
            }

            entries[index].next = buckets[bucket];
            entries[index].key = weakRef;
            entries[index].hash = hash;
            entries[index].value = value;
            buckets[bucket] = index;
            version++;

            return entries[index].key;
        }

        void Resize()
        {TRACE_IT(21180);
            int newSize = PrimePolicy::GetSize(count * 2);

            if (newSize <= count)
            {TRACE_IT(21181);
                // throw OOM if we can't increase the dictionary size
                Js::Throw::OutOfMemory();
            }

            int* newBuckets = RecyclerNewArrayLeaf(recycler, int, newSize);
            for (int i = 0; i < newSize; i++) newBuckets[i] = -1;
            EntryType* newEntries = RecyclerNewArray(recycler, EntryType, newSize);
            CopyArray<EntryType, Field(const RecyclerWeakReference<TKey>*)>(newEntries, newSize, entries, count);
            AnalysisAssert(count < newSize);
            for (int i = 0; i < count; i++)
            {TRACE_IT(21182);
                uint bucket = (uint)newEntries[i].hash % newSize;
                newEntries[i].next = newBuckets[bucket];
                newBuckets[bucket] = i;
            }
            buckets = newBuckets;
            size = newSize;
            entries = newEntries;
        }

        template <typename TLookup>
        inline hash_t GetHashCode(const TLookup* key)
        {TRACE_IT(21183);
            return TAGHASH(KeyComparer::GetHashCode(key));
        }

        template <typename TLookup>
        inline int FindEntry(const TLookup* key)
        {TRACE_IT(21184);
            if (buckets != nullptr)
            {TRACE_IT(21185);
                hash_t hash = GetHashCode(key);
                uint bucket = (uint)hash % size;
                int previous = -1;
                return FindEntry(key, hash, bucket, previous);
            }

            return -1;
        }

        template <typename TLookup>
        inline int FindEntry(const TLookup* key, hash_t const hash, uint& bucket, int& previous)
        {TRACE_IT(21186);
            if (buckets != nullptr)
            {TRACE_IT(21187);
                BOOL inSweep = this->recycler->IsSweeping();
                previous = -1;
                for (int i = buckets[bucket]; i >= 0; )
                {TRACE_IT(21188);
                    if (entries[i].hash == hash)
                    {TRACE_IT(21189);
                        TKey* strongRef = nullptr;

                        if (!inSweep)
                        {TRACE_IT(21190);
                            // Quickly check for null if we're not in sweep- if it's null, it's definitely been collected
                            // so remove
                            strongRef = entries[i].key->FastGet();
                        }
                        else
                        {TRACE_IT(21191);
                            // If we're in sweep, use the slower Get which checks if the object is getting collected
                            // This could return null too but we won't clean it up now, we'll clean it up later
                            strongRef = entries[i].key->Get();
                        }

                        if (strongRef == nullptr)
                        {TRACE_IT(21192);
                            i = RemoveEntry(i, previous, bucket);
                            continue;
                        }
                        else
                        {TRACE_IT(21193);
                            // if we get here, strongRef is not null
                            if (KeyComparer::Equals(strongRef, key))
                                return i;
                        }
                    }

                    previous = i;
                    i = entries[i].next;
                }
            }
            return -1;
        }

        void Initialize(int capacity)
        {TRACE_IT(21194);
            int size = PrimePolicy::GetSize(capacity);

            int* buckets = RecyclerNewArrayLeaf(recycler, int, size);
            EntryType * entries = RecyclerNewArray(recycler, EntryType, size);

            // No need for auto pointers here since these are both recycler
            // allocated objects
            if (buckets != nullptr && entries != nullptr)
            {TRACE_IT(21195);
                this->size = size;
                this->buckets = buckets;
                for (int i = 0; i < size; i++) buckets[i] = -1;
                this->entries = entries;
                this->freeList = -1;
            }
        }

        int RemoveEntry(int i, int previous, uint bucket)
        {TRACE_IT(21196);
            int next = entries[i].next;

            if (previous < 0) // Previous < 0 => first node
            {TRACE_IT(21197);
                buckets[bucket] = entries[i].next;
            }
            else
            {TRACE_IT(21198);
                entries[previous].next = entries[i].next;
            }

            if (this->entryRemovalCallback.fnCallback != nullptr)
            {TRACE_IT(21199);
                this->entryRemovalCallback.fnCallback(entries[i], this->entryRemovalCallback.cookie);
            }

            entries[i].next = freeList;
            entries[i].key = nullptr;
            entries[i].hash = EntryType::INVALID_HASH_VALUE;
            // Hold onto the pid here so that we can reuse it
            // entries[i].value = nullptr;
            freeList = i;
            freeCount++;
            version++;

            return next;
        }
    };
}
