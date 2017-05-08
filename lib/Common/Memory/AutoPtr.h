//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template <typename T, typename HeapAllocatorT = HeapAllocator>
class AutoPtr : public BasePtr<T>
{
public:
    AutoPtr(T * ptr) : BasePtr<T>(ptr) {TRACE_IT(22975);}
    ~AutoPtr()
    {TRACE_IT(22976);
        Clear();
    }

    AutoPtr& operator=(T * ptr)
    {TRACE_IT(22977);
        Clear();
        this->ptr = ptr;
        return *this;
    }

private:
    void Clear()
    {TRACE_IT(22978);
        if (this->ptr != nullptr)
        {
            AllocatorDelete(HeapAllocatorT, &HeapAllocatorT::Instance, this->ptr);
            this->ptr = nullptr;
        }
    }
};

template <typename T>
class AutoArrayPtr : public BasePtr<T>
{
protected:
    size_t m_elementCount;
public:
    AutoArrayPtr(T * ptr, size_t elementCount) : BasePtr<T>(ptr), m_elementCount(elementCount) {TRACE_IT(22979);}
    ~AutoArrayPtr()
    {TRACE_IT(22980);
        Clear();
    }

    void Set(T* ptr, int elementCount)
    {TRACE_IT(22981);
        Clear();
        this->ptr = ptr;
        this->m_elementCount = elementCount;
    }

private:
    void Clear()
    {TRACE_IT(22982);
        if (this->ptr != nullptr)
        {
            HeapDeleteArray(m_elementCount, this->ptr);
            this->ptr = nullptr;
        }
    }
};

template <typename T>
class AutoArrayAndItemsPtr : public AutoArrayPtr<T>
{
public:
    AutoArrayAndItemsPtr(T * ptr, size_t elementCount) : AutoArrayPtr<T>(ptr, elementCount) {TRACE_IT(22983);}

    ~AutoArrayAndItemsPtr()
    {TRACE_IT(22984);
        Clear();
    }

private:
    void Clear()
    {TRACE_IT(22985);
        if (ptr != nullptr){TRACE_IT(22986);
            for (size_t i = 0; i < this->m_elementCount; i++)
            {TRACE_IT(22987);
                if (ptr[i] != nullptr)
                {TRACE_IT(22988);
                    ptr[i]->CleanUp();
                    ptr[i] = nullptr;
                }
            }

            HeapDeleteArray(m_elementCount, ptr);
            ptr = nullptr;
        }
    }
};

template <typename T>
class AutoReleasePtr : public BasePtr<T>
{
    using BasePtr<T>::ptr;
public:
    AutoReleasePtr(T * ptr = nullptr) : BasePtr<T>(ptr) {TRACE_IT(22989);}
    ~AutoReleasePtr()
    {TRACE_IT(22990);
        Release();
    }

    void Release()
    {TRACE_IT(22991);
        if (ptr != nullptr)
        {TRACE_IT(22992);
            ptr->Release();
            this->ptr = nullptr;
        }
    }
};

template <typename T>
class AutoCOMPtr : public AutoReleasePtr<T>
{
public:
    AutoCOMPtr(T * ptr = nullptr) : AutoReleasePtr<T>(ptr)
    {TRACE_IT(22993);
        if (ptr != nullptr)
        {TRACE_IT(22994);
            ptr->AddRef();
        }
    }
};

class AutoBSTR : public BasePtr<OLECHAR>
{
public:
    AutoBSTR(BSTR ptr = nullptr) : BasePtr(ptr) {TRACE_IT(22995);}
    ~AutoBSTR()
    {TRACE_IT(22996);
        Release();
    }

    void Release()
    {TRACE_IT(22997);
        if (ptr != nullptr)
        {TRACE_IT(22998);
            ::SysFreeString(ptr);
            this->ptr = nullptr;
        }
    }
};

template <typename T>
class AutoDiscardPTR : public BasePtr<T>
{
public:
    AutoDiscardPTR(T * ptr) : BasePtr<T>(ptr) {TRACE_IT(22999);}
    ~AutoDiscardPTR()
    {TRACE_IT(23000);
        Clear();
    }

    AutoDiscardPTR& operator=(T * ptr)
    {TRACE_IT(23001);
        Clear();
        this->ptr = ptr;
        return *this;
    }

private:
    void Clear()
    {TRACE_IT(23002);
        if (this->ptr != nullptr)
        {TRACE_IT(23003);
            this->ptr->Discard();
            this->ptr = nullptr;
        }
    }
};
