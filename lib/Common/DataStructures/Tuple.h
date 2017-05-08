//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace regex
{
    struct Nothing { };
    template<typename T0, typename T1, typename T2 = Nothing, typename T3 = Nothing>
    class Tuple
    {
        T0 first;
        T1 second;
        T2 third;
        T3 forth;
    public:
        Tuple(T0 first, T1 second)
            : first(first), second(second)
        {TRACE_IT(22445);
            CompileAssert(sizeof(T2)==sizeof(Nothing));
            CompileAssert(sizeof(T3)==sizeof(Nothing));
        }

        Tuple(T0 first, T1 second, T2 third)
            : first(first), second(second), third(third)
        {TRACE_IT(22446);
            CompileAssert(sizeof(T3)==sizeof(Nothing));
        }

        T0 First() const
        {TRACE_IT(22447);
            return first;
        }

        T1 Second() const
        {TRACE_IT(22448);
            return second;
        }

        T2 Third() const
        {TRACE_IT(22449);
            CompileAssert(sizeof(T2)!=sizeof(Nothing));
            return third;
        }

        T3 Forth() const
        {TRACE_IT(22450);
            CompileAssert(sizeof(T3)!=sizeof(Nothing));
            return forth;
        }
    };
}

