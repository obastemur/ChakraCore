//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <typename T>
    class NoCaseComparer;

    template <>
    class NoCaseComparer<JsUtil::CharacterBuffer<CHAR_T>>
    {
    public:
        static bool Equals(JsUtil::CharacterBuffer<CHAR_T> const& x, JsUtil::CharacterBuffer<CHAR_T> const& y);
        static uint GetHashCode(JsUtil::CharacterBuffer<CHAR_T> const& i);
    private:
        static int Compare(JsUtil::CharacterBuffer<CHAR_T> const& x, JsUtil::CharacterBuffer<CHAR_T> const& y);
    };

}
