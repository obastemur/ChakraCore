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
        {
            return (classes[CTU(c)] & Word) != 0;
        }

        inline static bool IsNewline(Char c)
        {
            return (classes[CTU(c)] & Newline) != 0;
        }

        inline static bool IsWhitespace(Char c)
        {
            return (classes[CTU(c)] & Whitespace) != 0;
        }

        inline static bool IsLetter(Char c)
        {
            return (classes[CTU(c)] & Letter) != 0;
        }

        inline static bool IsDigit(Char c)
        {
            return (classes[CTU(c)] & Digit) != 0;
        }

        inline static bool IsOctal(Char c)
        {
            return (classes[CTU(c)] & Octal) != 0;
        }

        inline static bool IsHex(Char c)
        {
            return (classes[CTU(c)] & Hex) != 0;
        }

        inline static uint DigitValue(Char c)
        {
            return values[CTU(c)];
        }
    };

    template <>
    class StandardChars<uint8> : Chars<uint8>
    {
    public:
        inline StandardChars(ArenaAllocator* allocator) {}

        inline bool IsWord(Char c) const
        {
            return ASCIIChars::IsWord(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsNewline(Char c) const
        {
            return ASCIIChars::IsNewline(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsWhitespaceOrNewline(Char c) const
        {
            return ASCIIChars::IsWhitespace(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsLetter(Char c) const
        {
            return ASCIIChars::IsLetter(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsDigit(Char c) const
        {
            return ASCIIChars::IsDigit(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsOctal(Char c) const
        {
            return ASCIIChars::IsOctal(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsHex(Char c) const
        {
            return ASCIIChars::IsHex(ASCIIChars::UTC(CTU(c)));
        }

        inline uint DigitValue(Char c) const
        {
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
        {
            CompileAssert(sizeof(CHAR_T) == 2);
            CompileAssert(sizeof(uint) > sizeof(CHAR_T));

            const uint maxUChar = Chars<CHAR_T>::MaxUChar;
            uint l = 0;
            uint h = maxUChar;
            uint tblidx = 0;
            do {
                uint acth;
                CHAR_T equivl[CaseInsensitive::EquivClassSize];
                bool isNonTrivial = CaseInsensitive::RangeToEquivClassOnlyInSource(mappingSource, tblidx, l, h, acth, equivl);
                if (isNonTrivial)
                {
                    __assume(acth <= maxUChar); // property of algorithm: acth never greater than h
                    do
                    {
                        uint64 r = 0;
                        CompileAssert(sizeof(r) >= sizeof(CHAR_T) * CaseInsensitive::EquivClassSize);

                        for (int i = CaseInsensitive::EquivClassSize - 1; i >= 0; i--)
                        {
                            __assume(equivl[i] <= maxUChar); // property of algorithm: never map outside of range
                            r <<= 16;
                            r |= Chars<CHAR_T>::CTU(equivl[i]++);
                        }
                        toEquivs.Set(allocator, Chars<CHAR_T>::UTC(l++), r);
                    }
                    while (l <= acth);
                }
                else
                {
                    l = acth + 1;
                }
            }
            while (l <= h);
        }

        inline CHAR_T ToCanonical(CHAR_T c) const
        {
            uint64 r = toEquivs.Get(c);
            return r == EQUIV_MISSING ? fallbackMapper->ToCanonical(c) : Chars<CHAR_T>::UTC(r & 0xffff);
        }

        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        inline bool ToEquivs(CHAR_T c, __out_ecount(4) CHAR_T* equivs) const
        {
            uint64 r = toEquivs.Get(c);
            if (r == EQUIV_MISSING)
            {
                return fallbackMapper->ToEquivs(c, equivs);
            }
            else
            {
                for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
                {
                    equivs[i] = Chars<CHAR_T>::UTC(r & 0xffff);
                    r >>= 16;
                }
                return true;
            }
        }

        inline bool IsTrivialString(const CHAR_T* str, CharCount strLen) const
        {
            for (CharCount i = 0; i < strLen; i++)
            {
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
        CharMap<CHAR_T, uint64> toEquivs;

        const FallbackCaseMapper *fallbackMapper;
    };

    class TrivialCaseMapper
    {
    public:
        inline CHAR_T ToCanonical(CHAR_T c) const
        {
            return c;
        }

        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        inline bool ToEquivs(CHAR_T c, __out_ecount(4) CHAR_T* equivs) const
        {
            for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
                equivs[i] = c;
            return false;
        }

        inline bool IsTrivialString(const CHAR_T* str, CharCount strLen) const
        {
            return true;
        }

        // This class is instantiated as a global const instance
        // C++ requires that a default constructor be provided in that case
        // See http://stackoverflow.com/questions/7411515/why-does-c-require-a-user-provided-default-constructor-to-default-construct-a
        TrivialCaseMapper() {}

        static const TrivialCaseMapper Instance;
    };

    template <>
    class StandardChars<CHAR_T> : public Chars<CHAR_T>
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
        {
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsWord(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsNewline(Char c) const
        {
            return CTU(c) < ASCIIChars::NumChars ? ASCIIChars::IsNewline(ASCIIChars::UTC(CTU(c))) : (CTU(c) & 0xfffe) == 0x2028;
        }

        inline bool IsWhitespaceOrNewline(Char c) const
        {
            if (CTU(c) < ASCIIChars::NumChars)
                return ASCIIChars::IsWhitespace(ASCIIChars::UTC(CTU(c)));
            else
                return CTU(c) == 0x1680 || (CTU(c) >= 0x2000 && CTU(c) <= 0x200a) ||
                       CTU(c) == 0x2028 || CTU(c) == 0x2029 || CTU(c) == 0x202f || CTU(c) == 0x205f ||
                       CTU(c) == 0x3000 || CTU(c) == 0xfeff;
        }

        inline bool IsLetter(Char c) const
        {
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsLetter(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsDigit(Char c) const
        {
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsDigit(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsOctal(Char c) const
        {
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsOctal(ASCIIChars::UTC(CTU(c)));
        }

        inline bool IsHex(Char c) const
        {
            return CTU(c) < ASCIIChars::NumChars && ASCIIChars::IsHex(ASCIIChars::UTC(CTU(c)));
        }

        inline uint DigitValue(Char c) const
        {
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
        {
            if (mappingSource == CaseInsensitive::MappingSource::UnicodeData)
            {
                return unicodeDataCaseMapper.ToCanonical(c);
            }
            else
            {
                Assert(mappingSource == CaseInsensitive::MappingSource::CaseFolding);
                return caseFoldingCaseMapper.ToCanonical(c);
            }
        }

        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        inline bool ToEquivs(CaseInsensitive::MappingSource mappingSource, Char c, __out_ecount(4) Char* equivs) const
        {
            if (mappingSource == CaseInsensitive::MappingSource::UnicodeData)
            {
                return unicodeDataCaseMapper.ToEquivs(c, equivs);
            }
            else
            {
                Assert(mappingSource == CaseInsensitive::MappingSource::CaseFolding);
                return caseFoldingCaseMapper.ToEquivs(c, equivs);
            }
        }

        inline bool IsTrivialString(CaseInsensitive::MappingSource mappingSource, const Char* str, CharCount strLen) const
        {
            if (mappingSource == CaseInsensitive::MappingSource::UnicodeData)
            {
                return unicodeDataCaseMapper.IsTrivialString(str, strLen);
            }
            else
            {
                Assert(mappingSource == CaseInsensitive::MappingSource::CaseFolding);
                return caseFoldingCaseMapper.IsTrivialString(str, strLen);
            }
        }
    };

    typedef UnifiedRegex::StandardChars<uint8> UTF8StandardChars;
    typedef UnifiedRegex::StandardChars<CHAR_T> UnicodeStandardChars;
}
