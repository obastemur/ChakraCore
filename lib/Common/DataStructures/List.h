//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <class TListType, bool clearOldEntries>
    class CopyRemovePolicy;

    template <typename TListType, bool clearOldEntries>
    class FreeListedRemovePolicy;

    template <typename TListType, bool clearOldEntries>
    class WeakRefFreeListedRemovePolicy;
};

namespace JsUtil
{
    template <
        class T,
        class TAllocator = Recycler,
        template <typename Value> class TComparer = DefaultComparer>
    class ReadOnlyList
    {
    public:
        typedef TComparer<T> TComparerType;

    protected:
        Field(Field(T, TAllocator) *, TAllocator) buffer;
        Field(int) count;
        FieldNoBarrier(TAllocator*) alloc;

        ReadOnlyList(TAllocator* alloc)
            : buffer(nullptr),
            count(0),
            alloc(alloc)
        {TRACE_IT(21784);
        }

    public:
        virtual bool IsReadOnly() const
        {TRACE_IT(21785);
            return true;
        }

        virtual void Delete()
        {
            AllocatorDelete(TAllocator, alloc, this);
        }

        const T* GetBuffer() const
        {TRACE_IT(21786);
            return AddressOf(this->buffer[0]);
        }

        template<class TList>
        bool Equals(TList list)
        {TRACE_IT(21787);
            CompileAssert(sizeof(T) == sizeof(*list->GetBuffer()));
            return list->Count() == this->Count()
                && memcmp(this->buffer, list->GetBuffer(), sizeof(T)* this->Count()) == 0;
        }

        template<class TAllocator>
        static ReadOnlyList * New(TAllocator* alloc, __in_ecount(count) T* buffer, DECLSPEC_GUARD_OVERFLOW int count)
        {TRACE_IT(21788);
            return AllocatorNew(TAllocator, alloc, ReadOnlyList, buffer, count, alloc);
        }

        ReadOnlyList(__in_ecount(count) T* buffer, int count, TAllocator* alloc)
            : buffer(buffer),
            count(count),
            alloc(alloc)
        {TRACE_IT(21789);
        }

        virtual ~ReadOnlyList()
        {TRACE_IT(21790);
        }

        int Count() const
        {TRACE_IT(21791);
            return count;
        }

        bool Empty() const
        {TRACE_IT(21792);
            return Count() == 0;
        }

        // Gets the count of items using the specified criteria for considering an item.
        template <typename TConditionalFunction>
        int CountWhere(TConditionalFunction function) const
        {TRACE_IT(21793);
            int conditionalCount = 0;
            for (int i = 0; i < this->count; ++i)
            {TRACE_IT(21794);
                if (function(this->buffer[i]))
                {TRACE_IT(21795);
                    ++conditionalCount;
                }
            }

            return conditionalCount;
        }

        const T& Item(int index) const
        {TRACE_IT(21796);
            Assert(index >= 0 && index < count);
            return buffer[index];
        }

        bool Contains(const T& item) const
        {TRACE_IT(21797);
            for (int i = 0; i < count; i++)
            {TRACE_IT(21798);
                if (TComparerType::Equals(item, buffer[i]))
                {TRACE_IT(21799);
                    return true;
                }
            }
            return false;
        }

        // Checks if any of the elements satisfy the condition in the passed in function.
        template <typename TConditionalFunction>
        bool Any(TConditionalFunction function)
        {TRACE_IT(21800);
            for (int i = 0; i < count; ++i)
            {TRACE_IT(21801);
                if (function(this->buffer[i]))
                {TRACE_IT(21802);
                    return true;
                }
            }

            return false;
        }

        // Checks if all of the elements satisfy the condition in the passed in function.
        template <typename TConditionalFunction>
        bool All(TConditionalFunction function)
        {TRACE_IT(21803);
            for (int i = 0; i < count; ++i)
            {TRACE_IT(21804);
                if (!function(this->buffer[i]))
                {TRACE_IT(21805);
                    return false;
                }
            }

            return true;
        }

        // Performs a binary search on a range of elements in the list (assumes the list is sorted).
        template <typename TComparisonFunction>
        int BinarySearch(TComparisonFunction compare, int fromIndex, int toIndex)
        {TRACE_IT(21806);
            AssertMsg(fromIndex >= 0, "Invalid starting index for binary searching.");
            AssertMsg(toIndex < this->count, "Invalid ending index for binary searching.");

            while (fromIndex <= toIndex)
            {TRACE_IT(21807);
                int midIndex = fromIndex + (toIndex - fromIndex) / 2;
                T item = this->Item(midIndex);
                int compareResult = compare(item, midIndex);
                if (compareResult > 0)
                {TRACE_IT(21808);
                    toIndex = midIndex - 1;
                }
                else if (compareResult < 0)
                {TRACE_IT(21809);
                    fromIndex = midIndex + 1;
                }
                else
                {TRACE_IT(21810);
                    return midIndex;
                }
            }
            return -1;
        }

        // Performs a binary search on the elements in the list (assumes the list is sorted).
        template <typename TComparisonFunction>
        int BinarySearch(TComparisonFunction compare)
        {TRACE_IT(21811);
            return BinarySearch<TComparisonFunction>(compare, 0, this->Count() - 1);
        }
    };

    template <
        class T,
        class TAllocator = Recycler,
        bool isLeaf = false,
        template <class TListType, bool clearOldEntries> class TRemovePolicy = Js::CopyRemovePolicy,
        template <typename Value> class TComparer = DefaultComparer>
    class List : public ReadOnlyList<T, TAllocator, TComparer>
    {
    public:
        typedef ReadOnlyList<T, TAllocator, TComparer> ParentType;
        typedef typename ParentType::TComparerType TComparerType;
        typedef T TElementType;         // For TRemovePolicy
        static const int DefaultIncrement = 4;

    private:
        typedef List<T, TAllocator, isLeaf, TRemovePolicy, TComparer> TListType;
        friend TRemovePolicy<TListType, true>;
        typedef TRemovePolicy<TListType, true /* clearOldEntries */>  TRemovePolicyType;
        typedef ListTypeAllocatorFunc<TAllocator, isLeaf> AllocatorInfo;
        typedef typename AllocatorInfo::EffectiveAllocatorType EffectiveAllocatorType;

        Field(int) length;
        Field(int) increment;
        Field(TRemovePolicyType) removePolicy;

        Field(T, TAllocator) * AllocArray(DECLSPEC_GUARD_OVERFLOW int size)
        {TRACE_IT(21812);
            typedef Field(T, TAllocator) TField;
            return AllocatorNewArrayBaseFuncPtr(TAllocator, this->alloc, AllocatorInfo::GetAllocFunc(), TField, size);
        }

        void FreeArray(Field(T, TAllocator) * oldBuffer, int oldBufferSize)
        {TRACE_IT(21813);
            AllocatorFree(this->alloc, AllocatorInfo::GetFreeFunc(), oldBuffer, oldBufferSize);
        }

        PREVENT_COPY(List); // Disable copy constructor and operator=

    public:
        virtual bool IsReadOnly() const override
        {
            return false;
        }

        virtual void Delete() override
        {
            AllocatorDelete(TAllocator, this->alloc, this);
        }

        void EnsureArray()
        {TRACE_IT(21814);
            EnsureArray(0);
        }

        void EnsureArray(DECLSPEC_GUARD_OVERFLOW int32 requiredCapacity)
        {TRACE_IT(21815);
            if (this->buffer == nullptr)
            {TRACE_IT(21816);
                int32 newSize = max(requiredCapacity, increment);

                this->buffer = AllocArray(newSize);
                this->count = 0;
                this->length = newSize;
            }
            else if (this->count == length || requiredCapacity > this->length)
            {TRACE_IT(21817);
                int32 newLength = 0, newBufferSize = 0, oldBufferSize = 0;

                if (Int32Math::Add(length, 1u, &newLength)
                    || Int32Math::Shl(newLength, 1u, &newLength))
                {TRACE_IT(21818);
                    JsUtil::ExternalApi::RaiseOnIntOverflow();
                }

                newLength = max(requiredCapacity, newLength);

                if (Int32Math::Mul(sizeof(T), newLength, &newBufferSize)
                    || Int32Math::Mul(sizeof(T), length, &oldBufferSize))
                {TRACE_IT(21819);
                    JsUtil::ExternalApi::RaiseOnIntOverflow();
                }

                Field(T, TAllocator)* newbuffer = AllocArray(newLength);
                Field(T, TAllocator)* oldbuffer = this->buffer;
                CopyArray<Field(T, TAllocator), Field(T, TAllocator), EffectiveAllocatorType>(
                    newbuffer, newLength, oldbuffer, length);

                FreeArray(oldbuffer, oldBufferSize);

                this->length = newLength;
                this->buffer = newbuffer;
            }
        }

        template<class T>
        void Copy(const T* list)
        {TRACE_IT(21820);
            CompileAssert(sizeof(TElementType) == sizeof(typename T::TElementType));
            if (list->Count() > 0)
            {TRACE_IT(21821);
                this->EnsureArray(list->Count());
                js_memcpy_s(this->buffer, UInt32Math::Mul(sizeof(TElementType), this->length), list->GetBuffer(), UInt32Math::Mul(sizeof(TElementType), list->Count()));
            }
            this->count = list->Count();
        }

        static List * New(TAllocator * alloc, int increment = DefaultIncrement)
        {TRACE_IT(21822);
            return AllocatorNew(TAllocator, alloc, List, alloc, increment);
        }

        List(TAllocator* alloc, int increment = DefaultIncrement) :
            increment(increment), removePolicy(this), ParentType(alloc)
        {TRACE_IT(21823);
            this->buffer = nullptr;
            this->count = 0;
            length = 0;
        }

        virtual ~List() override
        {
            this->Reset();
        }

        TAllocator * GetAllocator() const
        {TRACE_IT(21824);
            return this->alloc;
        }

        const T& Item(int index) const
        {TRACE_IT(21825);
            return ParentType::Item(index);
        }

        Field(T, TAllocator)& Item(int index)
        {TRACE_IT(21826);
            Assert(index >= 0 && index < this->count);
            return this->buffer[index];
        }

        T& Last()
        {TRACE_IT(21827);
            Assert(this->count >= 1);
            return this->Item(this->count - 1);
        }

        // Finds the last element that satisfies the condition in the passed in function.
        // Returns true if the element was found; false otherwise.
        template <typename TConditionalFunction>
        bool Last(TConditionalFunction function, T& outElement)
        {TRACE_IT(21828);
            for (int i = count - 1; i >= 0; --i)
            {TRACE_IT(21829);
                if (function(this->buffer[i]))
                {TRACE_IT(21830);
                    outElement = this->buffer[i];
                    return true;
                }
            }

            return false;
        }

        void Item(int index, const T& item)
        {TRACE_IT(21831);
            Assert(index >= 0 && index < this->count);
            this->buffer[index] = item;
        }

        void SetItem(int index, const T& item)
        {TRACE_IT(21832);
            EnsureArray(index + 1);
            // TODO: (SWB)(leish) find a way to force user defined copy constructor
            this->buffer[index] = item;
            this->count = max(this->count, index + 1);
        }

        void SetExistingItem(int index, const T& item)
        {
            Item(index, item);
        }

        bool IsItemValid(int index)
        {TRACE_IT(21833);
            return removePolicy.IsItemValid(this, index);
        }

        int SetAtFirstFreeSpot(const T& item)
        {TRACE_IT(21834);
            int indexToSetAt = removePolicy.GetFreeItemIndex(this);

            if (indexToSetAt == -1)
            {TRACE_IT(21835);
                return Add(item);
            }

            this->buffer[indexToSetAt] = item;
            return indexToSetAt;
        }

        int Add(const T& item)
        {TRACE_IT(21836);
            EnsureArray();
            this->buffer[this->count] = item;
            int pos = this->count;
            this->count++;
            return pos;
        }

        int32 AddRange(__readonly _In_reads_(count) const T* items, int32 count)
        {TRACE_IT(21837);
            Assert(items != nullptr);
            Assert(count > 0);

            int32 requiredSize = 0, availableByteSpace = 0, givenBufferSize = 0;

            if (Int32Math::Add(this->count,  count, &requiredSize))
            {TRACE_IT(21838);
                JsUtil::ExternalApi::RaiseOnIntOverflow();
            }

            EnsureArray(requiredSize);

            if (Int32Math::Sub(this->length,  this->count, &availableByteSpace)
                || Int32Math::Mul(sizeof(T), availableByteSpace, &availableByteSpace)
                || Int32Math::Mul(sizeof(T), count, &givenBufferSize))
            {TRACE_IT(21839);
                JsUtil::ExternalApi::RaiseOnIntOverflow();
            }

            js_memcpy_s(buffer + this->count, availableByteSpace, items, givenBufferSize);
            this->count = requiredSize;

            return requiredSize; //Returns count
        }


        void AddRange(TListType const& list)
        {TRACE_IT(21840);
            list.Map([this](int index, T const& item)
            {
                this->Add(item);
            });
        }

        // Trims the end of the list
        template <bool weaklyRefItems>
        T CompactEnd()
        {TRACE_IT(21841);
            while (this->count != 0)
            {TRACE_IT(21842);
                AnalysisAssert(!weaklyRefItems || (this->buffer[this->count - 1] != nullptr));
                if (weaklyRefItems ?
                    this->buffer[this->count - 1]->Get() != nullptr :
                    this->buffer[this->count - 1] != nullptr)
                {TRACE_IT(21843);
                    return this->buffer[this->count - 1];
                }
                this->count--;
                this->buffer[this->count] = nullptr;
            }

            return nullptr;
        }

        void Remove(const T& item)
        {TRACE_IT(21844);
            removePolicy.Remove(this, item);
        }

        T RemoveAtEnd()
        {TRACE_IT(21845);
            Assert(this->count >= 1);
            T item = this->Item(this->count - 1);
            RemoveAt(this->count - 1);
            return item;
        }

        void RemoveAt(int index)
        {TRACE_IT(21846);
            removePolicy.RemoveAt(this, index);
        }

        void Clear()
        {TRACE_IT(21847);
            this->count = 0;
        }

        void ClearAndZero()
        {TRACE_IT(21848);
            if(this->count == 0)
            {TRACE_IT(21849);
                return;
            }

            ClearArray(this->buffer, this->count);
            Clear();
        }

        void Sort()
        {
            Sort([](void *, const void * a, const void * b) {
                return TComparerType::Compare(*(T*)a, *(T*)b);
            }, nullptr);
        }

        void Sort(int(__cdecl * _PtFuncCompare)(void *, const void *, const void *), void *_Context)
        {
            // We can call QSort only if the remove policy for this list is CopyRemovePolicy
            CompileAssert((IsSame<TRemovePolicyType, Js::CopyRemovePolicy<TListType, false> >::IsTrue) ||
                (IsSame<TRemovePolicyType, Js::CopyRemovePolicy<TListType, true> >::IsTrue));
            if (this->count)
            {
                qsort_s<Field(T, TAllocator), Field(T, TAllocator), EffectiveAllocatorType>(
                    this->buffer, this->count, _PtFuncCompare, _Context);
            }
        }

        template<class DebugSite, class TMapFunction>
        HRESULT Map(DebugSite site, TMapFunction map) const // external debugging version
        {TRACE_IT(21850);
            return Js::Map(site, PointerValue(this->buffer), this->count, map);
        }

        template<class TMapFunction>
        bool MapUntil(TMapFunction map) const
        {TRACE_IT(21851);
            return MapUntilFrom(0, map);
        }

        template<class TMapFunction>
        bool MapUntilFrom(int start, TMapFunction map) const
        {TRACE_IT(21852);
            for (int i = start; i < this->count; i++)
            {TRACE_IT(21853);
                if (TRemovePolicyType::IsItemValid(this->buffer[i]))
                {
                    if (map(i, this->buffer[i]))
                    {TRACE_IT(21854);
                        return true;
                    }
                }
            }
            return false;
        }

        template<class TMapFunction>
        void Map(TMapFunction map) const
        {
            MapFrom(0, map);
        }

        template<class TMapFunction>
        void MapAddress(TMapFunction map) const
        {TRACE_IT(21855);
            for (int i = 0; i < this->count; i++)
            {TRACE_IT(21856);
                if (TRemovePolicyType::IsItemValid(this->buffer[i]))
                {
                    map(i, &this->buffer[i]);
                }
            }
        }

        template<class TMapFunction>
        void MapFrom(int start, TMapFunction map) const
        {TRACE_IT(21857);
            for (int i = start; i < this->count; i++)
            {TRACE_IT(21858);
                if (TRemovePolicyType::IsItemValid(this->buffer[i]))
                {
                    map(i, this->buffer[i]);
                }
            }
        }

        template<class TMapFunction>
        void ReverseMap(TMapFunction map)
        {TRACE_IT(21859);
            for (int i = this->count - 1; i >= 0; i--)
            {TRACE_IT(21860);
                if (TRemovePolicyType::IsItemValid(this->buffer[i]))
                {
                    map(i, this->buffer[i]);
                }
            }
        }

        void Reset()
        {TRACE_IT(21861);
            if (this->buffer != nullptr)
            {TRACE_IT(21862);
                auto freeFunc = AllocatorInfo::GetFreeFunc();
                AllocatorFree(this->alloc, freeFunc, this->buffer, sizeof(T) * length); // TODO: Better version of DeleteArray?

                this->buffer = nullptr;
                this->count = 0;
                length = 0;
            }
        }
    };

}

namespace Js
{
    //
    // A simple wrapper on List to synchronize access.
    // Note that this wrapper class only exposes a few methods of List (through "private" inheritance).
    // It applies proper lock policy to exposed methods.
    //
    template <
        class T,                                    // Item type in the list
        class ListType,
        class LockPolicy = DefaultContainerLockPolicy,   // Controls lock policy for read/map/write/add/remove items
        class SyncObject = CriticalSection
    >
    class SynchronizableList sealed: private ListType // Make base class private to lock down exposed methods
    {
    private:
        FieldNoBarrier(SyncObject*) syncObj;

    public:
        template <class Arg1>
        SynchronizableList(Arg1 arg1, SyncObject* syncObj)
            : ListType(arg1), syncObj(syncObj)
        {TRACE_IT(21863);
        }

        template <class Arg1, class Arg2>
        SynchronizableList(Arg1 arg1, Arg2 arg2, SyncObject* syncObj)
            : ListType(arg1, arg2), syncObj(syncObj)
        {TRACE_IT(21864);
        }

        template <class Arg1, class Arg2, class Arg3>
        SynchronizableList(Arg1 arg1, Arg2 arg2, Arg3 arg3, SyncObject* syncObj)
            : ListType(arg1, arg2, arg3), syncObj(syncObj)
        {TRACE_IT(21865);
        }

        int Count() const
        {TRACE_IT(21866);
            typename LockPolicy::ReadLock autoLock(syncObj);
            return __super::Count();
        }

        const T& Item(int index) const
        {TRACE_IT(21867);
            typename LockPolicy::ReadLock autoLock(syncObj);
            return __super::Item(index);
        }

        void Item(int index, const T& item)
        {TRACE_IT(21868);
            typename LockPolicy::WriteLock autoLock(syncObj);
            __super::Item(index, item);
        }

        void SetExistingItem(int index, const T& item)
        {TRACE_IT(21869);
            typename LockPolicy::WriteLock autoLock(syncObj);
            __super::SetExistingItem(index, item);
        }

        bool IsItemValid(int index)
        {TRACE_IT(21870);
            typename LockPolicy::ReadLock autoLock(syncObj);
            return __super::IsItemValid(index);
        }

        int SetAtFirstFreeSpot(const T& item)
        {TRACE_IT(21871);
            typename LockPolicy::WriteLock autoLock(syncObj);
            return __super::SetAtFirstFreeSpot(item);
        }

        void ClearAndZero()
        {TRACE_IT(21872);
            typename LockPolicy::WriteLock autoLock(syncObj);
            __super::ClearAndZero();
        }

        void RemoveAt(int index)
        {TRACE_IT(21873);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);
            return __super::RemoveAt(index);
        }

        int Add(const T& item)
        {TRACE_IT(21874);
            typename LockPolicy::AddRemoveLock autoLock(syncObj);
            return __super::Add(item);
        }

        template<class TMapFunction>
        void Map(TMapFunction map) const
        {TRACE_IT(21875);
            typename LockPolicy::ReadLock autoLock(syncObj);
            __super::Map(map);
        }

        template<class TMapFunction>
        bool MapUntil(TMapFunction map) const
        {TRACE_IT(21876);
            typename LockPolicy::ReadLock autoLock(syncObj);
            return __super::MapUntil(map);
        }

        template<class DebugSite, class TMapFunction>
        HRESULT Map(DebugSite site, TMapFunction map) const // external debugging version
        {TRACE_IT(21877);
            // No lock needed. Threads are suspended during external debugging.
            return __super::Map(site, map);
        }
    };

    template <typename TListType, bool clearOldEntries = false>
    class CopyRemovePolicy
    {
        typedef typename TListType::TElementType TElementType;
        typedef typename TListType::TComparerType TComparerType;

    public:
        CopyRemovePolicy(TListType * list) {TRACE_IT(21878);};
        void Remove(TListType* list, const TElementType& item)
        {TRACE_IT(21879);
            TElementType* buffer = list->buffer;
            int& count = list->count;

            for (int i = 0; i < count; i++)
            {TRACE_IT(21880);
                if (TComparerType::Equals(buffer[i], item))
                {TRACE_IT(21881);
                    for (int j = i + 1; j < count; i++, j++)
                    {TRACE_IT(21882);
                        buffer[i] = buffer[j];
                    }
                    count--;

                    if (clearOldEntries)
                    {TRACE_IT(21883);
                        ClearArray(buffer + count, 1);
                    }
                    break;
                }
            }
        }

        int GetFreeItemIndex(TListType* list)
        {TRACE_IT(21884);
            return -1;
        }

        void RemoveAt(TListType* list, int index)
        {TRACE_IT(21885);
            Assert(index >= 0 && index < list->count);
            for (int j = index + 1; j < list->count; index++, j++)
            {TRACE_IT(21886);
                list->buffer[index] = list->buffer[j];
            }
            list->count--;

            if (clearOldEntries)
            {TRACE_IT(21887);
                ClearArray(list->buffer + list->count, 1);
            }
        }

        static bool IsItemValid(const TElementType& item)
        {TRACE_IT(21888);
            return true;
        }

        bool IsItemValid(TListType* list, int index)
        {TRACE_IT(21889);
            Assert(index >= 0 && index < list->count);
            return true;
        }
    };

    template <typename TListType, bool clearOldEntries = false>
    class FreeListedRemovePolicy
    {
    protected:
        typedef typename TListType::TElementType TElementType;
        typedef typename TListType::TComparerType TComparerType;

        Field(int) freeItemIndex;

    public:
        FreeListedRemovePolicy(TListType * list):
          freeItemIndex(-1)
        {TRACE_IT(21890);
            CompileAssert(IsPointer<TElementType>::IsTrue);
        }

        static bool IsItemValid(const TElementType& item)
        {TRACE_IT(21891);
            return (item != nullptr && (::Math::PointerCastToIntegralTruncate<unsigned int>(item) & 1) == 0);
        }

        bool IsItemValid(TListType* list, int index)
        {TRACE_IT(21892);
            const TElementType& item = list->Item(index);
            return IsItemValid(item);
        }

        void Remove(TListType* list, const TElementType& item)
        {TRACE_IT(21893);
            TElementType* buffer = list->buffer;
            int& count = list->count;

            for (int i = 0; i < count; i++)
            {TRACE_IT(21894);
                if (TComparerType::Equals(buffer[i], item))
                {
                    RemoveAt(list, i);
                    break;
                }
            }
        }

        int GetFreeItemIndex(TListType* list)
        {TRACE_IT(21895);
            int currentFreeIndex = this->freeItemIndex;
            if (currentFreeIndex != -1)
            {TRACE_IT(21896);
                unsigned int nextFreeIndex = ::Math::PointerCastToIntegralTruncate<unsigned int>(list->Item(currentFreeIndex));

                if (nextFreeIndex != ((unsigned int) -1))
                {TRACE_IT(21897);
                    // Since this is an unsigned shift, the sign bit is 0, which is what we want
                    this->freeItemIndex = (int) ((nextFreeIndex) >> 1);
                }
                else
                {TRACE_IT(21898);
                    this->freeItemIndex = -1;
                }

                return currentFreeIndex;
            }

            return -1;
        }

        void RemoveAt(TListType* list, int index)
        {TRACE_IT(21899);
            Assert(index >= 0 && index < list->Count());
            Assert(IsItemValid(list, index));

            unsigned int storedIndex = (unsigned int) this->freeItemIndex;

            // Sentinel value, so leave that as is
            // Otherwise, this has the range of all +ve integers
            if (this->freeItemIndex != -1)
            {TRACE_IT(21900);
                // Set a tag bit to indicate this is a free list index, rather than a list value
                // Pointers will be aligned anyway
                storedIndex = (storedIndex << 1) | 1;
            }

            list->SetExistingItem(index, (TElementType) (storedIndex));
            this->freeItemIndex = index;
        }
    };

    template <typename TListType, bool clearOldEntries = false>
    class WeakRefFreeListedRemovePolicy : public FreeListedRemovePolicy<TListType, clearOldEntries>
    {
        typedef FreeListedRemovePolicy<TListType, clearOldEntries> Base;
        typedef typename Base::TElementType TElementType;
    private:
        Field(uint) lastWeakReferenceCleanupId;

        void CleanupWeakReference(TListType * list)
        {TRACE_IT(21901);
            list->Map([list](int i, TElementType weakRef)
            {
                if (weakRef->Get() == nullptr)
                {TRACE_IT(21902);
                    list->RemoveAt(i);
                }
            });

            this->lastWeakReferenceCleanupId = list->alloc->GetWeakReferenceCleanupId();
        }
    public:
        WeakRefFreeListedRemovePolicy(TListType * list) : Base(list)
        {TRACE_IT(21903);
            this->lastWeakReferenceCleanupId = list->alloc->GetWeakReferenceCleanupId();
        }
        int GetFreeItemIndex(TListType * list)
        {TRACE_IT(21904);
            if (list->alloc->GetWeakReferenceCleanupId() != this->lastWeakReferenceCleanupId)
            {TRACE_IT(21905);
                CleanupWeakReference(list);
            }
            return __super::GetFreeItemIndex(list);
        }
    };
}
