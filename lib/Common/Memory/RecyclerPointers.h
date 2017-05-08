//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "WriteBarrierMacros.h"

namespace Memory
{
class Recycler;
class RecyclerNonLeafAllocator;

// Dummy tag class to mark no write barrier value
struct _no_write_barrier_tag {};

// Dummy tag classes to mark yes/no write barrier policy
//
struct _write_barrier_policy {};
struct _no_write_barrier_policy {};

// ----------------------------------------------------------------------------
// Field write barrier
//
//      Determines if a field needs to be wrapped in WriteBarrierPtr
// ----------------------------------------------------------------------------

// Type write barrier policy
//
// By default following potentially contains GC pointers and use write barrier policy:
//      pointer, WriteBarrierPtr, _write_barrier_policy
//
template <class T>
struct TypeWriteBarrierPolicy { typedef _no_write_barrier_policy Policy; };
template <class T>
struct TypeWriteBarrierPolicy<T*> { typedef _write_barrier_policy Policy; };
template <class T>
struct TypeWriteBarrierPolicy<T* const> { typedef _write_barrier_policy Policy; };
template <class T>
struct TypeWriteBarrierPolicy<WriteBarrierPtr<T>> { typedef _write_barrier_policy Policy; };
template <>
struct TypeWriteBarrierPolicy<_write_barrier_policy> { typedef _write_barrier_policy Policy; };

// AllocatorType write barrier policy
//
// Recycler allocator type => _write_barrier_policy
// Note that Recycler allocator type consists of multiple allocators:
//      Recycler, RecyclerNonLeafAllocator, RecyclerLeafAllocator
//
template <class AllocatorType>
struct _AllocatorTypeWriteBarrierPolicy { typedef _no_write_barrier_policy Policy; };
template <>
struct _AllocatorTypeWriteBarrierPolicy<Recycler> { typedef _write_barrier_policy Policy; };

template <class Policy1, class Policy2>
struct _AndWriteBarrierPolicy { typedef _no_write_barrier_policy Policy; };
template <>
struct _AndWriteBarrierPolicy<_write_barrier_policy, _write_barrier_policy>
{
    typedef _write_barrier_policy Policy;
};

// Combine Allocator + Data => write barrier policy
// Specialize RecyclerNonLeafAllocator
//
template <class Allocator, class T>
struct AllocatorWriteBarrierPolicy
{
    typedef typename AllocatorInfo<Allocator, void>::AllocatorType AllocatorType;
    typedef typename _AndWriteBarrierPolicy<
        typename _AllocatorTypeWriteBarrierPolicy<AllocatorType>::Policy,
        typename TypeWriteBarrierPolicy<T>::Policy>::Policy Policy;
};
template <class T>
struct AllocatorWriteBarrierPolicy<RecyclerNonLeafAllocator, T> { typedef _write_barrier_policy Policy; };
template <>
struct AllocatorWriteBarrierPolicy<RecyclerNonLeafAllocator, int> { typedef _no_write_barrier_policy Policy; };

// Choose write barrier Field type: T unchanged, or WriteBarrierPtr based on Policy.
//
template <class T, class Policy>
struct _WriteBarrierFieldType { typedef T Type; };
template <class T>
struct _WriteBarrierFieldType<T*, _write_barrier_policy> { typedef WriteBarrierPtr<T> Type; };
template <class T>
struct _WriteBarrierFieldType<T* const, _write_barrier_policy> { typedef const WriteBarrierPtr<T> Type; };

template <class T,
          class Allocator = Recycler,
          class Policy = typename AllocatorWriteBarrierPolicy<Allocator, T>::Policy>
struct WriteBarrierFieldTypeTraits { typedef typename _WriteBarrierFieldType<T, Policy>::Type Type; };


// ----------------------------------------------------------------------------
// Array write barrier
//
//      Determines if an array operation needs to trigger write barrier
// ----------------------------------------------------------------------------

// ArrayWriteBarrier behavior
//
typedef void FN_VerifyIsNotBarrierAddress(void*, size_t);
extern "C" FN_VerifyIsNotBarrierAddress* g_verifyIsNotBarrierAddress;
template <class Policy>
struct _ArrayWriteBarrier
{
    template <class T>
    static void WriteBarrier(T * address, size_t count)
    {TRACE_IT(26404);
#if defined(RECYCLER_WRITE_BARRIER)
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.StrictWriteBarrierCheck)
        {TRACE_IT(26405);
            if (g_verifyIsNotBarrierAddress)
            {
                g_verifyIsNotBarrierAddress(address, count);
            }
        }
#endif
#endif
    }

    template <class T>
    static void WriteBarrierSetVerifyBits(T * address, size_t count) {TRACE_IT(26406);   }
};

#ifdef RECYCLER_WRITE_BARRIER
template <>
struct _ArrayWriteBarrier<_write_barrier_policy>
{
    template <class T>
    static void WriteBarrier(T * address, size_t count)
    {TRACE_IT(26407);
        RecyclerWriteBarrierManager::WriteBarrier(address, sizeof(T) * count);
    }

    template <class T>
    static void WriteBarrierSetVerifyBits(T * address, size_t count)
    {TRACE_IT(26408);
#if DBG && GLOBAL_ENABLE_WRITE_BARRIER
        Recycler::WBSetBitRange((char*)address, (uint)(sizeof(T) * count / sizeof(void*)));
#endif
    }
};
#endif

// Determines array write barrier policy based on array item type.
//
// Note: If individual array item needs write barrier, we choose to use
// WriteBarrierPtr with the item type. Thus we only specialize on
// WriteBarrierPtr (and _write_barrier_policy if user wants to force write
// barrier).
//
template <class T>
struct _ArrayItemWriteBarrierPolicy
    { typedef _no_write_barrier_policy Policy; };
template <class T>
struct _ArrayItemWriteBarrierPolicy<WriteBarrierPtr<T>>
    { typedef _write_barrier_policy Policy; };
template <>
struct _ArrayItemWriteBarrierPolicy<_write_barrier_policy>
    { typedef _write_barrier_policy Policy; };

template <class T, int N> // in case calling with type WriteBarrierPtr<void> [N]
struct _ArrayItemWriteBarrierPolicy<WriteBarrierPtr<T>[N]>
    { typedef _write_barrier_policy Policy; };

// Trigger write barrier on changing array content if element type determines
// write barrier is needed. Ignore otherwise.
//
template <class T, class PolicyType = T, class Allocator = Recycler>
void ArrayWriteBarrier(T * address, size_t count)
{TRACE_IT(26409);
    typedef typename _ArrayItemWriteBarrierPolicy<PolicyType>::Policy ItemPolicy;
    typedef typename AllocatorWriteBarrierPolicy<Allocator, ItemPolicy>::Policy Policy;
    return _ArrayWriteBarrier<Policy>::WriteBarrier(address, count);
}

template <class T, class PolicyType = T, class Allocator = Recycler>
void ArrayWriteBarrierVerifyBits(T * address, size_t count)
{TRACE_IT(26410);
    typedef typename _ArrayItemWriteBarrierPolicy<PolicyType>::Policy ItemPolicy;
    typedef typename AllocatorWriteBarrierPolicy<Allocator, ItemPolicy>::Policy Policy;
    return _ArrayWriteBarrier<Policy>::WriteBarrierSetVerifyBits(address, count);
}

// Copy array content. Triggers write barrier on the dst array content if if
// Allocator and element type determines write barrier is needed.
//
template <class T, class PolicyType = T, class Allocator = Recycler>
void CopyArray(T* dst, size_t dstCount, const T* src, size_t srcCount)
{
    ArrayWriteBarrierVerifyBits<T, PolicyType, Allocator>(dst, dstCount);

    js_memcpy_s(reinterpret_cast<void*>(dst), sizeof(T) * dstCount,
                reinterpret_cast<const void*>(src), sizeof(T) * srcCount);

    ArrayWriteBarrier<T, PolicyType, Allocator>(dst, dstCount);
}
template <class T, class PolicyType = T, class Allocator = Recycler>
void CopyArray(WriteBarrierPtr<T>& dst, size_t dstCount,
               const WriteBarrierPtr<T>& src, size_t srcCount)
{TRACE_IT(26411);
    return CopyArray<T, PolicyType, Allocator>(
        static_cast<T*>(dst), dstCount, static_cast<const T*>(src), srcCount);
}
template <class T, class PolicyType = T, class Allocator = Recycler>
void CopyArray(T* dst, size_t dstCount,
               const WriteBarrierPtr<T>& src, size_t srcCount)
{TRACE_IT(26412);
    return CopyArray<T, PolicyType, Allocator>(
        dst, dstCount, static_cast<const T*>(src), srcCount);
}
template <class T, class PolicyType = T, class Allocator = Recycler>
void CopyArray(WriteBarrierPtr<T>& dst, size_t dstCount,
               const T* src, size_t srcCount)
{TRACE_IT(26413);
    return CopyArray<T, PolicyType, Allocator>(
        static_cast<T*>(dst), dstCount, src, srcCount);
}

// Copy pointer array to WriteBarrierPtr array
//
template <class T, class PolicyType = WriteBarrierPtr<T>, class Allocator = Recycler>
void CopyArray(WriteBarrierPtr<T>* dst, size_t dstCount,
               T* const * src, size_t srcCount)
{TRACE_IT(26414);
    CompileAssert(sizeof(WriteBarrierPtr<T>) == sizeof(T*));
    return CopyArray<T*, PolicyType, Allocator>(
        reinterpret_cast<T**>(dst), dstCount, src, srcCount);
}

// Move Array content (memmove)
//
template <class T, class PolicyType = T, class Allocator = Recycler>
void MoveArray(T* dst, const T* src, size_t count)
{
    ArrayWriteBarrierVerifyBits<T, PolicyType, Allocator>(dst, count);

    memmove(reinterpret_cast<void*>(dst),
            reinterpret_cast<const void*>(src),
            sizeof(T) * count);

    ArrayWriteBarrier<T, PolicyType, Allocator>(dst, count);
}

template <class T>
void ClearArray(T* dst, size_t count)
{TRACE_IT(26415);
    // assigning NULL don't need write barrier, just cast it and null it out
    memset(reinterpret_cast<void*>(dst), 0, sizeof(T) * count);
}
template <class T>
void ClearArray(WriteBarrierPtr<T>& dst, size_t count)
{TRACE_IT(26416);
    ClearArray(static_cast<T*>(dst), count);
}
template <class T, size_t N>
void ClearArray(T (&dst)[N])
{
    ClearArray(dst, N);
}


template <typename T>
class NoWriteBarrierField
{
public:
    NoWriteBarrierField() : value() {TRACE_IT(26417);}
    explicit NoWriteBarrierField(T const& value) : value(value) {TRACE_IT(26418);}

    // Getters
    operator T const&() const {TRACE_IT(26419); return value; }
    operator T&() {TRACE_IT(26420); return value; }

    T const* operator&() const {TRACE_IT(26421); return &value; }
    T* operator&() {TRACE_IT(26422); return &value; }

    // Setters
    NoWriteBarrierField& operator=(T const& value)
    {TRACE_IT(26423);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        RecyclerWriteBarrierManager::VerifyIsNotBarrierAddress(this);
#endif
        this->value = value;
        return *this;
    }

private:
    T value;
};

template <typename T>
class NoWriteBarrierPtr
{
public:
    NoWriteBarrierPtr() : value(nullptr) {TRACE_IT(26424);}
    NoWriteBarrierPtr(T * value) : value(value) {TRACE_IT(26425);}

    // Getters
    T * operator->() const {TRACE_IT(26426); return this->value; }
    operator T* const & () const {TRACE_IT(26427); return this->value; }

    T* const * operator&() const {TRACE_IT(26428); return &value; }
    T** operator&() {TRACE_IT(26429); return &value; }

    // Setters
    NoWriteBarrierPtr& operator=(T * value)
    {TRACE_IT(26430);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        RecyclerWriteBarrierManager::VerifyIsNotBarrierAddress(this);
#endif
        this->value = value;
        return *this;
    }
private:
    T * value;
};

template <typename T>
class WriteBarrierObjectConstructorTrigger
{
public:
    WriteBarrierObjectConstructorTrigger(T* object, Recycler* recycler):
        object((char*) object),
        recycler(recycler)
    {TRACE_IT(26431);
    }

    ~WriteBarrierObjectConstructorTrigger()
    {TRACE_IT(26432);
        // WriteBarrier-TODO: trigger write barrier if the GC is in concurrent mark state
    }

    operator T*()
    {TRACE_IT(26433);
        return object;
    }

private:
    T* object;
    Recycler* recycler;
};

template <typename T>
class WriteBarrierPtr
{
public:
    WriteBarrierPtr() : ptr(nullptr) { }
    WriteBarrierPtr(const std::nullptr_t&) : ptr(nullptr) { }
    WriteBarrierPtr(T * ptr)
    {
        // WriteBarrier
        WriteBarrierSet(ptr);
    }
    WriteBarrierPtr(T * ptr, const _no_write_barrier_tag&)
    {
        NoWriteBarrierSet(ptr);
    }
    WriteBarrierPtr(WriteBarrierPtr<T>& other)
    {
        WriteBarrierSet(other.ptr);
    }
    WriteBarrierPtr(WriteBarrierPtr<T>&& other)
    {
        WriteBarrierSet(other.ptr);
    }

    // Getters
    T * operator->() const { return ptr; }
    operator T* const & () const { return ptr; }

    // Taking immutable address is ok
    //
    T* const * AddressOf() const
    {
        return &ptr;
    }
    // Taking mutable address is not allowed

    // Setters
    WriteBarrierPtr& operator=(T * ptr)
    {
        WriteBarrierSet(ptr);
        return *this;
    }
    WriteBarrierPtr& operator=(const std::nullptr_t& ptr)
    {
        NoWriteBarrierSet(ptr);
        return *this;
    }
    void NoWriteBarrierSet(T * ptr)
    {
        this->ptr = ptr;
    }
    void WriteBarrierSet(T * ptr)
    {
#if DBG && GLOBAL_ENABLE_WRITE_BARRIER
        // set the verification bits before updating the reference so it's ready to verify while background marking hit the reference
        Recycler::WBSetBit((char*)this);
#endif

        NoWriteBarrierSet(ptr);

#ifdef RECYCLER_WRITE_BARRIER
        // set the barrier bit after updating the reference to prevent a race issue that background thread is resetting all the dirty pages
        RecyclerWriteBarrierManager::WriteBarrier(this);
#endif
    }

    WriteBarrierPtr& operator=(WriteBarrierPtr const& other)
    {
        WriteBarrierSet(other.ptr);
        return *this;
    }

    WriteBarrierPtr& operator++()  // prefix ++
    {
#if DBG && GLOBAL_ENABLE_WRITE_BARRIER
        Recycler::WBSetBit((char*)this);
#endif

        ++ptr;

#ifdef RECYCLER_WRITE_BARRIER
        RecyclerWriteBarrierManager::WriteBarrier(this);
#endif
        return *this;
    }

    WriteBarrierPtr operator++(int)  // postfix ++
    {
        WriteBarrierPtr result(*this);
        ++(*this);
        return result;
    }

    WriteBarrierPtr& operator--()  // prefix --
    {
#if DBG && GLOBAL_ENABLE_WRITE_BARRIER
        Recycler::WBSetBit((char*)this);
#endif

        --ptr;

#ifdef RECYCLER_WRITE_BARRIER
        RecyclerWriteBarrierManager::WriteBarrier(this);
#endif
        return *this;
    }

    WriteBarrierPtr operator--(int)  // postfix --
    {
        WriteBarrierPtr result(*this);
        --(*this);
        return result;
    }

private:
    T * ptr;
};


template <class T>
struct _AddressOfType
{
    typedef T ValueType;
    inline static const ValueType* AddressOf(const T& val) { return &val; }
};

template <class T>
struct _AddressOfType< WriteBarrierPtr<T> >
{
    typedef T* ValueType;
    inline static const ValueType* AddressOf(const WriteBarrierPtr<T>& val)
    {
        return val.AddressOf();
    }
};

template <class T>
inline const typename _AddressOfType<T>::ValueType* AddressOf(const T& val)
{
  return _AddressOfType<T>::AddressOf(val);
}

template <class T>
inline T* const& PointerValue(T* const& ptr) { return ptr; }
template <class T>
inline T* const& PointerValue(const WriteBarrierPtr<T>& ptr) { return ptr; }

// Unsafe NoWriteBarrierSet. Use only when necessary and you are sure skipping
// write barrier is ok.
//
template <class T>
inline void NoWriteBarrierSet(T*& dst, T* ptr) { dst = ptr; }
template <class T>
inline void NoWriteBarrierSet(WriteBarrierPtr<T>& dst, T* ptr)
{
    dst.NoWriteBarrierSet(ptr);
}
template <class T>
inline void NoWriteBarrierSet(WriteBarrierPtr<T>& dst, const WriteBarrierPtr<T>& ptr)
{
    dst.NoWriteBarrierSet(ptr);
}

}  // namespace Memory


template<class T> inline
const T& min(const T& a, const NoWriteBarrierField<T>& b) {TRACE_IT(26460); return a < b ? a : b; }

template<class T> inline
const T& min(const NoWriteBarrierField<T>& a, const T& b) {TRACE_IT(26461); return a < b ? a : b; }

template<class T> inline
const T& min(const NoWriteBarrierField<T>& a, const NoWriteBarrierField<T>& b) {TRACE_IT(26462); return a < b ? a : b; }

template<class T> inline
const T& max(const NoWriteBarrierField<T>& a, const T& b) {TRACE_IT(26463); return a > b ? a : b; }

// TODO: Add this method back once we figure out why OACR is tripping on it
template<class T> inline
const T& max(const T& a, const NoWriteBarrierField<T>& b) {TRACE_IT(26464); return a > b ? a : b; }

template<class T> inline
const T& max(const NoWriteBarrierField<T>& a, const NoWriteBarrierField<T>& b) {TRACE_IT(26465); return a > b ? a : b; }

// QuickSort Array content
//
template <class Policy>
struct _QuickSortImpl
{
    template<class T, class Comparer>
    static void Sort(T* arr, size_t count, const Comparer& comparer, void* context, size_t elementSize = sizeof(T))
    {TRACE_IT(26466);
#ifndef _WIN32
        JsUtil::QuickSort<Policy, char, Comparer>::Sort((char*)arr, count, elementSize, comparer, context);
#else
        // by default use system qsort_s
        ::qsort_s(arr, count, elementSize, comparer, context);
#endif
    }
};
#ifdef RECYCLER_WRITE_BARRIER
template <>
struct _QuickSortImpl<_write_barrier_policy>
{
    template<class T, class Comparer>
    static void Sort(T* arr, size_t count, const Comparer& comparer, void* context,
        size_t _ = 1 /* QuickSortSwap does not memcpy when SWB policy is in place*/)
    {TRACE_IT(26467);
        // Use custom implementation if policy needs write barrier
        JsUtil::QuickSort<_write_barrier_policy, T, Comparer>::Sort(arr, count, 1, comparer, context);
    }
};
#endif

template<class T, class PolicyType = T, class Allocator = Recycler, class Comparer>
void qsort_s(T* arr, size_t count, const Comparer& comparer, void* context)
{TRACE_IT(26468);
    typedef typename _ArrayItemWriteBarrierPolicy<PolicyType>::Policy ItemPolicy;
    typedef typename AllocatorWriteBarrierPolicy<Allocator, ItemPolicy>::Policy Policy;
    _QuickSortImpl<Policy>::Sort(arr, count, comparer, context);
}

#ifndef _WIN32
// on xplat we use our custom qsort_s
template<class T, class PolicyType = T, class Allocator = Recycler, class Comparer>
void qsort_s(T* arr, size_t count, size_t size, const Comparer& comparer, void* context)
{TRACE_IT(26469);
    typedef typename _ArrayItemWriteBarrierPolicy<PolicyType>::Policy ItemPolicy;
    typedef typename AllocatorWriteBarrierPolicy<Allocator, ItemPolicy>::Policy Policy;
    _QuickSortImpl<Policy>::Sort(arr, count, comparer, context, size);
}
#endif

template<class T, class Comparer>
void qsort_s(WriteBarrierPtr<T>* _Base, size_t _NumOfElements, size_t _SizeOfElements,
             const Comparer& comparer, void* _Context)
{TRACE_IT(26470);
    CompileAssert(false); // Disallow this. Use an overload above.
}

// Disallow memcpy, memmove of WriteBarrierPtr
template <typename T>
void *  __cdecl memmove(_Out_writes_bytes_all_opt_(_Size) WriteBarrierPtr<T> * _Dst, _In_reads_bytes_opt_(_Size) const void * _Src, _In_ size_t _Size)
{TRACE_IT(26471);
    CompileAssert(false);
}

template <typename T>
void* __cdecl memcpy(WriteBarrierPtr<T> *dst, const void *src, size_t count)
{TRACE_IT(26472);
    CompileAssert(false);
}

template <typename T>
errno_t __cdecl memcpy_s(WriteBarrierPtr<T> *dst, size_t dstSize, const void *src, size_t srcSize)
{
    static_assert(false, "Use CopyArray instead");
}

template <typename T>
void __stdcall js_memcpy_s(__bcount(sizeInBytes) WriteBarrierPtr<T> *dst, size_t sizeInBytes, __bcount(count) const void *src, size_t count)
{
    static_assert(false, "Use CopyArray instead");
}

template <typename T>
void *  __cdecl memset(_Out_writes_bytes_all_(_Size) WriteBarrierPtr<T> * _Dst, _In_ int _Val, _In_ size_t _Size)
{TRACE_IT(26473);
    CompileAssert(false);
}

// This class abstract a pointer value with its last 2 bits set to avoid conservative GC tracking.
template <class T>
class TaggedPointer
{
public:
    operator T*()          const {TRACE_IT(26474); return GetPointerValue(); }
    bool operator!= (T* p) const {TRACE_IT(26475); return GetPointerValue() != p; }
    bool operator== (T* p) const {TRACE_IT(26476); return GetPointerValue() == p; }
    T* operator-> ()       const {TRACE_IT(26477); return GetPointerValue(); }
    TaggedPointer<T>& operator= (T* inPtr)
    {TRACE_IT(26478);
        SetPointerValue(inPtr);
        return (*this);
    }
    TaggedPointer(T* inPtr) : ptr(inPtr)
    {TRACE_IT(26479);
        SetPointerValue(inPtr);
    }

    TaggedPointer() : ptr(NULL) {TRACE_IT(26480);};
private:
    T * GetPointerValue() const {TRACE_IT(26481); return reinterpret_cast<T*>(reinterpret_cast<ULONG_PTR>(ptr) & ~3); }
    T * SetPointerValue(T* inPtr)
    {TRACE_IT(26482);
        AssertMsg((reinterpret_cast<ULONG_PTR>(inPtr) & 3) == 0, "Invalid pointer value, 2 least significant bits must be zero");
        ptr = reinterpret_cast<T*>((reinterpret_cast<ULONG_PTR>(inPtr) | 3));
        return ptr;
    }

    FieldNoBarrier(T*) ptr;
};
