//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


template <typename T>
class BasePtr
{
public:
    BasePtr(T * ptr = nullptr) : ptr(ptr) {TRACE_IT(19623);}
    T ** operator&() {TRACE_IT(19624); Assert(ptr == nullptr); return &ptr; }
    T * operator->() const {TRACE_IT(19625); Assert(ptr != nullptr); return ptr; }
    operator T*() const {TRACE_IT(19626); return ptr; }

    // Detach currently owned ptr. WARNING: This object no longer owns/manages the ptr.
    T * Detach()
    {TRACE_IT(19627);
        T * ret = ptr;
        ptr = nullptr;
        return ret;
    }
protected:
    T * ptr;
private:
    BasePtr(const BasePtr<T>& ptr); // Disable
    BasePtr& operator=(BasePtr<T> const& ptr); // Disable
};
