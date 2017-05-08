//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
template <typename T>
class RecyclerRootPtr
{
public:
    RecyclerRootPtr() : ptr(nullptr) {TRACE_IT(26483);};
    ~RecyclerRootPtr() {TRACE_IT(26484); Assert(ptr == nullptr); }
    void Root(T * ptr, Recycler * recycler) {TRACE_IT(26485); Assert(this->ptr == nullptr); recycler->RootAddRef(ptr); this->ptr = ptr; }
    void Unroot(Recycler * recycler) {TRACE_IT(26486); Assert(this->ptr != nullptr); recycler->RootRelease(this->ptr); this->ptr = nullptr; }

    T * operator->() const {TRACE_IT(26487); Assert(ptr != nullptr); return ptr; }
    operator T*() const {TRACE_IT(26488); return ptr; }

    RecyclerRootPtr(RecyclerRootPtr<T>&&);
    RecyclerRootPtr& operator=(RecyclerRootPtr<T> &&);

protected:
    T * ptr;
private:
    RecyclerRootPtr(const RecyclerRootPtr<T>& ptr); // Disable
    RecyclerRootPtr& operator=(RecyclerRootPtr<T> const& ptr); // Disable
};


typedef RecyclerRootPtr<void> RecyclerRootVar;

template <typename T>
class AutoRecyclerRootPtr : public RecyclerRootPtr<T>
{
public:
    AutoRecyclerRootPtr(T * ptr, Recycler * recycler) : recycler(recycler)
    {TRACE_IT(26489);
        Root(ptr);
    }
    ~AutoRecyclerRootPtr()
    {TRACE_IT(26490);
        Unroot();
    }

    void Root(T * ptr)
    {TRACE_IT(26491);
        Unroot();
        __super::Root(ptr, recycler);
    }
    void Unroot()
    {TRACE_IT(26492);
        if (ptr != nullptr)
        {TRACE_IT(26493);
            __super::Unroot(recycler);
        }
    }
    Recycler * GetRecycler() const
    {TRACE_IT(26494);
        return recycler;
    }
private:
    Recycler * const recycler;
};

typedef AutoRecyclerRootPtr<void> AutoRecyclerRootVar;
}
