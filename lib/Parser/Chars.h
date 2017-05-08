//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


namespace UnifiedRegex
{
    template <typename C>
    struct Chars
    {
        typedef C Char;
    };

    template <>
    struct Chars<uint8>
    {
        typedef uint8 Char;
        typedef uint8 UChar;

        static const int CharWidth = sizeof(char) * 8;
        static const int NumChars = 1 << CharWidth;
        static const uint MaxUChar = (uint8)-1;
        static const uint MaxUCharAscii = (1 << 7) - 1;
        static const Char MinChar = (Char)0;
        static const Char MaxChar = (Char)MaxUChar;

        // Char to unsigned int
        static inline uint CTU(Char c)
        {TRACE_IT(29152);
            return (uint)c;
        }

        // Unsigned int to Char
        static inline Char UTC(uint u) {TRACE_IT(29153);
            Assert(u <= MaxUChar);
            return (Char)u;
        }

        // int to Char
        static inline Char ITC(int i) {TRACE_IT(29154);
            Assert(i >= 0 && i <= MaxUChar);
            return (Char)i;
        }

        // Char to char16
        static inline char16 CTW(Char c)
        {TRACE_IT(29155);
            return (char16)c;
        }

        // Offset, same buffer
        static inline CharCount OSB(const Char* ph, const Char* pl)
        {TRACE_IT(29156);
            Assert(ph >= pl && ph - pl <= MaxCharCount);
            return (CharCount)(ph - pl);
        }

        static inline Char Shift(Char c, int n)
        {TRACE_IT(29157);
            return UTC(CTU(c) + n);
        }
    };

    template <>
    struct Chars<char>
    {
        typedef char Char;
        typedef uint8 UChar;

        static const int CharWidth = sizeof(char) * 8;
        static const int NumChars = 1 << CharWidth;
        static const uint MaxUChar = (uint8)-1;
        static const uint MaxUCharAscii = (1 << 7) - 1;
        static const Char MinChar = (Char)0;
        static const Char MaxChar = (Char)MaxUChar;

        // Char to unsigned int
        static inline uint CTU(Char c)
        {TRACE_IT(29158);
            return (uint8)c;
        }

        // Unsigned int to Char
        static inline Char UTC(uint u) {TRACE_IT(29159);
            Assert(u <= MaxUChar);
            return (Char)u;
        }

        // int to Char
        static inline Char ITC(int i) {TRACE_IT(29160);
            Assert(i >= 0 && i <= MaxUChar);
            return (Char)(uint8)i;
        }

        // Char to char16
        static inline char16 CTW(Char c)
        {TRACE_IT(29161);
            return (char16)(uint8)c;
        }

        // Offset, same buffer
        static inline CharCount OSB(const Char* ph, const Char* pl)
        {TRACE_IT(29162);
            Assert(ph >= pl && ph - pl <= MaxCharCount);
            return (CharCount)(ph - pl);
        }

        static inline Char Shift(Char c, int n)
        {TRACE_IT(29163);
            return UTC(CTU(c) + n);
        }
    };


    template <>
    struct Chars<char16>
    {
        typedef char16 Char;
        typedef uint16 UChar;

        static const int CharWidth = sizeof(char16) * 8;
        static const int NumChars = 1 << CharWidth;
        static const uint MaxUChar = (uint16)-1;
        static const uint MaxUCharAscii = (1 << 7) - 1;
        static const Char MinChar = (Char)0;
        static const Char MaxChar = (Char)MaxUChar;

        // Char to unsigned int
        static inline uint CTU(Char c)
        {TRACE_IT(29164);
            return (uint16)c;
        }

        // Unsigned int to Char
        static inline Char UTC(uint u)
        {TRACE_IT(29165);
            Assert(u <= MaxUChar);
            return (Char)u;
        }

        // int to Char
        static inline Char ITC(int i) {TRACE_IT(29166);
            Assert(i >= 0 && i <= MaxUChar);
            return (Char)(uint16)i;
        }

        // Char to char16
        static inline char16 CTW(Char c)
        {TRACE_IT(29167);
            return c;
        }

        // Offset, same buffer
        static inline CharCount OSB(const Char* ph, const Char* pl)
        {TRACE_IT(29168);
            Assert(ph >= pl && ph - pl <= MaxCharCount);
            return (CharCount)(ph - pl);
        }

        static inline Char Shift(Char c, int n)
        {TRACE_IT(29169);
            return UTC(CTU(c) + n);
        }
    };

    template <>
    struct Chars<codepoint_t>
    {
        typedef codepoint_t Char;
        typedef codepoint_t UChar;

        static const int CharWidth = sizeof(codepoint_t) * 8;
        static const int NumChars = 0x110000;
        static const uint MaxUChar = (NumChars) - 1;
        static const uint MaxUCharAscii = (1 << 7) - 1;
        static const Char MinChar = (Char)0;
        static const Char MaxChar = (Char)MaxUChar;

        // Char to unsigned int
        static inline uint CTU(Char c)
        {TRACE_IT(29170);
            Assert(c <= MaxChar);
            return (codepoint_t)c;
        }

        // Unsigned int to Char
        static inline Char UTC(uint u)
        {TRACE_IT(29171);
            Assert(u <= MaxUChar);
            return (Char)u;
        }

        // int to Char
        static inline Char ITC(int i) {TRACE_IT(29172);
            Assert(i >= 0 && i <= MaxUChar);
            return (Char)(codepoint_t)i;
        }

        // Char to char16
        static inline char16 CTW(Char c)
        {TRACE_IT(29173);
            Assert(c < Chars<char16>::MaxUChar);
            return (char16)c;
        }

        // Offset, same buffer
        static inline CharCount OSB(const Char* ph, const Char* pl)
        {TRACE_IT(29174);
            Assert(ph >= pl && ph - pl <= MaxCharCount);
            return (CharCount)(ph - pl);
        }

        static inline Char Shift(Char c, int n)
        {TRACE_IT(29175);
            return UTC(CTU(c) + n);
        }
    };
}
