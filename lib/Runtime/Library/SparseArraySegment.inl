//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template<typename T>
    uint32 SparseArraySegment<T>::GetAlignedSize(uint32 size)
    {TRACE_IT(63558);
        return (uint32)(HeapInfo::GetAlignedSize(UInt32Math::MulAdd<sizeof(T), sizeof(SparseArraySegmentBase)>(size)) - sizeof(SparseArraySegmentBase)) / sizeof(T);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T> * SparseArraySegment<T>::Allocate(Recycler* recycler, uint32 left, uint32 length, uint32 size, uint32 fillStart /*= 0*/)
    {TRACE_IT(63559);
        Assert(length <= size);
        Assert(size <= JavascriptArray::MaxArrayLength - left);

        uint32 bufferSize = UInt32Math::Mul<sizeof(T)>(size);
        SparseArraySegment<T> *seg =
            isLeaf ?
            RecyclerNewPlusLeafZ(recycler, bufferSize, SparseArraySegment<T>, left, length, size) :
            RecyclerNewPlusZ(recycler, bufferSize, SparseArraySegment<T>, left, length, size);
        seg->FillSegmentBuffer(fillStart, size);

        return seg;
    }

    template<>
    inline SparseArraySegment<int> *SparseArraySegment<int>::AllocateLiteralHeadSegment(
        Recycler *const recycler,
        const uint32 length)
    {TRACE_IT(63560);
        if (DoNativeArrayLeafSegment())
        {TRACE_IT(63561);
            return SparseArraySegment<int>::AllocateLiteralHeadSegmentImpl<true>(recycler, length);
        }
        return SparseArraySegment<int>::AllocateLiteralHeadSegmentImpl<false>(recycler, length);
    }

    template<>
    inline SparseArraySegment<double> *SparseArraySegment<double>::AllocateLiteralHeadSegment(
        Recycler *const recycler,
        const uint32 length)
    {TRACE_IT(63562);
        if (DoNativeArrayLeafSegment())
        {TRACE_IT(63563);
            return SparseArraySegment<double>::AllocateLiteralHeadSegmentImpl<true>(recycler, length);
        }
        return SparseArraySegment<double>::AllocateLiteralHeadSegmentImpl<false>(recycler, length);
    }

    template<typename T>
    SparseArraySegment<T> *SparseArraySegment<T>::AllocateLiteralHeadSegment(
        Recycler *const recycler,
        const uint32 length)
    {TRACE_IT(63564);
        return SparseArraySegment<T>::AllocateLiteralHeadSegmentImpl<false>(recycler, length);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T> *SparseArraySegment<T>::AllocateLiteralHeadSegmentImpl(
        Recycler *const recycler,
        const uint32 length)
    {TRACE_IT(63565);
        Assert(length != 0);
        const uint32 size = GetAlignedSize(length);
        return SparseArraySegment<T>::Allocate<isLeaf>(recycler, 0, length, size, length);
    }

    template<>
    inline SparseArraySegment<int> * SparseArraySegment<int>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg)
    {TRACE_IT(63566);
        if (DoNativeArrayLeafSegment() && nextSeg == nullptr)
        {TRACE_IT(63567);
            return AllocateSegmentImpl<true>(recycler, left, length, nextSeg);
        }
        return AllocateSegmentImpl<false>(recycler, left, length, nextSeg);
    }

    template<>
    inline SparseArraySegment<double> * SparseArraySegment<double>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg)
    {TRACE_IT(63568);
        if (DoNativeArrayLeafSegment() && nextSeg == nullptr)
        {TRACE_IT(63569);
            return AllocateSegmentImpl<true>(recycler, left, length, nextSeg);
        }
        return AllocateSegmentImpl<false>(recycler, left, length, nextSeg);
    }

    template<typename T>
    SparseArraySegment<T> * SparseArraySegment<T>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg)
    {TRACE_IT(63570);
        return AllocateSegmentImpl<false>(recycler, left, length, nextSeg);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T> * SparseArraySegment<T>::AllocateSegmentImpl(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg)
    {TRACE_IT(63571);
        Assert(!isLeaf || nextSeg == nullptr);

        uint32 size;
        if ((length <= CHUNK_SIZE) && (left < BigLeft))
        {TRACE_IT(63572);
            size = GetAlignedSize(CHUNK_SIZE);
        }
        else
        {TRACE_IT(63573);
            size = GetAlignedSize(length);
        }

        //But don't overshoot next segment
        EnsureSizeInBound(left, length, size, nextSeg);

        return SparseArraySegment<T>::Allocate<isLeaf>(recycler, left, length, size);
    }

    template<>
    inline SparseArraySegment<int> * SparseArraySegment<int>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg)
    {TRACE_IT(63574);
        if (DoNativeArrayLeafSegment() && nextSeg == nullptr)
        {TRACE_IT(63575);
            return AllocateSegmentImpl<true>(recycler, left, length, size, nextSeg);
        }
        return AllocateSegmentImpl<false>(recycler, left, length, size, nextSeg);
    }

    template<>
    inline SparseArraySegment<double> * SparseArraySegment<double>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg)
    {TRACE_IT(63576);
        if (DoNativeArrayLeafSegment() && nextSeg == nullptr)
        {TRACE_IT(63577);
            return AllocateSegmentImpl<true>(recycler, left, length, size, nextSeg);
        }
        return AllocateSegmentImpl<false>(recycler, left, length, size, nextSeg);
    }

    template<typename T>
    inline SparseArraySegment<T> * SparseArraySegment<T>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg)
    {TRACE_IT(63578);
        return AllocateSegmentImpl<false>(recycler, left, length, size, nextSeg);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T> * SparseArraySegment<T>::AllocateSegmentImpl(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg)
    {TRACE_IT(63579);
        Assert(!isLeaf || nextSeg == nullptr);

        AssertMsg(size > 0, "size too small");
        if ((size <= CHUNK_SIZE) && (size < BigLeft))
        {TRACE_IT(63580);
            size = GetAlignedSize(CHUNK_SIZE);
        }
        else
        {TRACE_IT(63581);
            size = GetAlignedSize(size);
        }

        //But don't overshoot next segment
        EnsureSizeInBound(left, length, size, nextSeg);

        return SparseArraySegment<T>::Allocate<isLeaf>(recycler, left, length, size);
    }

    template<>
    inline SparseArraySegment<int>* SparseArraySegment<int>::AllocateSegment(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index)
    {TRACE_IT(63582);
        if (DoNativeArrayLeafSegment() && prev->next == nullptr)
        {TRACE_IT(63583);
            return AllocateSegmentImpl<true>(recycler, prev, index);
        }
        return AllocateSegmentImpl<false>(recycler, prev, index);
    }

    template<>
    inline SparseArraySegment<double>* SparseArraySegment<double>::AllocateSegment(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index)
    {TRACE_IT(63584);
        if (DoNativeArrayLeafSegment() && prev->next == nullptr)
        {TRACE_IT(63585);
            return AllocateSegmentImpl<true>(recycler, prev, index);
        }
        return AllocateSegmentImpl<false>(recycler, prev, index);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::AllocateSegment(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index)
    {TRACE_IT(63586);
        return AllocateSegmentImpl<false>(recycler, prev, index);
    }

    // Allocate a segment in between (prev, next) to contain an index
    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T>* SparseArraySegment<T>::AllocateSegmentImpl(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index)
    {TRACE_IT(63587);
        Assert(prev);
        Assert(index > prev->left && index - prev->left >= prev->size);

        SparseArraySegmentBase* next = prev->next;
        Assert(!next || index < next->left);
        Assert(!next || !isLeaf);

        uint32 left = index;
        uint32 size = (left < BigLeft ? GetAlignedSize(CHUNK_SIZE) : GetAlignedSize(1));

        // Try to move the segment leftwards if it overshoots next segment
        if (next && size > next->left - left)
        {TRACE_IT(63588);
            size = min(size, next->left - (prev->left + prev->size));
            left = next->left - size;
        }

        Assert(index >= left && index - left < size);
        uint32 length = index - left + 1;

        EnsureSizeInBound(left, length, size, next);
        return SparseArraySegment<T>::Allocate<isLeaf>(recycler, left, length, size);
    }

    template<typename T>
    void SparseArraySegment<T>::FillSegmentBuffer(uint32 start, uint32 size)
    {TRACE_IT(63589);
        // Fill the segment buffer using gp-register-sized stores. Avoid using the FPU for the sake
        // of perf (especially x86).
        Var fill = JavascriptArray::MissingItem;
        if (sizeof(Var) > sizeof(T))
        {TRACE_IT(63590);
            // Pointer size is greater than the element (int32 buffer on x64).
            // Fill as much as we can and do one int32-sized store at the end if necessary.
            uint32 i, step = sizeof(Var) / sizeof(T);
            if (start & 1)
            {TRACE_IT(63591);
                Assert(sizeof(T) == sizeof(int32));
                ((int32*)(this->elements))[start] = JavascriptNativeIntArray::MissingItem;
            }
            for (i = (start + step-1)/step; i < (size/step); i++)
            {TRACE_IT(63592);
                ((Var*)(this->elements))[i] = fill; // swb: no write barrier, set to non-GC pointer
            }
            if ((i *= step) < size)
            {TRACE_IT(63593);
                Assert(sizeof(T) == sizeof(int32));
                ((int32*)(this->elements))[i] = JavascriptNativeIntArray::MissingItem;
            }
        }
        else
        {TRACE_IT(63594);
            // Pointer size <= element size. Fill with pointer-sized stores.
            Assert(sizeof(T) % sizeof(Var) == 0);
            uint step = sizeof(T) / sizeof(Var);

            for (uint i = start; i < size * step; i++)
            {TRACE_IT(63595);
                ((Var*)(this->elements))[i] = fill; // swb: no write barrier, set to non-GC pointer
            }
        }
    }

    template<typename T>
    void SparseArraySegment<T>::SetElement(Recycler *recycler, uint32 index, T value)
    {TRACE_IT(63596);
        AssertMsg(index >= left && index - left < size, "Index out of range");
        uint32 offset = index - left;

        elements[offset] = value;

        if ((offset + 1) > length)
        {TRACE_IT(63597);
            length =  offset + 1;
        }
        Assert(length <= size);
        Assert(left + length > left);
    }

    template<typename T>
    SparseArraySegment<T> *SparseArraySegment<T>::SetElementGrow(Recycler *recycler, SparseArraySegmentBase* prev, uint32 index, T value)
    {TRACE_IT(63598);
        AssertMsg((index + 1) == left || index == (left + size), "Index out of range");

        uint32 offset = index - left;
        SparseArraySegment<T> *current = this;

        if (index + 1 == left)
        {TRACE_IT(63599);
            Assert(prev && prev->next == current);
            Assert(left > prev->left && left - prev->left > prev->size);
            Assert(left - prev->left - prev->size > 1); // Otherwise we would be growing/merging prev

            // Grow front up to (prev->left + prev->size + 1), so that setting (prev->left + prev->size)
            // later would trigger segment merging.
            current = GrowFrontByMax(recycler, left - prev->left - prev->size - 1);
            current->SetElement(recycler, index, value);
        }
        else if (offset == size)
        {TRACE_IT(63600);
            if (next == nullptr)
            {TRACE_IT(63601);
                current = GrowByMin(recycler, offset + 1 - size);
            }
            else
            {TRACE_IT(63602);
                current = GrowByMinMax(recycler, offset + 1 - size, next->left - left - size);
            }
            current->elements[offset] = value;
            current->length =  offset + 1;
        }
        else
        {
            AssertMsg(false, "Invalid call to SetElementGrow");
        }

        return current;
    }

    template<typename T>
    T SparseArraySegment<T>::GetElement(uint32 index)
    {TRACE_IT(63603);
        AssertMsg(index >= left && index <= left + length - 1, "Index is out of the segment range");
        return elements[index - left];
    }

    // This is a very inefficient function, we have to move element
    template<typename T>
    void SparseArraySegment<T>::RemoveElement(Recycler *recycler, uint32 index)
    {TRACE_IT(63604);
        AssertMsg(index >= left && index < left + length, "Index is out of the segment range");
        if (index + 1 < left + length)
        {TRACE_IT(63605);
            MoveArray(elements + index - left, elements + index + 1 - left, length - (index - left) - 1);
        }
        Assert(length);
        length--;
        elements[length] = SparseArraySegment<T>::GetMissingItem();
    }


    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::CopySegment(Recycler *recycler, SparseArraySegment<T>* dst, uint32 dstIndex, SparseArraySegment<T>* src, uint32 srcIndex, uint32 inputLen)
    {TRACE_IT(63606);
        AssertMsg(src != nullptr && dst != nullptr, "Null input!");

        uint32 newLen = dstIndex - dst->left + inputLen;
        if (newLen > dst->size)
        {TRACE_IT(63607);
            dst = dst->GrowBy(recycler, newLen - dst->size);
        }
        dst->length = newLen;
        Assert(dst->length <= dst->size);
        AssertMsg(srcIndex >= src->left,"src->left > srcIndex resulting in negative indexing of src->elements");
        CopyArray(dst->elements + dstIndex - dst->left, inputLen, src->elements + srcIndex - src->left, inputLen);
        return dst;
    }

    template<typename T>
    uint32 SparseArraySegment<T>::GetGrowByFactor()
    {TRACE_IT(63608);
        if (size < CHUNK_SIZE/2)
        {TRACE_IT(63609);
            return (GetAlignedSize(size * 4) - size);
        }
        else if (size < 1024)
        {TRACE_IT(63610);
            return (GetAlignedSize(size * 2) - size);
        }
        return (GetAlignedSize(UInt32Math::Mul(size, 5) / 3) - size);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowByMin(Recycler *recycler, uint32 minValue)
    {TRACE_IT(63611);
        Assert(size <= JavascriptArray::MaxArrayLength - left);

        uint32 maxGrow = JavascriptArray::MaxArrayLength - (left + size);
        return GrowByMinMax(recycler, minValue, maxGrow);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowByMinMax(Recycler *recycler, uint32 minValue, uint32 maxValue)
    {TRACE_IT(63612);
        Assert(size <= JavascriptArray::MaxArrayLength - left);
        Assert(maxValue <= JavascriptArray::MaxArrayLength - (left + size));
        AssertMsg(minValue <= maxValue, "Invalid values to GrowByMinMax");

        return GrowBy(recycler, max(minValue,min(maxValue, GetGrowByFactor())));
    }

    template<>
    inline SparseArraySegment<int>* SparseArraySegment<int>::GrowBy(Recycler *recycler, uint32 n)
    {TRACE_IT(63613);
        if (!DoNativeArrayLeafSegment() || this->next != nullptr)
        {TRACE_IT(63614);
            return GrowByImpl<false>(recycler, n);
        }
        return GrowByImpl<true>(recycler, n);
    }

    template<>
    inline SparseArraySegment<double>* SparseArraySegment<double>::GrowBy(Recycler *recycler, uint32 n)
    {TRACE_IT(63615);
        if (!DoNativeArrayLeafSegment() || this->next != nullptr)
        {TRACE_IT(63616);
            return GrowByImpl<false>(recycler, n);
        }
        return GrowByImpl<true>(recycler, n);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowBy(Recycler *recycler, uint32 n)
    {TRACE_IT(63617);
        return GrowByImpl<false>(recycler, n);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowByImpl(Recycler *recycler, uint32 n)
    {TRACE_IT(63618);
        Assert(length <= size);
        Assert(n != 0);

        uint32 newSize = size + n;
        if (newSize <= size)
        {TRACE_IT(63619);
            Throw::OutOfMemory();
        }

        SparseArraySegment<T> *newSeg = Allocate<isLeaf>(recycler, left, length, newSize);
        newSeg->next = this->next;
        // (sizeof(T) * newSize) will throw OOM in Allocate if it overflows.
        CopyArray(newSeg->elements, newSize, this->elements, length);

        return newSeg;
    }

    //
    // Grows segment in the front. Note that the result new segment's left is changed.
    //
    template<>
    inline SparseArraySegment<int>* SparseArraySegment<int>::GrowFrontByMax(Recycler *recycler, uint32 n)
    {TRACE_IT(63620);
        if (DoNativeArrayLeafSegment() && this->next == nullptr)
        {TRACE_IT(63621);
            return GrowFrontByMaxImpl<true>(recycler, n);
        }
        return GrowFrontByMaxImpl<false>(recycler, n);
    }

    template<>
    inline SparseArraySegment<double>* SparseArraySegment<double>::GrowFrontByMax(Recycler *recycler, uint32 n)
    {TRACE_IT(63622);
        if (DoNativeArrayLeafSegment() && this->next == nullptr)
        {TRACE_IT(63623);
            return GrowFrontByMaxImpl<true>(recycler, n);
        }
        return GrowFrontByMaxImpl<false>(recycler, n);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowFrontByMax(Recycler *recycler, uint32 n)
    {TRACE_IT(63624);
        return GrowFrontByMaxImpl<false>(recycler, n);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowFrontByMaxImpl(Recycler *recycler, uint32 n)
    {TRACE_IT(63625);
        Assert(length <= size);
        Assert(n > 0);
        Assert(n <= left);
        Assert(size + n > size);

        n = min(n, GetGrowByFactor());

        if (size + n <= size)
        {TRACE_IT(63626);
            Throw::OutOfMemory();
        }

        SparseArraySegment<T> *newSeg = Allocate<isLeaf>(recycler, left - n, length + n, size + n);
        newSeg->next = this->next;
        CopyArray(newSeg->elements + n, length, this->elements, length);

        return newSeg;
    }

    template<typename T>
    void SparseArraySegment<T>::ClearElements(__out_ecount(len) Field(T)* elements, uint32 len)
    {TRACE_IT(63627);
        T fill = SparseArraySegment<T>::GetMissingItem();
        for (uint i = 0; i < len; i++)
        {TRACE_IT(63628);
            elements[i] = fill;
        }
    }

    template<typename T>
    void SparseArraySegment<T>::Truncate(uint32 index)
    {TRACE_IT(63629);
        AssertMsg(index >= left && (index - left) < size, "Index out of range");

        ClearElements(elements + (index - left), size - (index - left));
        if (index - left < length)
        {TRACE_IT(63630);
            length = index - left;
        }
        Assert(length <= size);
    }


    template<typename T>
    void SparseArraySegment<T>::ReverseSegment(Recycler *recycler)
    {TRACE_IT(63631);
        if (length <= 1)
        {TRACE_IT(63632);
            return;
        }

        T temp;
        uint32 lower = 0;
        uint32 upper = length - 1;
        while (lower < upper)
        {TRACE_IT(63633);
            temp = elements[lower];
            elements[lower] = elements[upper];
            elements[upper] = temp;
            ++lower;
            --upper;
        }
    }

}
