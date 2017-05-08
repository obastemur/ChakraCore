//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template<class TFirst, class TSecond, template<class TValue> class Comparer = DefaultComparer>
    class Pair
    {
    private:
        TFirst first;
        TSecond second;
    #if DBG
        bool initialized;
    #endif

    public:
        Pair()
        #if DBG
            : initialized(false)
        #endif
        {
            Assert(!IsValid());
        }

        Pair(const TFirst &first, const TSecond &second)
            : first(first),
            second(second)
        #if DBG
            ,
            initialized(true)
        #endif
        {
            Assert(IsValid());
        }

    #if DBG
    private:
        bool IsValid() const
        {TRACE_IT(21965);
            return initialized;
        }
    #endif

    public:
        const TFirst &First() const
        {TRACE_IT(21966);
            Assert(IsValid());
            return first;
        }

        const TSecond &Second() const
        {TRACE_IT(21967);
            Assert(IsValid());
            return second;
        }

    public:
        bool operator ==(const Pair &other) const
        {TRACE_IT(21968);
            return Comparer<TFirst>::Equals(first, other.first) && Comparer<TSecond>::Equals(second, other.second);
        }

        operator hash_t() const
        {TRACE_IT(21969);
            return Comparer<TFirst>::GetHashCode(first) + Comparer<TSecond>::GetHashCode(second);
        }
    };
}
