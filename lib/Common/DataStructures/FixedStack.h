//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template<class T, int N>
class FixedStack
{
private:
    T itemList[N];
    int curIndex;

public:
    FixedStack(): curIndex(-1)
    {TRACE_IT(21470);
    }

    void Push(T item)
    {TRACE_IT(21471);
        AssertMsg(curIndex < N - 1, "Stack overflow");
        if (curIndex >= N - 1)
        {TRACE_IT(21472);
            Js::Throw::FatalInternalError();
        }
        this->itemList[++this->curIndex] = item;
    }

    T* Pop()
    {TRACE_IT(21473);
        AssertMsg(curIndex >= 0, "Stack Underflow");
        if (curIndex < 0)
        {TRACE_IT(21474);
            Js::Throw::FatalInternalError();
        }
        return &this->itemList[this->curIndex--];
    }

    T* Peek()
    {TRACE_IT(21475);
        AssertMsg(curIndex >= 0, "No element present");
        if (curIndex < 0)
        {TRACE_IT(21476);
            Js::Throw::FatalInternalError();
        }
        return & this->itemList[this->curIndex];
    }

    int Count()
    {TRACE_IT(21477);
        return 1 + this->curIndex;
    }
};
