//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace UnifiedRegex
{
    template <typename C>
    class CharSet;

    template <typename C>
    class RuntimeCharSet;

    class CharBitvec : private Chars<char>
    {
    public:
        static const int Width = Chars<char>::CharWidth;
        static const int Size = NumChars;

    private:
        static const int wordSize = sizeof(uint32) * 8;
        static const int vecSize = Size / wordSize;
        static const uint32 ones = (uint32)-1;

        static const uint8 oneBits[Size];

        uint32 vec[vecSize];

        inline static void setrng(uint32 &v, uint l, uint h)
        {TRACE_IT(29042);
            uint w = h - l + 1;
            if (w == wordSize)
                v = ones;
            else
                v |= ((1U << w) - 1) << l;
        }

        inline static void clearrng(uint32 &v, uint l, uint h)
        {TRACE_IT(29043);
            uint w = h - l + 1;
            if (w == wordSize)
                v = 0;
            else
                v &= ~(((1U << w) - 1) << l);
        }

    public:
        inline void CloneFrom(const CharBitvec& other)
        {TRACE_IT(29044);
            for (int w = 0; w < vecSize; w++)
                vec[w] = other.vec[w];
        }

        inline void Clear()
        {TRACE_IT(29045);
            for (int w = 0; w < vecSize; w++)
                vec[w] = 0;
        }

        inline void SetAll()
        {TRACE_IT(29046);
            for (int w = 0; w < vecSize; w++)
                vec[w] = ones;
        }

        inline void Set(uint k)
        {TRACE_IT(29047);
            Assert(k < Size);
            __assume(k < Size);
            if (k < Size)
                vec[k / wordSize] |= 1U << (k % wordSize);
        }

        inline void SetRange(uint l, uint h)
        {TRACE_IT(29048);
            Assert(l < Size);
            Assert(h < Size);
            __assume(l < Size);
            __assume(h < Size);
            if  (l < Size && h < Size)
            {TRACE_IT(29049);
                if (l == h)
                    vec[l / wordSize] |= 1U << (l % wordSize);
                else if (l < h)
                {TRACE_IT(29050);
                    int lw = l / wordSize;
                    int hw = h / wordSize;
                    int lo = l % wordSize;
                    int hio = h % wordSize;
                    if (lw == hw)
                        setrng(vec[lw], lo, hio);
                    else
                    {TRACE_IT(29051);
                        setrng(vec[lw], lo, wordSize-1);
                        for (int w = lw + 1; w < hw; w++)
                            vec[w] = ones;
                        setrng(vec[hw], 0, hio);
                    }
                }
            }
        }

        inline void ClearRange(uint l, uint h)
        {TRACE_IT(29052);
            Assert(l < Size);
            Assert(h < Size);
            __assume(l < Size);
            __assume(h < Size);
            if  (l < Size && h < Size)
            {TRACE_IT(29053);
                if (l == h)
                {TRACE_IT(29054);
                    vec[l / wordSize] &= ~(1U << (l % wordSize));
                }
                else if (l < h)
                {TRACE_IT(29055);
                    int lw = l / wordSize;
                    int hw = h / wordSize;
                    int lo = l % wordSize;
                    int hio = h % wordSize;
                    if (lw == hw)
                    {TRACE_IT(29056);
                        clearrng(vec[lw], lo, hio);
                    }
                    else
                    {TRACE_IT(29057);
                        clearrng(vec[lw], lo, wordSize-1);
                        for (int w = lw + 1; w < hw; w++)
                            vec[w] = 0;
                        clearrng(vec[hw], 0, hio);
                    }
                }
            }
        }

        inline bool IsEmpty()
        {TRACE_IT(29058);
            for (int i = 0; i < vecSize; i++)
            {TRACE_IT(29059);
                if(vec[i] != 0)
                {TRACE_IT(29060);
                    return false;
                }
            }
            return true;
        }

        inline void UnionInPlace(const CharBitvec& other)
        {TRACE_IT(29061);
            for (int w = 0; w < vecSize; w++)
                vec[w] |= other.vec[w];
        }

        inline bool UnionInPlaceFullCheck(const CharBitvec& other)
        {TRACE_IT(29062);
            bool isFull = true;
            for (int w = 0; w < vecSize; w++)
            {TRACE_IT(29063);
                vec[w] |= other.vec[w];
                if (vec[w] != ones)
                    isFull = false;
            }
            return isFull;
        }

        inline bool Get(uint k) const
        {TRACE_IT(29064);
            Assert(k < Size);
            __assume(k < Size);
            return ((vec[k / wordSize] >> (k % wordSize)) & 1) != 0;
        }

        inline bool IsFull() const
        {TRACE_IT(29065);
            for (int w = 0; w < vecSize; w++)
            {TRACE_IT(29066);
                if (vec[w] != ones)
                    return false;
            }
            return true;
        }

        inline bool IsSubsetOf(const CharBitvec& other) const
        {TRACE_IT(29067);
            for (int w = 0; w < vecSize; w++)
            {TRACE_IT(29068);
                uint32 v = other.vec[w];
                if (v != (vec[w] | v))
                    return false;
            }
            return true;
        }

        inline bool IsEqualTo(const CharBitvec& other) const
        {TRACE_IT(29069);
            for (int w = 0; w < vecSize; w++)
            {TRACE_IT(29070);
                if (vec[w] != other.vec[w])
                    return false;
            }
            return true;
        }

        uint Count() const;

        int NextSet(int k) const;
        int NextClear(int k) const;

        template <typename C>
        void ToComplement(ArenaAllocator* allocator, uint base, CharSet<C>& result) const;

        template <typename C>
        void ToEquivClass(ArenaAllocator* allocator, uint base, uint& tblidx, CharSet<C>& result, codepoint_t baseOffset = 0x0) const;
    };

    template <typename C>
    class CharSet {};

    struct CharSetNode : protected Chars<char16>
    {
        static const int directBits = Chars<char>::CharWidth;
        static const uint directSize = Chars<char>::NumChars;

        static const uint bitsPerInnerLevel = 4;
        static const uint branchingPerInnerLevel = 1 << bitsPerInnerLevel;
        static const uint innerMask = branchingPerInnerLevel - 1;

        static const int bitsPerLeafLevel = CharBitvec::Width;
        static const int branchingPerLeafLevel = CharBitvec::Size;
        static const uint leafMask = branchingPerLeafLevel - 1;

        static const uint levels = 1 + (CharWidth - bitsPerLeafLevel) / bitsPerInnerLevel;
        
        inline static uint innerIdx(uint level, uint v)
        {TRACE_IT(29071);
            return (v >> ((level + 1) * bitsPerInnerLevel)) & innerMask;
        }

        inline static uint indexToValue(uint level, uint index, uint offset)
        {TRACE_IT(29072);
            Assert((index & innerMask) == index);
            Assert((uint)(1 << ((level + 1) * bitsPerInnerLevel)) > offset);

            return (index << ((level + 1) * bitsPerInnerLevel)) + offset;
        }

        inline static uint leafIdx(uint v)
        {TRACE_IT(29073);
            return v & leafMask;
        }

        inline static uint lim(uint level)
        {TRACE_IT(29074);
            return (1U << (bitsPerLeafLevel + level * bitsPerInnerLevel)) - 1;
        }

        inline static uint remain(uint level, uint v)
        {TRACE_IT(29075);
            return v & lim(level);
        }

        virtual void FreeSelf(ArenaAllocator* allocator) = 0;
        virtual CharSetNode* Clone(ArenaAllocator* allocator) const = 0;
        virtual CharSetNode* Set(ArenaAllocator* allocator, uint level, uint l, uint h) = 0;
        virtual CharSetNode* ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h) = 0;
        virtual CharSetNode* UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other) = 0;
        virtual bool Get(uint level, uint k) const = 0;
        virtual void ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const = 0;
        virtual void ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<char16>& result) const = 0;
        virtual void ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const = 0;
        virtual bool IsSubsetOf(uint level, const CharSetNode* other) const = 0;
        virtual bool IsEqualTo(uint level, const CharSetNode* other) const = 0;
        virtual uint Count(uint level) const = 0;
        _Success_(return) virtual bool GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const = 0;
#if DBG
        virtual bool IsLeaf() const = 0;
#endif

        static CharSetNode* For(ArenaAllocator* allocator, int level);
    };

    struct CharSetFull : CharSetNode
    {
    private:
        template <typename C>
        void ToEquivClass(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<C>& result, codepoint_t baseOffset = 0x0) const;
    public:
        static CharSetFull Instance;
        static CharSetFull* const TheFullNode;

        CharSetFull();

        void FreeSelf(ArenaAllocator* allocator) override;
        CharSetNode* Clone(ArenaAllocator* allocator) const override;
        CharSetNode* Set(ArenaAllocator* allocator, uint level, uint l, uint h) override;
        CharSetNode* ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h) override;
        CharSetNode* UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other) override;
        bool Get(uint level, uint k) const override;
        void ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const override;
        void ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<char16>& result) const override;
        void ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const override;
        bool IsSubsetOf(uint level, const CharSetNode* other) const override;
        bool IsEqualTo(uint level, const CharSetNode* other) const override;
        uint Count(uint level) const override;
        _Success_(return) bool GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const override;
#if DBG
        bool IsLeaf() const override;
#endif
    };


    struct CharSetInner sealed : CharSetNode
    {
    private:
        template <typename C>
        void ToEquivClass(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<C>& result) const;
    public:
        CharSetNode* children[branchingPerInnerLevel];

        CharSetInner();
        void FreeSelf(ArenaAllocator* allocator) override;
        CharSetNode* Clone(ArenaAllocator* allocator) const override;
        CharSetNode* Set(ArenaAllocator* allocator, uint level, uint l, uint h) override;
        CharSetNode* ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h) override;
        CharSetNode* UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other) override;
        bool Get(uint level, uint k) const override;
        void ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const override;\
        void ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<char16>& result) const override;
        void ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const override;
        bool IsSubsetOf(uint level, const CharSetNode* other) const override;
        bool IsEqualTo(uint level, const CharSetNode* other) const override;
        uint Count(uint level) const override;
        _Success_(return) bool GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const override;
#if DBG
        bool IsLeaf() const override;
#endif
    };

    struct CharSetLeaf sealed: CharSetNode
    {
    private:
        template <typename C>
        void ToEquivClass(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<C>& result, codepoint_t baseOffset = 0x0) const;
    public:
        CharBitvec vec;

        CharSetLeaf();
        void FreeSelf(ArenaAllocator* allocator) override;
        CharSetNode* Clone(ArenaAllocator* allocator) const override;
        CharSetNode* Set(ArenaAllocator* allocator, uint level, uint l, uint h) override;
        CharSetNode* ClearRange(ArenaAllocator* allocator, uint level, uint l, uint h) override;
        CharSetNode* UnionInPlace(ArenaAllocator* allocator, uint level, const CharSetNode* other) override;
        bool Get(uint level, uint k) const override;
        void ToComplement(ArenaAllocator* allocator, uint level, uint base, CharSet<Char>& result) const override;
        void ToEquivClassW(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<char16>& result) const override;
        void ToEquivClassCP(ArenaAllocator* allocator, uint level, uint base, uint& tblidx, CharSet<codepoint_t>& result, codepoint_t baseOffset) const override;
        bool IsSubsetOf(uint level, const CharSetNode* other) const override;
        bool IsEqualTo(uint level, const CharSetNode* other) const override;
        uint Count(uint level) const override;
        _Success_(return) bool GetNextRange(uint level, Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar) const override;
#if DBG
        bool IsLeaf() const override;
#endif
    };

    template <>
    class CharSet<char16> : private Chars<char16>
    {
    public:
        static const uint MaxCompact = 4;

        static const uint emptySlot = (uint)-1;

        struct CompactRep
        {
            // 1 + number of distinct characters, 1..MaxCompact+1
            size_t countPlusOne;
            // Characters, in no particular order, or (uint)-1 for tail empty slots
            uint cs[MaxCompact];
            uint8 padding[sizeof(CharBitvec) - sizeof(uint) * MaxCompact];
        };

        struct FullRep
        {
            // Trie for remaining characters. Pointer value will be 0 or >> MaxCompact.
            CharSetNode* root;
            // Entries for first 256 characters
            CharBitvec direct;
        };

        union Rep
        {
            struct CompactRep compact;
            struct FullRep full;
        } rep;


        static const int compactSize = sizeof(CompactRep);
        static const int fullSize = sizeof(FullRep);

        inline bool IsCompact() const {TRACE_IT(29076); return rep.compact.countPlusOne - 1 <= MaxCompact; }
        void SwitchRepresentations(ArenaAllocator* allocator);
        void Sort();

    public:
        CharSet();
        void FreeBody(ArenaAllocator* allocator);
        void Clear(ArenaAllocator* allocator);
        void CloneFrom(ArenaAllocator* allocator, const CharSet<Char>& other);
        void CloneNonSurrogateCodeUnitsTo(ArenaAllocator* allocator, CharSet<Char>& other);
        void CloneSurrogateCodeUnitsTo(ArenaAllocator* allocator, CharSet<Char>& other);

        inline void Set(ArenaAllocator* allocator, Char kc) { SetRange(allocator, kc, kc); }

        void SetRange(ArenaAllocator* allocator, Char lc, Char hc);
        void SubtractRange(ArenaAllocator* allocator, Char lc, Char hc);
        void SetRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs);
        void SetNotRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs);
        void UnionInPlace(ArenaAllocator* allocator, const  CharSet<Char>& other);
        _Success_(return) bool GetNextRange(Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar);
        bool Get_helper(uint k) const;

        inline bool Get(Char kc) const
        {TRACE_IT(29077);
            if (IsCompact())
            {TRACE_IT(29078);
                Assert(MaxCompact == 4);
                return rep.compact.cs[0] == CTU(kc) ||
                       rep.compact.cs[1] == CTU(kc) ||
                       rep.compact.cs[2] == CTU(kc) ||
                       rep.compact.cs[3] == CTU(kc);
            }
            else
            {TRACE_IT(29079);
                if (CTU(kc) < CharSetNode::directSize)
                    return rep.full.direct.Get(CTU(kc));
                else if (rep.full.root == 0)
                    return false;
                else
                    return Get_helper(CTU(kc));
            }
        }

        inline bool IsEmpty() const
        {TRACE_IT(29080);
            return rep.compact.countPlusOne == 1;
        }

        inline bool IsSingleton() const
        {TRACE_IT(29081);
            return rep.compact.countPlusOne == 2;
        }

        // Helpers to clean up the code

        inline uint GetCompactLength() const
        {TRACE_IT(29082);
            Assert(IsCompact());
            return (uint)(rep.compact.countPlusOne - 1u);
        }

        inline void SetCompactLength(size_t length)
        {TRACE_IT(29083);
            rep.compact.countPlusOne = length + 1;
        }

        inline uint GetCompactCharU(uint index) const
        {TRACE_IT(29084);
            Assert(index < this->GetCompactLength());
            Assert(IsCompact());
            Assert(rep.compact.cs[index] <= MaxUChar);
            return rep.compact.cs[index];
        }

        inline Char GetCompactChar(uint index) const
        {TRACE_IT(29085);
            return (Char)(GetCompactCharU(index));
        }

        //Replaces an existing character with a new value
        inline void ReplaceCompactCharU(uint index, uint value)
        {TRACE_IT(29086);
            Assert(index < this->GetCompactLength());
            Assert(IsCompact());
            Assert(value <= MaxUChar);
            rep.compact.cs[index] = value;
        }

        inline void ClearCompactChar(uint index)
        {TRACE_IT(29087);
            Assert(index < this->GetCompactLength());
            Assert(IsCompact());
            rep.compact.cs[index] = emptySlot;
        }

        // Adds the character to the end, assuming there is enough space. (Assert in place)
        // Increments count.
        inline void AddCompactCharU(uint value)
        {TRACE_IT(29088);
            Assert(this->GetCompactLength() < MaxCompact);
            Assert(IsCompact());
            rep.compact.cs[this->GetCompactLength()] = value;
            rep.compact.countPlusOne += 1;
        }

        // Adds the character to the end, assuming there is enough space. (Assert in place)
        // Increments count.
        inline void AddCompactChar(Char value)
        {TRACE_IT(29089);
            AddCompactCharU((Char)(value));
        }

        // This performs a check to see if the index is the last char, if so sets it to emptySlot
        // If not, replaces it with last index.
        inline void RemoveCompactChar(uint index)
        {TRACE_IT(29090);
            Assert(index < this->GetCompactLength());
            Assert(IsCompact());

            if (index == this->GetCompactLength() - 1)
            {TRACE_IT(29091);
                this->ClearCompactChar(index);
            }
            else
            {TRACE_IT(29092);
                this->ReplaceCompactCharU(index, this->GetCompactCharU((uint)this->GetCompactLength() - 1));
            }

            rep.compact.countPlusOne -= 1;
        }

        inline char16 Singleton() const
        {TRACE_IT(29093);
            Assert(IsSingleton());
            Assert(rep.compact.cs[0] <= MaxUChar);
            return UTC(rep.compact.cs[0]);
        }

        int GetCompactEntries(uint max, __out_ecount(max) Char* entries) const;

        bool IsSubsetOf(const CharSet<Char>& other) const;
        bool IsEqualTo(const CharSet<Char>& other) const;

        inline uint Count() const
        {TRACE_IT(29094);
            if (IsCompact())
                return (uint)rep.compact.countPlusOne - 1;
            else if (rep.full.root == 0)
                return rep.full.direct.Count();
            else
            {TRACE_IT(29095);
                //The bit vector
                Assert(rep.full.root == CharSetFull::TheFullNode || rep.full.root->Count(CharSetNode::levels - 1) <= 0xFF00);
                return rep.full.direct.Count() + (rep.full.root == CharSetFull::TheFullNode ? 0xFF00 : rep.full.root->Count(CharSetNode::levels - 1));
            }
        }

        // NOTE: These are not 'const' methods since they may sort the compact representation internally
        void ToComplement(ArenaAllocator* allocator, CharSet<Char>& result);
        void ToEquivClass(ArenaAllocator* allocator, CharSet<Char>& result);
        void ToEquivClassCP(ArenaAllocator* allocator, CharSet<codepoint_t>& result, codepoint_t baseOffset);
#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif
    };

    template <>
    class CharSet<codepoint_t> : private Chars<codepoint_t>
    {
        static const int NumberOfPlanes = 17;
    private:
        // Character planes are composed of 65536 characters each.
        // First plane is the Basic Multilingual Plane (characters 0 - 65535)
        // Every subsequent plane also stores characters in the form [0 - 65535]; to get the actual value, add 'index * 0x10000' to it
        CharSet<char16> characterPlanes [NumberOfPlanes];

        // Takes a character, and returns the index of the CharSet<char16> that holds it.
        inline int CharToIndex(Char c) const
        {TRACE_IT(29096);
            Assert(c <= Chars<codepoint_t>::MaxUChar);
            return (int)(CTU(c) / (Chars<char16>::MaxUChar + 1));
        }

        // Takes a character, and removes the offset to make it < 0x10000
        inline char16 RemoveOffset(Char c) const
        {TRACE_IT(29097);
            Assert(c <= Chars<codepoint_t>::MaxUChar);
            return (char16)(CTU(c) % 0x10000);
        }

        // Takes a character, and removes the offset to make it < 0x10000
        inline Char AddOffset(char16 c, int index) const
        {TRACE_IT(29098);
            Assert(c <= Chars<char16>::MaxUChar);
            Assert(index >= 0);
            Assert(index < NumberOfPlanes);
            return (Char)(c) + 0x10000 * index;
        }

    public:
        CharSet();
        void FreeBody(ArenaAllocator* allocator);
        void Clear(ArenaAllocator* allocator);
        void CloneFrom(ArenaAllocator* allocator, const CharSet<Char>& other);
        void CloneSimpleCharsTo(ArenaAllocator* allocator, CharSet<char16>& other) const;

        inline void CloneNonSurrogateCodeUnitsTo(ArenaAllocator* allocator, CharSet<char16>& other)
        {TRACE_IT(29099);
            Assert(this->SimpleCharCount() > 0);
            AssertMsg(this->ContainSurrogateCodeUnits(), "This doesn't contain surrogate code units, a simple clone is faster.");
            this->characterPlanes[0].CloneNonSurrogateCodeUnitsTo(allocator, other);
        }

        inline void CloneSurrogateCodeUnitsTo(ArenaAllocator* allocator, CharSet<char16>& other)
        {TRACE_IT(29100);
            Assert(this->SimpleCharCount() > 0);
            AssertMsg(this->ContainSurrogateCodeUnits(), "This doesn't contain surrogate code units, will not produce any result.");
            this->characterPlanes[0].CloneSurrogateCodeUnitsTo(allocator, other);
        }

        inline void Set(ArenaAllocator* allocator, Char kc) { SetRange(allocator, kc, kc); }

        inline bool ContainSurrogateCodeUnits()
        {TRACE_IT(29101);
            char16 outLower = 0xFFFF, ignore = 0x0;
            return this->characterPlanes[0].GetNextRange(0xD800, &outLower, &ignore) ? outLower <= 0xDFFF : false;
        }

        void SetRange(ArenaAllocator* allocator, Char lc, Char hc);
        void SetRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs);
        void SetNotRanges(ArenaAllocator* allocator, int numSortedPairs, const Char* sortedPairs);
        void UnionInPlace(ArenaAllocator* allocator, const  CharSet<Char>& other);
        void UnionInPlace(ArenaAllocator* allocator, const  CharSet<char16>& other);
        _Success_(return) bool GetNextRange(Char searchCharStart, _Out_ Char *outLowerChar, _Out_ Char *outHigherChar);

        inline bool Get(Char kc) const
        {TRACE_IT(29102);
            return this->characterPlanes[CharToIndex(kc)].Get(RemoveOffset(kc));
        }

        inline bool IsEmpty() const
        {TRACE_IT(29103);
            for (int i = 0; i < NumberOfPlanes; i++)
            {TRACE_IT(29104);
                if (!this->characterPlanes[i].IsEmpty())
                {TRACE_IT(29105);
                    return false;
                }
            }

            return true;
        }

        inline bool IsSimpleCharASingleton() const
        {TRACE_IT(29106);
            return this->characterPlanes[0].IsSingleton();
        }

        inline char16 SimpleCharSingleton() const
        {TRACE_IT(29107);
            return this->characterPlanes[0].Singleton();
        }

        inline bool IsSingleton() const
        {TRACE_IT(29108);
            return this->Count() == 1;
        }

        inline codepoint_t Singleton() const
        {TRACE_IT(29109);
            Assert(IsSingleton());

            for (int i = 0; i < NumberOfPlanes; i++)
            {TRACE_IT(29110);
                if (this->characterPlanes[i].IsSingleton())
                {TRACE_IT(29111);
                    return AddOffset(this->characterPlanes[i].Singleton(), i);
                }
            }

            AssertMsg(false, "Should not reach here, first Assert verifies we are a singleton.");
            return INVALID_CODEPOINT;
        }

        bool IsSubsetOf(const CharSet<Char>& other) const;
        bool IsEqualTo(const CharSet<Char>& other) const;

        inline uint Count() const
        {TRACE_IT(29112);
            uint totalCount = 0;

            for (int i = 0; i < NumberOfPlanes; i++)
            {TRACE_IT(29113);
                totalCount += this->characterPlanes[i].Count();
            }

            return totalCount;
        }

        inline uint SimpleCharCount() const
        {TRACE_IT(29114);
            return this->characterPlanes[0].Count();
        }

        // NOTE: These are not 'const' methods since they may sort the compact representation internally
        void ToComplement(ArenaAllocator* allocator, CharSet<Char>& result);
        void ToSimpleComplement(ArenaAllocator* allocator, CharSet<codepoint_t>& result);
        void ToSimpleComplement(ArenaAllocator* allocator, CharSet<char16>& result);
        void ToEquivClass(ArenaAllocator* allocator, CharSet<Char>& result);
        void ToSurrogateEquivClass(ArenaAllocator* allocator, CharSet<Char>& result);
#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif
    };


    template <>
    class RuntimeCharSet<char16> : private Chars<char16>
    {
    private:
        // Trie for remaining characters. Pointer value will be 0 or >> MaxCompact.
        CharSetNode* root;
        // Entries for first 256 characters
        CharBitvec direct;

    public:
        RuntimeCharSet();
        void FreeBody(ArenaAllocator* allocator);
        void CloneFrom(ArenaAllocator* allocator, const CharSet<Char>& other);
        bool Get_helper(uint k) const;

        inline bool Get(Char kc) const
        {TRACE_IT(29115);
            if (CTU(kc) < CharSetNode::directSize)
                return direct.Get(CTU(kc));
            else if (root == 0)
                return false;
            else
                return Get_helper(CTU(kc));
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif
    };

    typedef CharSet<char16> UnicodeCharSet;
    typedef RuntimeCharSet<char16> UnicodeRuntimeCharSet;
}
