//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class IntConstantBounds
{
private:
    int32 lowerBound;
    int32 upperBound;

public:
    IntConstantBounds() : lowerBound(0), upperBound(0)
    {LOGMEIN("IntConstantBounds.h] 14\n");
    }

    IntConstantBounds(const int32 lowerBound, const int32 upperBound)
        : lowerBound(lowerBound), upperBound(upperBound)
    {LOGMEIN("IntConstantBounds.h] 19\n");
        Assert(lowerBound <= upperBound);
    }

public:
    int32 LowerBound() const
    {LOGMEIN("IntConstantBounds.h] 25\n");
        return lowerBound;
    }

    int32 UpperBound() const
    {LOGMEIN("IntConstantBounds.h] 30\n");
        return upperBound;
    }

    bool IsConstant() const
    {LOGMEIN("IntConstantBounds.h] 35\n");
        return lowerBound == upperBound;
    }

    bool IsTaggable() const
    {LOGMEIN("IntConstantBounds.h] 40\n");
#if INT32VAR
        // All 32-bit ints are taggable on 64-bit architectures
        return true;
#else
        return lowerBound >= Js::Constants::Int31MinValue && upperBound <= Js::Constants::Int31MaxValue;
#endif
    }

    bool IsLikelyTaggable() const
    {LOGMEIN("IntConstantBounds.h] 50\n");
#if INT32VAR
        // All 32-bit ints are taggable on 64-bit architectures
        return true;
#else
        return lowerBound <= Js::Constants::Int31MaxValue && upperBound >= Js::Constants::Int31MinValue;
#endif
    }

    ValueType GetValueType() const
    {LOGMEIN("IntConstantBounds.h] 60\n");
        return ValueType::GetInt(IsLikelyTaggable());
    }

    IntConstantBounds And_0x1f() const
    {LOGMEIN("IntConstantBounds.h] 65\n");
        const int32 mask = 0x1f;
        if(static_cast<UIntConstType>(upperBound) - static_cast<UIntConstType>(lowerBound) >= static_cast<UIntConstType>(mask) ||
            (lowerBound & mask) > (upperBound & mask))
        {LOGMEIN("IntConstantBounds.h] 69\n");
            // The range contains all items in the set {0-mask}, or the range crosses a boundary of {0-mask}. Since we cannot
            // represent ranges like {0-5,8-mask}, just use {0-mask}.
            return IntConstantBounds(0, mask);
        }
        return IntConstantBounds(lowerBound & mask, upperBound & mask);
    }

    bool Contains(const int32 value) const
    {LOGMEIN("IntConstantBounds.h] 78\n");
        return lowerBound <= value && value <= upperBound;
    }

    bool operator ==(const IntConstantBounds &other) const
    {LOGMEIN("IntConstantBounds.h] 83\n");
        return lowerBound == other.lowerBound && upperBound == other.upperBound;
    }
};
