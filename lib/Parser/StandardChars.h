//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace UnifiedRegex
{
    template <typename C>
    class StandardChars {};

    class ASCIIChars : public Chars<char>
    {
    private:
        enum CharClass : uint8
        {
            Word       = 1 << 0,
            Newline    = 1 << 1,
            Whitespace = 1 << 2,
            Letter     = 1 << 3,
            Digit      = 1 << 4,
            Octal      = 1 << 5,
            Hex        = 1 << 6
        };
        static const uint8 classes[NumChars];
        static const uint8 values[NumChars];

    public:
        inline static bool IsWord(Char c)
        {TRACE_IT(32871);
            return (classes[CTU(c)] & Word) != 0;
        }

        inline static bool IsNewline(Char c)
        {TRACE_IT(32872);
            return (classes[CTU(c)] & Newline) != 0;
        }

        inline static bool IsWhitespace(Char c)
        {TRACE_IT(32873);
            return (classes[CTU(c)] & Whitespace) != 0;
        }

        inline static bool IsLetter(Char c)
        {TRACE_IT(32874);
            return (classes[CTU(c)] & Letter) != 0;
        }

        inline static bool IsDigit(Char c)
        {TRACE_IT(32875);
            return (classes[CTU(c)] & Digit) != 0;
        }

        inline static bool IsOctal(Char c)
        {TRACE_IT(32876);
            return (classes[CTU(c)] & Octal) != 0;
        }

        inline static bool IsHex(Char c)
        {TRACE_IT(32877);
            return (classes[CTU(c)] & Hex) != 0;
        }

        inline static uint DigitValue(Char c)
        {TRACE_IT(32878);
            return values[CTU(c)];
        }
    };

    template <>
    class StandardChars<uint8> : Chars<uint8>
    {
    public:
        inline StandardChars(ArenaAllocator* allocator) {TRACE_IT(32879);}

        inline bool IsWord(Char c) const
        {TRACE_IT(32880);
            return ASCIIChars::IsWord(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsNewline(Char c) const
        {TRACE_IT(32881);
            return ASCIIChars::IsNewline(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsWhitespaceOrNewline(Char c) const
        {TRACE_IT(32882);
            return ASCIIChars::IsWhitespace(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsLetter(Char c) const
        {TRACE_IT(32883);
            return ASCIIChars::IsLetter(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsDigit(Char c) const
        {TRACE_IT(32884);
            return ASCIIChars::IsDigit(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsOctal(Char c) const
        {TRACE_IT(32885);
            return ASCIIChars::IsOctal(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsHex(Char c) const
        {TRACE_IT(32886);
            return ASCIIChars::IsHex(ASCIIChars::UTC(CTU(c)));
        }

        inline uint DigitValue(Char c) const
        {TRACE_IT(32887);
            return ASCIIChars::DigitValue(ASCIIChars::UTC(CTU(c)));
        }
    };

    template <typename FallbackCaseMapper>
    class CaseMapper
    {
    public:
        CaseMapper(ArenaAllocator *allocator, CaseInsensitive::MappingSource mappingSource, const FallbackCaseMapper *fallbackMapper) :
            toEquivs((uint64) -1),
            fallbackMapper(fallbackMapper)
        {TRACE_IT(32888);
            CompileAssert(sizeof(char16) == 2);
            CompileAssert(sizeof(uint) > sizeof(char16));

            const uint maxUChar = Chars<char16>::MaxUChar;
            uint l = 0;
            uint h = maxUChar;
            uint tblidx = 0;
            do {TRACE_IT(32889);
                uint acth;
                char16 equivl[CaseInsensitive::EquivClassSize];
                bool isNonTrivial = CaseInsensitive::RangeToEquivClassOnlyInSource(mappingSource, tblidx, l, h, acth, equivl);
                if (isNonTrivial)
                {TRACE_IT(32890);
                    __assume(acth <= maxUChar); // property of algorithm: acth never greater than h
                    do
                    {TRACE_IT(32891);
                        uint64 r = 0;
                        CompileAssert(sizeof(r) >= sizeof(char16) * CaseInsensitive::EquivClassSize);

                        for (int i = CaseInsensitive::EquivClassSize - 1; i >= 0; i--)
                        {TRACE_IT(32892);
                            __assume(equivl[i] <= maxUChar); // property of algorithm: never map outside of range
                            r <<= 16;
                            r |= Chars<char16>::CTU(equivl[i]++);
                        }
                        toEquivs.Set(allocator, Chars<char16>::UTC(l++), r);
                    }
                    while (l <= acth);
                }
                else
                {TRACE_IT(32893);
                    l = acth + 1;
                }
            }
            while (l <= h);
        }

        inline char16 ToCanonical(char16 c) const
        {TRACE_IT(32894);
            uint64 r = toEquivs.Get(c);
            return r == EQUIV_MISSING ? fallbackMapper->ToCanonical(c) : Chars<char16>::UTC(r & 0xffff);
        }

        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        inline bool ToEquivs(char16 c, __out_ecount(4) char16* equivs) const
        {TRACE_IT(32895);
            uint64 r = toEquivs.Get(c);
            if (r == EQUIV_MISSING)
            {TRACE_IT(32896);
                return fallbackMapper->ToEquivs(c, equivs);
            }
            else
            {TRACE_IT(32897);
                for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
                {TRACE_IT(32898);
                    equivs[i] = Chars<char16>::UTC(r & 0xffff);
                    r >>= 16;
                }
                return true;
            }
        }

        inline bool IsTrivialString(const char16* str, CharCount strLen) const
        {TRACE_IT(32899);
            for (CharCount i = 0; i < strLen; i++)
            {TRACE_IT(32900);
                if (toEquivs.Get(str[i]) != EQUIV_MISSING)
                    return false;
            }
            return fallbackMapper->IsTrivialString(str, strLen);
        }

    private:
        // Map character to:
        //  - -1 if trivial equivalence class
        //  - otherwise to four 16-bit fields: <equiv 4><equiv 3><equiv 2><equiv 1>
        const static uint64 EQUIV_MISSING = static_cast<uint64>(-1);
        CharMap<char16, uint64> toEquivs;

        const FallbackCaseMapper *fallbackMapper;
    };

    class TrivialCaseMapper
    {
    public:
        inline char16 ToCanonical(char16 c) const
        {TRACE_IT(32901);
            return c;
        }

        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        inline bool ToEquivs(char16 c, __out_ecount(4) char16* equivs) const
        {TRACE_IT(32902);
            for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
                equivs[i] = c;
            return false;
        }

        inline bool IsTrivialString(const char16* str, CharCount strLen) const
        {TRACE_IT(32903);
            return true;
        }

        // This class is instantiated as a global const instance
        // C++ requires that a default constructor be provided in that case
        // See http://stackoverflow.com/questions/7411515/why-does-c-require-a-user-provided-default-constructor-to-default-construct-a
        TrivialCaseMapper() {TRACE_IT(32904);}

        static const TrivialCaseMapper Instance;
    };

    template <>
    class StandardChars<char16> : public Chars<char16>
    {
    private:
        static const int numDigitPairs;
        static const Char* const digitStr;
        static const int numWhitespacePairs;
        static const Char* const whitespaceStr;
        static const int numWordPairs;
        static const Char* const wordStr;
        static const int numWordIUPairs;
        static const Char* const wordIUStr;
        static const int numNewlinePairs;
        static const Char* const newlineStr;

        ArenaAllocator* allocator;

        typedef CaseMapper<TrivialCaseMapper> UnicodeDataCaseMapper;
        const UnicodeDataCaseMapper unicodeDataCaseMapper;
        typedef CaseMapper<UnicodeDataCaseMapper> CaseFoldingCaseMapper;
        const CaseFoldingCaseMapper caseFoldingCaseMapper;

        CharSet<Char>* fullSet;
        CharSet<Char>* emptySet;
        CharSet<Char>* wordSet;
        CharSet<Char>* nonWordSet;
        CharSet<Char>* wordIUSet;
        CharSet<Char>* nonWordIUSet;
        CharSet<Char>* newlineSet;
        CharSet<Char>* whitespaceSet;
        CharSet<Char>* surrogateUpperRange;

    public:
        StandardChars(ArenaAllocator* allocator);

        inline bool IsWord(Char c) const
        {TRACE_IT(32905);
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsWord(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsNewline(Char c) const
        {TRACE_IT(32906);
            return CTU(c) < ASCIIChars::NumChars ? ASCIIChars::IsNewline(ASCIIChars::UTC(CTU(c))) : (CTU(c) & 0xfffe) == 0x2028;
        }

        inline bool IsWhitespaceOrNewline(Char c) const
        {TRACE_IT(32907);
            if (CTU(c) < ASCIIChars::NumChars)
                return ASCIIChars::IsWhitespace(ASCIIChars::UTC(CTU(c)));
            else
                return CTU(c) == 0x1680 || (CTU(c) >= 0x2000 && CTU(c) <= 0x200a) ||
                       CTU(c) == 0x2028 || CTU(c) == 0x2029 || CTU(c) == 0x202f || CTU(c) == 0x205f ||
                       CTU(c) == 0x3000 || CTU(c) == 0xfeff;
        }

        inline bool IsLetter(Char c) const
        {TRACE_IT(32908);
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsLetter(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsDigit(Char c) const
        {TRACE_IT(32909);
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsDigit(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsOctal(Char c) const
        {TRACE_IT(32910);
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsOctal(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsHex(Char c) const
        {TRACE_IT(32911);
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsHex(ASCIIChars::UTC(CTU(c)));
        }

        inline uint DigitValue(Char c) const
        {TRACE_IT(32912);
            return CTU(c) < ASCIIChars::NumChars ? ASCIIChars::DigitValue(ASCIIChars::UTC(CTU(c))) : 0;
        }

        void SetDigits(ArenaAllocator* setAllocator, CharSet<Char> &set);
        void SetNonDigits(ArenaAllocator* setAllocator, CharSet<Char> &set);
        void SetWhitespace(ArenaAllocator* setAllocator, CharSet<Char> &set);
        void SetNonWhitespace(ArenaAllocator* setAllocator, CharSet<Char> &set);
        void SetWordChars(ArenaAllocator* setAllocator, CharSet<Char> &set);
        void SetNonWordChars(ArenaAllocator* setAllocator, CharSet<Char> &set);
        void SetWordIUChars(ArenaAllocator* setAllocator, CharSet<Char> &set);
        void SetNonWordIUChars(ArenaAllocator* setAllocator, CharSet<Char> &set);
        void SetNewline(ArenaAllocator* setAllocator, CharSet<Char> &set);
        void SetNonNewline(ArenaAllocator* setAllocator, CharSet<Char> &set);

        CharSet<Char>* GetFullSet();
        CharSet<Char>* GetEmptySet();
        CharSet<Char>* GetWordSet();
        CharSet<Char>* GetNonWordSet();
        CharSet<Char>* GetNewlineSet();
        CharSet<Char>* GetWhitespaceSet();
        CharSet<Char>* GetSurrogateUpperRange();

        inline Char ToCanonical(CaseInsensitive::MappingSource mappingSource, Char c) const
        {TRACE_IT(32913);
            if (mappingSource == CaseInsensitive::MappingSource::UnicodeData)
            {TRACE_IT(32914);
                return unicodeDataCaseMapper.ToCanonical(c);
            }
            else
            {TRACE_IT(32915);
                Assert(mappingSource == CaseInsensitive::MappingSource::CaseFolding);
                return caseFoldingCaseMapper.ToCanonical(c);
            }
        }

        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        inline bool ToEquivs(CaseInsensitive::MappingSource mappingSource, Char c, __out_ecount(4) Char* equivs) const
        {TRACE_IT(32916);
            if (mappingSource == CaseInsensitive::MappingSource::UnicodeData)
            {TRACE_IT(32917);
                return unicodeDataCaseMapper.ToEquivs(c, equivs);
            }
            else
            {TRACE_IT(32918);
                Assert(mappingSource == CaseInsensitive::MappingSource::CaseFolding);
                return caseFoldingCaseMapper.ToEquivs(c, equivs);
            }
        }

        inline bool IsTrivialString(CaseInsensitive::MappingSource mappingSource, const Char* str, CharCount strLen) const
        {TRACE_IT(32919);
            if (mappingSource == CaseInsensitive::MappingSource::UnicodeData)
            {TRACE_IT(32920);
                return unicodeDataCaseMapper.IsTrivialString(str, strLen);
            }
            else
            {TRACE_IT(32921);
                Assert(mappingSource == CaseInsensitive::MappingSource::CaseFolding);
                return caseFoldingCaseMapper.IsTrivialString(str, strLen);
            }
        }
    };

    typedef UnifiedRegex::StandardChars<uint8> UTF8StandardChars;
    typedef UnifiedRegex::StandardChars<char16> UnicodeStandardChars;
}
