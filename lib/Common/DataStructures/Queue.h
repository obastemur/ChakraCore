//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <class T, class Allocator>
    class Queue
    {
    private:
        DList<T, Allocator> list;

    public:
        Queue(Allocator* alloc) : list(alloc)
        {TRACE_IT(21970);
        }

        bool Empty() const
        {TRACE_IT(21971);
            return list.Empty();
        }

        void Enqueue(const T& item)
        {TRACE_IT(21972);
            list.Append(item);
        }

        T Dequeue()
        {TRACE_IT(21973);
            T item = list.Head();
            list.RemoveHead();
            return item;
        }

        void Clear()
        {TRACE_IT(21974);
            list.Clear();
        }
    };
}
