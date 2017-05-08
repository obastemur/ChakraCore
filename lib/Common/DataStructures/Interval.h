//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace regex
{
    struct Interval
    {
        Field(int) begin;
        Field(int) end;

    public:
        Interval(): begin(0), end(0)
        {TRACE_IT(21733);
        }

        Interval(int start) : begin(start), end(start)
        {TRACE_IT(21734);
        }

        Interval(int start, int end) : begin(start), end(end)
        {TRACE_IT(21735);
        }

        inline int Begin() {TRACE_IT(21736); return begin; }
        inline void Begin(int value) {TRACE_IT(21737); begin = value; }

        inline int End() {TRACE_IT(21738); return end; }
        inline void End(int value) {TRACE_IT(21739); end = value; }

        bool Includes(int value) const;
        bool Includes(Interval other) const;
        int CompareTo(Interval other);
        static int Compare(Interval x, Interval y);
        bool Equals(Interval other);
        static bool Equals(Interval x, Interval y);
        int GetHashCode();
        static int GetHashCode(Interval item);
    };
}
