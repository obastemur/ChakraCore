//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
    template<class T>
    InlineCachePointerArray<T>::InlineCachePointerArray()
        : inlineCaches(nullptr)
#if DBG
        , inlineCacheCount(0)
#endif
    {
    }

    template<class T>
    void InlineCachePointerArray<T>::EnsureInlineCaches(Recycler *const recycler, FunctionBody *const functionBody)
    {TRACE_IT(48563);
        Assert(recycler);
        Assert(functionBody);
        Assert(functionBody->GetInlineCacheCount() != 0);

        if(inlineCaches)
        {TRACE_IT(48564);
            Assert(functionBody->GetInlineCacheCount() == inlineCacheCount);
            return;
        }

        inlineCaches = RecyclerNewArrayZ(recycler, Field(T*), functionBody->GetInlineCacheCount());
#if DBG
        inlineCacheCount = functionBody->GetInlineCacheCount();
#endif
    }

    template<class T>
    T *InlineCachePointerArray<T>::GetInlineCache(FunctionBody *const functionBody, const uint index) const
    {TRACE_IT(48565);
        Assert(functionBody);
        Assert(!inlineCaches || functionBody->GetInlineCacheCount() == inlineCacheCount);
        Assert(index < functionBody->GetInlineCacheCount());
        return inlineCaches ? inlineCaches[index] : nullptr;
    }

    template<class T>
    T *InlineCachePointerArray<T>::GetInlineCache(const uint index) const
    {TRACE_IT(48566);
        Assert(index < inlineCacheCount);
        return inlineCaches[index];
    }

    template<class T>
    bool InlineCachePointerArray<T>::HasInlineCaches() const
    {TRACE_IT(48567);
        return inlineCaches != nullptr;
    }

    template<class T>
    void InlineCachePointerArray<T>::SetInlineCache(
        Recycler *const recycler,
        FunctionBody *const functionBody,
        const uint index,
        T *const inlineCache)
    {TRACE_IT(48568);
        Assert(recycler);
        Assert(functionBody);
        Assert(!inlineCaches || functionBody->GetInlineCacheCount() == inlineCacheCount);
        Assert(index < functionBody->GetInlineCacheCount());
        Assert(inlineCache);

        EnsureInlineCaches(recycler, functionBody);
        inlineCaches[index] = inlineCache;
    }

    template<class T>
    void InlineCachePointerArray<T>::Reset()
    {TRACE_IT(48569);
        inlineCaches = nullptr;
#if DBG
        inlineCacheCount = 0;
#endif
    }
}
