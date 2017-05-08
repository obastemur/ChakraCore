//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"

BVFixed::BVFixed(BVFixed * initBv) :
   len(initBv->Length())
{TRACE_IT(21314);
   this->Copy(initBv);
}

BVFixed::BVFixed(BVIndex length, bool initialSet) :
    len(length)
{TRACE_IT(21315);
    Assert(length != 0);
    if (initialSet)
    {TRACE_IT(21316);
        this->SetAll();
    }
    else
    {TRACE_IT(21317);
        this->ClearAll();
    }
}

size_t
BVFixed::GetAllocSize(BVIndex length)
{TRACE_IT(21318);
    Assert(length != 0);
    return sizeof(BVFixed) + sizeof(BVUnit) * BVFixed::WordCount(length);
}

void
BVFixed::Init(BVIndex length)
{TRACE_IT(21319);
    Assert(length != 0);
    len = length;

}

BVIndex
BVFixed::WordCount(BVIndex length)
{TRACE_IT(21320);
    Assert(length != 0);
    return ((length - 1) >> BVUnit::ShiftValue) + 1;
}

BVIndex
BVFixed::WordCount() const
{TRACE_IT(21321);
    return WordCount(Length());
}

const BVUnit *
BVFixed::BitsFromIndex(BVIndex i) const
{TRACE_IT(21322);
    AssertRange(i);
    return &this->data[BVUnit::Position(i)];
}

BVUnit *
BVFixed::BitsFromIndex(BVIndex i)
{TRACE_IT(21323);
    AssertRange(i);
    return &this->data[BVUnit::Position(i)];
}

const BVUnit *
BVFixed::BeginUnit() const
{TRACE_IT(21324);
    return &this->data[0];
}

const BVUnit *
BVFixed::EndUnit() const
{TRACE_IT(21325);
    return &this->data[WordCount()];
}

BVUnit *
BVFixed::BeginUnit()
{TRACE_IT(21326);
    return &this->data[0];
}

BVUnit *
BVFixed::EndUnit()
{TRACE_IT(21327);
    return &this->data[WordCount()];
}

BVIndex
BVFixed::GetNextBit(BVIndex i) const
{TRACE_IT(21328);
    AssertRange(i);

    const   BVUnit * chunk      = BitsFromIndex(i);
            BVIndex  base       = BVUnit::Floor(i);


    BVIndex offset = chunk->GetNextBit(BVUnit::Offset(i));
    if(-1 != offset)
    {TRACE_IT(21329);
        return base + offset;
    }

    while(++chunk != this->EndUnit())
    {TRACE_IT(21330);
        base  += BVUnit::BitsPerWord;
        offset = chunk->GetNextBit();
        if(-1 != offset)
        {TRACE_IT(21331);
            return base + offset;
        }
    }

   return BVInvalidIndex;
}

void
BVFixed::AssertRange(BVIndex i) const
{TRACE_IT(21332);
    AssertMsg(i < this->Length(), "index out of bound");
}

void
BVFixed::AssertBV(const BVFixed *bv) const
{TRACE_IT(21333);
    AssertMsg(NULL != bv, "Cannot operate on NULL bitvector");
}

BVIndex
BVFixed::Length() const
{TRACE_IT(21334);
    return this->len;
}

void
BVFixed::SetAll()
{TRACE_IT(21335);
    memset(&this->data[0], -1, WordCount() * sizeof(BVUnit));
    ClearEnd();
}

void
BVFixed::ClearAll()
{TRACE_IT(21336);
    ZeroMemory(&this->data[0], WordCount() * sizeof(BVUnit));
}

BOOLEAN
BVFixed::TestAndSet(BVIndex i)
{TRACE_IT(21337);
    AssertRange(i);
    BVUnit * bvUnit = this->BitsFromIndex(i);
    BVIndex offset = BVUnit::Offset(i);
    BOOLEAN bit = bvUnit->Test(offset);
    bvUnit->Set(offset);
    return bit;
}

BOOLEAN
BVFixed::TestAndClear(BVIndex i)
{TRACE_IT(21338);
    AssertRange(i);
    BVUnit * bvUnit = this->BitsFromIndex(i);
    BVIndex offset = BVUnit::Offset(i);
    BOOLEAN bit = bvUnit->Test(offset);
    bvUnit->Clear(offset);
    return bit;
}

BOOLEAN
BVFixed::operator[](BVIndex i) const
{TRACE_IT(21339);
    AssertRange(i);
    return this->Test(i);
}

void
BVFixed::Or(const BVFixed*bv)
{TRACE_IT(21340);
    AssertBV(bv);
    this->for_each(bv, &BVUnit::Or);
}

//
// Xors the two bit vectors and returns the count of bits which are different.
//
uint
BVFixed::DiffCount(const BVFixed*bv) const
{TRACE_IT(21341);
    const BVUnit *i, *j;
    uint count = 0;
    for(i  =  this->BeginUnit(), j = bv->BeginUnit();
        i !=  this->EndUnit() && j != bv->EndUnit();
        i++, j++)
    {TRACE_IT(21342);
        count += i->DiffCount(*j);
    }

    // Assumes that the default value of is 0
    while(i != this->EndUnit())
    {TRACE_IT(21343);
        count += i->Count();
        i++;
    }
    while(j != bv->EndUnit())
    {TRACE_IT(21344);
        count += j->Count();
        j++;
    }
    return count;
}

void
BVFixed::OrComplimented(const BVFixed*bv)
{TRACE_IT(21345);
    AssertBV(bv);
    this->for_each(bv, &BVUnit::OrComplimented);
    ClearEnd();
}

void
BVFixed::And(const BVFixed*bv)
{TRACE_IT(21346);
    AssertBV(bv);
    this->for_each(bv, &BVUnit::And);
}

void
BVFixed::Minus(const BVFixed*bv)
{TRACE_IT(21347);
    AssertBV(bv);
    this->for_each(bv, &BVUnit::Minus);
}

void
BVFixed::Copy(const BVFixed*bv)
{TRACE_IT(21348);
    AssertBV(bv);
    Assert(len >= bv->len);

#if 1
    js_memcpy_s(&this->data[0], WordCount() * sizeof(BVUnit), &bv->data[0], bv->WordCount() * sizeof(BVUnit));
#else
    this->for_each(bv, &BVUnit::Copy);
#endif
}

void
BVFixed::CopyBits(const BVFixed * bv, BVIndex i)
{TRACE_IT(21349);
    AssertBV(bv);
    BVIndex offset = BVUnit::Offset(i);
    BVIndex position = BVUnit::Position(i);
    BVIndex len = bv->WordCount() - position;
    BVIndex copylen = min(WordCount(), len);
    if (offset == 0)
    {TRACE_IT(21350);
        js_memcpy_s(&this->data[0], copylen * sizeof(BVUnit), &bv->data[BVUnit::Position(i)], copylen * sizeof(BVUnit));
    }
    else
    {TRACE_IT(21351);
        BVIndex pos = position;
        for (BVIndex j = 0; j < copylen; j++)
        {TRACE_IT(21352);
            Assert(pos < bv->WordCount());
            this->data[j] = bv->data[pos];
            this->data[j].ShiftRight(offset);

            pos++;
            if (pos >= bv->WordCount())
            {TRACE_IT(21353);
                break;
            }
            BVUnit temp = bv->data[pos];
            temp.ShiftLeft(BVUnit::BitsPerWord - offset);
            this->data[j].Or(temp);
        }
    }
#if DBG
    for (BVIndex curr = i; curr < i + this->Length(); curr++)
    {TRACE_IT(21354);
        Assert(this->Test(curr - i) == bv->Test(curr));
    }
#endif
}

void
BVFixed::ComplimentAll()
{TRACE_IT(21355);
    for(BVIndex i=0; i < this->WordCount(); i++)
    {TRACE_IT(21356);
        this->data[i].ComplimentAll();
    }

    ClearEnd();
}

void
BVFixed::ClearEnd()
{TRACE_IT(21357);
    uint offset = BVUnit::Offset(this->Length());
    if (offset != 0)
    {TRACE_IT(21358);
        Assert((((uint64)1 << BVUnit::Offset(this->Length())) - 1) == BVUnit::GetTopBitsClear(this->Length()));
        this->data[this->WordCount() - 1].And(BVUnit::GetTopBitsClear(this->Length()));
    }
}

BVIndex
BVFixed::Count() const
{TRACE_IT(21359);
    BVIndex sum = 0;
    for (BVIndex i=0; i < this->WordCount(); i++)
    {TRACE_IT(21360);
        sum += this->data[i].Count();
    }
    Assert(sum <= this->Length());
    return sum;
}

JsUtil::FBVEnumerator
BVFixed::BeginSetBits()
{TRACE_IT(21361);
    return JsUtil::FBVEnumerator(this->BeginUnit(), this->EndUnit());
}

bool BVFixed::IsAllClear() const
{TRACE_IT(21362);
    bool isClear = true;
    for (BVIndex i=0; i < this->WordCount(); i++)
    {TRACE_IT(21363);
        isClear = this->data[i].IsEmpty() && isClear;
        if(!isClear)
        {TRACE_IT(21364);
            break;
        }
    }
    return isClear;
}


#if DBG_DUMP

void
BVFixed::Dump() const
{TRACE_IT(21365);
    bool hasBits = false;
    Output::Print(_u("[  "));
    for(BVIndex i=0; i < this->WordCount(); i++)
    {TRACE_IT(21366);
        hasBits = this->data[i].Dump(i * BVUnit::BitsPerWord, hasBits);
    }
    Output::Print(_u("]\n"));
}
#endif
