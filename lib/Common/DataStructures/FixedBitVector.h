//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#define FOREACH_BITSET_IN_FIXEDBV(index, bv) \
{TRACE_IT(21367); \
    BVIndex index; \
    for(JsUtil::FBVEnumerator _bvenum = bv->BeginSetBits(); \
        !_bvenum.End(); \
        _bvenum++) \
    {TRACE_IT(21368); \
        index = _bvenum.GetCurrent(); \

#define NEXT_BITSET_IN_FIXEDBV              }}


class BVFixed
{
// Data
protected:
    BVIndex       len;
    BVUnit      data[];

private:
    BVFixed(BVFixed * initBv);
    BVFixed(BVIndex length, bool initialSet = false);
    void ClearEnd();

// Creation Factory
public:

    template <typename TAllocator>
    static  BVFixed *       New(TAllocator* alloc, BVFixed * initBv);

    template <typename TAllocator>
    static  BVFixed *       New(DECLSPEC_GUARD_OVERFLOW BVIndex length, TAllocator* alloc, bool initialSet = false);

    template <typename TAllocator>
    static  BVFixed *       NewNoThrow(DECLSPEC_GUARD_OVERFLOW BVIndex length, TAllocator* alloc, bool initialSet = false);

    template <typename TAllocator>
    void                    Delete(TAllocator * alloc);

    // For preallocated memory
    static size_t           GetAllocSize(BVIndex length);
    void Init(BVIndex length);

// Implementation
protected:
            void            AssertRange(BVIndex i) const;
            void            AssertBV(const BVFixed * bv) const;

    static  BVIndex         WordCount(BVIndex length);

    const   BVUnit *        BitsFromIndex(BVIndex i) const;
            BVUnit *        BitsFromIndex(BVIndex i);
    const   BVUnit *        BeginUnit() const;
            BVUnit *        BeginUnit();
    const   BVUnit *        EndUnit() const;
            BVUnit *        EndUnit();


    template<class Fn>
    inline void for_each(const BVFixed *bv2, const Fn callback)
    {TRACE_IT(21369);
        AssertMsg(this->len == bv2->len, "Fatal: The 2 bitvectors should have had the same length.");

        BVUnit *        i;
        const BVUnit *  j;

        for(i  =  this->BeginUnit(), j = bv2->BeginUnit();
            i !=  this->EndUnit() ;
            i++, j++)
        {TRACE_IT(21370);
            (i->*callback)(*j);
        }
    }

// Methods
public:

    void Set(BVIndex i)
    {TRACE_IT(21371);
        AssertRange(i);
        this->BitsFromIndex(i)->Set(BVUnit::Offset(i));
    }

    void Clear(BVIndex i)
    {TRACE_IT(21372);
        AssertRange(i);
        this->BitsFromIndex(i)->Clear(BVUnit::Offset(i));
    }

    void Compliment(BVIndex i)
    {TRACE_IT(21373);
        AssertRange(i);
        this->BitsFromIndex(i)->Complement(BVUnit::Offset(i));
    }

    BOOLEAN Test(BVIndex i) const
    {TRACE_IT(21374);
        AssertRange(i);
        return this->BitsFromIndex(i)->Test(BVUnit::Offset(i));
    }

    BOOLEAN         operator[](BVIndex i) const;

    BVIndex         GetNextBit(BVIndex i) const;

    BOOLEAN         TestAndSet(BVIndex i);
    BOOLEAN         TestAndClear(BVIndex i);

    void            OrComplimented(const BVFixed * bv);
    void            Or(const BVFixed *bv);
    uint            DiffCount(const BVFixed* bv) const;
    void            And(const BVFixed *bv);
    void            Minus(const BVFixed *bv);
    void            Copy(const BVFixed *bv);
    void            CopyBits(const BVFixed * bv, BVIndex i);
    void            ComplimentAll();
    void            SetAll();
    void            ClearAll();

    BVIndex         Count() const;
    BVIndex         Length() const;
    JsUtil::FBVEnumerator BeginSetBits();

    BVIndex         WordCount() const;
    bool            IsAllClear() const;
    template<typename Container>
    Container GetRange(BVIndex start, BVIndex len) const;
    template<typename Container>
    void SetRange(Container* value, BVIndex start, BVIndex len);

    BVUnit* GetData() const
    {TRACE_IT(21375);
        return (BVUnit*)data;
    }
#if DBG_DUMP
    void            Dump() const;
#endif
};

template <typename TAllocator>
BVFixed * BVFixed::New(TAllocator * alloc, BVFixed * initBv)
{TRACE_IT(21376);
    BVIndex length = initBv->Length();
    BVFixed *result = AllocatorNewPlusLeaf(TAllocator, alloc, sizeof(BVUnit) * BVFixed::WordCount(length), BVFixed, initBv);
    return result;
}

template <typename TAllocator>
BVFixed * BVFixed::New(DECLSPEC_GUARD_OVERFLOW BVIndex length, TAllocator * alloc, bool initialSet)
{TRACE_IT(21377);
    BVFixed *result = AllocatorNewPlusLeaf(TAllocator, alloc, sizeof(BVUnit) * BVFixed::WordCount(length), BVFixed, length, initialSet);
    return result;
}

template <typename TAllocator>
BVFixed * BVFixed::NewNoThrow(DECLSPEC_GUARD_OVERFLOW BVIndex length, TAllocator * alloc, bool initialSet)
{TRACE_IT(21378);
    BVFixed *result = AllocatorNewNoThrowPlus(TAllocator, alloc, sizeof(BVUnit) * BVFixed::WordCount(length), BVFixed, length, initialSet);
    return result;
}

template <typename TAllocator>
void BVFixed::Delete(TAllocator * alloc)
{
    AllocatorDeletePlus(TAllocator, alloc, sizeof(BVUnit) * this->WordCount(), this);
}

template<typename Container>
Container BVFixed::GetRange(BVIndex start, BVIndex len) const
{TRACE_IT(21379);
    AssertRange(start);
    if (len == 0)
    {TRACE_IT(21380);
        return Container(0);
    }
    Assert(len <= sizeof(Container) * MachBits);
    AssertMsg(len <= 64, "Currently doesn't support range bigger than 64 bits");
    BVIndex end = start + len - 1;
    AssertRange(end);
    BVIndex iStart = BVUnit::Position(start);
    BVIndex iEnd = BVUnit::Position(end);
    BVIndex oStart = BVUnit::Offset(start);
    BVIndex oEnd = BVUnit::Offset(end);
    // Simply using uint64 because it is much easier than to juggle with BVUnit::BVUnitTContainer's size
    // Special case, if oEnd == 63, 1 << 64 == 1. Therefore the result is incorrect
    uint64 mask = oEnd < 63 ? (((uint64)1 << (oEnd + 1)) - 1) : 0xFFFFFFFFFFFFFFFF;
    uint64 range;
    // Trivial case
    if (iStart == iEnd)
    {TRACE_IT(21381);
        // remove the bits after oEnd with mask, then remove the bits before start with shift
        range = (mask & this->data[iStart].GetWord()) >> oStart;
    }
    // Still simple enough
    else if (iStart + 1 == iEnd)
    {TRACE_IT(21382);
        auto startWord = this->data[iStart].GetWord();
        auto endWord = this->data[iEnd].GetWord();
        // remove the bits before start with shift
        range = startWord >> oStart;
        // remove the bits after oEnd with mask then position it after start bits
        range |= (mask & endWord) << (BVUnit::BitsPerWord - oStart);
    }
    // Spans over multiple value, need to loop
    else
    {TRACE_IT(21383);
        // Get the first bits and move them to the beginning
        range = this->data[iStart].GetWord() >> oStart;
        // track how many bits have been read so far
        int nBitsUsed = BVUnit::BitsPerWord - oStart;
        for (uint i = iStart + 1; i < iEnd; ++i)
        {TRACE_IT(21384);
            // put all bits from the data in the mid-range. Use the tracked read bits to position them
            range |= ((uint64)(this->data[i].GetWord())) << nBitsUsed;
            nBitsUsed += BVUnit::BitsPerWord;
        }
        // Read the last bits and remove those after oEnd with mask
        range |= (mask & this->data[iEnd].GetWord()) << nBitsUsed;
    }
    return Container(range);
}

template<typename Container>
void BVFixed::SetRange(Container* value, BVIndex start, BVIndex len)
{TRACE_IT(21385);
    AssertRange(start);
    if (len == 0)
    {TRACE_IT(21386);
        return;
    }
    Assert(len <= sizeof(Container) * MachBits);
    BVIndex end = start + len - 1;
    AssertRange(end);
    BVIndex iStart = BVUnit::Position(start);
    BVIndex iEnd = BVUnit::Position(end);
    BVIndex oStart = BVUnit::Offset(start);
    BVIndex oEnd = BVUnit::Offset(end);

    BVUnit::BVUnitTContainer temp;
    BVUnit::BVUnitTContainer* bits;
    static_assert(sizeof(Container) == 1 || sizeof(Container) == sizeof(BVUnit::BVUnitTContainer),
        "Container is not suitable to represent the calculated value");
    if (sizeof(BVUnit::BVUnitTContainer) == 1)
    {TRACE_IT(21387);
        temp = *((BVUnit::BVUnitTContainer*)value);
        bits = &temp;
    }
    else
    {TRACE_IT(21388);
        bits = (BVUnit::BVUnitTContainer*)value;
    }
    const int oStartComplement = BVUnit::BitsPerWord - oStart;
    static_assert((BVUnit::BVUnitTContainer)BVUnit::AllOnesMask > 0, "Container type of BVFixed must be unsigned");
    //When making the mask, check the special case when we need all bits
#define MAKE_MASK(start, end) ( ((end) == BVUnit::BitsPerWord ? BVUnit::AllOnesMask : (((BVUnit::BVUnitTContainer)1 << ((end) - (start))) - 1))   << (start))
    // Or the value to set the bits to 1. And the value to set the bits to 0
    // The mask is used to make sure we don't modify the bits outside the range
#define SET_RANGE(i, value, mask) \
    this->data[i].Or((value) & mask);\
    this->data[i].And((value) | ~mask);

    BVUnit::BVUnitTContainer bitsToSet;
    // Fast Path
    if (iEnd == iStart)
    {TRACE_IT(21389);
        const BVUnit::BVUnitTContainer mask = MAKE_MASK(oStart, oEnd + 1);
        // Shift to position the bits
        bitsToSet = (*bits << oStart);
        SET_RANGE(iStart, bitsToSet, mask);
    }
    // TODO: case iEnd == iStart + 1 to avoid a loop
    else if (oStart == 0)
    {TRACE_IT(21390);
        // Simpler case where we don't have to shift the bits around
        for (uint i = iStart; i < iEnd; ++i)
        {
            SET_RANGE(i, *bits, BVUnit::AllOnesMask);
            ++bits;
        }
        // We still need to use a mask to remove the unused bits
        const BVUnit::BVUnitTContainer mask = MAKE_MASK(0, oEnd + 1);
        SET_RANGE(iEnd, *bits, mask);
    }
    else
    {TRACE_IT(21391);
        // Default case. We need to process everything 1 at a time
        {TRACE_IT(21392);
            // First set the first bits
            const BVUnit::BVUnitTContainer mask = MAKE_MASK(oStart, BVUnit::BitsPerWord);
            SET_RANGE(iStart, *bits << oStart, mask);
        }
        // Set the bits in the middle
        for (uint i = iStart + 1; i < iEnd; ++i)
        {TRACE_IT(21393);
            bitsToSet = *bits >> oStartComplement;
            ++bits;
            bitsToSet |= *bits << oStart;
            SET_RANGE(i, bitsToSet, BVUnit::AllOnesMask);
        }
        // Set the last bits
        bitsToSet = *bits >> oStartComplement;
        ++bits;
        bitsToSet |= *bits << oStart;
        {
            const BVUnit::BVUnitTContainer mask = MAKE_MASK(0, oEnd + 1);
            SET_RANGE(iEnd, bitsToSet, mask);
        }
    }

    if (sizeof(Container) == 1)
    {TRACE_IT(21394);
        // Calculation above might overflow the original container.
        // normalize the overflow value. LE only
        temp = (*((char*)bits)) + (*(((char*)bits) + 1));
        memcpy(value, bits, 1);
    }
#undef MAKE_MASK
#undef SET_RANGE
}

template <size_t bitCount>
class BVStatic
{
public:
    // Made public to allow for compile-time use
    static const size_t wordCount = ((bitCount - 1) >> BVUnit::ShiftValue) + 1;

// Data
private:
    Field(BVUnit) data[wordCount];

public:
    // Break on member changes. We rely on the layout of this class being static so we can
    // use initializer lists to generate collections of BVStatic.
    BVStatic()
    {TRACE_IT(21395);
        Assert(sizeof(BVStatic<bitCount>) == sizeof(data));
        Assert((void*)this == (void*)&this->data);
    }

// Implementation
private:
    void AssertRange(BVIndex i) const { Assert(i < bitCount); }

    const BVUnit * BitsFromIndex(BVIndex i) const {TRACE_IT(21397); AssertRange(i); return &this->data[BVUnit::Position(i)]; }
    BVUnit * BitsFromIndex(BVIndex i) {TRACE_IT(21398); AssertRange(i); return &this->data[BVUnit::Position(i)]; }

    const BVUnit * BeginUnit() const {TRACE_IT(21399); return &this->data[0]; }
    BVUnit * BeginUnit() {TRACE_IT(21400); return &this->data[0]; }

    const BVUnit * EndUnit() const {TRACE_IT(21401); return &this->data[wordCount]; }
    BVUnit * EndUnit() {TRACE_IT(21402); return &this->data[wordCount]; }

    template<class Fn>
    inline void for_each(const BVStatic *bv2, const Fn callback)
    {TRACE_IT(21403);
        BVUnit *        i;
        const BVUnit *  j;

        for(i  =  this->BeginUnit(), j = bv2->BeginUnit();
            i !=  this->EndUnit() ;
            i++, j++)
        {TRACE_IT(21404);
            (i->*callback)(*j);
        }
    }

    template<class Fn>
    static bool MapUntil(const BVStatic *bv1, const BVStatic *bv2, const Fn callback)
    {TRACE_IT(21405);
        const BVUnit *  i;
        const BVUnit *  j;

        for(i  =  bv1->BeginUnit(), j = bv2->BeginUnit();
            i !=  bv1->EndUnit() ;
            i++, j++)
        {
            if (!callback(*i, *j))
            {TRACE_IT(21406);
                return false;
            }
        }
        return true;
    }

    void ClearEnd()
    {TRACE_IT(21407);
        uint offset = BVUnit::Offset(bitCount);
        if (offset != 0)
        {TRACE_IT(21408);
            this->data[wordCount - 1].And((1 << offset) - 1);
        }
    }

// Methods
public:
    void Set(BVIndex i)
    {TRACE_IT(21409);
        AssertRange(i);
        this->BitsFromIndex(i)->Set(BVUnit::Offset(i));
    }

    void Clear(BVIndex i)
    {TRACE_IT(21410);
        AssertRange(i);
        this->BitsFromIndex(i)->Clear(BVUnit::Offset(i));
    }

    void Compliment(BVIndex i)
    {TRACE_IT(21411);
        AssertRange(i);
        this->BitsFromIndex(i)->Complement(BVUnit::Offset(i));
    }

    BOOLEAN Equal(BVStatic<bitCount> const * o)
    {TRACE_IT(21412);
        return MapUntil(this, o, [](BVUnit const& i, BVUnit const &j) { return i.Equal(j); });
    }

    BOOLEAN Test(BVIndex i) const
    {TRACE_IT(21413);
        AssertRange(i);
        return this->BitsFromIndex(i)->Test(BVUnit::Offset(i));
    }

    BOOLEAN TestAndSet(BVIndex i)
    {TRACE_IT(21414);
        AssertRange(i);
        return PlatformAgnostic::_BitTestAndSet((LONG *)this->data, (LONG) i);
    }

    BOOLEAN TestIntrinsic(BVIndex i) const
    {TRACE_IT(21415);
        AssertRange(i);
        return PlatformAgnostic::_BitTest((LONG *)this->data, (LONG) i);
    }

    BOOLEAN TestAndSetInterlocked(BVIndex i)
    {TRACE_IT(21416);
        AssertRange(i);
        return PlatformAgnostic::_InterlockedBitTestAndSet((LONG *)this->data, (LONG) i);
    }

    BOOLEAN TestAndClear(BVIndex i)
    {TRACE_IT(21417);
        AssertRange(i);
        BVUnit * bvUnit = this->BitsFromIndex(i);
        BVIndex offset = BVUnit::Offset(i);
        BOOLEAN bit = bvUnit->Test(offset);
        bvUnit->Clear(offset);
        return bit;
    }

    BOOLEAN TestAndClearInterlocked(BVIndex i)
    {TRACE_IT(21418);
        AssertRange(i);
        return PlatformAgnostic::_InterlockedBitTestAndReset((LONG *)this->data, (LONG)i);
    }

    void OrComplimented(const BVStatic * bv) {TRACE_IT(21419); this->for_each(bv, &BVUnit::OrComplimented); ClearEnd(); }
    void Or(const BVStatic *bv) {TRACE_IT(21420); this->for_each(bv, &BVUnit::Or); }
    void And(const BVStatic *bv) {TRACE_IT(21421); this->for_each(bv, &BVUnit::And); }
    void Minus(const BVStatic *bv) {TRACE_IT(21422); this->for_each(bv, &BVUnit::Minus); }

    void Copy(const BVStatic *bv) {TRACE_IT(21423); js_memcpy_s(&this->data[0], wordCount * sizeof(BVUnit), &bv->data[0], wordCount * sizeof(BVUnit)); }

    void SetAll() {TRACE_IT(21424); memset(&this->data[0], -1, wordCount * sizeof(BVUnit)); ClearEnd(); }
    void ClearAll() {TRACE_IT(21425); memset(&this->data[0], 0, wordCount * sizeof(BVUnit)); }

    void ComplimentAll()
    {TRACE_IT(21426);
        for (BVIndex i = 0; i < wordCount; i++)
        {TRACE_IT(21427);
            this->data[i].ComplimentAll();
        }

        ClearEnd();
    }

    BVIndex Count() const
    {TRACE_IT(21428);
        BVIndex sum = 0;
        for (BVIndex i = 0; i < wordCount; i++)
        {TRACE_IT(21429);
            sum += this->data[i].Count();
        }

        Assert(sum <= bitCount);
        return sum;
    }

    BVIndex Length() const
    {TRACE_IT(21430);
        return bitCount;
    }

    JsUtil::FBVEnumerator   BeginSetBits() {TRACE_IT(21431); return JsUtil::FBVEnumerator(this->BeginUnit(), this->EndUnit()); }

    BVIndex GetNextBit(BVIndex i) const
    {TRACE_IT(21432);
        AssertRange(i);

        const BVUnit * chunk = BitsFromIndex(i);
        BVIndex base = BVUnit::Floor(i);

        BVIndex offset = chunk->GetNextBit(BVUnit::Offset(i));
        if (-1 != offset)
        {TRACE_IT(21433);
            return base + offset;
        }

        while (++chunk != this->EndUnit())
        {TRACE_IT(21434);
            base += BVUnit::BitsPerWord;
            offset = chunk->GetNextBit();
            if (-1 != offset)
            {TRACE_IT(21435);
                return base + offset;
            }
        }

       return BVInvalidIndex;
    }

    const BVUnit * GetRawData() const {TRACE_IT(21436); return data; }

    template <size_t rangeSize>
    BVStatic<rangeSize> * GetRange(BVIndex startOffset)
    {TRACE_IT(21437);
        AssertRange(startOffset);
        AssertRange(startOffset + rangeSize - 1);

        // Start offset and size must be word-aligned
        Assert(BVUnit::Offset(startOffset) == 0);
        Assert(BVUnit::Offset(rangeSize) == 0);

        return (BVStatic<rangeSize> *)BitsFromIndex(startOffset);
    }

    BOOLEAN TestRange(const BVIndex index, uint length) const
    {TRACE_IT(21438);
        AssertRange(index);
        AssertRange(index + length - 1);

        const BVUnit * bvUnit = BitsFromIndex(index);
        uint offset = BVUnit::Offset(index);

        if (offset + length <= BVUnit::BitsPerWord)
        {TRACE_IT(21439);
            // Bit range is in a single word
            return bvUnit->TestRange(offset, length);
        }

        // Bit range spans words.
        // Test the first word, from start offset to end of word
        if (!bvUnit->TestRange(offset, (BVUnit::BitsPerWord - offset)))
        {TRACE_IT(21440);
            return FALSE;
        }

        bvUnit++;
        length -= (BVUnit::BitsPerWord - offset);

        // Test entire words until we are at the last word
        while (length >= BVUnit::BitsPerWord)
        {TRACE_IT(21441);
            if (!bvUnit->IsFull())
            {TRACE_IT(21442);
                return FALSE;
            }

            bvUnit++;
            length -= BVUnit::BitsPerWord;
        }

        // Test last word (unless we already ended on a word boundary)
        if (length > 0)
        {TRACE_IT(21443);
            if (!bvUnit->TestRange(0, length))
            {TRACE_IT(21444);
                return FALSE;
            }
        }

        return TRUE;
    }

    void SetRange(const BVIndex index, uint length)
    {TRACE_IT(21445);
        AssertRange(index);
        AssertRange(index + length - 1);

        BVUnit * bvUnit = BitsFromIndex(index);
        uint offset = BVUnit::Offset(index);

        if (offset + length <= BVUnit::BitsPerWord)
        {TRACE_IT(21446);
            // Bit range is in a single word
            return bvUnit->SetRange(offset, length);
        }

        // Bit range spans words.
        // Set the first word, from start offset to end of word
        bvUnit->SetRange(offset, (BVUnit::BitsPerWord - offset));

        bvUnit++;
        length -= (BVUnit::BitsPerWord - offset);

        // Set entire words until we are at the last word
        while (length >= BVUnit::BitsPerWord)
        {TRACE_IT(21447);
            bvUnit->SetAll();

            bvUnit++;
            length -= BVUnit::BitsPerWord;
        }

        // Set last word (unless we already ended on a word boundary)
        if (length > 0)
        {TRACE_IT(21448);
            bvUnit->SetRange(0, length);
        }
    }

    void ClearRange(const BVIndex index, uint length)
    {TRACE_IT(21449);
        AssertRange(index);
        AssertRange(index + length - 1);

        BVUnit * bvUnit = BitsFromIndex(index);
        uint offset = BVUnit::Offset(index);

        if (offset + length <= BVUnit::BitsPerWord)
        {TRACE_IT(21450);
            // Bit range is in a single word
            return bvUnit->ClearRange(offset, length);
        }

        // Bit range spans words.
        // Clear the first word, from start offset to end of word
        bvUnit->ClearRange(offset, (BVUnit::BitsPerWord - offset));

        bvUnit++;
        length -= (BVUnit::BitsPerWord - offset);

        // Set entire words until we are at the last word
        while (length >= BVUnit::BitsPerWord)
        {TRACE_IT(21451);
            bvUnit->ClearAll();

            bvUnit++;
            length -= BVUnit::BitsPerWord;
        }

        // Set last word (unless we already ended on a word boundary)
        if (length > 0)
        {TRACE_IT(21452);
            bvUnit->ClearRange(0, length);
        }
    }

    bool IsAllClear()
    {TRACE_IT(21453);
        for (BVIndex i = 0; i < wordCount; i++)
        {TRACE_IT(21454);
            if (!this->data[i].IsEmpty())
            {TRACE_IT(21455);
                return false;
            }
        }

        return true;
    }

#if DBG_DUMP
    void Dump() const
    {TRACE_IT(21456);
        bool hasBits = false;
        Output::Print(_u("[  "));
        for (BVIndex i = 0; i < wordCount; i++)
        {TRACE_IT(21457);
            hasBits = this->data[i].Dump(i * BVUnit::BitsPerWord, hasBits);
        }
        Output::Print(_u("]\n"));
    }
#endif
};
