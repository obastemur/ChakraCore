//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace PlatformAgnostic
{
namespace Numbers
{
    class Utility
    {
#ifndef ENABLE_GLOBALIZATION
        class NumbersLocale
        {
            CHAR_T localeThousands;
            CHAR_T localeDecimal;
            CHAR_T localeNegativeSign;

            int maxDigitsAfterDecimals;
            CHAR_T defaultDecimalDot;
            CHAR_T defaultDecimalComma;

        public:

            NumbersLocale();

            inline int   GetMaxDigitsAfterDecimals() { return maxDigitsAfterDecimals; }
            inline CHAR_T GetLocaleThousands()        { return localeThousands; }
            inline bool  HasLocaleThousands()        { return localeThousands != 0; }
            inline CHAR_T GetLocaleDecimal()          { return localeDecimal; }
            inline CHAR_T GetLocaleNegativeSign()     { return localeNegativeSign; }

            inline bool  IsDecimalPoint(const CHAR_T wc)
                                                     { return wc == defaultDecimalDot
                                                           || wc == defaultDecimalComma; }
        };

        // non-ICU implementation keeps user locale intact process wide
        // xplat-todo: While implementing ICU option, make both per context.
        static NumbersLocale numbersLocale;
#endif
    public:

        static size_t NumberToDefaultLocaleString(const CHAR_T *number_string,
                                                  const size_t length,
                                                  CHAR_T *buffer,
                                                  const size_t pre_allocated_buffer_size);
    };
} // namespace Numbers
} // namespace PlatformAgnostic
