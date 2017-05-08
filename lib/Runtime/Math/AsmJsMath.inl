//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

namespace Js
{

    template<typename T>
    inline T minCheckNan(T aLeft, T aRight)
    {TRACE_IT(64620);
        if (NumberUtilities::IsNan(aLeft) || NumberUtilities::IsNan(aRight))
        {TRACE_IT(64621);
            return (T)NumberConstants::NaN;
        }
        if (aLeft < aRight)
        {TRACE_IT(64622);
            return aLeft;
        }
        if (aLeft == aRight)
        {TRACE_IT(64623);
            if (aRight == 0 && JavascriptNumber::IsNegZero(aLeft))
            {TRACE_IT(64624);
                return aLeft;
            }
        }
        return aRight;
    }

    template<>
    inline double AsmJsMath::Min<double>(double aLeft, double aRight)
    {TRACE_IT(64625);
        return minCheckNan(aLeft, aRight);
    }

    template<>
    inline float AsmJsMath::Min<float>(float aLeft, float aRight)
    {TRACE_IT(64626);
        return minCheckNan(aLeft, aRight);
    }

    template<typename T>
    inline T maxCheckNan(T aLeft, T aRight)
    {TRACE_IT(64627);
        if (NumberUtilities::IsNan(aLeft) || NumberUtilities::IsNan(aRight))
        {TRACE_IT(64628);
            return (T)NumberConstants::NaN;
        }
        if (aLeft > aRight)
        {TRACE_IT(64629);
            return aLeft;
        }
        if (aLeft == aRight)
        {TRACE_IT(64630);
            if (aLeft == 0 && JavascriptNumber::IsNegZero(aRight))
            {TRACE_IT(64631);
                return aLeft;
            }
        }
        return aRight;
    }

    template<>
    inline double AsmJsMath::Max<double>(double aLeft, double aRight)
    {TRACE_IT(64632);
        return maxCheckNan(aLeft, aRight);
    }

    template<>
    inline float AsmJsMath::Max<float>(float aLeft, float aRight)
    {TRACE_IT(64633);
        return maxCheckNan(aLeft, aRight);
    }

    template<>
    inline double AsmJsMath::Abs<double>(double aLeft)
    {TRACE_IT(64634);
        uint64 x = (*(uint64*)(&aLeft) & 0x7FFFFFFFFFFFFFFF);
        return *(double*)(&x);
    }

    template<>
    inline float AsmJsMath::Abs<float>(float aLeft)
    {TRACE_IT(64635);
        uint32 x = (*(uint32*)(&aLeft) & 0x7FFFFFFF);
        return *(float*)(&x);
    }

    template<typename T>
    inline T AsmJsMath::Add( T aLeft, T aRight )
    {TRACE_IT(64636);
        return aLeft + aRight;
    }

    template<typename T>
    inline T AsmJsMath::Sub( T aLeft, T aRight )
    {TRACE_IT(64637);
        return aLeft - aRight;
    }

    template<typename T> inline int AsmJsMath::CmpLt( T aLeft, T aRight ){TRACE_IT(64638);return (int)(aLeft <  aRight);}
    template<typename T> inline int AsmJsMath::CmpLe( T aLeft, T aRight ){TRACE_IT(64639);return (int)(aLeft <= aRight);}
    template<typename T> inline int AsmJsMath::CmpGt( T aLeft, T aRight ){TRACE_IT(64640);return (int)(aLeft >  aRight);}
    template<typename T> inline int AsmJsMath::CmpGe( T aLeft, T aRight ){TRACE_IT(64641);return (int)(aLeft >= aRight);}
    template<typename T> inline int AsmJsMath::CmpEq( T aLeft, T aRight ){TRACE_IT(64642);return (int)(aLeft == aRight);}
    template<typename T> inline int AsmJsMath::CmpNe( T aLeft, T aRight ){TRACE_IT(64643);return (int)(aLeft != aRight);}

    template<typename T>
    inline T AsmJsMath::Rem( T aLeft, T aRight )
    {TRACE_IT(64644);
        return (aRight == 0) ? 0 : aLeft % aRight;
    }

    template<>
    inline int AsmJsMath::Rem<int>( int aLeft, int aRight )
    {TRACE_IT(64645);
        return ((aRight == 0) || (aLeft == (1<<31) && aRight == -1)) ? 0 : aLeft % aRight;
    }

    template<>
    inline double AsmJsMath::Rem<double>( double aLeft, double aRight )
    {TRACE_IT(64646);
        return NumberUtilities::Modulus( aLeft, aRight );
    }

    template<typename T> 
    inline T AsmJsMath::And( T aLeft, T aRight )
    {TRACE_IT(64647);
        return aLeft & aRight;
    }

    template<typename T> 
    inline T AsmJsMath::Or( T aLeft, T aRight )
    {TRACE_IT(64648);
        return aLeft | aRight;
    }

    template<typename T> 
    inline T AsmJsMath::Xor( T aLeft, T aRight )
    {TRACE_IT(64649);
        return aLeft ^ aRight;
    }

    template<typename T> 
    inline T AsmJsMath::Shl( T aLeft, T aRight )
    {TRACE_IT(64650);
        return aLeft << aRight;
    }

    template<typename T> 
    inline T AsmJsMath::Shr( T aLeft, T aRight )
    {TRACE_IT(64651);
        return aLeft >> aRight;
    }

    template<typename T> 
    inline T AsmJsMath::ShrU( T aLeft, T aRight )
    {TRACE_IT(64652);
        return aLeft >> aRight;
    }

    template<typename T>
    inline T AsmJsMath::Neg( T aLeft )
    {TRACE_IT(64653);
        return -aLeft;
    }

    inline int AsmJsMath::Not( int aLeft )
    {TRACE_IT(64654);
        return ~aLeft;
    }

    inline int AsmJsMath::LogNot( int aLeft )
    {TRACE_IT(64655);
        return !aLeft;
    }

    inline int AsmJsMath::ToBool( int aLeft )
    {TRACE_IT(64656);
        return !!aLeft;
    }

    inline int AsmJsMath::Clz32( int value)
    {TRACE_IT(64657);
        DWORD index;
        if (_BitScanReverse(&index, value))
        {TRACE_IT(64658);
            return 31 - index;
        }
        return 32;
    }
}
