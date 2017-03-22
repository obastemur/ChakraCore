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
        {LOGMEIN("InlineCachePointerArray.h] 37\n");
            if (NULL != inlineCaches)
            {LOGMEIN("InlineCachePointerArray.h] 39\n");
                for (uint i =0; i < count; i++)
                {LOGMEIN("InlineCachePointerArray.h] 41\n");
                    T* inlineCache = inlineCaches[i];

                    if (inlineCache != NULL)
                    {LOGMEIN("InlineCachePointerArray.h] 45\n");
                        fn(inlineCache);
                    }
                }
            }
        };

        PREVENT_COPY(InlineCachePointerArray)
    };
}
