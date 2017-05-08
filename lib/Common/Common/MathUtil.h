//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

///---------------------------------------------------------------------------
///
/// class Math
///
///---------------------------------------------------------------------------

class Math
{
public:

    static uint32 PopCnt32(uint32 x)
    {TRACE_IT(19094);
        // sum set bits in every bit pair
        x -= (x >> 1) & 0x55555555u;
        // sum pairs into quads
        x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
        // sum quads into octets
        x = (x + (x >> 4)) & 0x0f0f0f0fu;
        // sum octets into topmost octet
        x *= 0x01010101u;
        return x >> 24;
    }

    // Explicit cast to integral (may truncate).  Avoids warning C4302 'type cast': truncation
    template <typename T>
    static T PointerCastToIntegralTruncate(void * pointer)
    {TRACE_IT(19095);
        return (T)(uintptr_t)pointer;
    }

    // Explicit cast to integral. Assert that it doesn't truncate.  Avoids warning C4302 'type cast': truncation
    template <typename T>
    static T PointerCastToIntegral(void * pointer)
    {TRACE_IT(19096);
        T value = PointerCastToIntegralTruncate<T>(pointer);
        Assert((uintptr_t)value == (uintptr_t)pointer);
        return value;
    }

    static bool     FitsInDWord(int32 value) {TRACE_IT(19097); return true; }
    static bool     FitsInDWord(size_t value) {TRACE_IT(19098); return ((size_t)(signed int)(value & 0xFFFFFFFF) == value); }
    static bool     FitsInDWord(int64 value) {TRACE_IT(19099); return ((int64)(signed int)(value & 0xFFFFFFFF) == value); }

    static UINT_PTR Rand();
    static bool     IsPow2(int32 val) {TRACE_IT(19100); return (val > 0 && ((val-1) & val) == 0); }
    static uint32   NextPowerOf2(uint32 n);

    // Use for compile-time evaluation of powers of 2
    template<uint32 val> struct Is
    {
        static const bool Pow2 = ((val-1) & val) == 0;
    };

    // Defined in the header so that the Recycler static lib doesn't
    // need to pull in jscript.common.common.lib
    static uint32 Log2(uint32 value)
    {TRACE_IT(19101);
        int i;

        for (i = 0; value >>= 1; i++);
        return i;
    }

    // Define a couple of overflow policies for the UInt32Math routines.

    // The default policy for overflow is to throw an OutOfMemory exception
    __declspec(noreturn) static void DefaultOverflowPolicy();

    // A functor (class with operator()) which records whether a the calculation
    // encountered an overflow condition.
    class RecordOverflowPolicy
    {
        bool fOverflow;
    public:
        RecordOverflowPolicy() : fOverflow(false)
        {TRACE_IT(19102);
        }

        // Called when an overflow is detected
        void operator()()
        {TRACE_IT(19103);
            fOverflow = true;
        }

        bool HasOverflowed() const
        {TRACE_IT(19104);
            return fOverflow;
        }
    };

    template <typename T>
    static T Align(T size, T alignment)
    {TRACE_IT(19105);
        return ((size + (alignment-1)) & ~(alignment-1));
    }

    template <typename T, class Func>
    static T AlignOverflowCheck(T size, T alignment, __inout Func& overflowFn)
    {TRACE_IT(19106);
        Assert(size >= 0);
        T alignSize = Align(size, alignment);
        if (alignSize < size)
        {TRACE_IT(19107);
            overflowFn();
        }
        return alignSize;
    }

    template <typename T>
    static T AlignOverflowCheck(T size, T alignment)
    {TRACE_IT(19108);
        return AlignOverflowCheck(size, alignment, DefaultOverflowPolicy);
    }

};
