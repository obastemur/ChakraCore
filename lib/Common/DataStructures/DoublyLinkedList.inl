//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template<class T, class TAllocator>
    DoublyLinkedList<T, TAllocator>::DoublyLinkedList() : head(nullptr), tail(nullptr)
    {TRACE_IT(21251);
    }

    template<class T, class TAllocator>
    T *DoublyLinkedList<T, TAllocator>::Head() const
    {TRACE_IT(21252);
        return head;
    }

    template<class T, class TAllocator>
    T *DoublyLinkedList<T, TAllocator>::Tail() const
    {TRACE_IT(21253);
        return tail;
    }

    template<class T, class TAllocator>
    bool DoublyLinkedList<T, TAllocator>::Contains(T *const element) const
    {TRACE_IT(21254);
        return T::Contains(element, head);
    }

    template<class T, class TAllocator>
    bool DoublyLinkedList<T, TAllocator>::ContainsSubsequence(T *const first, T *const last) const
    {TRACE_IT(21255);
        return T::ContainsSubsequence(first, last, head);
    }

    template<class T, class TAllocator>
    bool DoublyLinkedList<T, TAllocator>::IsEmpty()
    {TRACE_IT(21256);
        return head == nullptr;
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::Clear()
    {TRACE_IT(21257);
        tail = head = nullptr;
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::LinkToBeginning(T *const element)
    {TRACE_IT(21258);
        T::LinkToBeginning(element, &head, &tail);
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::LinkToEnd(T *const element)
    {TRACE_IT(21259);
        T::LinkToEnd(element, &head, &tail);
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::LinkBefore(T *const element, T *const nextElement)
    {TRACE_IT(21260);
        T::LinkBefore(element, nextElement, &head, &tail);
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::LinkAfter(T *const element, T *const previousElement)
    {TRACE_IT(21261);
        T::LinkAfter(element, previousElement, &head, &tail);
    }

    template<class T, class TAllocator>
    T *DoublyLinkedList<T, TAllocator>::UnlinkFromBeginning()
    {TRACE_IT(21262);
        T *const element = head;
        if(element)
            T::UnlinkFromBeginning(element, &head, &tail);
        return element;
    }

    template<class T, class TAllocator>
    T *DoublyLinkedList<T, TAllocator>::UnlinkFromEnd()
    {TRACE_IT(21263);
        T *const element = tail;
        if(element)
            T::UnlinkFromEnd(element, &head, &tail);
        return element;
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::UnlinkPartial(T *const element)
    {TRACE_IT(21264);
        T::UnlinkPartial(element, &head, &tail);
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::Unlink(T *const element)
    {TRACE_IT(21265);
        T::Unlink(element, &head, &tail);
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::MoveToBeginning(T *const element)
    {TRACE_IT(21266);
        T::MoveToBeginning(element, &head, &tail);
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::UnlinkSubsequenceFromEnd(T *const first)
    {TRACE_IT(21267);
        T::UnlinkSubsequenceFromEnd(first, &head, &tail);
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::UnlinkSubsequence(T *const first, T *const last)
    {TRACE_IT(21268);
        T::UnlinkSubsequence(first, last, &head, &tail);
    }

    template<class T, class TAllocator>
    void DoublyLinkedList<T, TAllocator>::MoveSubsequenceToBeginning(T *const first, T *const last)
    {TRACE_IT(21269);
        T::MoveSubsequenceToBeginning(first, last, &head, &tail);
    }
}
