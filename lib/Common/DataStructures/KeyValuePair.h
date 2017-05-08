//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <class TKey, class TValue> struct KeyValuePair
    {
    private:
        TKey key;
        TValue value;

    public:
        KeyValuePair()
        {TRACE_IT(21740);
        }

        KeyValuePair(TKey key, TValue value)
        {TRACE_IT(21741);
            this->key = key;
            this->value = value;
        }

        KeyValuePair(const KeyValuePair& other)
            : key(other.key), value(other.value)
        {TRACE_IT(21742);}

        TKey Key() {TRACE_IT(21743); return key; }
        const TKey Key() const {TRACE_IT(21744); return key; }

        TValue Value() {TRACE_IT(21745); return value; }
        const TValue Value() const {TRACE_IT(21746); return value; }
    };

}
