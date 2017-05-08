//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template<class T, class TAllocator>
    DoublyLinkedListElement<T, TAllocator>::DoublyLinkedListElement() : previous(nullptr), next(nullptr)
    {TRACE_IT(21270);
        TemplateParameter::SameOrDerivedFrom<T, DoublyLinkedListElement<T, TAllocator>>();
    }

    template<class T, class TAllocator>
    T *DoublyLinkedListElement<T, TAllocator>::Previous() const
    {TRACE_IT(21271);
        return previous;
    }

    template<class T, class TAllocator>
    T *DoublyLinkedListElement<T, TAllocator>::Next() const
    {TRACE_IT(21272);
        return next;
    }

    template<class T, class TAllocator>
    template<class D>
    bool DoublyLinkedListElement<T, TAllocator>::Contains(
        D *const element, Field(D *, TAllocator) const head)
    {TRACE_IT(21273);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(element);
        Assert(!head || !head->previous);

        if(!element->previous && !element->next)
            return element == head;

        for(T *e = head; e; e = e->next)
        {TRACE_IT(21274);
            if(e == element)
                return true;
        }
        return false;
    }

    template<class T, class TAllocator>
    template<class D>
    bool DoublyLinkedListElement<T, TAllocator>::ContainsSubsequence(
        D *const first, D *const last, Field(D *, TAllocator) const head)
    {TRACE_IT(21275);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(first);
        Assert(last);
        Assert(!head || !head->previous);

        if(first == last && !first->previous && !first->next)
            return first == head;

        bool foundFirst = false;
        for(T *e = head; e; e = e->next)
        {TRACE_IT(21276);
            if(e == first)
                foundFirst = true;
            if(e == last)
                return foundFirst;
        }
        return false;
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::LinkToBeginning(
        D *const element,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21277);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(element);
        Assert(head);
        Assert(tail);
        Assert(!*head || !(*head)->previous);
        Assert(!*tail || !(*tail)->next);
        Assert(!element->previous);
        Assert(!element->next);
        Assert(!Contains(element, *head));

        element->previous = nullptr;
        element->next = *head;
        *head = element;
        if(element->next)
            element->next->previous = element;
        else
        {TRACE_IT(21278);
            Assert(!*tail);
            *tail = element;
        }
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::LinkToEnd(
        D *const element,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21279);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(element);
        Assert(head);
        Assert(tail);
        Assert(!*head || !(*head)->previous);
        Assert(!*tail || !(*tail)->next);
        Assert(!element->previous);
        Assert(!element->next);
        Assert(!Contains(element, *head));

        element->previous = *tail;
        element->next = nullptr;
        *tail = element;
        if(element->previous)
            element->previous->next = element;
        else
        {TRACE_IT(21280);
            Assert(!*head);
            *head = element;
        }
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::LinkBefore(
        D *const element, D *const nextElement,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21281);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(element);
        Assert(nextElement);
        Assert(element != nextElement);
        Assert(head);
        Assert(tail);
        Assert(*head);
        Assert(*tail);
        Assert(!(*head)->previous);
        Assert(!(*tail)->next);
        Assert(!element->previous);
        Assert(!element->next);
        Assert(Contains(nextElement, *head));

        element->next = nextElement;
        T *const previousElement = nextElement->previous;
        element->previous = previousElement;
        nextElement->previous = element;
        if(previousElement)
            previousElement->next = element;
        else
        {TRACE_IT(21282);
            Assert(*head == nextElement);
            *head = element;
        }
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::LinkAfter(
        D *const element, D *const previousElement,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21283);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(element);
        Assert(previousElement);
        Assert(element != previousElement);
        Assert(head);
        Assert(tail);
        Assert(*head);
        Assert(*tail);
        Assert(!(*head)->previous);
        Assert(!(*tail)->next);
        Assert(!element->previous);
        Assert(!element->next);
        Assert(Contains(previousElement, *head));

        element->previous = previousElement;
        T *const nextElement = previousElement->next;
        element->next = nextElement;
        previousElement->next = element;
        if(nextElement)
            nextElement->previous = element;
        else
        {TRACE_IT(21284);
            Assert(*tail == previousElement);
            *tail = element;
        }
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::UnlinkFromBeginning(
        D *const element,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21285);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(element);
        Assert(head);
        Assert(tail);
        Assert(*head);
        Assert(*tail);
        Assert(!(*head)->previous);
        Assert(!(*tail)->next);
        Assert(!element->previous);

        Assert(*head == element);
        *head = static_cast<D *>(element->next);

        if(element->next)
        {TRACE_IT(21286);
            element->next->previous = nullptr;
            element->next = nullptr;
        }
        else
        {TRACE_IT(21287);
            Assert(*tail == element);
            *tail = nullptr;
        }
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::UnlinkFromEnd(
        D *const element,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21288);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(element);
        Assert(head);
        Assert(tail);
        Assert(*head);
        Assert(*tail);
        Assert(!(*head)->previous);
        Assert(!(*tail)->next);
        Assert(!element->next);

        Assert(*tail == element);
        *tail = static_cast<D *>(element->previous);

        if(element->previous)
        {TRACE_IT(21289);
            element->previous->next = nullptr;
            element->previous = nullptr;
        }
        else
        {TRACE_IT(21290);
            Assert(*head == element);
            *head = nullptr;
        }
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::UnlinkPartial(
        D *const element,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21291);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(element);
        Assert(head);
        Assert(tail);
        Assert(*head);
        Assert(*tail);
        Assert(!(*head)->previous);
        Assert(!(*tail)->next);
        Assert(Contains(element, *head));

        if(element->previous)
            element->previous->next = element->next;
        else
        {TRACE_IT(21292);
            Assert(*head == element);
            *head = static_cast<D *>(element->next);
        }

        if(element->next)
            element->next->previous = element->previous;
        else
        {TRACE_IT(21293);
            Assert(*tail == element);
            *tail = static_cast<D *>(element->previous);
        }

        // Partial unlink does not zero the previous and next links of the unlinked element so that the linked list can be
        // iterated on a separate thread while unlinking, without missing elements that are in the linked list before and after
        // this unlink
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::Unlink(
        D *const element,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {
        UnlinkPartial(element, head, tail);
        element->previous = nullptr;
        element->next = nullptr;
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::MoveToBeginning(
        D *const element,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21294);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(element);
        Assert(head);
        Assert(tail);
        Assert(*head);
        Assert(*tail);
        Assert(!(*head)->previous);
        Assert(!(*tail)->next);
        Assert(Contains(element, *head));

        if(!element->previous)
        {TRACE_IT(21295);
            Assert(*head == element);
            return;
        }

        element->previous->next = element->next;
        if(element->next)
            element->next->previous = element->previous;
        else
        {TRACE_IT(21296);
            Assert(*tail == element);
            *tail = static_cast<D *>(element->previous);
        }

        element->previous = nullptr;
        element->next = *head;
        *head = element;
        element->next->previous = element;
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::UnlinkSubsequenceFromEnd(
        D *const first,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21297);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(first);
        Assert(head);
        Assert(tail);
        Assert(*head);
        Assert(*tail);
        Assert(!(*head)->previous);
        Assert(!(*tail)->next);
        Assert(Contains(first, *head));

        if(first->previous)
            first->previous->next = nullptr;
        else
        {TRACE_IT(21298);
            Assert(*head == first);
            *head = nullptr;
        }

        *tail = static_cast<D *>(first->previous);
        first->previous = nullptr;
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::UnlinkSubsequence(
        D *const first, D *const last,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21299);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(first);
        Assert(last);
        Assert(head);
        Assert(tail);
        Assert(*head);
        Assert(*tail);
        Assert(!(*head)->previous);
        Assert(!(*tail)->next);
        Assert(ContainsSubsequence(first, last, *head));

        if(first->previous)
            first->previous->next = last->next;
        else
        {TRACE_IT(21300);
            Assert(*head == first);
            *head = static_cast<D *>(last->next);
        }

        if(last->next)
            last->next->previous = first->previous;
        else
        {TRACE_IT(21301);
            Assert(*tail == last);
            *tail = static_cast<D *>(first->previous);
        }

        first->previous = nullptr;
        last->next = nullptr;
    }

    template<class T, class TAllocator>
    template<class D>
    void DoublyLinkedListElement<T, TAllocator>::MoveSubsequenceToBeginning(
        D *const first, D *const last,
        Field(D *, TAllocator) *const head, Field(D *, TAllocator) *const tail)
    {TRACE_IT(21302);
        TemplateParameter::SameOrDerivedFrom<D, T>();
        Assert(first);
        Assert(last);
        Assert(head);
        Assert(tail);
        Assert(*head);
        Assert(*tail);
        Assert(!(*head)->previous);
        Assert(!(*tail)->next);
        Assert(ContainsSubsequence(first, last, *head));

        if(!first->previous)
        {TRACE_IT(21303);
            Assert(*head == first);
            return;
        }

        first->previous->next = last->next;
        if(last->next)
            last->next->previous = first->previous;
        else
        {TRACE_IT(21304);
            Assert(*tail == last);
            *tail = static_cast<D *>(first->previous);
        }

        first->previous = nullptr;
        last->next = *head;
        *head = static_cast<D *>(first);
        last->next->previous = last;
    }
}
