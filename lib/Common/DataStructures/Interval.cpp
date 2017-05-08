//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"
#include "DataStructures/Interval.h"

namespace regex
{
    bool Interval::Includes(int value) const
    {TRACE_IT(21719);
        return (begin <= value) && (end >= value);
    }

    bool Interval::Includes(Interval other) const
    {TRACE_IT(21720);
        return (Includes(other.Begin()) && (Includes(other.End())));
    }

    int Interval::CompareTo(Interval other)
    {TRACE_IT(21721);
        if (begin < other.begin)
        {TRACE_IT(21722);
            return -1;
        }
        else if (begin == other.begin)
        {TRACE_IT(21723);
            if (end < other.end)
            {TRACE_IT(21724);
                return -1;
            }
            else if (end == other.end)
            {TRACE_IT(21725);
                return 0;
            }
            else
            {TRACE_IT(21726);
                return 1;
            }
        }
        else
        {TRACE_IT(21727);
            return 1;
        }
    }

    int Interval::Compare(Interval x, Interval y)
    {TRACE_IT(21728);
        return x.CompareTo(y);
    }

    bool Interval::Equals(Interval other)
    {TRACE_IT(21729);
        return CompareTo(other) == 0;
    }

    bool Interval::Equals(Interval x, Interval y)
    {TRACE_IT(21730);
        return x.CompareTo(y) == 0;
    }

    int Interval::GetHashCode()
    {TRACE_IT(21731);
        return  _rotl(begin, 7) ^ end;
    }

    int Interval::GetHashCode(Interval item)
    {TRACE_IT(21732);
        return item.GetHashCode();
    }
}
