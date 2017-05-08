//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
class UInt16Math
{
public:
    template< class Func >
    static uint16 Add(uint16 lhs, uint16 rhs, __inout Func& overflowFn)
    {TRACE_IT(19524);
        uint16 result = lhs + rhs;

        // If the result is smaller than the LHS, then we overflowed
        if( result < lhs )
        {TRACE_IT(19525);
            overflowFn();
        }

        return result;
    }

    template< class Func >
    static void Inc(uint16& lhs, __inout Func& overflowFn)
    {TRACE_IT(19526);
        ++lhs;

        // If lhs becomes 0, then we overflowed
        if(!lhs)
        {TRACE_IT(19527);
            overflowFn();
        }
    }

    // Convenience function which uses DefaultOverflowPolicy (throws OOM upon overflow)
    static uint16 Add(uint16 lhs, uint16 rhs)
    {TRACE_IT(19528);
        return Add(lhs, rhs, ::Math::DefaultOverflowPolicy);
    }

    // Convenience function which returns a bool indicating overflow
    static bool Add(uint16 lhs, uint16 rhs, __out uint16* result)
    {TRACE_IT(19529);
        ::Math::RecordOverflowPolicy overflowGuard;
        *result = Add(lhs, rhs, overflowGuard);
        return overflowGuard.HasOverflowed();
    }

    // Convenience function which uses DefaultOverflowPolicy (throws OOM upon overflow)
    static void Inc(uint16& lhs)
    {
        Inc(lhs, ::Math::DefaultOverflowPolicy);
    }
};
