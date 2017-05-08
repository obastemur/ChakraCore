//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

typedef  BVUnit64 SparseBVUnit;

#define FOREACH_BITSET_IN_SPARSEBV(index, bv) \
{TRACE_IT(22225); \
    BVIndex index; \
    for(auto * _curNode = (bv)->head; _curNode != 0 ; _curNode = _curNode->next) \
    {TRACE_IT(22226); \
        BVIndex _offset; \
        BVIndex _startIndex = _curNode->startIndex; \
        SparseBVUnit  _unit = _curNode->data; \
        for(_offset = _unit.GetNextBit(); _offset != -1; _offset = _unit.GetNextBit()) \
        {TRACE_IT(22227); \
            index = _startIndex + _offset; \
            _unit.Clear(_offset); \
        \

#define BREAK_BITSET_IN_SPARSEBV \
            _curNode = 0; \
            break;

#define NEXT_BITSET_IN_SPARSEBV \
        } \
        if(_curNode == 0) \
        {TRACE_IT(22228); \
            break; \
        } \
    } \
}

#define FOREACH_BITSET_IN_SPARSEBV_EDITING(index, bv) \
{TRACE_IT(22229); \
    BVIndex index;  \
    BVSparseNode * _curNodeEdit = (bv)->head; \
    while (_curNodeEdit != nullptr) \
    {TRACE_IT(22230); \
        BVSparseNode * _next = _curNodeEdit->next; \
        BVIndex _offset; \
        BVIndex _startIndex = _curNodeEdit->startIndex; \
        SparseBVUnit  _unit = _curNodeEdit->data; \
        for(_offset = _unit.GetNextBit(); _offset != -1; _offset = _unit.GetNextBit()) \
        {TRACE_IT(22231); \
            index = _startIndex + _offset; \
            _unit.Clear(_offset); \
        \

#define NEXT_BITSET_IN_SPARSEBV_EDITING           \
        } \
        _curNodeEdit = _next; \
    } \
}

#define SPARSEBV_CLEAR_CURRENT_BIT() _curNodeEdit->data.Clear(_offset)

template <class TAllocator>
struct BVSparseNode
{
    Field(BVSparseNode*, TAllocator)    next;
    Field(BVIndex)                      startIndex;
    Field(SparseBVUnit)                 data;

    BVSparseNode(BVIndex beginIndex, BVSparseNode * nextNode);

    void init(BVIndex beginIndex, BVSparseNode * nextNode);

    // Needed for the NatVis Extension for visualizing BitVectors
    // in Visual Studio
#ifdef _WIN32
    bool ToString(
        __out_ecount(strSize) char *const str,
        const size_t strSize,
        size_t *const writtenLengthRef = nullptr,
        const bool isInSequence = false,
        const bool isFirstInSequence = false,
        const bool isLastInSequence = false) const;
#endif
};

template <class TAllocator>
class BVSparse
{
    typedef BVSparseNode<TAllocator> BVSparseNode;

// Data
public:
    Field(BVSparseNode*, TAllocator)    head;

private:
    FieldNoBarrier(TAllocator*)         alloc;
    Field(Field(BVSparseNode*, TAllocator)*, TAllocator) lastUsedNodePrevNextField;

    static const SparseBVUnit s_EmptyUnit;

// Constructor
public:
    BVSparse(TAllocator* allocator);
    ~BVSparse();

// Implementation
protected:
    template <class TOtherAllocator>
    static  void    AssertBV(const BVSparse<TOtherAllocator> * bv);

    SparseBVUnit *  BitsFromIndex(BVIndex i, bool create = true);
    const SparseBVUnit * BitsFromIndex(BVIndex i) const;
    BVSparseNode*   NodeFromIndex(BVIndex i, Field(BVSparseNode*, TAllocator)** prevNextFieldOut,
                                  bool create = true);
    const BVSparseNode* NodeFromIndex(BVIndex i, Field(BVSparseNode*, TAllocator) const** prevNextFieldOut) const;
    BVSparseNode *  DeleteNode(BVSparseNode *node, bool bResetLastUsed = true);
    void            QueueInFreeList(BVSparseNode* node);
    BVSparseNode *  Allocate(const BVIndex searchIndex, BVSparseNode *prevNode);

    template<void (SparseBVUnit::*callback)(SparseBVUnit)>
    void for_each(const BVSparse<TAllocator> *bv2);

    template<void (SparseBVUnit::*callback)(SparseBVUnit)>
    void for_each(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);

// Methods
public:
    BOOLEAN         operator[](BVIndex i) const;
    BOOLEAN         Test(BVIndex i) const;
    BVIndex         GetNextBit(BVIndex i) const;
    BVIndex         GetNextBit(BVSparseNode * node) const;

    BOOLEAN         TestEmpty() const;
    BOOLEAN         TestAndSet(BVIndex i);
    BOOLEAN         TestAndClear(BVIndex i);
    void            Set(BVIndex i);
    void            Clear(BVIndex i);
    void            Compliment(BVIndex i);


    // this |= bv;
    void            Or(const BVSparse<TAllocator> *bv);
    // this = bv1 | bv2;
    void            Or(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);
    // newBv = this | bv;
    BVSparse<TAllocator> *      OrNew(const BVSparse<TAllocator> *bv, TAllocator* allocator) const;
    BVSparse<TAllocator> *      OrNew(const BVSparse<TAllocator> *bv) const {TRACE_IT(22232); return this->OrNew(bv, this->alloc); }

    // this &= bv;
    void            And(const BVSparse<TAllocator> *bv);
    // this = bv1 & bv2;
    void            And(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);
    // newBv = this & bv;
    BVSparse<TAllocator> *      AndNew(const BVSparse<TAllocator> *bv, TAllocator* allocator) const;
    BVSparse<TAllocator> *      AndNew(const BVSparse<TAllocator> *bv) const {TRACE_IT(22233); return this->AndNew(bv, this->alloc); }

    // this ^= bv;
    void            Xor(const BVSparse<TAllocator> *bv);
    // this = bv1 ^ bv2;
    void            Xor(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);
    // newBv = this ^ bv;
    BVSparse<TAllocator> *      XorNew(const BVSparse<TAllocator> *bv, TAllocator* allocator) const;
    BVSparse<TAllocator> *      XorNew(const BVSparse<TAllocator> *bv) const {TRACE_IT(22234); return this->XorNew(bv, this->alloc); }

    // this -= bv;
    void            Minus(const BVSparse<TAllocator> *bv);
    // this = bv1 - bv2;
    void            Minus(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);
    // newBv = this - bv;
    BVSparse<TAllocator> *      MinusNew(const BVSparse<TAllocator> *bv, TAllocator* allocator) const;
    BVSparse<TAllocator> *      MinusNew(const BVSparse<TAllocator> *bv) const {TRACE_IT(22235); return this->MinusNew(bv, this->alloc); }

    template <class TSrcAllocator>
    void            Copy(const BVSparse<TSrcAllocator> *bv);
    template <class TSrcAllocator>
    void            CopyFromNode(const ::BVSparseNode<TSrcAllocator> * node2);
    BVSparse<TAllocator> *      CopyNew(TAllocator* allocator) const;
    BVSparse<TAllocator> *      CopyNew() const;
    void            ComplimentAll();
    void            ClearAll();

    BVIndex         Count() const;
    bool            IsEmpty() const;
    bool            Equal(BVSparse<TAllocator> const * bv) const;

    // this & bv != empty
    bool            Test(BVSparse const * bv) const;

    // Needed for the VS NatVis Extension
#ifdef _WIN32
    void            ToString(__out_ecount(strSize) char *const str, const size_t strSize) const;
    template<class F> void ToString(__out_ecount(strSize) char *const str, const size_t strSize, const F ReadNode) const;
#endif

    TAllocator *    GetAllocator() const {TRACE_IT(22236); return alloc; }
#if DBG_DUMP
    void            Dump() const;
#endif
};


template <class TAllocator>
BVSparseNode<TAllocator>::BVSparseNode(BVIndex beginIndex, BVSparseNode<TAllocator> * nextNode) :
    startIndex(beginIndex),
    data(0),
    next(nextNode)
{TRACE_IT(22237);
    // Performance assert, BVSparseNode is heavily used in the backend, do perf
    // measurement before changing this.
#if defined(_M_ARM64) || defined(_M_X64)
    CompileAssert(sizeof(BVSparseNode) == 24);
#else
    CompileAssert(sizeof(BVSparseNode) == 16);
#endif
}

template <class TAllocator>
void BVSparseNode<TAllocator>::init(BVIndex beginIndex, BVSparseNode<TAllocator> * nextNode)
{TRACE_IT(22238);
    this->startIndex = beginIndex;
    this->data = 0;
    this->next = nextNode;
}

#ifdef _WIN32
template <class TAllocator>
bool BVSparseNode<TAllocator>::ToString(
    __out_ecount(strSize) char *const str,
    const size_t strSize,
    size_t *const writtenLengthRef,
    const bool isInSequence,
    const bool isFirstInSequence,
    const bool isLastInSequence) const
{TRACE_IT(22239);
    Assert(str);
    Assert(!isFirstInSequence || isInSequence);
    Assert(!isLastInSequence || isInSequence);

    if (strSize == 0)
    {TRACE_IT(22240);
        if (writtenLengthRef)
        {TRACE_IT(22241);
            *writtenLengthRef = 0;
        }
        return false;
    }
    str[0] = '\0';

    const size_t reservedLength = _countof(", ...}");
    if (strSize <= reservedLength)
    {TRACE_IT(22242);
        if (writtenLengthRef)
        {TRACE_IT(22243);
            *writtenLengthRef = 0;
        }
        return false;
    }

    size_t length = 0;
    if (!isInSequence || isFirstInSequence)
    {TRACE_IT(22244);
        str[length++] = '{';
    }

    bool insertComma = isInSequence && !isFirstInSequence;
    char tempStr[13];
    for (BVIndex i = data.GetNextBit(); i != BVInvalidIndex; i = data.GetNextBit(i + 1))
    {TRACE_IT(22245);
        const size_t copyLength = sprintf_s(tempStr, insertComma ? ", %u" : "%u", startIndex + i);
        Assert(static_cast<int>(copyLength) > 0);

        Assert(strSize > length);
        Assert(strSize - length > reservedLength);
        if (strSize - length - reservedLength <= copyLength)
        {TRACE_IT(22246);
            strcpy_s(&str[length], strSize - length, insertComma ? ", ...}" : "...}");
            if (writtenLengthRef)
            {TRACE_IT(22247);
                *writtenLengthRef = length + (insertComma ? _countof(", ...}") : _countof("...}"));
            }
            return false;
        }

        strcpy_s(&str[length], strSize - length - reservedLength, tempStr);
        length += copyLength;
        insertComma = true;
    }
    if (!isInSequence || isLastInSequence)
    {TRACE_IT(22248);
        Assert(_countof("}") < strSize - length);
        strcpy_s(&str[length], strSize - length, "}");
        length += _countof("}");
    }
    if (writtenLengthRef)
    {TRACE_IT(22249);
        *writtenLengthRef = length;
    }
    return true;
}
#endif


#if DBG_DUMP
template <typename T> void Dump(T const& t);

namespace Memory{ class JitArenaAllocator; }
template<>
inline void Dump(BVSparse<JitArenaAllocator> * const& bv)
{TRACE_IT(22250);
    bv->Dump();
}

namespace Memory { class Recycler; }
template<>
inline void Dump(BVSparse<Recycler> * const& bv)
{TRACE_IT(22251);
    bv->Dump();
}
#endif

template <class TAllocator>
const SparseBVUnit BVSparse<TAllocator>::s_EmptyUnit(0);

template <class TAllocator>
BVSparse<TAllocator>::BVSparse(TAllocator* allocator) :
   alloc(allocator),
   head(nullptr)
{TRACE_IT(22252);
    this->lastUsedNodePrevNextField = &this->head;
}

template <class TAllocator>
void
BVSparse<TAllocator>::QueueInFreeList(BVSparseNode *curNode)
{
    AllocatorDelete(TAllocator, this->alloc, curNode);
}

template <class TAllocator>
BVSparseNode<TAllocator> *
BVSparse<TAllocator>::Allocate(const BVIndex searchIndex, BVSparseNode *nextNode)
{TRACE_IT(22253);
    return AllocatorNew(TAllocator, this->alloc, BVSparseNode, searchIndex, nextNode);
}

template <class TAllocator>
BVSparse<TAllocator>::~BVSparse()
{TRACE_IT(22254);
    BVSparseNode * curNode = this->head;
    while (curNode != nullptr)
    {TRACE_IT(22255);
        curNode = this->DeleteNode(curNode);
    }
}


// Searches for a node which would contain the required bit. If not found, then it inserts
// a new node in the appropriate position.
//
template <class TAllocator>
BVSparseNode<TAllocator> *
BVSparse<TAllocator>::NodeFromIndex(BVIndex i, Field(BVSparseNode*, TAllocator)** prevNextFieldOut, bool create)
{TRACE_IT(22256);
    const BVIndex searchIndex = SparseBVUnit::Floor(i);

    Field(BVSparseNode*, TAllocator)* prevNextField = this->lastUsedNodePrevNextField;
    BVSparseNode* curNode = *prevNextField;
    if (curNode != nullptr)
    {TRACE_IT(22257);
        if (curNode->startIndex == searchIndex)
        {TRACE_IT(22258);
            *prevNextFieldOut = prevNextField;
            return curNode;
        }

        if (curNode->startIndex > searchIndex)
        {TRACE_IT(22259);
            prevNextField = &this->head;
            curNode = this->head;
        }
    }
    else
    {TRACE_IT(22260);
        prevNextField = &this->head;
        curNode = this->head;
    }

    for (; curNode && searchIndex > curNode->startIndex; curNode = curNode->next)
    {TRACE_IT(22261);
        prevNextField = &curNode->next;
    }

    if(curNode && searchIndex == curNode->startIndex)
    {TRACE_IT(22262);
        *prevNextFieldOut = prevNextField;
        this->lastUsedNodePrevNextField = prevNextField;
        return curNode;
    }

    if(!create)
    {TRACE_IT(22263);
        return nullptr;
    }

    BVSparseNode * newNode = Allocate(searchIndex, *prevNextField);
    *prevNextField = newNode;
    *prevNextFieldOut = prevNextField;
    this->lastUsedNodePrevNextField = prevNextField;
    return newNode;
}

template <class TAllocator>
const BVSparseNode<TAllocator> *
BVSparse<TAllocator>::NodeFromIndex(BVIndex i, Field(BVSparseNode*, TAllocator) const** prevNextFieldOut) const
{TRACE_IT(22264);
    const BVIndex searchIndex = SparseBVUnit::Floor(i);

    Field(BVSparseNode*, TAllocator) const* prevNextField = &this->head;
    const BVSparseNode * curNode = *prevNextField;
    if (curNode != nullptr)
    {TRACE_IT(22265);
        if (curNode->startIndex == searchIndex)
        {TRACE_IT(22266);
            *prevNextFieldOut = prevNextField;
            return curNode;
        }

        if (curNode->startIndex > searchIndex)
        {TRACE_IT(22267);
            prevNextField = &this->head;
            curNode = this->head;
        }
    }
    else
    {TRACE_IT(22268);
        prevNextField = &this->head;
        curNode = this->head;
    }

    for (; curNode && searchIndex > curNode->startIndex; curNode = curNode->next)
    {TRACE_IT(22269);
        prevNextField = &curNode->next;
    }

    if (curNode && searchIndex == curNode->startIndex)
    {TRACE_IT(22270);
        *prevNextFieldOut = prevNextField;
        return curNode;
    }

    return nullptr;
}


template <class TAllocator>
SparseBVUnit *
BVSparse<TAllocator>::BitsFromIndex(BVIndex i, bool create)
{
    Field(BVSparseNode*, TAllocator)* prevNextField;
    BVSparseNode * node = NodeFromIndex(i, &prevNextField, create);
    if (node)
    {TRACE_IT(22271);
        return &node->data;
    }
    else
    {TRACE_IT(22272);
        return (SparseBVUnit *)&BVSparse::s_EmptyUnit;
    }
}

template <class TAllocator>
const SparseBVUnit *
BVSparse<TAllocator>::BitsFromIndex(BVIndex i) const
{
    Field(BVSparseNode*, TAllocator) const* prevNextField;
    const BVSparseNode * node = NodeFromIndex(i, &prevNextField);
    if (node)
    {TRACE_IT(22273);
        return &node->data;
    }
    else
    {TRACE_IT(22274);
        return (SparseBVUnit *)&BVSparse::s_EmptyUnit;
    }
}

template <class TAllocator>
BVSparseNode<TAllocator> *
BVSparse<TAllocator>::DeleteNode(BVSparseNode *node, bool bResetLastUsed)
{TRACE_IT(22275);
    BVSparseNode *next = node->next;
    QueueInFreeList(node);

    if (bResetLastUsed)
    {TRACE_IT(22276);
        this->lastUsedNodePrevNextField = &this->head;
    }
    else
    {TRACE_IT(22277);
        Assert(this->lastUsedNodePrevNextField != &node->next);
    }
    return next;
}

template <class TAllocator>
BVIndex
BVSparse<TAllocator>::GetNextBit(BVSparseNode *node) const
{TRACE_IT(22278);
    while(0 != node)
    {TRACE_IT(22279);
        BVIndex ret = node->data.GetNextBit();
        if(-1 != ret)
        {TRACE_IT(22280);
            return ret + node->startIndex;
        }
    }
    return -1;
}

template <class TAllocator>
BVIndex
BVSparse<TAllocator>::GetNextBit(BVIndex i) const
{TRACE_IT(22281);
    const BVIndex startIndex = SparseBVUnit::Floor(i);

    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {TRACE_IT(22282);
        if(startIndex == node->startIndex)
        {TRACE_IT(22283);
            BVIndex ret = node->data.GetNextBit(SparseBVUnit::Offset(i));
            if(-1 != ret)
            {TRACE_IT(22284);
                return ret + node->startIndex;
            }
            else
            {TRACE_IT(22285);
                return GetNextBit(node->next);
            }
        }
        else if(startIndex < node->startIndex)
        {TRACE_IT(22286);
            return GetNextBit(node->next);
        }
    }

    return  -1;
}

template <class TAllocator>
template <class TOtherAllocator>
void
BVSparse<TAllocator>::AssertBV(const BVSparse<TOtherAllocator> *bv)
{TRACE_IT(22287);
    AssertMsg(nullptr != bv, "Cannot operate on NULL bitvector");
}

template <class TAllocator>
void
BVSparse<TAllocator>::ClearAll()
{TRACE_IT(22288);
    BVSparseNode* nextNode;
    for(BVSparseNode * node = this->head; node != 0 ; node = nextNode)
    {TRACE_IT(22289);
        nextNode = node->next;
        QueueInFreeList(node);
    }
    this->head = nullptr;
    this->lastUsedNodePrevNextField = &this->head;
}

template <class TAllocator>
void
BVSparse<TAllocator>::Set(BVIndex i)
{TRACE_IT(22290);
    this->BitsFromIndex(i)->Set(SparseBVUnit::Offset(i));
}

template <class TAllocator>
void
BVSparse<TAllocator>::Clear(BVIndex i)
{
    Field(BVSparseNode*, TAllocator)* prevNextField;
    BVSparseNode * current = this->NodeFromIndex(i, &prevNextField, false /* create */);
    if(current)
    {TRACE_IT(22291);
        current->data.Clear(SparseBVUnit::Offset(i));
        if (current->data.IsEmpty())
        {TRACE_IT(22292);
            *prevNextField = this->DeleteNode(current, false);
        }
    }
}

template <class TAllocator>
void
BVSparse<TAllocator>::Compliment(BVIndex i)
{TRACE_IT(22293);
    this->BitsFromIndex(i)->Complement(SparseBVUnit::Offset(i));
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::TestEmpty() const
{TRACE_IT(22294);
    return this->head != nullptr;
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::Test(BVIndex i) const
{TRACE_IT(22295);
    return this->BitsFromIndex(i)->Test(SparseBVUnit::Offset(i));
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::TestAndSet(BVIndex i)
{TRACE_IT(22296);
    SparseBVUnit * bvUnit = this->BitsFromIndex(i);
    BVIndex bvIndex = SparseBVUnit::Offset(i);
    BOOLEAN bit = bvUnit->Test(bvIndex);
    bvUnit->Set(bvIndex);
    return bit;
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::TestAndClear(BVIndex i)
{
    Field(BVSparseNode*, TAllocator)* prevNextField;
    BVSparseNode * current = this->NodeFromIndex(i, &prevNextField, false /* create */);
    if (current == nullptr)
    {TRACE_IT(22297);
        return false;
    }
    BVIndex bvIndex = SparseBVUnit::Offset(i);
    BOOLEAN bit = current->data.Test(bvIndex);
    current->data.Clear(bvIndex);
    if (current->data.IsEmpty())
    {TRACE_IT(22298);
        *prevNextField = this->DeleteNode(current, false);
    }
    return bit;
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::operator[](BVIndex i) const
{TRACE_IT(22299);
    return this->Test(i);
}

template<class TAllocator>
template<void (SparseBVUnit::*callback)(SparseBVUnit)>
void BVSparse<TAllocator>::for_each(const BVSparse *bv2)
{TRACE_IT(22300);
    Assert(callback == &SparseBVUnit::And || callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor || callback == &SparseBVUnit::Minus);
    AssertBV(bv2);

          BVSparseNode * node1      = this->head;
    const BVSparseNode * node2      = bv2->head;
          Field(BVSparseNode*, TAllocator)* prevNodeNextField = &this->head;

    while(node1 != nullptr && node2 != nullptr)
    {TRACE_IT(22301);
        if(node2->startIndex == node1->startIndex)
        {TRACE_IT(22302);
            (node1->data.*callback)(node2->data);
            prevNodeNextField = &node1->next;
            node1 = node1->next;
            node2 = node2->next;
        }
        else if(node2->startIndex > node1->startIndex)
        {TRACE_IT(22303);

            if (callback == &SparseBVUnit::And)
            {TRACE_IT(22304);
                node1 = this->DeleteNode(node1);
                *prevNodeNextField = node1;
            }
            else
            {TRACE_IT(22305);
                prevNodeNextField = &node1->next;
                node1 = node1->next;
            }

        }
        else
        {TRACE_IT(22306);
            if (callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor)
            {TRACE_IT(22307);
                BVSparseNode * newNode = Allocate(node2->startIndex, node1);
                (newNode->data.*callback)(node2->data);
                *prevNodeNextField = newNode;
                prevNodeNextField = &newNode->next;
            }
            node2 = node2->next;
        }
    }

    if (callback == &SparseBVUnit::And)
    {TRACE_IT(22308);
        while (node1 != nullptr)
        {TRACE_IT(22309);
            node1 = this->DeleteNode(node1);
        }
        *prevNodeNextField = nullptr;
    }
    else if (callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor)
    {TRACE_IT(22310);
        while(node2 != 0)
        {TRACE_IT(22311);
            Assert(*prevNodeNextField == nullptr);
            BVSparseNode * newNode = Allocate(node2->startIndex, nullptr);
            *prevNodeNextField = newNode;

            (newNode->data.*callback)(node2->data);
            node2       = node2->next;
            prevNodeNextField    = &newNode->next;
        }
    }
}

template<class TAllocator>
template<void (SparseBVUnit::*callback)(SparseBVUnit)>
void BVSparse<TAllocator>::for_each(const BVSparse *bv1, const BVSparse *bv2)
{TRACE_IT(22312);
    Assert(callback == &SparseBVUnit::And || callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor || callback == &SparseBVUnit::Minus);
    Assert(this->IsEmpty());
    AssertBV(bv1);
    AssertBV(bv2);

          BVSparseNode * node1      = bv1->head;
    const BVSparseNode * node2      = bv2->head;
          BVSparseNode * lastNode   = nullptr;
          Field(BVSparseNode*, TAllocator)* prevNextField = &this->head;

    while(node1 != nullptr && node2 != nullptr)
    {TRACE_IT(22313);
        lastNode = node1;
        BVIndex startIndex;
        SparseBVUnit  bvUnit1;
        SparseBVUnit  bvUnit2;

        if (node2->startIndex == node1->startIndex)
        {TRACE_IT(22314);
            startIndex = node1->startIndex;
            bvUnit1 = node1->data;
            bvUnit2 = node2->data;
            node1 = node1->next;
            node2 = node2->next;
        }
        else if (node2->startIndex > node1->startIndex)
        {TRACE_IT(22315);
            startIndex = node1->startIndex;
            bvUnit1 = node1->data;
            node1 = node1->next;
        }
        else
        {TRACE_IT(22316);
            startIndex = node2->startIndex;
            bvUnit2 = node2->data;
            node2 = node2->next;
        }

        (bvUnit1.*callback)(bvUnit2);
        if (!bvUnit1.IsEmpty())
        {TRACE_IT(22317);
            BVSparseNode * newNode = Allocate(startIndex, nullptr);
            newNode->data = bvUnit1;
            *prevNextField = newNode;
            prevNextField = &newNode->next;
        }
    }


    if (callback == &SparseBVUnit::Minus || callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor)
    {TRACE_IT(22318);
        BVSparseNode const * copyNode = (callback == &SparseBVUnit::Minus || node1 != nullptr)? node1 : node2;

        while (copyNode != nullptr)
        {TRACE_IT(22319);
            if (!copyNode->data.IsEmpty())
            {TRACE_IT(22320);
                BVSparseNode * newNode = Allocate(copyNode->startIndex, nullptr);
                newNode->data = copyNode->data;
                *prevNextField = newNode;
                prevNextField = &newNode->next;
            }
            copyNode = copyNode->next;
        }
    }
}

template <class TAllocator>
void
BVSparse<TAllocator>::Or(const BVSparse*bv)
{TRACE_IT(22321);
    this->for_each<&SparseBVUnit::Or>(bv);
}

template <class TAllocator>
void
BVSparse<TAllocator>::Or(const BVSparse * bv1, const BVSparse * bv2)
{TRACE_IT(22322);
    this->ClearAll();
    this->for_each<&SparseBVUnit::Or>(bv1, bv2);
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::OrNew(const BVSparse* bv,  TAllocator* allocator) const
{TRACE_IT(22323);
    BVSparse * newBv = AllocatorNew(TAllocator, allocator, BVSparse, allocator);
    newBv->for_each<&SparseBVUnit::Or>(this, bv);
    return newBv;
}

template <class TAllocator>
void
BVSparse<TAllocator>::And(const BVSparse*bv)
{TRACE_IT(22324);
    this->for_each<&SparseBVUnit::And>(bv);
}

template <class TAllocator>
void
BVSparse<TAllocator>::And(const BVSparse * bv1, const BVSparse * bv2)
{TRACE_IT(22325);
    this->ClearAll();
    this->for_each<&SparseBVUnit::And>(bv1, bv2);
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::AndNew(const BVSparse* bv, TAllocator* allocator) const
{TRACE_IT(22326);
    BVSparse * newBv = AllocatorNew(TAllocator, allocator, BVSparse, allocator);
    newBv->for_each<&SparseBVUnit::And>(this, bv);
    return newBv;
}

template <class TAllocator>
void
BVSparse<TAllocator>::Xor(const BVSparse*bv)
{TRACE_IT(22327);
    this->for_each<&SparseBVUnit::Xor>(bv);
}

template <class TAllocator>
void
BVSparse<TAllocator>::Xor(const BVSparse * bv1, const BVSparse * bv2)
{TRACE_IT(22328);
    this->ClearAll();
    this->for_each<&SparseBVUnit::Xor>(bv1, bv2);
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::XorNew(const BVSparse* bv, TAllocator* allocator) const
{TRACE_IT(22329);
    BVSparse * newBv = AllocatorNew(TAllocator, allocator, BVSparse, allocator);
    newBv->for_each<&SparseBVUnit::Xor>(this, bv);
    return newBv;
}

template <class TAllocator>
void
BVSparse<TAllocator>::Minus(const BVSparse*bv)
{TRACE_IT(22330);
    this->for_each<&SparseBVUnit::Minus>(bv);
}

template <class TAllocator>
void
BVSparse<TAllocator>::Minus(const BVSparse * bv1, const BVSparse * bv2)
{TRACE_IT(22331);
    this->ClearAll();
    this->for_each<&SparseBVUnit::Minus>(bv1, bv2);
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::MinusNew(const BVSparse* bv, TAllocator* allocator) const
{TRACE_IT(22332);
    BVSparse * newBv = AllocatorNew(TAllocator, allocator, BVSparse, allocator);
    newBv->for_each<&SparseBVUnit::Minus>(this, bv);
    return newBv;
}

template <class TAllocator>
template <class TSrcAllocator>
void
BVSparse<TAllocator>::Copy(const BVSparse<TSrcAllocator> * bv2)
{TRACE_IT(22333);
    AssertBV(bv2);
    CopyFromNode(bv2->head);
}

template <class TAllocator>
template <class TSrcAllocator>
void
BVSparse<TAllocator>::CopyFromNode(const ::BVSparseNode<TSrcAllocator> * node2)
{TRACE_IT(22334);
    BVSparseNode * node1 = this->head;
    Field(BVSparseNode*, TAllocator)* prevNextField = &this->head;

    while (node1 != nullptr && node2 != nullptr)
    {TRACE_IT(22335);
        if (!node2->data.IsEmpty())
        {TRACE_IT(22336);
            node1->startIndex = node2->startIndex;
            node1->data.Copy(node2->data);
            prevNextField = &node1->next;
            node1 = node1->next;
        }

        node2 = node2->next;
    }

    if (node1 != nullptr)
    {TRACE_IT(22337);
        while (node1 != nullptr)
        {TRACE_IT(22338);
            node1 = this->DeleteNode(node1);
        }
        *prevNextField = nullptr;
    }
    else
    {TRACE_IT(22339);
        while (node2 != nullptr)
        {TRACE_IT(22340);
            if (!node2->data.IsEmpty())
            {TRACE_IT(22341);
                BVSparseNode * newNode = Allocate(node2->startIndex, nullptr);
                newNode->data.Copy(node2->data);
                *prevNextField = newNode;
                prevNextField = &newNode->next;
            }
            node2 = node2->next;
        }
    }
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::CopyNew(TAllocator* allocator) const
{TRACE_IT(22342);
    BVSparse * bv = AllocatorNew(TAllocator, allocator, BVSparse<TAllocator>, allocator);
    bv->Copy(this);
    return bv;
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::CopyNew() const
{TRACE_IT(22343);
    return this->CopyNew(this->alloc);
}

template <class TAllocator>
void
BVSparse<TAllocator>::ComplimentAll()
{TRACE_IT(22344);
    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {TRACE_IT(22345);
        node->data.ComplimentAll();
    }
}

template <class TAllocator>
BVIndex
BVSparse<TAllocator>::Count() const
{TRACE_IT(22346);
    BVIndex sum = 0;
    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {TRACE_IT(22347);
        sum += node->data.Count();
    }
    return sum;
}

template <class TAllocator>
bool
BVSparse<TAllocator>::IsEmpty() const
{TRACE_IT(22348);
    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {TRACE_IT(22349);
        if (!node->data.IsEmpty())
        {TRACE_IT(22350);
            return false;
        }
    }
    return true;
}

template <class TAllocator>
bool
BVSparse<TAllocator>::Equal(BVSparse const * bv) const
{TRACE_IT(22351);
    BVSparseNode const * bvNode1 = this->head;
    BVSparseNode const * bvNode2 = bv->head;

    while (true)
    {TRACE_IT(22352);
        while (bvNode1 != nullptr && bvNode1->data.IsEmpty())
        {TRACE_IT(22353);
            bvNode1 = bvNode1->next;
        }
        while (bvNode2 != nullptr && bvNode2->data.IsEmpty())
        {TRACE_IT(22354);
            bvNode2 = bvNode2->next;
        }
        if (bvNode1 == nullptr)
        {TRACE_IT(22355);
            return (bvNode2 == nullptr);
        }
        if (bvNode2 == nullptr)
        {TRACE_IT(22356);
            return false;
        }
        if (bvNode1->startIndex != bvNode2->startIndex)
        {TRACE_IT(22357);
            return false;
        }
        if (!bvNode1->data.Equal(bvNode2->data))
        {TRACE_IT(22358);
            return false;
        }
        bvNode1 = bvNode1->next;
        bvNode2 = bvNode2->next;
    }
}

template <class TAllocator>
bool
BVSparse<TAllocator>::Test(BVSparse const * bv) const
{TRACE_IT(22359);
    BVSparseNode const * bvNode1 = this->head;
    BVSparseNode const * bvNode2 = bv->head;

    while (bvNode1 != nullptr && bvNode2 != nullptr)
    {TRACE_IT(22360);
        if (bvNode1->data.IsEmpty() || bvNode1->startIndex < bvNode2->startIndex)
        {TRACE_IT(22361);
            bvNode1 = bvNode1->next;
            continue;
        }
        if (bvNode2->data.IsEmpty() || bvNode1->startIndex > bvNode2->startIndex)
        {TRACE_IT(22362);
            bvNode2 = bvNode2->next;
            continue;
        }
        Assert(bvNode1->startIndex == bvNode2->startIndex);
        if (bvNode1->data.Test(bvNode2->data))
        {TRACE_IT(22363);
            return true;
        }
        bvNode1 = bvNode1->next;
        bvNode2 = bvNode2->next;
    }

    return false;
}

#ifdef _WIN32

template<class TAllocator>
template<class F>
void BVSparse<TAllocator>::ToString(__out_ecount(strSize) char *const str, const size_t strSize, const F ReadNode) const
{TRACE_IT(22364);
    Assert(str);

    if (strSize == 0)
    {TRACE_IT(22365);
        return;
    }
    str[0] = '\0';

    bool empty = true;
    bool isFirstInSequence = true;
    size_t length = 0;
    BVSparseNode *nodePtr = head;
    while (nodePtr)
    {TRACE_IT(22366);
        bool readSuccess;
        const BVSparseNode node(ReadNode(nodePtr, &readSuccess));
        if (!readSuccess)
        {TRACE_IT(22367);
            str[0] = '\0';
            return;
        }
        if (node.data.IsEmpty())
        {TRACE_IT(22368);
            nodePtr = node.next;
            continue;
        }
        empty = false;

        size_t writtenLength;
        if (!node.ToString(&str[length], strSize - length, &writtenLength, true, isFirstInSequence, !node.next))
        {TRACE_IT(22369);
            return;
        }
        length += writtenLength;

        isFirstInSequence = false;
        nodePtr = node.next;
    }

    if (empty && _countof("{}") < strSize)
    {
        strcpy_s(str, strSize, "{}");
    }
}

template<class TAllocator>
void BVSparse<TAllocator>::ToString(__out_ecount(strSize) char *const str, const size_t strSize) const
{
    ToString(
        str,
        strSize,
        [](BVSparseNode *const nodePtr, bool *const successRef) -> BVSparseNode
    {
        Assert(nodePtr);
        Assert(successRef);

        *successRef = true;
        return *nodePtr;
    });
}
#endif

#if DBG_DUMP

template <class TAllocator>
void
BVSparse<TAllocator>::Dump() const
{TRACE_IT(22370);
    bool hasBits = false;
    Output::Print(_u("[  "));
    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {TRACE_IT(22371);
        hasBits = node->data.Dump(node->startIndex, hasBits);
    }
    Output::Print(_u("]\n"));
}
#endif
