//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Common.h"
#include "ChakraPlatform.h"

namespace PlatformAgnostic
{
namespace DateTime
{
    int GetTZ(double tv, WCHAR* dst_name, bool* is_dst, int* offset)
    {
        struct tm tm_local, *tm_result;
        time_t time_noms = (time_t) (tv / 1000 /* drop ms */);
        tm_result = localtime_r(&time_noms, &tm_local);
        if (!tm_result)
        {
            *is_dst = false;
            *offset = 0;
            if (dst_name != nullptr)
            {
                dst_name[0] = (WCHAR) 0;
            }
            return 0;
        }

        *is_dst = tm_result->tm_isdst > 0;
        *offset = (int) tm_result->tm_gmtoff;

        if (dst_name != nullptr)
        {
            if (!tm_result->tm_zone)
            {
                dst_name[0] = 0;
                return 0;
            }

            uint32 length = 0;
            for (; length < __CC_PA_TIMEZONE_ABVR_NAME_LENGTH
                && tm_result->tm_zone[length] != 0; length++)
            {
                dst_name[length] = (WCHAR) tm_result->tm_zone[length];
            }

            if (length >= __CC_PA_TIMEZONE_ABVR_NAME_LENGTH)
            {
                length = __CC_PA_TIMEZONE_ABVR_NAME_LENGTH - 1;
            }

            dst_name[length] = (WCHAR)0;
            return length;
        }
        else
        {
            return 0;
        }
    }
    
    const WCHAR * Utility::GetDaylightInfoYMD(size_t* nameLength, int* offset,
                                           bool* isDaylightSavings, const DateTime::YMD *ymd)
    {
        double tv = Js::DateUtilities::TvFromDate(ymd->year, ymd->mon, ymd->mday, ymd->time);
        return GetDaylightInfoDouble(nameLength, offset, isDaylightSavings, tv);
    }
    
    const WCHAR * Utility::GetDaylightInfoDouble(size_t* nameLength, int* offset,
                                              bool* isDaylightSavings, double tv)
    {
        data.standardNameLength = GetTZ(tv, data.standardName, isDaylightSavings, offset);
        *nameLength = data.standardNameLength;
        *offset /= 60;
        
        return data.standardName;
    }

    static void YMDLocalToUtc(double localtv, YMD *utc)
    {
        int mOffset = 0;
        bool isDST;
        GetTZ(localtv, nullptr, &isDST, &mOffset);
        localtv -= DateTimeTicks_PerSecond * mOffset;
        Js::DateUtilities::GetYmdFromTv(localtv, utc);
    }

    static void YMDUtcToLocal(double utctv, YMD *local, int &offset, bool &isDaylightSavings)
    {
        int mOffset = 0;
        bool isDST;
        GetTZ(utctv, nullptr, &isDST, &mOffset);
        utctv += DateTimeTicks_PerSecond * mOffset;
        Js::DateUtilities::GetYmdFromTv(utctv, local);
        isDaylightSavings = isDST;
        offset /= 60;
    }

    // DaylightTimeHelper ******
    double DaylightTimeHelper::UtcToLocal(double utcTime, int &offset, bool &isDaylightSavings)
    {
        YMD local;
        YMDUtcToLocal(utcTime, &local, offset, isDaylightSavings);

        return Js::DateUtilities::TvFromDate(local.year, local.mon, local.mday, local.time);
    }
    
    double DaylightTimeHelper::LocalToUtc(double localTime)
    {
        YMD utc;
        YMDLocalToUtc(localTime, &utc);

        return Js::DateUtilities::TvFromDate(utc.year, utc.mon, utc.mday, utc.time);
    }
} // namespace DateTime
} // namespace PlatformAgnostic
