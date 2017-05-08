//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimePlatformAgnosticPch.h"
#include "Common.h"
#include <time.h>
#include "ChakraPlatform.h"

namespace PlatformAgnostic
{
namespace DateTime
{

    #define updatePeriod 1000

    // minimal year for which windows has time zone information
    static const double criticalMin = Js::DateUtilities::TvFromDate(1601, 0, 1, 0);
    static const double criticalMax = Js::DateUtilities::TvFromDate(USHRT_MAX-1, 0, 0, 0);

    typedef BOOL(*DateConversionFunction)(
        _In_opt_ CONST PVOID lpTimeZoneInformation,
        _In_ CONST SYSTEMTIME * lpLocalTime,
        _Out_ LPSYSTEMTIME lpUniversalTime
        );

    static DateConversionFunction sysLocalToUtc = NULL;
    static DateConversionFunction sysUtcToLocal = NULL;
    static HINSTANCE g_timezonedll = NULL;

    static HINSTANCE TryLoadLibrary()
    {TRACE_IT(65131);
        if (g_timezonedll == NULL)
        {TRACE_IT(65132);
            HMODULE hLocal = LoadLibraryExW(_u("api-ms-win-core-timezone-l1-1-0.dll"),
                                            nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (hLocal != NULL)
            {TRACE_IT(65133);
                if (InterlockedCompareExchangePointer((PVOID*) &g_timezonedll, hLocal, NULL) != NULL)
                {TRACE_IT(65134);
                    FreeLibrary(hLocal);
                }
            }
        }

        if (g_timezonedll == NULL)
        {TRACE_IT(65135);
            HMODULE hLocal = LoadLibraryExW(_u("kernel32.dll"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (hLocal != NULL)
            {TRACE_IT(65136);
                if (InterlockedCompareExchangePointer((PVOID*) &g_timezonedll, hLocal, NULL) != NULL)
                {TRACE_IT(65137);
                    FreeLibrary(hLocal);
                }
            }
        }
        return g_timezonedll;
    }

    BOOL SysLocalToUtc(SYSTEMTIME *local, SYSTEMTIME *utc)
    {TRACE_IT(65138);
        if (sysLocalToUtc == NULL)
        {TRACE_IT(65139);
            HINSTANCE library = TryLoadLibrary();
            if (library != NULL && !CONFIG_ISENABLED(Js::ForceOldDateAPIFlag))
            {TRACE_IT(65140);
                sysLocalToUtc = (DateConversionFunction)
                                GetProcAddress(library, "TzSpecificLocalTimeToSystemTimeEx");
            }
            if (sysLocalToUtc == NULL)
            {TRACE_IT(65141);
                sysLocalToUtc = (DateConversionFunction)TzSpecificLocalTimeToSystemTime;
            }
        }
        return sysLocalToUtc(NULL, local, utc);
    }

    BOOL SysUtcToLocal(SYSTEMTIME *utc, SYSTEMTIME *local)
    {TRACE_IT(65142);
        if (sysUtcToLocal == NULL)
        {TRACE_IT(65143);
            HINSTANCE library = TryLoadLibrary();
            if (library != NULL)
            {TRACE_IT(65144);
                sysUtcToLocal = (DateConversionFunction)
                                GetProcAddress(library, "SystemTimeToTzSpecificLocalTimeEx");
            }
            if (sysUtcToLocal == NULL)
            {TRACE_IT(65145);
                sysUtcToLocal = (DateConversionFunction)SystemTimeToTzSpecificLocalTime;
            }
        }
        return sysUtcToLocal(NULL, utc, local);
    }

    static TimeZoneInfo* GetTimeZoneInfo(DaylightTimeHelperPlatformData &data, const double time)
    {TRACE_IT(65146);
        if (data.cache1.IsValid(time)) return &(data.cache1);
        if (data.cache2.IsValid(time)) return &(data.cache2);

        if (data.useFirstCache)
        {TRACE_IT(65147);
            data.cache1.Update(time);
            data.useFirstCache = false;
            return &(data.cache1);
        }
        else
        {TRACE_IT(65148);
            data.cache2.Update(time);
            data.useFirstCache = true;
            return &(data.cache2);
        }
    }

    // we consider January 1st, December 31st and days when daylight savings time
    // starts and ands to be critical, because there might be ambiguous cases in
    // local->utc->local conversions, so in order to be consistent with Windows
    // we rely on it to perform conversions. But it is slow.
    static inline bool IsCritical(const double time, TimeZoneInfo *timeZoneInfo)
    {TRACE_IT(65149);
        return time > criticalMin && time < criticalMax &&
            (fabs(time - timeZoneInfo->daylightDate) < DateTimeTicks_PerLargestTZOffset ||
            fabs(time - timeZoneInfo->standardDate) < DateTimeTicks_PerLargestTZOffset ||
            time > timeZoneInfo->january1 + DateTimeTicks_PerSafeEndOfYear ||
            (timeZoneInfo->isJanuary1Critical
              && time - timeZoneInfo->january1 < DateTimeTicks_PerLargestTZOffset));
    }

    // in slow path we use system API to perform conversion, but we still need
    // to know whether current time is standard or daylight savings in order to
    // create a string representation of a date. So just compare whether difference
    // between local and utc time equal to bias.
    static inline bool IsDaylightSavings(const double utcTime, const double localTime, const int bias)
    {TRACE_IT(65150);
        return ((int)(utcTime - localTime)) / ((int)(DateTimeTicks_PerMinute)) != bias;
    }


    // This function does not properly handle boundary cases.
    // But while we use IsCritical we don't care about it.
    static bool IsDaylightSavingsUnsafe(const double time, TimeZoneInfo *timeZoneInfo)
    {TRACE_IT(65151);
        return timeZoneInfo->isDaylightTimeApplicable
                && ((timeZoneInfo->daylightDate < timeZoneInfo->standardDate)
            ? timeZoneInfo->daylightDate <= time && time < timeZoneInfo->standardDate
            : time < timeZoneInfo->standardDate || timeZoneInfo->daylightDate <= time);
    }

    double UtcToLocalFast(const double utcTime, TimeZoneInfo *timeZoneInfo, int &bias,
                          int &offset, bool &isDaylightSavings)
    {TRACE_IT(65152);
        double localTime;
        localTime = utcTime - DateTimeTicks_PerMinute * timeZoneInfo->bias;
        isDaylightSavings = IsDaylightSavingsUnsafe(utcTime, timeZoneInfo);
        if (isDaylightSavings)
        {TRACE_IT(65153);
            localTime -= DateTimeTicks_PerMinute * timeZoneInfo->daylightBias;
        } else {TRACE_IT(65154);
            localTime -= DateTimeTicks_PerMinute * timeZoneInfo->standardBias;
        }

        bias = timeZoneInfo->bias;
        offset = ((int)(localTime - utcTime)) / ((int)(DateTimeTicks_PerMinute));

        return localTime;
    }

    static double UtcToLocalCritical(DaylightTimeHelperPlatformData &data, const double utcTime,
                              TimeZoneInfo *timeZoneInfo, int &bias, int &offset, bool &isDaylightSavings)
    {TRACE_IT(65155);
        double localTime;
        SYSTEMTIME utcSystem, localSystem;
        YMD ymd;

        Js::DateUtilities::GetYmdFromTv(utcTime, &ymd);
        ymd.ToSystemTime(&utcSystem);

        if (!SysUtcToLocal(&utcSystem, &localSystem))
        {TRACE_IT(65156);
            // SysUtcToLocal can fail if the date is beyond extreme internal
            // boundaries (e.g. > ~30000 years). Fall back to our fast (but
            // less accurate) version if the call fails.
            return UtcToLocalFast(utcTime, timeZoneInfo, bias, offset, isDaylightSavings);
        }

        localTime = Js::DateUtilities::TimeFromSt(&localSystem);
        if (localSystem.wYear != utcSystem.wYear)
        {TRACE_IT(65157);
            timeZoneInfo = GetTimeZoneInfo(data, localTime);
        }

        bias = timeZoneInfo->bias;
        isDaylightSavings = IsDaylightSavings(utcTime, localTime,
          timeZoneInfo->bias + timeZoneInfo->standardBias);

        offset = ((int)(localTime - utcTime)) / ((int)(DateTimeTicks_PerMinute));

        return localTime;
    }

    double DaylightTimeHelper::UtcToLocal(const double utcTime, int &bias, int &offset, bool &isDaylightSavings)
    {TRACE_IT(65158);
        TimeZoneInfo *timeZoneInfo = GetTimeZoneInfo(data, utcTime);

        if (IsCritical(utcTime, timeZoneInfo))
        {TRACE_IT(65159);
            return UtcToLocalCritical(data, utcTime, timeZoneInfo, bias, offset, isDaylightSavings);
        }
        else
        {TRACE_IT(65160);
            return UtcToLocalFast(utcTime, timeZoneInfo, bias, offset, isDaylightSavings);
        }
    }

    static double LocalToUtcFast(const double localTime, TimeZoneInfo *timeZoneInfo)
    {TRACE_IT(65161);
        double utcTime = localTime + DateTimeTicks_PerMinute * timeZoneInfo->bias;
        bool isDaylightSavings = IsDaylightSavingsUnsafe(localTime, timeZoneInfo);

        if (isDaylightSavings)
        {TRACE_IT(65162);
            utcTime += DateTimeTicks_PerMinute * timeZoneInfo->daylightBias;
        } else {TRACE_IT(65163);
            utcTime += DateTimeTicks_PerMinute * timeZoneInfo->standardBias;
        }

        return utcTime;
    }

    static double LocalToUtcCritical(const double localTime, TimeZoneInfo *timeZoneInfo)
    {TRACE_IT(65164);
        SYSTEMTIME localSystem, utcSystem;
        YMD ymd;

        Js::DateUtilities::GetYmdFromTv(localTime, &ymd);
        ymd.ToSystemTime(&localSystem);

        if (!SysLocalToUtc(&localSystem, &utcSystem))
        {TRACE_IT(65165);
            // Fall back to our fast (but less accurate) version if the call fails.
            return LocalToUtcFast(localTime, timeZoneInfo);
        }

        return Js::DateUtilities::TimeFromSt(&utcSystem);
    }

    double DaylightTimeHelper::LocalToUtc(const double localTime)
    {TRACE_IT(65166);
        TimeZoneInfo *timeZoneInfo = GetTimeZoneInfo(data, localTime);

        if (IsCritical(localTime, timeZoneInfo))
        {TRACE_IT(65167);
            return LocalToUtcCritical(localTime, timeZoneInfo);
        }
        else
        {TRACE_IT(65168);
            return LocalToUtcFast(localTime, timeZoneInfo);
        }
    }

} // namespace DateTime
} // namespace PlatformAgnostic
