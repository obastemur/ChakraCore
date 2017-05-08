//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template<class T>
    class InlineCachePointerArray
    {
    public:
        typename FieldWithBarrier(Field(T*)*) inlineCaches;
    private:
#if DBG
        Field(uint) inlineCacheCount;
#endif

    public:
        InlineCachePointerArray<T>();

    private:
        void EnsureInlineCaches(Recycler *const recycler, FunctionBody *const functionBody);

    public:
        T *GetInlineCache(FunctionBody *const functionBody, const uint index) const;
        T *GetInlineCache(const uint index) const;
        bool HasInlineCaches() const;
        void SetInlineCache(
            Recycler *const recycler,
            FunctionBody *const functionBody,
            const uint index,
            T *const inlineCache);
        void Reset();

        template <class Fn>
        void Map(Fn fn, uint count) const
        {TRACE_IT(48559);
            if (NULL != inlineCaches)
            {TRACE_IT(48560);
                for (uint i =0; i < count; i++)
                {TRACE_IT(48561);
                    T* inlineCache = inlineCaches[i];

                    if (inlineCache != NULL)
                    {TRACE_IT(48562);
                        fn(inlineCache);
                    }
                }
            }
        };

        PREVENT_COPY(InlineCachePointerArray)
    };
}
