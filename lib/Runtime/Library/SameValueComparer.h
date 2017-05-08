//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename Key, bool zero>
    struct SameValueComparerCommon
    {
        static bool Equals(Key, Key) { static_assert(false, "Can only use SameValueComparer with Var as the key type"); }
        static hash_t GetHashCode(Key) { static_assert(false, "Can only use SameValueComparer with Var as the key type"); }
    };

    template <typename Key> using SameValueComparer = SameValueComparerCommon<Key, false>;
    template <typename Key> using SameValueZeroComparer = SameValueComparerCommon<Key, true>;

    template <bool zero>
    struct SameValueComparerCommon<Var, zero>
    {
        static bool Equals(Var x, Var y)
        {TRACE_IT(62850);
            if (zero)
            {TRACE_IT(62851);
                return JavascriptConversion::SameValueZero(x, y);
            }
            else
            {TRACE_IT(62852);
                return JavascriptConversion::SameValue(x, y);
            }
        }

        static hash_t HashDouble(double d)
        {TRACE_IT(62853);
            if (JavascriptNumber::IsNan(d))
            {TRACE_IT(62854);
                return 0;
            }

            if (zero)
            {TRACE_IT(62855);
                // SameValueZero treats -0 and +0 the same, so normalize to get same hash code
                if (JavascriptNumber::IsNegZero(d))
                {TRACE_IT(62856);
                    d = 0.0;
                }
            }

            __int64 v = *(__int64*)&d;
            return (uint)v ^ (uint)(v >> 32);
        }

        static hash_t GetHashCode(Var i)
        {TRACE_IT(62857);
            switch (JavascriptOperators::GetTypeId(i))
            {
            case TypeIds_Integer:
                // int32 can be fully represented in a double, so hash it as a double
                // to ensure that tagged ints hash to the same value as JavascriptNumbers.
                return HashDouble((double)TaggedInt::ToInt32(i));

            case TypeIds_Int64Number:
            case TypeIds_UInt64Number:
                {TRACE_IT(62858);
                    __int64 v = JavascriptInt64Number::FromVar(i)->GetValue();
                    double d = (double) v;
                    if (v != (__int64) d)
                    {TRACE_IT(62859);
                        // this int64 is too large to represent in a double
                        // and thus will never be equal to a double so hash it
                        // as an int64
                        return (uint)v ^ (uint)(v >> 32);
                    }

                    // otherwise hash it as a double
                    return HashDouble(d);
                }

            case TypeIds_Number:
                {TRACE_IT(62860);
                    double d = JavascriptNumber::GetValue(i);
                    return HashDouble(d);
                }

            case TypeIds_String:
                {TRACE_IT(62861);
                    JavascriptString* v = JavascriptString::FromVar(i);
                    return JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(v->GetString(), v->GetLength());
                }

            default:
                return RecyclerPointerComparer<Var>::GetHashCode(i);
            }
        }
    };
}
