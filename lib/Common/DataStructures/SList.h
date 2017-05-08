//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// File: SList.h
//
// Template for Singly Linked List
//
//----------------------------------------------------------------------------

class FakeCount
{
protected:
    void IncrementCount() {TRACE_IT(21997);}
    void DecrementCount() {TRACE_IT(21998);}
    void SetCount(uint count) {TRACE_IT(21999);}
    void AddCount(FakeCount& c) {TRACE_IT(22000);}
};

class RealCount
{
protected:
    RealCount() : count(0) {TRACE_IT(22001);}
    void IncrementCount() {TRACE_IT(22002); count++; }
    void DecrementCount() {TRACE_IT(22003); count--; }
    void SetCount(uint count) {TRACE_IT(22004); this->count = count; }
    void AddCount(RealCount const& c) {TRACE_IT(22005); this->count += c.Count(); }
public:
    uint Count() const {TRACE_IT(22006); return count; }
private:
    Field(uint) count;
};

#if DBG
typedef RealCount DefaultCount;
#else
typedef FakeCount DefaultCount;
#endif

template <typename TData,
          typename TAllocator = ArenaAllocator,
          typename TCount = DefaultCount> class SListBase;

template <typename TAllocator>
class SListNodeBase
{
public:
    Field(SListNodeBase*, TAllocator) Next() const {TRACE_IT(22007); return next; }
    Field(SListNodeBase*, TAllocator)& Next() {TRACE_IT(22008); return next; }

protected:
    // The next node can be a real node with data, or it point back to the start of the list
    Field(SListNodeBase*, TAllocator) next;
};

template <typename TData, typename TAllocator>
class SListNode : public SListNodeBase<TAllocator>
{
    friend class SListBase<TData, TAllocator, FakeCount>;
    friend class SListBase<TData, TAllocator, RealCount>;
public:
    TData* GetData()
    {TRACE_IT(22009);
        return &data;
    }
private:
    SListNode() : data() {TRACE_IT(22010);}

    // Constructing with parameter
    template <typename TParam>
    SListNode(const TParam& param) : data(param) {TRACE_IT(22011);}

    // Constructing with parameter
    template <typename TParam1, typename TParam2>
    SListNode(const TParam1& param1, const TParam2& param2) : data(param1, param2) {}

    Field(TData, TAllocator) data;
};

template<typename TData, typename TAllocator, typename TCount>
class SListBase : protected SListNodeBase<TAllocator>, public TCount
{
private:
    typedef SListNodeBase<TAllocator> NodeBase;
    typedef SListNode<TData, TAllocator> Node;

    bool IsHead(NodeBase const * node) const
    {TRACE_IT(22012);
        return (node == this);
    }

public:
    class Iterator
    {
    public:
        Iterator() : list(nullptr), current(nullptr) {TRACE_IT(22013);}
        Iterator(SListBase const * list) : list(list), current(list) {TRACE_IT(22014);};

        bool IsValid() const
        {TRACE_IT(22015);
            return (current != nullptr && !list->IsHead(current));
        }
        void Reset()
        {TRACE_IT(22016);
            current = list;
        }

        // forceinline only needed for SListBase<FlowEdge *, RealCount>::Iterator::Next()
        __forceinline
        bool Next()
        {TRACE_IT(22017);
            Assert(current != nullptr);
            if (list->IsHead(current->Next()))
            {TRACE_IT(22018);
                current = nullptr;
                return false;
            }
            current = current->Next();
            return true;
        }
        Field(TData, TAllocator) const& Data() const
        {TRACE_IT(22019);
            Assert(this->IsValid());
            return ((Node *)current)->data;
        }
        Field(TData, TAllocator)& Data()
        {TRACE_IT(22020);
            Assert(this->IsValid());
            return ((Node *)current)->data;
        }
    protected:
        SListBase const * list;
        NodeBase const * current;
    };

    class EditingIterator : public Iterator
    {
    public:
        EditingIterator() : Iterator(), last(nullptr) {TRACE_IT(22021);};
        EditingIterator(SListBase * list) : Iterator(list), last(nullptr) {TRACE_IT(22022);};

        bool Next()
        {TRACE_IT(22023);
            if (last != nullptr && last->Next() != this->current)
            {TRACE_IT(22024);
                this->current = last;
            }
            else
            {TRACE_IT(22025);
                last = this->current;
            }
            return Iterator::Next();
        }

        void UnlinkCurrent()
        {TRACE_IT(22026);
            UnlinkCurrentNode();
        }

        void RemoveCurrent(TAllocator * allocator)
        {TRACE_IT(22027);
            const NodeBase *dead = this->current;
            UnlinkCurrent();

            auto freeFunc = TypeAllocatorFunc<TAllocator, TData>::GetFreeFunc();

            AllocatorFree(allocator, freeFunc, (Node *) dead, sizeof(Node));
        }

        Field(TData, TAllocator) * InsertNodeBefore(TAllocator * allocator)
        {TRACE_IT(22028);
            Assert(last != nullptr);
            Node * newNode = AllocatorNew(TAllocator, allocator, Node);
            if (newNode)
            {TRACE_IT(22029);
                newNode->Next() = last->Next();
                const_cast<NodeBase *>(last)->Next() = newNode;
                const_cast<SListBase *>(this->list)->IncrementCount();
                last = newNode;
                return &newNode->data;
            }
            return nullptr;
        }

        Field(TData, TAllocator) * InsertNodeBeforeNoThrow(TAllocator * allocator)
        {TRACE_IT(22030);
            Assert(last != nullptr);
            Node * newNode = AllocatorNewNoThrow(TAllocator, allocator, Node);
            if (newNode)
            {TRACE_IT(22031);
                newNode->Next() = last->Next();
                const_cast<NodeBase *>(last)->Next() = newNode;
                const_cast<SListBase *>(this->list)->IncrementCount();
                last = newNode;
                return &newNode->data;
            }
            return nullptr;
        }

        template <typename TParam1, typename TParam2>
        Field(TData, TAllocator) * InsertNodeBefore(TAllocator * allocator, TParam1 param1, TParam2 param2)
        {TRACE_IT(22032);
            Assert(last != nullptr);
            Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1, param2);
            if (newNode)
            {TRACE_IT(22033);
                newNode->Next() = last->Next();
                const_cast<NodeBase *>(last)->Next() = newNode;
                const_cast<SListBase *>(this->list)->IncrementCount();
                last = newNode;
                return &newNode->data;
            }
            return nullptr;
        }

        bool InsertBefore(TAllocator * allocator, TData const& data)
        {TRACE_IT(22034);
            Assert(last != nullptr);
            Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
            if (newNode)
            {TRACE_IT(22035);
                newNode->Next() = last->Next();
                const_cast<NodeBase *>(last)->Next() = newNode;
                const_cast<SListBase *>(this->list)->IncrementCount();
                last = newNode;
                return true;
            }
            return false;
        }

        void MoveCurrentTo(SListBase * toList)
        {TRACE_IT(22036);
            NodeBase * node = UnlinkCurrentNode();
            node->Next() = toList->Next();
            toList->Next() = node;
            toList->IncrementCount();
        }

        void SetNext(SListBase * newNext)
        {TRACE_IT(22037);
            Assert(last != nullptr);
            const_cast<NodeBase *>(last)->Next() = newNext;
        }

    private:
        NodeBase const * last;

        NodeBase * UnlinkCurrentNode()
        {TRACE_IT(22038);
            NodeBase * unlinkedNode = const_cast<NodeBase *>(this->current);
            Assert(this->current != nullptr);
            Assert(!this->list->IsHead(this->current));
            Assert(last != nullptr);

            const_cast<NodeBase *>(last)->Next() = this->current->Next();
            this->current = last;
            last = nullptr;
            const_cast<SListBase *>(this->list)->DecrementCount();
            return unlinkedNode;
        }
    };

    inline Iterator GetIterator() const {TRACE_IT(22039); return Iterator(this); }
    inline EditingIterator GetEditingIterator() {TRACE_IT(22040); return EditingIterator(this); }

    explicit SListBase()
    {TRACE_IT(22041);
        Reset();
    }

    ~SListBase()
    {TRACE_IT(22042);
        AssertMsg(this->Empty(), "SListBase need to be cleared explicitly with an allocator");
    }

    void Reset()
    {TRACE_IT(22043);
        this->Next() = this;
        this->SetCount(0);
    }

    __forceinline
    void Clear(TAllocator * allocator)
    {TRACE_IT(22044);
        NodeBase * current = this->Next();
        while (!this->IsHead(current))
        {TRACE_IT(22045);
            NodeBase * next = current->Next();

            auto freeFunc = TypeAllocatorFunc<TAllocator, TData>::GetFreeFunc();

            AllocatorFree(allocator, freeFunc, (Node *)current, sizeof(Node));
            current = next;
        }

        this->Reset();
    }

    bool Empty() const {TRACE_IT(22046); return this->IsHead(this->Next()); }
    bool HasOne() const {TRACE_IT(22047); return !Empty() && this->IsHead(this->Next()->Next()); }
    bool HasTwo() const {TRACE_IT(22048); return !Empty() && this->IsHead(this->Next()->Next()->Next()); }
    Field(TData, TAllocator) const& Head() const
        {TRACE_IT(22049); Assert(!Empty()); return ((Node *)this->Next())->data; }
    Field(TData, TAllocator)& Head()
        {TRACE_IT(22050); Assert(!Empty()); return ((Node *)this->Next())->data; }

    bool Prepend(TAllocator * allocator, TData const& data)
    {TRACE_IT(22051);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
        if (newNode)
        {TRACE_IT(22052);
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return true;
        }
        return false;
    }

    bool PrependNoThrow(TAllocator * allocator, TData const& data)
    {TRACE_IT(22053);
        Node * newNode = AllocatorNewNoThrow(TAllocator, allocator, Node, data);
        if (newNode)
        {TRACE_IT(22054);
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return true;
        }
        return false;
    }

    Field(TData, TAllocator) * PrependNode(TAllocator * allocator)
    {TRACE_IT(22055);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node);
        if (newNode)
        {TRACE_IT(22056);
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TParam>
    Field(TData, TAllocator) * PrependNode(TAllocator * allocator, TParam param)
    {TRACE_IT(22057);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param);
        if (newNode)
        {TRACE_IT(22058);
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TParam1, typename TParam2>
    Field(TData, TAllocator) * PrependNode(TAllocator * allocator, TParam1 param1, TParam2 param2)
    {TRACE_IT(22059);
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1, param2);
        if (newNode)
        {TRACE_IT(22060);
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    void RemoveHead(TAllocator * allocator)
    {TRACE_IT(22061);
        Assert(!this->Empty());

        NodeBase * node = this->Next();
        this->Next() = node->Next();

        auto freeFunc = TypeAllocatorFunc<TAllocator, TData>::GetFreeFunc();
        AllocatorFree(allocator, freeFunc, (Node *) node, sizeof(Node));
        this->DecrementCount();
    }

    bool Remove(TAllocator * allocator, TData const& data)
    {TRACE_IT(22062);
        EditingIterator iter(this);
        while (iter.Next())
        {TRACE_IT(22063);
            if (iter.Data() == data)
            {TRACE_IT(22064);
                iter.RemoveCurrent(allocator);
                return true;
            }
        }
        return false;
    }

    bool Has(TData data) const
    {TRACE_IT(22065);
        Iterator iter(this);
        while (iter.Next())
        {TRACE_IT(22066);
            if (iter.Data() == data)
            {TRACE_IT(22067);
                return true;
            }
        }
        return false;
    }

    void MoveTo(SListBase * list)
    {TRACE_IT(22068);
        while (!Empty())
        {TRACE_IT(22069);
            this->MoveHeadTo(list);
        }
    }

    void MoveHeadTo(SListBase * list)
    {TRACE_IT(22070);
        Assert(!this->Empty());
        NodeBase * node = this->Next();
        this->Next() = node->Next();
        node->Next() = list->Next();
        list->Next() = node;

        list->IncrementCount();
        this->DecrementCount();
    }

    // Moves the first element that satisfies the predicate to the toList
    template<class Fn>
    Field(TData, TAllocator)* MoveTo(SListBase* toList, Fn predicate)
    {TRACE_IT(22071);
        Assert(this != toList);

        EditingIterator iter(this);
        while (iter.Next())
        {TRACE_IT(22072);
            if (predicate(iter.Data()))
            {
                Field(TData, TAllocator)* data = &iter.Data();
                iter.MoveCurrentTo(toList);
                return data;
            }
        }
        return nullptr;
    }

    template<class Fn>
    Field(TData, TAllocator)* Find(Fn predicate)
    {TRACE_IT(22073);
        Iterator iter(this);
        while(iter.Next())
        {TRACE_IT(22074);
            if(predicate(iter.Data()))
            {TRACE_IT(22075);
                return &iter.Data();
            }
        }
        return nullptr;
    }

    template<class Fn>
    void Iterate(Fn fn)
    {TRACE_IT(22076);
        Iterator iter(this);
        while(iter.Next())
        {TRACE_IT(22077);
            fn(iter.Data());
        }
    }

    void Reverse()
    {TRACE_IT(22078);
        NodeBase * prev = this;
        NodeBase * current = this->Next();
        while (!this->IsHead(current))
        {TRACE_IT(22079);
            NodeBase * next = current->Next();
            current->Next() = prev;
            prev = current;
            current = next;
        }
        current->Next() = prev;
    }

    bool Equals(SListBase const& other)
    {TRACE_IT(22080);
        SListBase::Iterator iter(this);
        SListBase::Iterator iter2(&other);
        while (iter.Next())
        {TRACE_IT(22081);
            if (!iter2.Next() || iter.Data() != iter2.Data())
            {TRACE_IT(22082);
                return false;
            }
        }
        return !iter2.Next();
    }

    bool CopyTo(TAllocator * allocator, SListBase& to) const
    {TRACE_IT(22083);
        return CopyTo<DefaultCopyElement>(allocator, to);
    }

    template <void (*CopyElement)(
        Field(TData, TAllocator) const& from, Field(TData, TAllocator)& to)>
    bool CopyTo(TAllocator * allocator, SListBase& to) const
    {TRACE_IT(22084);
        to.Clear(allocator);
        SListBase::Iterator iter(this);
        NodeBase ** next = &to.Next();
        while (iter.Next())
        {TRACE_IT(22085);
            Node * node = AllocatorNew(TAllocator, allocator, Node);
            if (node == nullptr)
            {TRACE_IT(22086);
                return false;
            }
            CopyElement(iter.Data(), node->data);
            *next = node;
            next = &node->Next();
            *next = &to;            // Do this every time, in case an OOM exception occurs, to keep the list correct
            to.IncrementCount();
        }
        return true;
    }

    template <class Fn>
    void Map(Fn fn) const
    {
        MapUntil([fn](Field(TData, TAllocator)& data) { fn(data); return false; });
    }

    template <class Fn>
    bool MapUntil(Fn fn) const
    {TRACE_IT(22087);
        Iterator iter(this);
        while (iter.Next())
        {TRACE_IT(22088);
            if (fn(iter.Data()))
            {TRACE_IT(22089);
                return true;
            }
        }
        return false;
    }

private:
    static void DefaultCopyElement(
        Field(TData, TAllocator) const& from, Field(TData, TAllocator)& to)
    {TRACE_IT(22090);
        to = from;
    }

    // disable copy constructor
    SListBase(SListBase const& list);
};


template <typename TData, typename TAllocator = ArenaAllocator>
class SListBaseCounted : public SListBase<TData, TAllocator, RealCount>
{
};

template <typename TData, typename TAllocator = ArenaAllocator, typename TCount = DefaultCount>
class SList : public SListBase<TData, TAllocator, TCount>
{
public:
    class EditingIterator : public SListBase<TData, TAllocator, TCount>::EditingIterator
    {
    public:
        EditingIterator() : SListBase<TData, TAllocator, TCount>::EditingIterator() {TRACE_IT(22091);}
        EditingIterator(SList * list) : SListBase<TData, TAllocator, TCount>::EditingIterator(list) {TRACE_IT(22092);}
        void RemoveCurrent()
        {TRACE_IT(22093);
            __super::RemoveCurrent(Allocator());
        }
        Field(TData, TAllocator) * InsertNodeBefore()
        {TRACE_IT(22094);
            return __super::InsertNodeBefore(Allocator());
        }
        bool InsertBefore(TData const& data)
        {TRACE_IT(22095);
            return __super::InsertBefore(Allocator(), data);
        }
    private:
        TAllocator * Allocator() const
        {TRACE_IT(22096);
            return ((SList const *)this->list)->allocator;
        }
    };

    inline EditingIterator GetEditingIterator() {TRACE_IT(22097); return EditingIterator(this); }

    explicit SList(TAllocator * allocator) : allocator(allocator) {TRACE_IT(22098);}
    ~SList()
    {TRACE_IT(22099);
        Clear();
    }
    void Clear()
    {TRACE_IT(22100);
        __super::Clear(allocator);
    }
    bool Prepend(TData const& data)
    {TRACE_IT(22101);
        return __super::Prepend(allocator, data);
    }
    Field(TData, TAllocator) * PrependNode()
    {TRACE_IT(22102);
        return __super::PrependNode(allocator);
    }
    template <typename TParam>
    Field(TData, TAllocator) * PrependNode(TParam param)
    {TRACE_IT(22103);
        return __super::PrependNode(allocator, param);
    }
    template <typename TParam1, typename TParam2>
    Field(TData, TAllocator) * PrependNode(TParam1 param1, TParam2 param2)
    {TRACE_IT(22104);
        return __super::PrependNode(allocator, param1, param2);
    }
    void RemoveHead()
    {TRACE_IT(22105);
        __super::RemoveHead(allocator);
    }
    bool Remove(TData const& data)
    {TRACE_IT(22106);
        return __super::Remove(allocator, data);
    }

    // Stack like interface
    bool Push(TData const& data)
    {TRACE_IT(22107);
        return Prepend(data);
    }

    TData Pop()
    {TRACE_IT(22108);
        TData data = this->Head();
        RemoveHead();
        return data;
    }

    Field(TData, TAllocator) const& Top() const
    {TRACE_IT(22109);
        return this->Head();
    }
    Field(TData, TAllocator)& Top()
    {TRACE_IT(22110);
        return this->Head();
    }

private:
    FieldNoBarrier(TAllocator *) allocator;
};

template <typename TData, typename TAllocator = ArenaAllocator>
class SListCounted : public SList<TData, TAllocator, RealCount>
{
public:
    explicit SListCounted(TAllocator * allocator)
        : SList<TData, TAllocator, RealCount>(allocator)
    {TRACE_IT(22111);}
};


#define _FOREACH_LIST_ENTRY_EX(List, T, Iterator, iter, data, list) \
    auto iter = (list)->Get##Iterator(); \
    while (iter.Next()) \
    {TRACE_IT(22112); \
        T& data = iter.Data();

#define _NEXT_LIST_ENTRY_EX  \
    }

#define _FOREACH_LIST_ENTRY(List, T, data, list) { _FOREACH_LIST_ENTRY_EX(List, T, Iterator, __iter, data, list)
#define _NEXT_LIST_ENTRY _NEXT_LIST_ENTRY_EX }

#define FOREACH_SLISTBASE_ENTRY(T, data, list) _FOREACH_LIST_ENTRY(SListBase, T, data, list)
#define NEXT_SLISTBASE_ENTRY _NEXT_LIST_ENTRY

#define FOREACH_SLISTBASE_ENTRY_EDITING(T, data, list, iter) _FOREACH_LIST_ENTRY_EX(SListBase, T, EditingIterator, iter, data, list)
#define NEXT_SLISTBASE_ENTRY_EDITING _NEXT_LIST_ENTRY_EX

#define FOREACH_SLISTBASECOUNTED_ENTRY(T, data, list) _FOREACH_LIST_ENTRY(SListBaseCounted, T, data, list)
#define NEXT_SLISTBASECOUNTED_ENTRY _NEXT_LIST_ENTRY

#define FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(T, data, list, iter) _FOREACH_LIST_ENTRY_EX(SListBaseCounted, T, EditingIterator, iter, data, list)
#define NEXT_SLISTBASECOUNTED_ENTRY_EDITING _NEXT_LIST_ENTRY_EX

#define FOREACH_SLIST_ENTRY(T, data, list) _FOREACH_LIST_ENTRY(SList, T, data, list)
#define NEXT_SLIST_ENTRY _NEXT_LIST_ENTRY

#define FOREACH_SLIST_ENTRY_EDITING(T, data, list, iter) _FOREACH_LIST_ENTRY_EX(SList, T, EditingIterator, iter, data, list)
#define NEXT_SLIST_ENTRY_EDITING _NEXT_LIST_ENTRY_EX

#define FOREACH_SLISTCOUNTED_ENTRY(T, data, list) _FOREACH_LIST_ENTRY(SListCounted, T, data, list)
#define NEXT_SLISTCOUNTED_ENTRY _NEXT_LIST_ENTRY

#define FOREACH_SLISTCOUNTED_ENTRY_EDITING(T, data, list, iter) _FOREACH_LIST_ENTRY_EX(SListCounted, T, EditingIterator, iter, data, list)
#define NEXT_SLISTCOUNTED_ENTRY_EDITING _NEXT_LIST_ENTRY_EX
