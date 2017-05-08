//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <
        class T,
        class Allocator = ArenaAllocator,
        bool isLeaf = false,
        template <typename Value> class TComparer = DefaultComparer>
    class Stack
    {
    private:
        List<T, Allocator, isLeaf, Js::CopyRemovePolicy, TComparer> list;

    public:
        Stack(Allocator* alloc) : list(alloc)
        {TRACE_IT(22372);
        }

        int Count() const {TRACE_IT(22373); return list.Count(); }
        bool Empty() const {TRACE_IT(22374); return Count() == 0; }

        void Clear()
        {TRACE_IT(22375);
            list.Clear();
        }

        bool Contains(const T& item) const
        {TRACE_IT(22376);
            return list.Contains(item);
        }

        const T& Top() const
        {TRACE_IT(22377);
            return list.Item(list.Count() - 1);
        }

        const T& Peek(int stepsBack = 0) const
        {TRACE_IT(22378);
            return list.Item(list.Count() - 1 - stepsBack);
        }

        T Pop()
        {TRACE_IT(22379);
            T item = list.Item(list.Count() - 1);
            list.RemoveAt(list.Count() - 1);
            return item;
        }

        T Pop(int count)
        {TRACE_IT(22380);
            T item = T();
            while (count-- > 0)
            {TRACE_IT(22381);
                item = Pop();
            }
            return item;
        }

        void Push(const T& item)
        {TRACE_IT(22382);
            list.Add(item);
        }
    };
}
