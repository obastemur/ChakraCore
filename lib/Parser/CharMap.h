//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace UnifiedRegex
{
    enum CharMapScheme
    {
         CharMapScheme_Linear,
         CharMapScheme_Full
    };
    template <typename C, typename V, CharMapScheme scheme = CharMapScheme_Full>
    class CharMap {};

    template <typename V, CharMapScheme scheme>
    class CharMap<char, V, scheme> : private Chars<char>
    {
    private:
        V map[NumChars];

    public:
        CharMap(V defv)
        {TRACE_IT(28699);
            for (int i = 0; i < NumChars; i++)
                map[i] = defv;
        }

        void FreeBody(ArenaAllocator* allocator)
        {TRACE_IT(28700);
        }

        inline void Set(ArenaAllocator* allocator, Char k, V v)
        {TRACE_IT(28701);
            map[CTU(k)] = v;
        }

        inline V Get(Char k) const
        {TRACE_IT(28702);
            return map[CTU(k)];
        }
    };

    static const uint MaxCharMapLinearChars = 4;

    template <typename V>
    class CharMap<char16, V, CharMapScheme_Linear> : private Chars<char16>
    {
        template <typename C>
        friend class TextbookBoyerMooreWithLinearMap;
        using typename Chars<char16>::Char;
        using Chars<char16>::CTU;


    private:
        V defv;
        uint map[MaxCharMapLinearChars];
        V lastOcc[MaxCharMapLinearChars];

    public:
        CharMap(V defv) : defv(defv)
        {TRACE_IT(28703);
            for (uint i = 0; i < MaxCharMapLinearChars; i++)
            {TRACE_IT(28704);
                map[i] = 0;
                lastOcc[i] = defv;
            }
        }

        inline void Set(uint numLinearChars, Char const * map, V const * lastOcc)
        {TRACE_IT(28705);
            Assert(numLinearChars <= MaxCharMapLinearChars);
            for (uint i = 0; i < numLinearChars; i++)
            {TRACE_IT(28706);
                this->map[i] = CTU(map[i]);
                this->lastOcc[i] = lastOcc[i];
            }
        }

        uint GetChar(uint index) const
        {TRACE_IT(28707);
            Assert(index < MaxCharMapLinearChars);
            __analysis_assume(index < MaxCharMapLinearChars);
            return map[index];
        }

        V GetLastOcc(uint index) const
        {TRACE_IT(28708);
            Assert(index < MaxCharMapLinearChars);
            __analysis_assume(index < MaxCharMapLinearChars);
            return lastOcc[index];
        }

        inline V Get(uint inputChar) const
        {TRACE_IT(28709);
            if (map[0] == inputChar)
                return lastOcc[0];
            if (map[1] == inputChar)
                return lastOcc[1];
            if (map[2] == inputChar)
                return lastOcc[2];
            if (map[3] == inputChar)
                return lastOcc[3];
            return defv;
        }

        inline V Get(Char k) const
        {TRACE_IT(28710);
            return Get(CTU(k));
        }
    };


    template <typename V, CharMapScheme scheme>
    class CharMap<char16, V, scheme> : private Chars<char16>
    {
//    public:
        using Chars<char16>::Char;
        using Chars<char16>::CharWidth;
        using Chars<char16>::CTU;
    private:
        static const int directBits = Chars<char>::CharWidth;
        static const int directSize = Chars<char>::NumChars;
        static const int bitsPerLevel = 4;
        static const int branchingPerLevel = 1 << bitsPerLevel;
        static const uint mask = branchingPerLevel - 1;
        static const int levels = CharWidth / bitsPerLevel;

        inline static uint innerIdx(int level, uint v)
        {TRACE_IT(28711);
            return (v >> (level * bitsPerLevel)) & mask;
        }

        inline static uint leafIdx(uint v)
        {TRACE_IT(28712);
            return v & mask;
        }

        struct Node
        {
            virtual void FreeSelf(ArenaAllocator* allocator) = 0;
            virtual void Set(ArenaAllocator* allocator, V defv, int level, uint k, V v) = 0;
            virtual V Get(V defv, int level, uint k) const = 0;

            static inline Node* For(ArenaAllocator* allocator, int level, V defv)
            {TRACE_IT(28713);
                if (level == 0)
                    return Anew(allocator, Leaf, defv);
                else
                    return Anew(allocator, Inner);
            }
        };

        struct Inner : Node
        {
            Node* children[branchingPerLevel];

            Inner()
            {TRACE_IT(28714);
                for (int i = 0; i < branchingPerLevel; i++)
                    children[i] = 0;
            }

            void FreeSelf(ArenaAllocator* allocator) override
            {
                for (int i = 0; i < branchingPerLevel; i++)
                {TRACE_IT(28715);
                    if (children[i] != 0)
                    {TRACE_IT(28716);
                        children[i]->FreeSelf(allocator);
#if DBG
                        children[i] = 0;
#endif
                    }
                }
                Adelete(allocator, this);
            }

            void Set(ArenaAllocator* allocator, V defv, int level, uint k, V v) override
            {
                Assert(level > 0);
                uint i = innerIdx(level--, k);
                if (children[i] == 0)
                {TRACE_IT(28717);
                    if (v == defv)
                        return;
                    children[i] = Node::For(allocator, level, defv);
                }
                children[i]->Set(allocator, defv, level, k, v);
            }

            V Get(V defv, int level, uint k) const override
            {
                Assert(level > 0);
                uint i = innerIdx(level--, k);
                if (children[i] == 0)
                    return defv;
                else
                    return children[i]->Get(defv, level, k);
            }
        };

        struct Leaf : Node
        {
            V values[branchingPerLevel];

            Leaf(V defv)
            {TRACE_IT(28718);
                for (int i = 0; i < branchingPerLevel; i++)
                    values[i] = defv;
            }

            void FreeSelf(ArenaAllocator* allocator) override
            {
                Adelete(allocator, this);
            }

            void Set(ArenaAllocator* allocator, V defv, int level, uint k, V v) override
            {
                Assert(level == 0);
                values[leafIdx(k)] = v;
            }

            V Get(V defv, int level, uint k) const override
            {
                Assert(level == 0);
                return values[leafIdx(k)];
            }
        };

        Field(BVStatic<directSize>) isInMap;
        Field(V) defv;
        Field(V) directMap[directSize];
        FieldNoBarrier(Node*) root;

    public:
        CharMap(V defv)
            : defv(defv)
            , root(0)
        {TRACE_IT(28719);
            for (int i = 0; i < directSize; i++)
                directMap[i] = defv;
        }

        void FreeBody(ArenaAllocator* allocator)
        {TRACE_IT(28720);
            if (root != 0)
            {TRACE_IT(28721);
                root->FreeSelf(allocator);
#if DBG
                root = 0;
#endif
            }
        }

        void Set(ArenaAllocator* allocator, Char kc, V v)
        {TRACE_IT(28722);
            uint k = CTU(kc);
            if (k < directSize)
            {TRACE_IT(28723);
                isInMap.Set(k);
                directMap[k] = v;
            }
            else
            {TRACE_IT(28724);
                if (root == 0)
                {TRACE_IT(28725);
                    if (v == defv)
                        return;
                    root = Anew(allocator, Inner);
                }
                root->Set(allocator, defv, levels - 1, k, v);
            }
        }

        bool GetNonDirect(uint k, V& lastOcc) const
        {TRACE_IT(28726);
            Assert(k >= directSize);
            if (root == nullptr)
            {TRACE_IT(28727);
                return false;
            }
            Node* curr = root;
            for (int level = levels - 1; level > 0; level--)
            {TRACE_IT(28728);
                Inner* inner = (Inner*)curr;
                uint i = innerIdx(level, k);
                if (inner->children[i] == 0)
                    return false;
                else
                    curr = inner->children[i];
            }
            Leaf* leaf = (Leaf*)curr;
            lastOcc = leaf->values[leafIdx(k)];
            return true;
        }

        uint GetDirectMapSize() const {TRACE_IT(28729); return directSize; }
        BOOL IsInDirectMap(uint c) const {TRACE_IT(28730); Assert(c < directSize); return isInMap.Test(c); }
        V GetDirectMap(uint c) const
        {TRACE_IT(28731);
            Assert(c < directSize);
            __analysis_assume(c < directSize);
            return directMap[c];
        }
        inline V Get(Char kc) const
        {TRACE_IT(28732);
            if (CTU(kc) < GetDirectMapSize())
            {TRACE_IT(28733);
                if (!IsInDirectMap(CTU(kc)))
                {TRACE_IT(28734);
                    return defv;
                }
                return GetDirectMap(CTU(kc));
            }
            else
            {TRACE_IT(28735);
                V lastOcc;
                if (!GetNonDirect(CTU(kc), lastOcc))
                {TRACE_IT(28736);
                    return defv;
                }
                return lastOcc;
            }
        }
    };
}
