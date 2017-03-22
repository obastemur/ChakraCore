//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#ifndef __APPLE__
// todo: for BSD consider moving this file into macOS folder
#include "../Linux/DateTime.cpp"
#else
#include "Common.h"
#include "ChakraPlatform.h"
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFTimeZone.h>

namespace PlatformAgnostic
{
namespace DateTime
{
    const WCHAR *Utility::GetStandardName(size_t *nameLength, const DateTime::YMD *ymd)
    {LOGMEIN("DateTime.cpp] 19\n");
        AssertMsg(ymd != NULL, "xplat needs DateTime::YMD is defined for this call");
        double tv = Js::DateUtilities::TvFromDate(ymd->year, ymd->mon, ymd->mday, ymd->time);
        int64_t absoluteTime = tv / 1000;
        absoluteTime -= kCFAbsoluteTimeIntervalSince1970;

        CFTimeZoneRef timeZone = CFTimeZoneCopySystem();

        int offset = (int)CFTimeZoneGetSecondsFromGMT(timeZone, (CFAbsoluteTime)absoluteTime);
        absoluteTime -= offset;

        char tz_name[128];
        CFStringRef abbr = CFTimeZoneCopyAbbreviation(timeZone, absoluteTime);
        CFRelease(timeZone);
        CFStringGetCString(abbr, tz_name, sizeof(tz_name), kCFStringEncodingUTF16);
        wcscpy_s(data.standardName, 32, reinterpret_cast<WCHAR*>(tz_name));
        data.standardNameLength = CFStringGetLength(abbr);
        CFRelease(abbr);

        *nameLength = data.standardNameLength;
        return data.standardName;
    }

    const WCHAR *Utility::GetDaylightName(size_t *nameLength, const DateTime::YMD *ymd)
    {LOGMEIN("DateTime.cpp] 43\n");
        // xplat only gets the actual zone name for the given date
        return GetStandardName(nameLength, ymd);
    }

    static time_t IsDST(double tv, int *offset)
    {LOGMEIN("DateTime.cpp] 49\n");
        CFTimeZoneRef timeZone = CFTimeZoneCopySystem();
        int64_t absoluteTime = tv / 1000;
        absoluteTime -= kCFAbsoluteTimeIntervalSince1970;
        *offset = (int)CFTimeZoneGetSecondsFromGMT(timeZone, (CFAbsoluteTime)absoluteTime);

        time_t result = CFTimeZoneIsDaylightSavingTime(timeZone, (CFAbsoluteTime)absoluteTime);
        CFRelease(timeZone);
        return result;
    }

    static void YMDLocalToUtc(double localtv, YMD *utc)
    {LOGMEIN("DateTime.cpp] 61\n");
        int mOffset = 0;
        bool isDST = IsDST(localtv, &mOffset);
        localtv -= DateTimeTicks_PerSecond * mOffset;
        Js::DateUtilities::GetYmdFromTv(localtv, utc);
    }

    static void YMDUtcToLocal(double utctv, YMD *local,
                          int &bias, int &offset, bool &isDaylightSavings)
    {LOGMEIN("DateTime.cpp] 70\n");
        int mOffset = 0;
        bool isDST = IsDST(utctv, &mOffset);
        utctv += DateTimeTicks_PerSecond * mOffset;
        Js::DateUtilities::GetYmdFromTv(utctv, local);
        isDaylightSavings = isDST;
        bias = mOffset / 60;
        offset = bias;
    }

    // DaylightTimeHelper ******
    double DaylightTimeHelper::UtcToLocal(double utcTime, int &bias,
                                          int &offset, bool &isDaylightSavings)
    {LOGMEIN("DateTime.cpp] 83\n");
        YMD local;
        YMDUtcToLocal(utcTime, &local, bias, offset, isDaylightSavings);

        return Js::DateUtilities::TvFromDate(local.year, local.mon, local.mday, local.time);
    }

    double DaylightTimeHelper::LocalToUtc(double localTime)
    {LOGMEIN("DateTime.cpp] 91\n");
        YMD utc;
        YMDLocalToUtc(localTime, &utc);

        return Js::DateUtilities::TvFromDate(utc.year, utc.mon, utc.mday, utc.time);
    }
} // namespace DateTime
} // namespace PlatformAgnostic
#endif
