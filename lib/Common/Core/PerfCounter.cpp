//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"
#include "PerfCounterImpl.cpp"

#ifdef PERF_COUNTERS
namespace PerfCounter
{
    Counter& Counter::operator+=(size_t value)
    {TRACE_IT(20275);
        Assert(count);
        ::InterlockedExchangeAdd(count, (DWORD)value);
        return *this;
    }
    Counter& Counter::operator-=(size_t value)
    {TRACE_IT(20276);
        Assert(count);
        ::InterlockedExchangeSubtract(count, (DWORD)value);
        return *this;
    }
    Counter& Counter::operator++()
    {TRACE_IT(20277);
        Assert(count);
        ::InterlockedIncrement(count);
        return *this;
    }
    Counter& Counter::operator--()
    {TRACE_IT(20278);
        Assert(count);
        ::InterlockedDecrement(count);
        return *this;
    }
}
#endif
