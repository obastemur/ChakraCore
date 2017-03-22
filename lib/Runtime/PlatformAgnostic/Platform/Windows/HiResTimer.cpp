//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimePlatformAgnosticPch.h"
#include "Common.h"
#include "ChakraPlatform.h"

namespace PlatformAgnostic
{
namespace DateTime
{

    double HiResTimer::GetSystemTime()
    {LOGMEIN("HiResTimer.cpp] 15\n");
        SYSTEMTIME stTime;
        ::GetSystemTime(&stTime);
        return Js::DateUtilities::TimeFromSt(&stTime);
    }

    // determine if the system time is being adjusted every tick to gradually
    // bring it inline with a time server.
    static double GetAdjustFactor()
    {LOGMEIN("HiResTimer.cpp] 24\n");
        DWORD dwTimeAdjustment = 0;
        DWORD dwTimeIncrement = 0;
        BOOL fAdjustmentDisabled = FALSE;
        BOOL fSuccess = GetSystemTimeAdjustment(&dwTimeAdjustment, &dwTimeIncrement, &fAdjustmentDisabled);
        if (!fSuccess || fAdjustmentDisabled)
        {LOGMEIN("HiResTimer.cpp] 30\n");
            return 1;
        }
        return ((double)dwTimeAdjustment) / ((double)dwTimeIncrement);
    }

    double HiResTimer::Now()
    {LOGMEIN("HiResTimer.cpp] 37\n");
        if(!data.fHiResAvailable)
        {LOGMEIN("HiResTimer.cpp] 39\n");
            return GetSystemTime();
        }

        if(!data.fInit)
        {LOGMEIN("HiResTimer.cpp] 44\n");
            if (!QueryPerformanceFrequency((LARGE_INTEGER *) &(data.freq)))
            {LOGMEIN("HiResTimer.cpp] 46\n");
                data.fHiResAvailable = false;
                return GetSystemTime();
            }
            data.fInit = true;
        }

#if DBG
        uint64 f;
        Assert(QueryPerformanceFrequency((LARGE_INTEGER *)&f) && f == data.freq);
#endif
        // try better resolution time using perf counters
        uint64 count;
        if( !QueryPerformanceCounter((LARGE_INTEGER *) &count))
        {LOGMEIN("HiResTimer.cpp] 60\n");
            data.fHiResAvailable = false;
            return GetSystemTime();
        }

        double time = GetSystemTime();

        // there is a base time and count set.
        if (!data.fReset
            && (count >= data.baseMsCount)) // Make sure we don't regress
        {LOGMEIN("HiResTimer.cpp] 70\n");
            double elapsed = ((double)(count - data.baseMsCount)) * 1000 / data.freq;

            // if the system time is being adjusted every tick, adjust the
            // precise time delta accordingly.
            if (data.dAdjustFactor != 1)
            {LOGMEIN("HiResTimer.cpp] 76\n");
                elapsed = elapsed * data.dAdjustFactor;
            }

            double preciseTime = data.dBaseTime + elapsed;

            if (fabs(preciseTime - time) < 25 // the time computed via perf counter is off by 25ms
                && preciseTime >= data.dLastTime)  // the time computed via perf counter is running backwards
            {LOGMEIN("HiResTimer.cpp] 84\n");
                data.dLastTime = preciseTime;
                return data.dLastTime;
            }
        }

        //reset
        data.dBaseTime = time;
        data.dAdjustFactor = GetAdjustFactor();
        data.baseMsCount = count;

        double dSinceLast = time - data.dLastTime;
        if (dSinceLast < -3000) // if new time is significantly behind (3s), use it:
        {LOGMEIN("HiResTimer.cpp] 97\n");                       // the clock may have been set backwards.
            data.dLastTime = time;
        }
        else
        {
            data.dLastTime = max(data.dLastTime, time); // otherwise, make sure we don't regress the time.
        }

        data.fReset = false;
        return data.dLastTime;
    }

} // namespace DateTime
} // namespace PlatformAgnostic
