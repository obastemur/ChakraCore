//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// File: DList.h
//
// Template for Doubly Linked List
//----------------------------------------------------------------------------

template <typename TData, typename TCount = DefaultCount> class DListBase;
template <typename TData> class DListNode;
template <typename TData>
class DListNodeBase
{
public:
    DListNodeBase<TData> * Next() const {TRACE_IT(21033); return next.base; }
    DListNodeBase<TData> *& Next() {TRACE_IT(21034); return next.base; }
    DListNodeBase<TData> * Prev() const {TRACE_IT(21035); return prev.base; }
    DListNodeBase<TData> *& Prev() {TRACE_IT(21036); return prev.base; }
private:
    // The next node can be a real node with data, or  it point back to the start of the list
    // Use a union to show it in the debugger (instead of casting everywhere)
    union
    {
        DListNodeBase<TData> * base;
        DListNode<TData> * node;
        DListBase<TData>  * list;
    } next;

    union
    {
        DListNodeBase<TData> * base;
        DListNode<TData> * node;
        DListBase<TData>  * list;
    } prev;
};

template <typename TData>
class DListNode : public DListNodeBase<TData>
{
public:
    DListNode() : data() {TRACE_IT(21037);}

    // Constructing with parameter
    template <typename TParam1>
    DListNode(TParam1 param1) : data(param1) {TRACE_IT(21038);}

    // Constructing with parameter
    template <typename TParam1, typename TParam2>
    DListNode(TParam1 param1, TParam2 param2) : data(param1, param2) {TRACE_IT(21039);}

    // Constructing with parameter
    template <typename TParam1, typename TParam2, typename TParam3>
    DListNode(TParam1 param1, TParam2 param2, TParam3 param3) : data(param1, param2, param3) {TRACE_IT(21040);}

    // Constructing with parameter
    template <typename TParam1, typename TParam2, typename TParam3, typename TParam4>
    DListNode(TParam1 param1, TParam2 param2, TParam3 param3, TParam4 param4) : data(param1, param2, param3, param4) {TRACE_IT(21041);}

    // Constructing with parameter
    template <typename TParam1, typename TParam2, typename TParam3, typename TParam4, typename TParam5>
    DListNode(TParam1 param1, TParam2 param2, TParam3 param3, TParam4 param4, TParam5 param5) : data(param1, param2, param3, param4, param5) {TRACE_IT(21042);}

    // Constructing using copy constructor
    DListNode(TData const& data) : data(data) {TRACE_IT(21043);};
    TData data;
};


template<typename TData, typename TCount>
class DListBase : protected DListNodeBase<TData>, public TCount
{
private:
    typedef DListNodeBase<TData> NodeBase;
    typedef DListNode<TData> Node;
    bool IsHead(NodeBase const * node) const
    {TRACE_IT(21044);
        return (node == this);
    }
public:
    class Iterator
    {
    public:
        Iterator() : list(nullptr), current(nullptr) {TRACE_IT(21045);}
        Iterator(DListBase const * list) : list(list), current(list) {TRACE_IT(21046);};

        bool IsValid() const
        {TRACE_IT(21047);
            return (current != nullptr && !list->IsHead(current));
        }
        void Reset()
        {TRACE_IT(21048);
            current = list;
        }

        // TODO: only need inline for DListBase<Segment, FakeCount>::Iterator::Next
        __forceinline
        bool Next()
        {TRACE_IT(21049);
            Assert(current != nullptr);
            if (list->IsHead(current->Next()))
            {TRACE_IT(21050);
                current = nullptr;
                return false;
            }
            current = current->Next();
            return true;
        }
        TData const& Data() const
        {TRACE_IT(21051);
            Assert(this->IsValid());
            return ((Node *)current)->data;
        }
        TData& Data()
        {TRACE_IT(21052);
            Assert(this->IsValid());
            return ((Node *)current)->data;
        }
    protected:
        DListBase const * list;
        NodeBase const * current;
    };

    class EditingIterator : public Iterator
    {
    public:
        EditingIterator() : Iterator() {TRACE_IT(21053);};
        EditingIterator(DListBase  * list) : Iterator(list) {TRACE_IT(21054);};

        template <typename TAllocator>
        void RemoveCurrent(TAllocator * allocator)
        {TRACE_IT(21055);
            Assert(this->current != nullptr);
            Assert(!this->list->IsHead(this->current));

            NodeBase * last = this->current->Prev();
            NodeBase * node = const_cast<NodeBase *>(this->current);
            DListBase::RemoveNode(node);
            AllocatorDelete(TAllocator, allocator, (Node *)node);
            this->current = last;
            const_cast<DListBase *>(this->list)->DecrementCount();
        }

        template <typename TAllocator>
        TData * InsertNodeBefore(TAllocator * allocator)
        {TRACE_IT(21056);
            Node * newNode = AllocatorNew(TAllocator, allocator, Node);
            if (newNode)
            {TRACE_IT(21057);
                NodeBase * node = const_cast<NodeBase *>(this->current);
                DListBase::InsertNodeBefore(node, newNode);
                const_cast<DListBase *>(this->list)->IncrementCount();
                return newNode->data;
            }
        }

        template <typename TAllocator>
        bool InsertBefore(TAllocator * allocator, TData const& data)
        {TRACE_IT(21058);
            Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
            if (newNode)
            {TRACE_IT(21059);
                NodeBase * node = const_cast<NodeBase *>(this->current);
                DListBase::InsertNodeBefore(node, newNode);
                const_cast<DListBase *>(this->list)->IncrementCount();
                return true;
            }
            return false;
        }

        void MoveCurrentTo(DListBase * toList)
        {TRACE_IT(21060);
            NodeBase * last = this->current->Prev();
            NodeBase * node = const_cast<NodeBase *>(this->current);
            DListBase::RemoveNode(node);
            DListBase::InsertNodeBefore(toList->Next(), node);
            this->current = last;
            const_cast<DListBase *>(this->list)->DecrementCount();
            toList->IncrementCount();
        }
    };

    explicit DListBase()
    {TRACE_IT(21061);
        Reset();
    }

    ~DListBase()
    {TRACE_IT(21062);
        AssertMsg(this->Empty(), "DListBase need to be cleared explicitly with an allocator");
    }

    void Reset()
    {TRACE_IT(21063);
        this->Next() = this;
        this->Prev() = this;
        this->SetCount(0);
    }

    template <typename TAllocator>
    void Clear(TAllocator * allocator)
    {TRACE_IT(21064);
        NodeBase * current = this->Next();
        while (!this->IsHead(current))
        {TRACE_IT(21065);
            NodeBase * next = current->Next();
            AllocatorDelete(TAllocator, allocator, (Node *)current);
            current = next;
        }

        this->Next() = this;
        this->Prev() = this;
        this->SetCount(0);
    }
    bool Empty() const {TRACE_IT(21066); return this->IsHead(this->Next()); }
    bool HasOne() const {TRACE_IT(21067); return !Empty() && this->IsHead(this->Next()->Next()); }
    TData const& Head() const {TRACE_IT(21068); Assert(!Empty()); return ((Node *)this->Next())->data; }
    TData& Head() {TRACE_IT(21069); Assert(!Empty()); return ((Node *)this->Next())->data; }
    TData const& Tail() const {TRACE_IT(21070); Assert(!Empty()); return ((Node *)this->Prev())->data; }
    TData & Tail()  {TRACE_IT(21071); Assert(!Empty()); return ((Node *)this->Prev())->data; }

    template <typename TAllocator>
    bool Append(TAllocator * allocator, TData const& data)
    {TRACE_IT(21072);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
        if (newNode)
        {TRACE_IT(21073);
            DListBase::InsertNodeAfter(this->Prev(), newNode);
            this->IncrementCount();
            return true;
        }
        return false;
    }

    template <typename TAllocator>
    bool Prepend(TAllocator * allocator, TData const& data)
    {TRACE_IT(21074);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
        if (newNode)
        {TRACE_IT(21075);
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return true;
        }
        return false;
    }

    template <typename TAllocator>
    TData * PrependNode(TAllocator * allocator)
    {TRACE_IT(21076);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node);
        if (newNode)
        {TRACE_IT(21077);
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam1>
    TData * PrependNode(TAllocator * allocator, TParam1 param1)
    {TRACE_IT(21078);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1);
        if (newNode)
        {TRACE_IT(21079);
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam1, typename TParam2>
    TData * PrependNode(TAllocator * allocator, TParam1 param1, TParam2 param2)
    {TRACE_IT(21080);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1, param2);
        if (newNode)
        {TRACE_IT(21081);
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam1, typename TParam2, typename TParam3>
    TData * PrependNode(TAllocator * allocator, TParam1 param1, TParam2 param2, TParam3 param3)
    {TRACE_IT(21082);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1, param2, param3);
        if (newNode)
        {TRACE_IT(21083);
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam1, typename TParam2, typename TParam3, typename TParam4>
    TData * PrependNode(TAllocator * allocator, TParam1 param1, TParam2 param2, TParam3 param3, TParam4 param4)
    {TRACE_IT(21084);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1, param2, param3, param4);
        if (newNode)
        {TRACE_IT(21085);
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam1, typename TParam2, typename TParam3, typename TParam4, typename TParam5>
    TData * PrependNode(TAllocator * allocator, TParam1 param1, TParam2 param2, TParam3 param3, TParam4 param4, TParam5 param5)
    {TRACE_IT(21086);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1, param2, param3, param4, param5);
        if (newNode)
        {TRACE_IT(21087);
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator>
    void RemoveHead(TAllocator * allocator)
    {TRACE_IT(21088);
        Assert(!this->Empty());

        NodeBase * node = this->Next();
        DListBase::RemoveNode(node);
        AllocatorDelete(TAllocator, allocator, (Node *)node);

        this->DecrementCount();
    }

    template <typename TAllocator>
    bool Remove(TAllocator * allocator, TData const& data)
    {TRACE_IT(21089);
        EditingIterator iter(this);
        while (iter.Next())
        {TRACE_IT(21090);
            if (iter.Data() == data)
            {TRACE_IT(21091);
                iter.RemoveCurrent(allocator);
                return true;
            }
        }
        return false;
    }

    template <typename TAllocator>
    void RemoveElement(TAllocator * allocator, TData * element)
    {TRACE_IT(21092);
        Node * node = CONTAINING_RECORD(element, Node, data);
#if DBG_DUMP
        Assert(HasNode(node));
#endif
        DListBase::RemoveNode(node);
        AllocatorDelete(TAllocator, allocator, node);

        this->DecrementCount();

    }

    bool Has(TData data) const
    {TRACE_IT(21093);
        Iterator iter(this);
        while (iter.Next())
        {TRACE_IT(21094);
            if (iter.Data() == data)
            {TRACE_IT(21095);
                return true;
            }
        }
        return false;
    }

    void MoveTo(DListBase * list)
    {TRACE_IT(21096);

        list->Prev()->Next() = this->Next();
        this->Next()->Prev() = list->Prev();

        list->Prev() = this->Prev();
        this->Prev()->Next() = list;

        this->Prev() = this;
        this->Next() = this;

        list->AddCount(*this);
        this->SetCount(0);
    }

    void MoveHeadTo(DListBase * list)
    {TRACE_IT(21097);
        Assert(!this->Empty());
        NodeBase * node = this->Next();
        DListBase::RemoveNode(node);
        DListBase::InsertNodeBefore(list->Next(), node);
        this->DecrementCount();
        list->IncrementCount();
    }

    void MoveElementTo(TData * element, DListBase * list)
    {TRACE_IT(21098);
        Node * node = CONTAINING_RECORD(element, Node, data);
#if DBG_DUMP
        Assert(HasNode(node));
#endif
        DListBase::RemoveNode(node);
        DListBase::InsertNodeBefore(list->Next(), node);
        this->DecrementCount();
        list->IncrementCount();
    }

#if DBG_DUMP
    bool HasElement(TData const * element) const
    {TRACE_IT(21099);
        Node * node = CONTAINING_RECORD(element, Node, data);
        return HasNode(node);
    }
#endif

private:
#if DBG_DUMP
    bool HasNode(NodeBase * node) const
    {TRACE_IT(21100);
        NodeBase * current = this->Next();
        while (!this->IsHead(current))
        {TRACE_IT(21101);
            if (node == current)
            {TRACE_IT(21102);
                return true;
            }
            current = current->Next();
        }
        return false;
    }
#endif

    // disable copy constructor
    DListBase(DListBase const& list);

    static void InsertNodeAfter(NodeBase * node, NodeBase * newNode)
    {TRACE_IT(21103);
        newNode->Prev() = node;
        newNode->Next() = node->Next();
        node->Next()->Prev() = newNode;
        node->Next() = newNode;
    }

    static void InsertNodeBefore(NodeBase * node, NodeBase * newNode)
    {TRACE_IT(21104);
        newNode->Prev() = node->Prev();
        newNode->Next() = node;
        node->Prev()->Next() = newNode;
        node->Prev() = newNode;
    }

    static void RemoveNode(NodeBase * node)
    {TRACE_IT(21105);
        node->Prev()->Next() = node->Next();
        node->Next()->Prev() = node->Prev();
    }
};

#define FOREACH_DLISTBASE_ENTRY(T, data, list) \
{TRACE_IT(21106); \
    _TYPENAME DListBase<T>::Iterator __iter(list); \
    while (__iter.Next()) \
    {TRACE_IT(21107); \
        T& data = __iter.Data();

#define NEXT_DLISTBASE_ENTRY \
    }  \
}

#define FOREACH_DLISTBASE_ENTRY_EDITING(T, data, list, iter) \
    _TYPENAME DListBase<T>::EditingIterator iter(list); \
    while (iter.Next()) \
    {TRACE_IT(21108); \
        T& data = iter.Data();

#define NEXT_DLISTBASE_ENTRY_EDITING \
    }

template <typename TData, typename TAllocator, typename TCount = DefaultCount>
class DList : public DListBase<TData, TCount>
{
public:
    class EditingIterator : public DListBase<TData, TCount>::EditingIterator
    {
    public:
        EditingIterator() : DListBase<TData, TCount>::EditingIterator() {TRACE_IT(21109);}
        EditingIterator(DList * list) : DListBase<TData, TCount>::EditingIterator(list) {TRACE_IT(21110);}
        void RemoveCurrent()
        {TRACE_IT(21111);
            __super::RemoveCurrent(Allocator());
        }
        TData& InsertNodeBefore()
        {TRACE_IT(21112);
            return __super::InsertNodeBefore(Allocator());
        }
        void InsertBefore(TData const& data)
        {TRACE_IT(21113);
            __super::InsertBefore(Allocator(), data);
        }

    private:
        TAllocator * Allocator() const
        {TRACE_IT(21114);
            return ((DList const *)this->list)->allocator;
        }
    };

    explicit DList(TAllocator * allocator) : allocator(allocator) {TRACE_IT(21115);}
    ~DList()
    {TRACE_IT(21116);
        Clear();
    }
    void Clear()
    {TRACE_IT(21117);
        __super::Clear(allocator);
    }
    bool Append(TData const& data)
    {TRACE_IT(21118);
        return __super::Append(allocator, data);
    }
    bool Prepend(TData const& data)
    {TRACE_IT(21119);
        return __super::Prepend(allocator, data);
    }
    TData * PrependNode()
    {TRACE_IT(21120);
        return __super::PrependNode(allocator);
    }
    template <typename TParam1>
    TData * PrependNode(TParam1 param1)
    {TRACE_IT(21121);
        return __super::PrependNode(allocator, param1);
    }
    template <typename TParam1, typename TParam2>
    TData * PrependNode(TParam1 param1, TParam2 param2)
    {TRACE_IT(21122);
        return __super::PrependNode(allocator, param1, param2);
    }
    template <typename TParam1, typename TParam2, typename TParam3>
    TData * PrependNode(TParam1 param1, TParam2 param2, TParam3 param3)
    {TRACE_IT(21123);
        return __super::PrependNode(allocator, param1, param2, param3);
    }
    template <typename TParam1, typename TParam2, typename TParam3, typename TParam4>
    TData * PrependNode(TParam1 param1, TParam2 param2, TParam3 param3, TParam4 param4)
    {TRACE_IT(21124);
        return __super::PrependNode(allocator, param1, param2, param3, param4);
    }
    void RemoveHead()
    {TRACE_IT(21125);
        __super::RemoveHead(allocator);
    }
    bool Remove(TData const& data)
    {TRACE_IT(21126);
        return __super::Remove(allocator, data);
    }

    void RemoveElement(TData * data)
    {TRACE_IT(21127);
        return __super::RemoveElement(allocator, data);
    }

private:
    TAllocator * allocator;
};

template <typename TData, typename TAllocator = ArenaAllocator>
class DListCounted : public DList<TData, TAllocator, RealCount>
{
public:
    explicit DListCounted(TAllocator * allocator) : DList<TData, TAllocator, RealCount>(allocator) {TRACE_IT(21128);}
};

#define FOREACH_DLIST_ENTRY(T, alloc, data, list) \
{ \
    DList<T, alloc>::Iterator __iter(list); \
    while (__iter.Next()) \
    {TRACE_IT(21129); \
        T& data = __iter.Data();

#define NEXT_DLIST_ENTRY \
    }  \
}

#define FOREACH_DLIST_ENTRY_EDITING(T, alloc, data, list, iter) \
    DList<T, alloc>::EditingIterator iter(list); \
    while (iter.Next()) \
    {TRACE_IT(21130); \
        T& data = iter.Data();

#define NEXT_DLIST_ENTRY_EDITING \
    }
