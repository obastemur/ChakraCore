//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "pal/thread.hpp"
#include "pal/cs.hpp"
#include "pal_assert.h"

#include <sched.h>
#include <pthread.h>

void CCSpinLock::Enter()
{
    size_t currentThreadId = -1, spin = 0;
    if (shouldTrackThreadId)
    {
        currentThreadId = GetCurrentThreadId();
        if (threadId == currentThreadId)
        {
            _ASSERTE(enterCount > 0);
            enterCount++;
            return;
        }
    }

    while(__sync_lock_test_and_set(&lock, 1))
    {
        while(lock)
        {
            if (++spin > 11)
            {
                sched_yield();
                spin = 0;
            }
        }
    }

    if (shouldTrackThreadId)
    {
        threadId = GetCurrentThreadId();
        _ASSERTE(enterCount == 0);
        enterCount = 1;
    }
}

bool CCSpinLock::TryEnter()
{
    int spin = 0;
    bool locked = true;

    while(__sync_lock_test_and_set(&lock, 1))
    {
        while(lock)
        {
            if (++spin > 11)
            {
               locked = false;
               goto FAIL;
            }
        }
    }

FAIL:

    if (shouldTrackThreadId)
    {
        size_t currentThreadId = GetCurrentThreadId();
        if (locked)
        {
            threadId = currentThreadId;
            _ASSERTE(enterCount == 0);
            enterCount = 1;
        }
        else if (threadId == currentThreadId)
        {
            _ASSERTE(enterCount > 0);
            enterCount++;
            locked = true;
        }
    }

    return locked;
}

void CCSpinLock::Leave()
{
    if (shouldTrackThreadId)
    {
        _ASSERTE(threadId == GetCurrentThreadId() && "Something is terribly wrong.");
        _ASSERTE(enterCount > 0);
        if (--enterCount == 0)
        {
            threadId = 0;
        }
        else
        {
            return;
        }
    }
    __sync_lock_release(&lock);
}

bool CCSpinLock::IsLocked() const
{
    return threadId == GetCurrentThreadId();
}

bool CCSpinLock::IsLockedByAnyThread()
{
    _ASSERTE(shouldTrackThreadId &&
      "CCSpinLock::IsLockedByAnyThread Called! shouldTrackThreadId(false)");

    return threadId != 0;
}
