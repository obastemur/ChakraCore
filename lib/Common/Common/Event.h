//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Event
{
private:
    const HANDLE handle;

public:
    Event(const bool autoReset, const bool signaled = false);

private:
    Event(const Event &) : handle(0)
    {TRACE_IT(18833);
    }

    Event &operator =(const Event &)
    {TRACE_IT(18834);
        return *this;
    }

public:
    ~Event()
    {TRACE_IT(18835);
        CloseHandle(handle);
    }

public:
    HANDLE Handle() const
    {TRACE_IT(18836);
        return handle;
    }

    void Set() const
    {TRACE_IT(18837);
        SetEvent(handle);
    }

    void Reset() const
    {TRACE_IT(18838);
        ResetEvent(handle);
    }

    bool Wait(const unsigned int milliseconds = INFINITE) const;
};
