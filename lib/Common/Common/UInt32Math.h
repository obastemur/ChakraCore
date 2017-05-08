//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
class UInt32Math
{
public:
    template< class Func >
    static uint32 Add(uint32 lhs, uint32 rhs, __inout Func& overflowFn)
    {TRACE_IT(19530);
        uint32 result = lhs + rhs;

        // If the result is smaller than the LHS, then we overflowed
        if( result < lhs )
        {TRACE_IT(19531);
            overflowFn();
        }

        return result;
    }

    template< class Func >
    static void Inc(uint32& lhs, __inout Func& overflowFn)
    {TRACE_IT(19532);
        ++lhs;

        // If lhs becomes 0, then we overflowed
        if(!lhs)
        {TRACE_IT(19533);
            overflowFn();
        }
    }

    template< class Func >
    static uint32 Mul(uint32 lhs, uint32 rhs, __inout Func& overflowFn)
    {TRACE_IT(19534);
        // Do the multiplication using 64-bit unsigned math.
        uint64 result = static_cast<uint64>(lhs) * static_cast<uint64>(rhs);

        // Does the result fit in 32-bits?
        if(result >= (1ui64 << 32))
        {TRACE_IT(19535);
            overflowFn();
        }

        return static_cast<uint32>(result);
    }

    template<uint32 mul, class Func >
    static uint32 Mul(uint32 left, __inout Func& overflowFn)
    {TRACE_IT(19536);
        CompileAssert(mul != 0);

        if (left > (UINT_MAX / mul))
        {TRACE_IT(19537);
            overflowFn();
        }

        // If mul is a power of 2, the compiler will convert this to a shift left.
        return left * mul;
    }

    // Using 0 for mul will result in compile-time divide by zero error (which is desired behavior)
    template< uint32 add, uint32 mul, class Func >
    static uint32 AddMul(uint32 left, __inout Func& overflowFn)
    {TRACE_IT(19538);
        //
        // The result will overflow if (left+add)*mul > UINT_MAX
        // Rearranging, this becomes: left > (UINT_MAX / mul ) - add
        //
        // If mul and add are compile-time constants then LTCG will collapse
        // this to a simple constant comparison.
        //
        CompileAssert(UINT_MAX/mul >= add);

        if( left > ((UINT_MAX / mul) - add) )
        {TRACE_IT(19539);
            overflowFn();
        }

        // When add and mul are small constants, the compiler is
        // typically able to use the LEA instruction here.
        return (left + add) * mul;
    }

    // Using 0 for mul will result in compile-time divide by zero error (which is desired behavior)
    template< uint32 mul, uint32 add, class Func >
    static uint32 MulAdd(uint32 left, __inout Func& overflowFn)
    {TRACE_IT(19540);
        //
        // The result will overflow if (left*mul)+add > UINT_MAX
        // Rearranging, this becomes: left > (UINT_MAX - add) / mul
        //
        // If add and mul are compile-time constants then LTCG will collapse
        // this to a simple constant comparison.
        //
        if( left > ((UINT_MAX - add) / mul) )
        {TRACE_IT(19541);
            overflowFn();
        }

        // When add and mul are small constants, the compiler is
        // typically able to use the LEA instruction here.
        return (left * mul) + add;
    }

    // Convenience functions which use the DefaultOverflowPolicy (throw OOM upon overflow)
    template< uint32 add, uint32 mul >
    static uint32 AddMul(uint32 left)
    {TRACE_IT(19542);
        return AddMul<add,mul>(left, ::Math::DefaultOverflowPolicy);
    }

    static uint32 Add(uint32 lhs, uint32 rhs)
    {TRACE_IT(19543);
        return Add( lhs, rhs, ::Math::DefaultOverflowPolicy );
    }

    static uint32 Mul(uint32 lhs, uint32 rhs)
    {TRACE_IT(19544);
        return Mul(lhs, rhs, ::Math::DefaultOverflowPolicy );
    }

    template<uint32 mul>
    static uint32 Mul(uint32 lhs)
    {TRACE_IT(19545);
        return Mul<mul>(lhs, ::Math::DefaultOverflowPolicy);
    }

    template< uint32 mul, uint32 add >
    static uint32 MulAdd(uint32 left)
    {TRACE_IT(19546);
        return MulAdd<mul,add>(left, ::Math::DefaultOverflowPolicy);
    }

    // Convenience functions which return a bool indicating overflow
    static bool Add(uint32 lhs, uint32 rhs, __out uint32* result)
    {TRACE_IT(19547);
        ::Math::RecordOverflowPolicy overflowGuard;
        *result = Add(lhs, rhs, overflowGuard);
        return overflowGuard.HasOverflowed();
    }

    static bool Mul(uint32 lhs, uint32 rhs, __out uint32* result)
    {TRACE_IT(19548);
        ::Math::RecordOverflowPolicy overflowGuard;
        *result = Mul(lhs, rhs, overflowGuard);
        return overflowGuard.HasOverflowed();
    }

    // Convenience function which uses DefaultOverflowPolicy (throws OOM upon overflow)
    static void Inc(uint32& lhs)
    {
        Inc(lhs, ::Math::DefaultOverflowPolicy);
    }
};
