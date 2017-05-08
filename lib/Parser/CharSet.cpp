//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"
#include "Common/MathUtil.h"

namespace UnifiedRegex
{
    // ----------------------------------------------------------------------
    // CharBitVec
    // ----------------------------------------------------------------------

    uint CharBitvec::Count() const
    {TRACE_IT(28737);
        uint n = 0;
        for (int w = 0; w < vecSize; w++)
        {TRACE_IT(28738);
            n += Math::PopCnt32(vec[w]);
        }
        return n;
    }

    int CharBitvec::NextSet(int k) const
    {TRACE_IT(28739);
        if (k < 0 || k >= Size)
            return -1;
        uint w = k / wordSize;
        uint o = k % wordSize;
        uint32 v = vec[w] >> o;
        do
        {TRACE_IT(28740);
            if (v == 0)
            {TRACE_IT(28741);
                k += wordSize - o;
                break;
            }
            else if ((v & 0x1) != 0)
                return k;
            else
            {TRACE_IT(28742);
                v >>= 1;
                o++;
                k++;
            }
        }
        while (o < wordSize);

        w++;
        while (w < vecSize)
        {TRACE_IT(28743);
            o = 0;
            v = vec[w];
            do
            {TRACE_IT(28744);
                if (v == 0)
                {TRACE_IT(28745);
                    k += wordSize - o;
                    break;
                }
                else if ((v & 0x1) != 0)
                    return k;
                else
                {TRACE_IT(28746);
                    v >>= 1;
                    o++;
                    k++;
                }

            }
            while (o < wordSize);
            w++;
        }
        return -1;
    }

    int CharBitvec::NextClear(int k) const
    {TRACE_IT(28747);
        if (k < 0 || k >= Size)
            return -1;
        uint w = k / wordSize;
        uint o = k % wordSize;
        uint32 v = vec[w] >> o;
        do
        {TRACE_IT(28748);
            if (v == ones)
            {TRACE_IT(28749);
                k += wordSize - o;
                break;
            }
            else if ((v & 0x1) == 0)
                return k;
            else
            {TRACE_IT(28750);
                v >>= 1;
                o++;
                k++;
            }
        }
        while (o < wordSize);

        w++;
        while (w < vecSize)
        {TRACE_IT(28751);
            o = 0;
            v = vec[w];
            do
            {TRACE_IT(28752);
                if (v == ones)
                {TRACE_IT(28753);
                    k += wordSize - o;
                    break;
                }
                else if ((v & 0x1) == 0)
                    return k;
                else
                {TRACE_IT(28754);
                    v >>= 1;
                    o++;
                    k++;
                }

            }
            while (o < wordSize);
            w++;
        }
        return -1;
    }

    template <typename C>
    void CharBitvec::ToComplement(ArenaAllocator* allocator, uint base, CharSet<C>& result) const
    {TRACE_IT(28755);
        int hi = -1;
        while (true)
        {TRACE_IT(28756);
            // Find the next range of clear bits in vector
            int li = NextClear(hi + 1);
            if (li < 0)
                return;
            hi = NextSet(li + 1);
            if (hi < 0)
                hi = Size - 1;
            else
            {TRACE_IT(28757);
                Assert(hi > 0);
                hi--;
            }

            // Add range as characters
            result.SetRange(allocator, Chars<C>::ITC(base + li), Chars<C>::ITC(base + hi));
        }
    }

    template <typename C>
    void CharBitvec::ToEquivClass(ArenaAllocator* allocator, uint base, uint& tblidx, CharSet<C>& result, codepoint_t baseOffset) const
    {TRACE_IT(28758);
        int hi = -1;
        while (true)
        {TRACE_IT(28759);
            // Find the next range of set bits in vector
            int li = NextSet(hi + 1);
            if (li < 0)
                return;
            hi = NextClear(li + 1);
            if (hi < 0)
                hi = Size - 1;
            else
            {TRACE_IT(28760);
                Assert(hi > 0);
                hi--;
            }

            // Convert to character codes
            uint l = base + li + baseOffset;
            uint h = base + hi + baseOffset;

            do
            {TRACE_IT(28761);
                uint acth;
                C equivl[CaseInsensitive::EquivClassSize];
                CaseInsensitive::RangeToEquivClass(tblidx, l, h, acth, equivl);
                uint n = acth - l;
                for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
                {TRACE_IT(28762);
                    result.SetRange(allocator, equivl[i], Chars<C>::Shift(equivl[i], n));
                }

                // Go around again for rest of this range
                l = acth + 1;
            }
            while (l <= h);
        }
    }

    // ----------------------------------------------------------------------
    // CharSetNode
    // ----------------------------------------------------------------------

    CharSetNode* CharSetNode::For(ArenaAllocator* allocator, int level)
    {TRACE_IT(28763);
        if (level == 0)
            return Anew(allocator, CharSetLeaf);
        else
            return Anew(allocator, CharSetInner);
    }

    // ----------------------------------------------------------------------
    // CharSetFull
    // ----------------------------------------------------------------------

    CharSetFull CharSetFull::Instance;

    CharSetFull* const CharSetFull::TheFullNode = &CharSetFull::Instance;

    CharSetFull::CharSetFull() {TRACE_IT(28764);}

    void CharSetFull::FreeSelf(ArenaAllocator* allocator)
    {TRACE_IT(28765);
        Assert(this == TheFullNode);
        // Never allocated
    }

    CharSetNode* CharSetFull::Clone(ArenaAllocator* allocator) const
    {TRACE_IT(28766);
        // Always shared
        return (CharSetNode*)this;
    }

    CharSetNode* CharSetFull::Set(ArenaAllocator* allocator, uint level, uint l, uint h)
    {TRACE_IT(28767);
        return this;
    }

    CharSetNode* CharSetFull::ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h)
    {TRACE_IT(28768);
        AssertMsg(h <= lim(level), "The range for clearing provided is invalid for this level.");
        AssertMsg(l <= h, "Can't clear where lower is bigger than the higher.");
        if (l == 0 && h == lim(level))
        {TRACE_IT(28769);
            return nullptr;
        }

        CharSetNode* toReturn = For(allocator, level);

        if (l > 0)
        {TRACE_IT(28770);
            AssertVerify(toReturn->Set(allocator, level, 0, l - 1) == toReturn);
        }

        if (h < lim(level))
        {TRACE_IT(28771);
            AssertVerify(toReturn->Set(allocator, level, h + 1, lim(level)) == toReturn);
        }

        return toReturn;
    }

    CharSetNode* CharSetFull::UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other)
    {TRACE_IT(28772);
        return this;
    }

    bool CharSetFull::Get(uint level, uint k) const
    {TRACE_IT(28773);
        return true;
    }

    void CharSetFull::ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const
    {TRACE_IT(28774);
        // Empty, so add nothing
    }

    void CharSetFull::ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<char16>& result) const
    {TRACE_IT(28775);
        this->ToEquivClass<char16>(allocator, level, base, tblidx, result);
    }

    void CharSetFull::ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const
    {TRACE_IT(28776);
        this->ToEquivClass<codepoint_t>(allocator, level, base, tblidx, result, baseOffset);
    }

    template <typename C>
    void CharSetFull::ToEquivClass(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<C>& result, codepoint_t baseOffset) const
    {TRACE_IT(28777);
        uint l = base + (CharSetNode::levels - 1 == level ? 0xff : 0) + baseOffset;
        uint h = base + lim(level) + baseOffset;

        do
        {TRACE_IT(28778);
            uint acth;
            C equivl[CaseInsensitive::EquivClassSize];
            CaseInsensitive::RangeToEquivClass(tblidx, l, h, acth, equivl);
            uint n = acth - l;
            for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
            {TRACE_IT(28779);
                result.SetRange(allocator, equivl[i], Chars<C>::Shift(equivl[i], n));
            }

            // Go around again for rest of this range
            l = acth + 1;
        }
        while (l <= h);
    }

    bool CharSetFull::IsSubsetOf(uint level, const CharSetNode* other) const
    {TRACE_IT(28780);
        Assert(other != nullptr);
        return other == TheFullNode;
    }

    bool CharSetFull::IsEqualTo(uint level, const CharSetNode* other) const
    {TRACE_IT(28781);
        Assert(other != nullptr);
        return other == TheFullNode;
    }

    uint CharSetFull::Count(uint level) const
    {TRACE_IT(28782);
        return lim(level) + 1;
    }

    _Success_(return)
    bool CharSetFull::GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const
    {TRACE_IT(28783);
        Assert(searchCharStart < this->Count(level));

        *outLowerChar = searchCharStart;
        *outHigherChar = (Char)this->Count(level) - 1;

        return true;
    }

#if DBG
    bool CharSetFull::IsLeaf() const
    {TRACE_IT(28784);
        return false;
    }
#endif

    // ----------------------------------------------------------------------
    // CharSetInner
    // ----------------------------------------------------------------------

    CharSetInner::CharSetInner()
    {TRACE_IT(28785);
        for (uint i = 0; i < branchingPerInnerLevel; i++)
            children[i] = 0;
    }

    void CharSetInner::FreeSelf(ArenaAllocator* allocator)
    {TRACE_IT(28786);
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28787);
            if (children[i] != 0)
            {TRACE_IT(28788);
                children[i]->FreeSelf(allocator);
#if DBG
                children[i] = 0;
#endif
            }
        }
        Adelete(allocator, this);
    }

    CharSetNode* CharSetInner::Clone(ArenaAllocator* allocator) const
    {TRACE_IT(28789);
        CharSetInner* res = Anew(allocator, CharSetInner);
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28790);
            if (children[i] != 0)
                res->children[i] = children[i]->Clone(allocator);
        }
        return res;
    }

    CharSetNode* CharSetInner::ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h)
    {TRACE_IT(28791);
        Assert(level > 0);
        AssertMsg(h <= lim(level), "The range for clearing provided is invalid for this level.");
        AssertMsg(l <= h, "Can't clear where lower is bigger than the higher.");
        if (l == 0 && h == lim(level))
        {TRACE_IT(28792);
            return nullptr;
        }

        uint lowerIndex = innerIdx(level, l);
        uint higherIndex = innerIdx(level--, h);
        l = l & lim(level);
        h = h & lim(level);
        if (lowerIndex == higherIndex)
        {TRACE_IT(28793);
            if (children[lowerIndex] != nullptr)
            {TRACE_IT(28794);
                children[lowerIndex] = children[lowerIndex]->ClearRange(allocator, level, l, h);
            }
        }
        else
        {TRACE_IT(28795);
            if (children[lowerIndex] != nullptr)
            {TRACE_IT(28796);
                children[lowerIndex] = children[lowerIndex]->ClearRange(allocator, level, l, lim(level));
            }

            for (uint i = lowerIndex + 1; i < higherIndex; i++)
            {TRACE_IT(28797);
                if (children[i] != nullptr)
                {TRACE_IT(28798);
                    children[i]->FreeSelf(allocator);
                }

                children[i] = nullptr;
            }

            if (children[higherIndex] != nullptr)
            {TRACE_IT(28799);
                children[higherIndex] = children[higherIndex]->ClearRange(allocator, level, 0, h);
            }
        }
        for (int i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28800);
            if (children[i] != nullptr)
            {TRACE_IT(28801);
                return this;
            }
        }

        return nullptr;
    }

    CharSetNode* CharSetInner::Set(ArenaAllocator* allocator, uint level, uint l, uint h)
    {TRACE_IT(28802);
        Assert(level > 0);
        uint li = innerIdx(level, l);
        uint hi = innerIdx(level--, h);
        bool couldBeFull = true;
        if (li == hi)
        {TRACE_IT(28803);
            if (children[li] == nullptr)
            {
                if (remain(level, l) == 0 && remain(level, h + 1) == 0)
                    children[li] = CharSetFull::TheFullNode;
                else
                {TRACE_IT(28804);
                    children[li] = For(allocator, level);
                    children[li] = children[li]->Set(allocator, level, l, h);
                    couldBeFull = false;
                }
            }
            else
                children[li] = children[li]->Set(allocator, level, l, h);
        }
        else
        {TRACE_IT(28805);
            if (children[li] == nullptr)
            {
                if (remain(level, l) == 0)
                    children[li] = CharSetFull::TheFullNode;
                else
                {TRACE_IT(28806);
                    children[li] = For(allocator, level);
                    children[li] = children[li]->Set(allocator, level, l, lim(level));
                    couldBeFull = false;
                }
            }
            else
                children[li] = children[li]->Set(allocator, level, l, lim(level));
            for (uint i = li + 1; i < hi; i++)
            {TRACE_IT(28807);
                if (children[i] != nullptr)
                    children[i]->FreeSelf(allocator);
                children[i] = CharSetFull::TheFullNode;
            }
            if (children[hi] == nullptr)
            {
                if (remain(level, h + 1) == 0)
                    children[hi] = CharSetFull::TheFullNode;
                else
                {TRACE_IT(28808);
                    children[hi] = For(allocator, level);
                    children[hi] = children[hi]->Set(allocator, level, 0, h);
                    couldBeFull = false;
                }
            }
            else
                children[hi] = children[hi]->Set(allocator, level, 0, h);
        }
        if (couldBeFull)
        {TRACE_IT(28809);
            for (uint i = 0; i < branchingPerInnerLevel; i++)
            {TRACE_IT(28810);
                if (children[i] != CharSetFull::TheFullNode)
                    return this;
            }
            FreeSelf(allocator);
            return CharSetFull::TheFullNode;
        }
        else
            return this;
    }

    CharSetNode* CharSetInner::UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other)
    {TRACE_IT(28811);
        Assert(level > 0);
        Assert(other != nullptr && other != CharSetFull::TheFullNode && !other->IsLeaf());
        CharSetInner* otherInner = (CharSetInner*)other;
        level--;
        bool isFull = true;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28812);
            if (otherInner->children[i] != nullptr)
            {TRACE_IT(28813);
                if (otherInner->children[i] == CharSetFull::TheFullNode)
                {TRACE_IT(28814);
                    if (children[i] != nullptr)
                        children[i]->FreeSelf(allocator);
                    children[i] = CharSetFull::TheFullNode;
                }
                else
                {TRACE_IT(28815);
                    if (children[i] == nullptr)
                        children[i] = For(allocator, level);
                    children[i] = children[i]->UnionInPlace(allocator, level, otherInner->children[i]);
                    if (children[i] != CharSetFull::TheFullNode)
                        isFull = false;
                }
            }
            else if (children[i] != CharSetFull::TheFullNode)
                isFull = false;
        }
        if (isFull)
        {TRACE_IT(28816);
            FreeSelf(allocator);
            return CharSetFull::TheFullNode;
        }
        else
            return this;
    }

    bool CharSetInner::Get(uint level, uint k) const
    {TRACE_IT(28817);
        Assert(level > 0);
        uint i = innerIdx(level--, k);
        if (children[i] == nullptr)
            return false;
        else
            return children[i]->Get(level, k);
    }

    void CharSetInner::ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const
    {TRACE_IT(28818);
        Assert(level > 0);
        level--;
        uint delta = lim(level) + 1;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28819);
            if (children[i] == nullptr)
                // Caution: Part of the range for this child may overlap with direct vector
                result.SetRange(allocator, UTC(max(base, directSize)), UTC(base + delta - 1));
            else
                children[i]->ToComplement(allocator, level, base, result);
            base += delta;
        }
    }

    void CharSetInner::ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<char16>& result) const
    {TRACE_IT(28820);
        Assert(level > 0);
        level--;
        uint delta = lim(level) + 1;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28821);
            if (children[i] != nullptr)
            {TRACE_IT(28822);
                children[i]->ToEquivClassW(allocator, level, base, tblidx, result);
            }
            base += delta;
        }
    }

    void CharSetInner::ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const
    {TRACE_IT(28823);
        Assert(level > 0);
        level--;
        uint delta = lim(level) + 1;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28824);
            if (children[i] != nullptr)
            {TRACE_IT(28825);
                children[i]->ToEquivClassCP(allocator, level, base, tblidx, result, baseOffset);
            }
            base += delta;
        }
    }

    bool CharSetInner::IsSubsetOf(uint level, const CharSetNode* other) const
    {TRACE_IT(28826);
        Assert(level > 0);
        Assert(other != nullptr && !other->IsLeaf());
        if (other == CharSetFull::TheFullNode)
            return true;
        level--;
        const CharSetInner* otherInner = (CharSetInner*)other;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28827);
            if (children[i] != nullptr)
            {TRACE_IT(28828);
                if (otherInner->children[i] == nullptr)
                    return false;
                if (children[i]->IsSubsetOf(level, otherInner->children[i]))
                    return false;
            }
        }
        return true;
    }

    bool CharSetInner::IsEqualTo(uint level, const CharSetNode* other) const
    {TRACE_IT(28829);
        Assert(level > 0);
        Assert(other != nullptr && !other->IsLeaf());
        if (other == CharSetFull::TheFullNode)
            return false;
        level--;
        const CharSetInner* otherInner = (CharSetInner*)other;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28830);
            if (children[i] != 0)
            {TRACE_IT(28831);
                if (otherInner->children[i] == nullptr)
                    return false;
                if (children[i]->IsSubsetOf(level, otherInner->children[i]))
                    return false;
            }
        }
        return true;
    }

    uint CharSetInner::Count(uint level) const
    {TRACE_IT(28832);
        uint n = 0;
        Assert(level >  0);
        level--;
        for (uint i = 0; i < branchingPerInnerLevel; i++)
        {TRACE_IT(28833);
            if (children[i] != nullptr)
                n += children[i]->Count(level);
        }
        return n;
    }

    _Success_(return)
    bool CharSetInner::GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const
    {TRACE_IT(28834);
        Assert(searchCharStart < this->lim(level) + 1);
        uint innerIndex = innerIdx(level--, searchCharStart);

        Char currentLowChar = 0, currentHighChar = 0;

        for (; innerIndex < branchingPerInnerLevel; innerIndex++)
        {TRACE_IT(28835);
            if (children[innerIndex] != nullptr && children[innerIndex]->GetNextRange(level, (Char)remain(level, searchCharStart), &currentLowChar, &currentHighChar))
            {TRACE_IT(28836);
                break;
            }

            if (innerIndex < branchingPerInnerLevel - 1)
            {TRACE_IT(28837);
                searchCharStart = (Char)indexToValue(level + 1, innerIndex + 1, 0);
            }
        }

        if (innerIndex == branchingPerInnerLevel)
        {TRACE_IT(28838);
            return false;
        }

        currentLowChar = (Char)indexToValue(level + 1, innerIndex, currentLowChar);
        currentHighChar = (Char)indexToValue(level + 1, innerIndex, currentHighChar);

        innerIndex += 1;

        for (; remain(level, currentHighChar) == lim(level) && innerIndex < branchingPerInnerLevel; innerIndex++)
        {TRACE_IT(28839);
            Char tempLower, tempHigher;
            if (children[innerIndex] == nullptr || !children[innerIndex]->GetNextRange(level, 0x0, &tempLower, &tempHigher) || remain(level, tempLower) != 0)
            {TRACE_IT(28840);
                break;
            }

            currentHighChar = (Char)indexToValue(level + 1, innerIndex, tempHigher);
        }

        *outLowerChar = currentLowChar;
        *outHigherChar = currentHighChar;

        return true;
    }

#if DBG
    bool CharSetInner::IsLeaf() const
    {TRACE_IT(28841);
        return false;
    }
#endif

    // ----------------------------------------------------------------------
    // CharSetLeaf
    // ----------------------------------------------------------------------

    CharSetLeaf::CharSetLeaf()
    {TRACE_IT(28842);
        vec.Clear();
    }

    void CharSetLeaf::FreeSelf(ArenaAllocator* allocator)
    {
        Adelete(allocator, this);
    }

    CharSetNode* CharSetLeaf::Clone(ArenaAllocator* allocator) const
    {TRACE_IT(28843);
        return Anew(allocator, CharSetLeaf, *this);
    }

    CharSetNode* CharSetLeaf::Set(ArenaAllocator* allocator, uint level, uint l, uint h)
    {TRACE_IT(28844);
        Assert(level == 0);
        vec.SetRange(leafIdx(l), leafIdx(h));
        if (vec.IsFull())
        {TRACE_IT(28845);
            FreeSelf(allocator);
            return CharSetFull::TheFullNode;
        }
        else
            return this;
    }

    CharSetNode* CharSetLeaf::ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h)
    {TRACE_IT(28846);
        Assert(level == 0);
        AssertMsg(h <= lim(level), "The range for clearing provided is invalid for this level.");
        AssertMsg(l <= h, "Can't clear where lower is bigger than the higher.");
        if (l == 0 && h == lim(level))
        {TRACE_IT(28847);
            return nullptr;
        }

        vec.ClearRange(leafIdx(l), leafIdx(h));

        if (vec.IsEmpty())
        {TRACE_IT(28848);
            FreeSelf(allocator);
            return nullptr;
        }

        return this;
    }

    CharSetNode* CharSetLeaf::UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other)
    {TRACE_IT(28849);
        Assert(level == 0);
        Assert(other != nullptr && other->IsLeaf());
        CharSetLeaf* otherLeaf = (CharSetLeaf*)other;
        if (vec.UnionInPlaceFullCheck(otherLeaf->vec))
        {TRACE_IT(28850);
            FreeSelf(allocator);
            return CharSetFull::TheFullNode;
        }
        else
            return this;
    }

    bool CharSetLeaf::Get(uint level, uint k) const
    {TRACE_IT(28851);
        Assert(level == 0);
        return vec.Get(leafIdx(k));
    }

    void CharSetLeaf::ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const
    {TRACE_IT(28852);
        Assert(level == 0);
        vec.ToComplement<char16>(allocator, base, result);
    }

    void CharSetLeaf::ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<char16>& result) const
    {TRACE_IT(28853);
        this->ToEquivClass<char16>(allocator, level, base, tblidx, result);
    }

    void CharSetLeaf::ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const
    {TRACE_IT(28854);
        this->ToEquivClass<codepoint_t>(allocator, level, base, tblidx, result, baseOffset);
    }

    template <typename C>
    void CharSetLeaf::ToEquivClass(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<C>& result, codepoint_t baseOffset) const
    {TRACE_IT(28855);
        Assert(level == 0);
        vec.ToEquivClass<C>(allocator, base, tblidx, result, baseOffset);
    }

    bool CharSetLeaf::IsSubsetOf(uint level, const CharSetNode* other) const
    {TRACE_IT(28856);
        Assert(level == 0);
        Assert(other != nullptr);
        if (other == CharSetFull::TheFullNode)
            return true;
        Assert(other->IsLeaf());
        CharSetLeaf* otherLeaf = (CharSetLeaf*)other;
        return vec.IsSubsetOf(otherLeaf->vec);
    }

    bool CharSetLeaf::IsEqualTo(uint level, const CharSetNode* other) const
    {TRACE_IT(28857);
        Assert(level == 0);
        Assert(other != nullptr);
        if (other == CharSetFull::TheFullNode)
            return false;
        Assert(other->IsLeaf());
        CharSetLeaf* otherLeaf = (CharSetLeaf*)other;
        return vec.IsSubsetOf(otherLeaf->vec);
    }

    uint CharSetLeaf::Count(uint level) const
    {TRACE_IT(28858);
        Assert(level == 0);
        return vec.Count();
    }

    _Success_(return)
    bool CharSetLeaf::GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const
    {TRACE_IT(28859);
        Assert(searchCharStart < lim(level) + 1);
        int nextSet = vec.NextSet(searchCharStart);

        if (nextSet == -1)
        {TRACE_IT(28860);
            return false;
        }

        *outLowerChar = (char16)nextSet;

        int nextClear = vec.NextClear(nextSet);

        *outHigherChar = UTC(nextClear == -1 ? lim(level) : nextClear - 1);

        return true;
    }

#if DBG
    bool CharSetLeaf::IsLeaf() const
    {TRACE_IT(28861);
        return true;
    }
#endif

    // ----------------------------------------------------------------------
    // CharSet<char16>
    // ----------------------------------------------------------------------

    void CharSet<char16>::SwitchRepresentations(ArenaAllocator* allocator)
    {TRACE_IT(28862);
        Assert(IsCompact());
        uint existCount = this->GetCompactLength();
        __assume(existCount <= MaxCompact);
        if (existCount <= MaxCompact)
        {TRACE_IT(28863);
            Char existCs[MaxCompact];
            for (uint i = 0; i < existCount; i++)
            {TRACE_IT(28864);
                existCs[i] = GetCompactChar(i);
            }
            rep.full.root = nullptr;
            rep.full.direct.Clear();
            for (uint i = 0; i < existCount; i++)
                Set(allocator, existCs[i]);
        }
    }

    void CharSet<char16>::Sort()
    {TRACE_IT(28865);
        Assert(IsCompact());
        __assume(this->GetCompactLength() <= MaxCompact);
        for (uint i = 1; i < this->GetCompactLength(); i++)
        {TRACE_IT(28866);
            uint curr = GetCompactCharU(i);
            for (uint j = 0; j < i; j++)
            {TRACE_IT(28867);
                if (GetCompactCharU(j) > curr)
                {TRACE_IT(28868);
                    for (int k = i; k > (int)j; k--)
                    {TRACE_IT(28869);
                        this->ReplaceCompactCharU(k, this->GetCompactCharU(k - 1));
                    }
                    this->ReplaceCompactCharU(j, curr);
                    break;
                }
            }
        }
    }

    CharSet<char16>::CharSet()
    {TRACE_IT(28870);
        Assert(sizeof(Node*) == sizeof(size_t));
        Assert(sizeof(CompactRep) == sizeof(FullRep));
        rep.compact.countPlusOne = 1;
        for (int i = 0; i < MaxCompact; i++)
            rep.compact.cs[i] = emptySlot;
    }

    void CharSet<char16>::FreeBody(ArenaAllocator* allocator)
    {TRACE_IT(28871);
        if (!IsCompact() && rep.full.root != nullptr)
        {TRACE_IT(28872);
            rep.full.root->FreeSelf(allocator);
#if DBG
            rep.full.root = nullptr;
#endif
        }
    }

    void CharSet<char16>::Clear(ArenaAllocator* allocator)
    {TRACE_IT(28873);
        if (!IsCompact() && rep.full.root != nullptr)
            rep.full.root->FreeSelf(allocator);
        rep.compact.countPlusOne = 1;
        for (int i = 0; i < MaxCompact; i++)
            rep.compact.cs[i] = emptySlot;
    }

    void CharSet<char16>::CloneFrom(ArenaAllocator* allocator, const CharSet<Char>& other)
    {TRACE_IT(28874);
        Clear(allocator);
        Assert(IsCompact());
        if (other.IsCompact())
        {TRACE_IT(28875);
            this->SetCompactLength(other.GetCompactLength());
            for (uint i = 0; i < other.GetCompactLength(); i++)
            {TRACE_IT(28876);
                this->ReplaceCompactCharU(i, other.GetCompactCharU(i));
            }
        }
        else
        {TRACE_IT(28877);
            rep.full.root = other.rep.full.root == nullptr ? nullptr : other.rep.full.root->Clone(allocator);
            rep.full.direct.CloneFrom(other.rep.full.direct);
        }
    }

    void CharSet<char16>::CloneNonSurrogateCodeUnitsTo(ArenaAllocator* allocator, CharSet<Char>& other)
    {TRACE_IT(28878);
        if (this->IsCompact())
        {TRACE_IT(28879);
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {TRACE_IT(28880);
                Char c = this->GetCompactChar(i);
                uint uChar = CTU(c);
                if (uChar < 0xD800 || uChar > 0xDFFF)
                {TRACE_IT(28881);
                    other.Set(allocator, c);
                }
            }
        }
        else
        {TRACE_IT(28882);
            other.rep.full.direct.CloneFrom(rep.full.direct);
            if (rep.full.root == nullptr)
            {TRACE_IT(28883);
                other.rep.full.root = nullptr;
            }
            else
            {TRACE_IT(28884);
                other.rep.full.root = rep.full.root->Clone(allocator);
                other.rep.full.root->ClearRange(allocator, CharSetNode::levels - 1, 0xD800, 0XDFFF);
            }
        }
    }

    void CharSet<char16>::CloneSurrogateCodeUnitsTo(ArenaAllocator* allocator, CharSet<Char>& other)
    {TRACE_IT(28885);
        if (this->IsCompact())
        {TRACE_IT(28886);
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {TRACE_IT(28887);
                Char c = this->GetCompactChar(i);
                uint uChar = CTU(c);
                if (0xD800 <= uChar && uChar <= 0xDFFF)
                {TRACE_IT(28888);
                    other.Set(allocator, c);
                }
            }
        }
        else
        {TRACE_IT(28889);
            other.rep.full.direct.CloneFrom(rep.full.direct);
            if (rep.full.root == nullptr)
            {TRACE_IT(28890);
                other.rep.full.root = nullptr;
            }
            else
            {TRACE_IT(28891);
                other.rep.full.root = rep.full.root->Clone(allocator);
                other.rep.full.root->ClearRange(allocator, CharSetNode::levels - 1, 0, 0xD7FF);
            }
        }
    }


    void CharSet<char16>::SubtractRange(ArenaAllocator* allocator, Char lowerChar, Char higherChar)
    {TRACE_IT(28892);
        uint lowerValue = CTU(lowerChar);
        uint higherValue = CTU(higherChar);

        if (higherValue < lowerValue)
            return;

        if (IsCompact())
        {TRACE_IT(28893);
            for (uint i = 0; i < this->GetCompactLength(); )
            {TRACE_IT(28894);
                uint value = this->GetCompactCharU(i);

                if (value >= lowerValue && value <= higherValue)
                {TRACE_IT(28895);
                    this->RemoveCompactChar(i);
                }
                else
                {TRACE_IT(28896);
                    i++;
                }
            }
        }
        else if(lowerValue == 0 && higherValue == MaxUChar)
        {TRACE_IT(28897);
            this->Clear(allocator);
        }
        else
        {TRACE_IT(28898);
            if (lowerValue < CharSetNode::directSize)
            {TRACE_IT(28899);
                uint maxDirectValue = min(higherValue, CharSetNode::directSize - 1);
                rep.full.direct.ClearRange(lowerValue, maxDirectValue);
            }

            if (rep.full.root != nullptr)
            {TRACE_IT(28900);
                rep.full.root = rep.full.root->ClearRange(allocator, CharSetNode::levels - 1, lowerValue, higherValue);
            }
        }
    }

    void CharSet<char16>::SetRange(ArenaAllocator* allocator, Char lc, Char hc)
    {TRACE_IT(28901);
        uint l = CTU(lc);
        uint h = CTU(hc);
        if (h < l)
            return;

        if (IsCompact())
        {TRACE_IT(28902);
            if (h - l < MaxCompact)
            {TRACE_IT(28903);
                do
                {TRACE_IT(28904);
                    uint i;
                    for (i = 0; i < this->GetCompactLength(); i++)
                    {TRACE_IT(28905);
                        __assume(l <= MaxUChar);
                        if (l <= MaxUChar && i < MaxCompact)
                        {TRACE_IT(28906);
                            if (this->GetCompactCharU(i) == l)
                                break;
                        }
                    }
                    if (i == this->GetCompactLength())
                    {TRACE_IT(28907);
                        // Character not already in compact set
                        if (i < MaxCompact)
                        {TRACE_IT(28908);
                            this->AddCompactCharU(l);
                        }
                        else
                            // Must switch representations
                            break;
                    }
                    l++;
                }
                while (l <= h);
                if (h < l)
                    // All chars are now in compact set
                    return;
                // else: fall-through to general case for remaining chars
            }
            // else: no use even trying

            SwitchRepresentations(allocator);
        }

        Assert(!IsCompact());

        if (l == 0 && h == MaxUChar)
        {TRACE_IT(28909);
            rep.full.direct.SetRange(0, CharSetNode::directSize - 1);
            if (rep.full.root != nullptr)
                rep.full.root->FreeSelf(allocator);
            rep.full.root = CharSetFull::TheFullNode;
        }
        else
        {TRACE_IT(28910);
            if (l < CharSetNode::directSize)
            {TRACE_IT(28911);
                if (h < CharSetNode::directSize)
                {TRACE_IT(28912);
                    rep.full.direct.SetRange(l, h);
                    return;
                }
                rep.full.direct.SetRange(l, CharSetNode::directSize - 1);
                l = CharSetNode::directSize;
            }

            if (rep.full.root == nullptr)
                rep.full.root = Anew(allocator, CharSetInner);
            rep.full.root = rep.full.root->Set(allocator, CharSetNode::levels - 1, l, h);
        }
    }

    void CharSet<char16>::SetRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs)
    {TRACE_IT(28913);
        for (int i = 0; i < numSortedPairs * 2; i += 2)
        {TRACE_IT(28914);
            Assert(i == 0 || sortedPairs[i-1] < sortedPairs[i]);
            Assert(sortedPairs[i] <= sortedPairs[i+1]);
            SetRange(allocator, sortedPairs[i], sortedPairs[i+1]);
        }
    }

    void CharSet<char16>::SetNotRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs)
    {TRACE_IT(28915);
        if (numSortedPairs == 0)
            SetRange(allocator, MinChar, MaxChar);
        else
        {TRACE_IT(28916);
            if (sortedPairs[0] != MinChar)
                SetRange(allocator, MinChar, sortedPairs[0] - 1);
            for (int i = 1; i < numSortedPairs * 2 - 1; i += 2)
                SetRange(allocator, sortedPairs[i] + 1, sortedPairs[i+1] - 1);
            if (sortedPairs[numSortedPairs * 2 - 1] != MaxChar)
                SetRange(allocator, sortedPairs[numSortedPairs * 2 - 1] + 1, MaxChar);
        }
    }

    void CharSet<char16>::UnionInPlace(ArenaAllocator* allocator, const CharSet<Char>& other)
    {TRACE_IT(28917);
        if (other.IsCompact())
        {TRACE_IT(28918);
            for (uint i = 0; i < other.GetCompactLength(); i++)
            {
                Set(allocator, other.GetCompactChar(i));
            }
            return;
        }

        if (IsCompact())
            SwitchRepresentations(allocator);

        Assert(!IsCompact() && !other.IsCompact());

        rep.full.direct.UnionInPlace(other.rep.full.direct);

        if (other.rep.full.root != nullptr)
        {TRACE_IT(28919);
            if (other.rep.full.root == CharSetFull::TheFullNode)
            {TRACE_IT(28920);
                if (rep.full.root != nullptr)
                    rep.full.root->FreeSelf(allocator);
                rep.full.root = CharSetFull::TheFullNode;
            }
            else
            {TRACE_IT(28921);
                if (rep.full.root == nullptr)
                    rep.full.root = Anew(allocator, CharSetInner);
                rep.full.root = rep.full.root->UnionInPlace(allocator, CharSetNode::levels - 1, other.rep.full.root);
            }
        }
    }
    _Success_(return)
    bool CharSet<char16>::GetNextRange(Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar)
    {TRACE_IT(28922);
        int count = this->Count();
        if (count == 0)
        {TRACE_IT(28923);
            return false;
        }
        else if (count == 1)
        {TRACE_IT(28924);
            Char singleton = this->Singleton();
            if (singleton < searchCharStart)
            {TRACE_IT(28925);
                return false;
            }

            *outLowerChar = *outHigherChar = singleton;

            return true;
        }

        if (IsCompact())
        {TRACE_IT(28926);
            this->Sort();
            uint i = 0;
            size_t compactLength = this->GetCompactLength();
            for (; i < compactLength; i++)
            {TRACE_IT(28927);
                Char nextChar = this->GetCompactChar(i);
                if (nextChar >= searchCharStart)
                {TRACE_IT(28928);
                    *outLowerChar = *outHigherChar = nextChar;
                    break;
                }
            }

            if (i == compactLength)
            {TRACE_IT(28929);
                return false;
            }

            i++;

            for (; i < compactLength; i++)
            {TRACE_IT(28930);
                Char nextChar = this->GetCompactChar(i);
                if (nextChar != *outHigherChar + 1)
                {TRACE_IT(28931);
                    return true;
                }
                *outHigherChar += 1;
            }

            return true;
        }
        else
        {TRACE_IT(28932);
            bool found = false;
            if (CTU(searchCharStart) < CharSetNode::directSize)
            {TRACE_IT(28933);
                int nextSet = rep.full.direct.NextSet(searchCharStart);

                if (nextSet != -1)
                {TRACE_IT(28934);
                    found = true;

                    *outLowerChar = (char16)nextSet;

                    int nextClear = rep.full.direct.NextClear(nextSet);

                    if (nextClear != -1)
                    {TRACE_IT(28935);
                        *outHigherChar = UTC(nextClear - 1);
                        return true;
                    }

                    *outHigherChar = CharSetNode::directSize - 1;
                }
            }

            if (rep.full.root == nullptr)
            {TRACE_IT(28936);
                return found;
            }
            Char tempLowChar = 0, tempHighChar = 0;

            if (found)
            {TRACE_IT(28937);
                searchCharStart = *outHigherChar + 1;
            }
            else
            {TRACE_IT(28938);
                searchCharStart = searchCharStart > CharSetNode::directSize ? searchCharStart : CharSetNode::directSize;
            }

            if (rep.full.root->GetNextRange(CharSetNode::levels - 1, searchCharStart, &tempLowChar, &tempHighChar) && (!found || tempLowChar == *outHigherChar + 1))
            {TRACE_IT(28939);
                if (!found)
                {TRACE_IT(28940);
                    *outLowerChar = tempLowChar;
                }
                *outHigherChar = tempHighChar;
                return true;
            }

            return found;
        }
    }

    bool CharSet<char16>::Get_helper(uint k) const
    {TRACE_IT(28941);
        Assert(!IsCompact());
        CharSetNode* curr = rep.full.root;
        for (int level = CharSetNode::levels - 1; level > 0; level--)
        {TRACE_IT(28942);
            if (curr == CharSetFull::TheFullNode)
                return true;
            CharSetInner* inner = (CharSetInner*)curr;
            uint i = CharSetNode::innerIdx(level, k);
            if (inner->children[i] == 0)
                return false;
            else
                curr = inner->children[i];
        }
        if (curr == CharSetFull::TheFullNode)
            return true;
        CharSetLeaf* leaf = (CharSetLeaf*)curr;
        return leaf->vec.Get(CharSetNode::leafIdx(k));
    }

    void CharSet<char16>::ToComplement(ArenaAllocator* allocator, CharSet<Char>& result)
    {TRACE_IT(28943);
        if (IsCompact())
        {TRACE_IT(28944);
            Sort();
            if (this->GetCompactLength() > 0)
            {TRACE_IT(28945);
                if (this->GetCompactCharU(0) > 0)
                    result.SetRange(allocator, UTC(0), UTC(this->GetCompactCharU(0) - 1));
                for (uint i = 0; i < this->GetCompactLength() - 1; i++)
                {TRACE_IT(28946);
                    result.SetRange(allocator, UTC(this->GetCompactCharU(i) + 1), UTC(this->GetCompactCharU(i + 1) - 1));
                }
                if (this->GetCompactCharU(this->GetCompactLength() - 1) < MaxUChar)
                {TRACE_IT(28947);
                    result.SetRange(allocator, UTC(this->GetCompactCharU(this->GetCompactLength() - 1) + 1), UTC(MaxUChar));
                }
            }
            else if (this->GetCompactLength() == 0)
            {TRACE_IT(28948);
                result.SetRange(allocator, UTC(0), UTC(MaxUChar));
            }
        }
        else
        {TRACE_IT(28949);
            rep.full.direct.ToComplement<char16>(allocator, 0, result);
            if (rep.full.root == nullptr)
                result.SetRange(allocator, UTC(CharSetNode::directSize), MaxChar);
            else
                rep.full.root->ToComplement(allocator, CharSetNode::levels - 1, 0, result);
        }
    }

    void CharSet<char16>::ToEquivClass(ArenaAllocator* allocator, CharSet<Char>& result)
    {TRACE_IT(28950);
        uint tblidx = 0;
        if (IsCompact())
        {TRACE_IT(28951);
            Sort();
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {TRACE_IT(28952);
                uint acth;
                Char equivs[CaseInsensitive::EquivClassSize];
                if (CaseInsensitive::RangeToEquivClass(tblidx, this->GetCompactCharU(i), this->GetCompactCharU(i), acth, equivs))
                {TRACE_IT(28953);
                    for (int j = 0; j < CaseInsensitive::EquivClassSize; j++)
                    {TRACE_IT(28954);
                        result.Set(allocator, equivs[j]);
                    }
                }
                else
                {TRACE_IT(28955);
                    result.Set(allocator, this->GetCompactChar(i));
                }
            }
        }
        else
        {TRACE_IT(28956);
            rep.full.direct.ToEquivClass<char16>(allocator, 0, tblidx, result);
            if (rep.full.root != nullptr)
            {TRACE_IT(28957);
                rep.full.root->ToEquivClassW(allocator, CharSetNode::levels - 1, 0, tblidx, result);
            }
        }
    }

    void CharSet<char16>::ToEquivClassCP(ArenaAllocator* allocator, CharSet<codepoint_t>& result, codepoint_t baseOffset)
    {TRACE_IT(28958);
        uint tblidx = 0;
        if (IsCompact())
        {TRACE_IT(28959);
            Sort();
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {TRACE_IT(28960);
                uint acth;
                codepoint_t equivs[CaseInsensitive::EquivClassSize];
                if (CaseInsensitive::RangeToEquivClass(tblidx, this->GetCompactCharU(i) + baseOffset, this->GetCompactCharU(i) + baseOffset, acth, equivs))
                {TRACE_IT(28961);
                    for (int j = 0; j < CaseInsensitive::EquivClassSize; j++)
                    {TRACE_IT(28962);
                        result.Set(allocator, equivs[j]);
                    }
                }
                else
                {TRACE_IT(28963);
                    result.Set(allocator, this->GetCompactChar(i) + baseOffset);
                }
            }
        }
        else
        {TRACE_IT(28964);
            rep.full.direct.ToEquivClass<codepoint_t>(allocator, 0, tblidx, result, baseOffset);
            if (rep.full.root != nullptr)
            {TRACE_IT(28965);
                rep.full.root->ToEquivClassCP(allocator, CharSetNode::levels - 1, 0, tblidx, result, baseOffset);
            }
        }
    }

    int CharSet<char16>::GetCompactEntries(uint max, __out_ecount(max) Char* entries) const
    {TRACE_IT(28966);
        Assert(max <= MaxCompact);
        if (!IsCompact())
            return -1;

        uint count = min(max, (uint)(this->GetCompactLength()));
        __analysis_assume(count <= max);
        for (uint i = 0; i < count; i++)
        {TRACE_IT(28967);
            // Bug in oacr. it can't figure out count is less than or equal to max
#pragma warning(suppress: 22102)
            entries[i] = this->GetCompactChar(i);
        }
        return static_cast<int>(rep.compact.countPlusOne - 1);
    }

    bool CharSet<char16>::IsSubsetOf(const CharSet<Char>& other) const
    {TRACE_IT(28968);
        if (IsCompact())
        {TRACE_IT(28969);
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {TRACE_IT(28970);
                if (!other.Get(this->GetCompactChar(i)))
                    return false;
            }
            return true;
        }
        else
        {TRACE_IT(28971);
            if (other.IsCompact())
                return false;
            if (!rep.full.direct.IsSubsetOf(other.rep.full.direct))
                return false;
            if (rep.full.root == nullptr)
                return true;
            if (other.rep.full.root == nullptr)
                return false;
            return rep.full.root->IsSubsetOf(CharSetNode::levels - 1, other.rep.full.root);
        }
    }

    bool CharSet<char16>::IsEqualTo(const CharSet<Char>& other) const
    {TRACE_IT(28972);
        if (IsCompact())
        {TRACE_IT(28973);
            if (!other.IsCompact())
                return false;
            if (rep.compact.countPlusOne != other.rep.compact.countPlusOne)
                return false;
            for (uint i = 0; i < this->GetCompactLength(); i++)
            {TRACE_IT(28974);
                if (!other.Get(this->GetCompactChar(i)))
                    return false;
            }
            return true;
        }
        else
        {TRACE_IT(28975);
            if (other.IsCompact())
                return false;
            if (!rep.full.direct.IsEqualTo(other.rep.full.direct))
                return false;
            if ((rep.full.root == nullptr) != (other.rep.full.root == nullptr))
                return false;
            if (rep.full.root == nullptr)
                return true;
            return rep.full.root->IsEqualTo(CharSetNode::levels - 1, other.rep.full.root);
        }
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    // CAUTION: This method is very slow.
    void CharSet<char16>::Print(DebugWriter* w) const
    {TRACE_IT(28976);
        w->Print(_u("["));
        int start = -1;
        for (uint i = 0; i < NumChars; i++)
        {TRACE_IT(28977);
            if (Get(UTC(i)))
            {TRACE_IT(28978);
                if (start < 0)
                {TRACE_IT(28979);
                    start = i;
                    w->PrintEscapedChar(UTC(i));
                }
            }
            else
            {TRACE_IT(28980);
                if (start >= 0)
                {TRACE_IT(28981);
                    if (i > (uint)(start + 1))
                    {TRACE_IT(28982);
                        if (i  > (uint)(start + 2))
                            w->Print(_u("-"));
                        w->PrintEscapedChar(UTC(i - 1));
                    }
                    start = -1;
                }
            }
        }
        if (start >= 0)
        {TRACE_IT(28983);
            if ((uint)start < MaxUChar - 1)
                w->Print(_u("-"));
            w->PrintEscapedChar(MaxChar);
        }
        w->Print(_u("]"));
    }
#endif

    // ----------------------------------------------------------------------
    // CharSet<codepoint_t>
    // ----------------------------------------------------------------------
    CharSet<codepoint_t>::CharSet()
    {TRACE_IT(28984);
#if DBG
        for (int i = 0; i < NumberOfPlanes; i++)
        {TRACE_IT(28985);
            this->characterPlanes[i].IsEmpty();
        }
#endif
    }

    void CharSet<codepoint_t>::FreeBody(ArenaAllocator* allocator)
    {TRACE_IT(28986);
        for (int i = 0; i < NumberOfPlanes; i++)
        {TRACE_IT(28987);
            this->characterPlanes[i].FreeBody(allocator);
        }
    }

    void CharSet<codepoint_t>::Clear(ArenaAllocator* allocator)
    {TRACE_IT(28988);
        for (int i = 0; i < NumberOfPlanes; i++)
        {TRACE_IT(28989);
            this->characterPlanes[i].Clear(allocator);
        }
    }

    void CharSet<codepoint_t>::CloneFrom(ArenaAllocator* allocator, const CharSet<Char>& other)
    {TRACE_IT(28990);
        for (int i = 0; i < NumberOfPlanes; i++)
        {TRACE_IT(28991);
            this->characterPlanes[i].Clear(allocator);
            this->characterPlanes[i].CloneFrom(allocator, other.characterPlanes[i]);
        }
    }

    void CharSet<codepoint_t>::CloneSimpleCharsTo(ArenaAllocator* allocator, CharSet<char16>& other) const
    {TRACE_IT(28992);
        other.CloneFrom(allocator, this->characterPlanes[0]);
    }

    void CharSet<codepoint_t>::SetRange(ArenaAllocator* allocator, Char lc, Char hc)
    {TRACE_IT(28993);
        Assert(lc <= hc);

        int lowerIndex = this->CharToIndex(lc);
        int upperIndex = this->CharToIndex(hc);

        if (lowerIndex == upperIndex)
        {TRACE_IT(28994);
            this->characterPlanes[lowerIndex].SetRange(allocator, this->RemoveOffset(lc), this->RemoveOffset(hc));
        }
        else
        {TRACE_IT(28995);
            // Do the partial ranges
            char16 partialLower = this->RemoveOffset(lc);
            char16 partialHigher = this->RemoveOffset(hc);

            if (partialLower != 0)
            {TRACE_IT(28996);
                this->characterPlanes[lowerIndex].SetRange(allocator, partialLower, Chars<char16>::MaxUChar);
                lowerIndex++;
            }

            for(; lowerIndex < upperIndex; lowerIndex++)
            {TRACE_IT(28997);
                this->characterPlanes[lowerIndex].SetRange(allocator, 0, Chars<char16>::MaxUChar);
            }

            this->characterPlanes[upperIndex].SetRange(allocator, 0, partialHigher);
        }
    }

    void CharSet<codepoint_t>::SetRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs)
    {TRACE_IT(28998);
        for (int i = 0; i < numSortedPairs * 2; i += 2)
        {TRACE_IT(28999);
            Assert(i == 0 || sortedPairs[i-1] < sortedPairs[i]);
            Assert(sortedPairs[i] <= sortedPairs[i+1]);
            SetRange(allocator, sortedPairs[i], sortedPairs[i+1]);
        }
    }
    void CharSet<codepoint_t>::SetNotRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs)
    {TRACE_IT(29000);
        if (numSortedPairs == 0)
        {TRACE_IT(29001);
            for (int i = 0; i < NumberOfPlanes; i++)
            {TRACE_IT(29002);
                this->characterPlanes[i].SetRange(allocator, 0, Chars<char16>::MaxUChar);
            }
        }
        else
        {TRACE_IT(29003);
            if (sortedPairs[0] != MinChar)
            {
                SetRange(allocator, MinChar, sortedPairs[0] - 1);
            }

            for (int i = 1; i < numSortedPairs * 2 - 1; i += 2)
            {
                SetRange(allocator, sortedPairs[i] + 1, sortedPairs[i+1] - 1);
            }

            if (sortedPairs[numSortedPairs * 2 - 1] != MaxChar)
            {
                SetRange(allocator, sortedPairs[numSortedPairs * 2 - 1] + 1, MaxChar);
            }
        }
    }
    void CharSet<codepoint_t>::UnionInPlace(ArenaAllocator* allocator, const  CharSet<Char>& other)
    {TRACE_IT(29004);
        for (int i = 0; i < NumberOfPlanes; i++)
        {TRACE_IT(29005);
            this->characterPlanes[i].UnionInPlace(allocator, other.characterPlanes[i]);
        }
    }

    void CharSet<codepoint_t>::UnionInPlace(ArenaAllocator* allocator, const  CharSet<char16>& other)
    {TRACE_IT(29006);
        this->characterPlanes[0].UnionInPlace(allocator, other);
    }

    _Success_(return)
    bool CharSet<codepoint_t>::GetNextRange(Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar)
    {TRACE_IT(29007);
        Assert(outLowerChar != nullptr);
        Assert(outHigherChar != nullptr);
        if (searchCharStart >= 0x110000)
        {TRACE_IT(29008);
            return false;
        }

        char16 currentLowChar = 1, currentHighChar = 0;
        int index = this->CharToIndex(searchCharStart);
        char16 offsetLessSearchCharStart = this->RemoveOffset(searchCharStart);

        for (; index < NumberOfPlanes; index++)
        {TRACE_IT(29009);
            if (this->characterPlanes[index].GetNextRange(offsetLessSearchCharStart, &currentLowChar, &currentHighChar))
            {TRACE_IT(29010);
                break;
            }
            offsetLessSearchCharStart = 0x0;
        }

        if (index == NumberOfPlanes)
        {TRACE_IT(29011);
            return false;
        }
        Assert(currentHighChar >= currentLowChar);
        // else found range
        *outLowerChar = this->AddOffset(currentLowChar, index);
        *outHigherChar = this->AddOffset(currentHighChar, index);

        // Check if range crosses plane boundaries
        index ++;
        for (; index < NumberOfPlanes; index++)
        {TRACE_IT(29012);
            if (!this->characterPlanes[index].GetNextRange(0x0, &currentLowChar, &currentHighChar) || *outHigherChar + 1 != this->AddOffset(currentLowChar, index))
            {TRACE_IT(29013);
                break;
            }
            Assert(this->AddOffset(currentHighChar, index) > *outHigherChar);
            *outHigherChar = this->AddOffset(currentHighChar, index);
        }

        return true;
    }

    void CharSet<codepoint_t>::ToComplement(ArenaAllocator* allocator, CharSet<Char>& result)
    {TRACE_IT(29014);
        for (int i = 0; i < NumberOfPlanes; i++)
        {TRACE_IT(29015);
            this->characterPlanes[i].ToComplement(allocator, result.characterPlanes[i]);
        }
    }

    void CharSet<codepoint_t>::ToSimpleComplement(ArenaAllocator* allocator, CharSet<Char>& result)
    {TRACE_IT(29016);
        this->characterPlanes[0].ToComplement(allocator, result.characterPlanes[0]);
    }

    void CharSet<codepoint_t>::ToSimpleComplement(ArenaAllocator* allocator, CharSet<char16>& result)
    {TRACE_IT(29017);
        this->characterPlanes[0].ToComplement(allocator, result);
    }

    void CharSet<codepoint_t>::ToEquivClass(ArenaAllocator* allocator, CharSet<Char>& result)
    {TRACE_IT(29018);
        for (int i = 0; i < NumberOfPlanes; i++)
        {TRACE_IT(29019);
            this->characterPlanes[i].ToEquivClassCP(allocator, result, AddOffset(0, i));
        }
    }

    void CharSet<codepoint_t>::ToSurrogateEquivClass(ArenaAllocator* allocator, CharSet<Char>& result)
    {TRACE_IT(29020);
        this->CloneSimpleCharsTo(allocator, result.characterPlanes[0]);
        for (int i = 1; i < NumberOfPlanes; i++)
        {TRACE_IT(29021);
            this->characterPlanes[i].ToEquivClassCP(allocator, result, AddOffset(0, i));
        }
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void CharSet<codepoint_t>::Print(DebugWriter* w) const
    {TRACE_IT(29022);
        w->Print(_u("Characters 0 - 65535"));

        for (int i = 0; i < NumberOfPlanes; i++)
        {TRACE_IT(29023);
            int base = (i + 1) * 0x10000;
            w->Print(_u("Characters %d - %d"), base, base + 0xFFFF);
            this->characterPlanes[i].Print(w);
        }
    }
#endif

    // ----------------------------------------------------------------------
    // RuntimeCharSet<char16>
    // ----------------------------------------------------------------------

    RuntimeCharSet<char16>::RuntimeCharSet()
    {TRACE_IT(29024);
        root = nullptr;
        direct.Clear();
    }

    void RuntimeCharSet<char16>::FreeBody(ArenaAllocator* allocator)
    {TRACE_IT(29025);
        if (root != nullptr)
        {TRACE_IT(29026);
            root->FreeSelf(allocator);
#if DBG
            root = nullptr;
#endif
        }
    }

    void RuntimeCharSet<char16>::CloneFrom(ArenaAllocator* allocator, const CharSet<Char>& other)
    {TRACE_IT(29027);
        Assert(root == nullptr);
        Assert(direct.Count() == 0);
        if (other.IsCompact())
        {TRACE_IT(29028);
            for (uint i = 0; i < other.GetCompactLength(); i++)
            {TRACE_IT(29029);
                uint k = other.GetCompactCharU(i);
                if (k < CharSetNode::directSize)
                    direct.Set(k);
                else
                {TRACE_IT(29030);
                    if (root == nullptr)
                        root = Anew(allocator, CharSetInner);
#if DBG
                    CharSetNode* newRoot =
#endif
                    root->Set(allocator, CharSetNode::levels - 1, k, k);
#if DBG
                    // NOTE: Since we can add at most MaxCompact characters, we can never fill a leaf or inner node,
                    //       thus we will never need to reallocated nodes
                    Assert(newRoot == root);
#endif
                }
            }
        }
        else
        {TRACE_IT(29031);
            root = other.rep.full.root == nullptr ? nullptr : other.rep.full.root->Clone(allocator);
            direct.CloneFrom(other.rep.full.direct);
        }
    }

    bool RuntimeCharSet<char16>::Get_helper(uint k) const
    {TRACE_IT(29032);
        CharSetNode* curr = root;
        for (int level = CharSetNode::levels - 1; level > 0; level--)
        {TRACE_IT(29033);
            if (curr == CharSetFull::TheFullNode)
                return true;
            CharSetInner* inner = (CharSetInner*)curr;
            uint i = CharSetNode::innerIdx(level, k);
            if (inner->children[i] == 0)
                return false;
            else
                curr = inner->children[i];
        }
        if (curr == CharSetFull::TheFullNode)
            return true;
        CharSetLeaf* leaf = (CharSetLeaf*)curr;
        return leaf->vec.Get(CharSetNode::leafIdx(k));
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    // CAUTION: This method is very slow.
    void RuntimeCharSet<char16>::Print(DebugWriter* w) const
    {TRACE_IT(29034);
        w->Print(_u("["));
        int start = -1;
        for (uint i = 0; i < NumChars; i++)
        {TRACE_IT(29035);
            if (Get(UTC(i)))
            {TRACE_IT(29036);
                if (start < 0)
                {TRACE_IT(29037);
                    start = i;
                    w->PrintEscapedChar(UTC(i));
                }
            }
            else
            {TRACE_IT(29038);
                if (start >= 0)
                {TRACE_IT(29039);
                    if (i > (uint)(start + 1))
                    {TRACE_IT(29040);
                        if (i  > (uint)(start + 2))
                            w->Print(_u("-"));
                        w->PrintEscapedChar(UTC(i - 1));
                    }
                    start = -1;
                }
            }
        }
        if (start >= 0)
        {TRACE_IT(29041);
            if ((uint)start < MaxUChar - 1)
                w->Print(_u("-"));
            w->PrintEscapedChar(MaxChar);
        }
        w->Print(_u("]"));
    }
#endif

    // The VS2013 linker treats this as a redefinition of an already
    // defined constant and complains. So skip the declaration if we're compiling
    // with VS2013 or below.
#if !defined(_MSC_VER) || _MSC_VER >= 1900
    const int  CharSetNode::directBits;
    const uint CharSetNode::directSize;
    const uint CharSetNode::innerMask;
    const int  CharSetNode::bitsPerLeafLevel;
    const int  CharSetNode::branchingPerLeafLevel;
    const uint CharSetNode::leafMask;
    const uint CharSetNode::levels;
#endif
}
