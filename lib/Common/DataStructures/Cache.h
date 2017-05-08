//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <typename T, uint size, class TAllocator>
    class CircularBuffer
    {
    public:
        CircularBuffer():
          writeIndex(0),
          filled(false)
        {TRACE_IT(20911);
        }

        void Clear()
        {TRACE_IT(20912);
            this->writeIndex = 0;
            this->filled = false;
        }

        void Add(const T& value)
        {TRACE_IT(20913);
            if (!Contains(value))
            {TRACE_IT(20914);
                entries[writeIndex] = value;
                uint nextIndex = (writeIndex + 1) % size;
                if (nextIndex < writeIndex && !filled)
                {TRACE_IT(20915);
                    filled = true;
                }

                writeIndex = nextIndex;
            }
        }

        bool Contains(const T& value)
        {TRACE_IT(20916);
            for (uint i = 0; i < GetMaxIndex(); i++)
            {TRACE_IT(20917);
                if (DefaultComparer<T>::Equals(entries[i], value))
                {TRACE_IT(20918);
                    return true;
                }
            }

            return false;
        }

        uint GetMaxIndex()
        {TRACE_IT(20919);
            return (filled ? size : writeIndex);
        }

        const T& Item(uint index)
        {TRACE_IT(20920);
            Assert(index < GetMaxIndex());

            return entries[index];
        }

#ifdef VERBOSE_EVAL_MAP
        void Dump()
        {TRACE_IT(20921);
            Output::Print(_u("Length: %d, writeIndex: %d, filled: %d\n"), size, writeIndex, filled);
            for (uint i = 0; i < GetMaxIndex(); i++)
            {TRACE_IT(20922);
                Output::Print(_u("Item %d: %s\n"), i, entries[i].str.GetBuffer());
            }
            Output::Flush();
        }
#endif

        bool IsEmpty()
        {TRACE_IT(20923);
            return (writeIndex == 0 && !filled);
        }

        int GetCount()
        {TRACE_IT(20924);
            if (!filled) return writeIndex;

            return size;
        }

    private:
        Field(uint) writeIndex;
        Field(bool) filled;
        Field(T, TAllocator) entries[size];
    };

    template <class TKey, int MRUSize, class TAllocator = Recycler>
    class MRURetentionPolicy
    {
    public:
        typedef CircularBuffer<TKey, MRUSize, TAllocator> TMRUStoreType;
        MRURetentionPolicy(TAllocator* allocator)
        {TRACE_IT(20925);
            store = AllocatorNew(TAllocator, allocator, TMRUStoreType);
        }

        void NotifyAdd(const TKey& key)
        {TRACE_IT(20926);
            this->store->Add(key);
        }

        bool CanEvict(const TKey& key)
        {TRACE_IT(20927);
            return !store->Contains(key);
        }

        void DumpKeepAlives()
        {TRACE_IT(20928);
            store->Dump();
        }

    private:
        Field(TMRUStoreType*, TAllocator) store;
    };

    template <
        class TKey,
        class TValue,
        class TAllocator,
        class SizePolicy,
        class CacheRetentionPolicy,
        template <typename ValueOrKey> class Comparer = DefaultComparer,
        template <typename K, typename V> class Entry = SimpleDictionaryEntry
    >
    class Cache
    {
    private:
        typedef BaseDictionary<TKey, TValue, TAllocator, SizePolicy, Comparer, Entry> TCacheStoreType;
        typedef typename TCacheStoreType::AllocatorType AllocatorType;
        class CacheStore : public TCacheStoreType
        {
        public:
            CacheStore(AllocatorType* allocator, int capacity) : TCacheStoreType(allocator, capacity), inAdd(false) {TRACE_IT(20929);};
            bool IsInAdd()
            {TRACE_IT(20930);
                return this->inAdd;
            }
            int Add(const TKey& key, const TValue& value)
            {TRACE_IT(20931);
                AutoRestoreValue<bool> var(&this->inAdd, true);

                return __super::Add(key, value);
            }
            void SetIsInAdd(bool value) {TRACE_IT(20932);inAdd = value; }
        private:
            bool inAdd;
        };
    public:
        typedef TKey KeyType;
        typedef TValue ValueType;
        typedef void (*OnItemEvictedCallback)(const TKey& key, TValue value);

        Cache(AllocatorType * allocator, int capacity = 0):
            cachePolicyType(allocator)
        {TRACE_IT(20933);
            this->cacheStore = AllocatorNew(AllocatorType, allocator, CacheStore, allocator, capacity);
        }

        int Add(const TKey& key, const TValue& value)
        {TRACE_IT(20934);
            int index = this->cacheStore->Add(key, value);
            this->cachePolicyType.NotifyAdd(key);

            return index;
        }

        void SetIsInAdd(bool value) {TRACE_IT(20935);this->cacheStore->SetIsInAdd(value); }

        void NotifyAdd(const TKey& key)
        {TRACE_IT(20936);
            this->cachePolicyType.NotifyAdd(key);
        }

        bool TryGetValue(const TKey& key, TValue* value)
        {TRACE_IT(20937);
            return cacheStore->TryGetValue(key, value);
        }

        bool TryGetReference(const TKey& key, const TValue** value, int* index)
        {TRACE_IT(20938);
            return cacheStore->TryGetReference(key, value, index);
        }

        bool TryGetValueAndRemove(const TKey& key, TValue* value)
        {TRACE_IT(20939);
            return cacheStore->TryGetValueAndRemove(key, value);
        }

        TKey const& GetKeyAt(const int& index)
        {TRACE_IT(20940);
            return cacheStore->GetKeyAt(index);
        }

        template <class Fn>
        void Clean(Fn callback)
        {TRACE_IT(20941);
            if (!this->cacheStore->IsInAdd())
            {TRACE_IT(20942);
                // Queue up items to be removed
                // TODO: Don't use Contains since that's linear- store pointers to the eval map key instead, and set a bit indicating that its in the dictionary?
                cacheStore->MapAndRemoveIf([this, callback](const typename CacheStore::EntryType &entry) {
                    if (this->cachePolicyType.CanEvict(entry.Key()) || CONFIG_FLAG(ForceCleanCacheOnCollect))
                    {TRACE_IT(20943);
                        callback(entry.Key(), entry.Value());

                        if (!CONFIG_FLAG(ForceCleanCacheOnCollect))
                        {TRACE_IT(20944);
                            return true;
                        }
                    }
                    return false;
                });

                if (CONFIG_FLAG(ForceCleanCacheOnCollect))
                {TRACE_IT(20945);
                    this->cacheStore->Clear();
                    Assert(this->cacheStore->Count() == 0);
                }
            }
        }

        template <class Fn>
        void CleanAll(Fn callback)
        {TRACE_IT(20946);
            Assert(!this->cacheStore->IsInAdd());
            cacheStore->MapAndRemoveIf([this, callback](const CacheStore::EntryType &entry) -> bool {
                callback(entry.Key(), entry.Value());
                return true;
            });
        }

        void DumpKeepAlives()
        {TRACE_IT(20947);
            cachePolicyType.DumpKeepAlives();
        }

    private:
        Field(CacheStore*) cacheStore;
        Field(CacheRetentionPolicy) cachePolicyType;
    };

}
