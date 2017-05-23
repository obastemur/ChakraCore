//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "pal/thread.hpp"
#include "pal/cs.hpp"
#include "pal_assert.h"

#include <sched.h>
#include <pthread.h>

#define TRY_ENTER_IF_NOT_(TASK)                         \
    size_t currentThreadId = -1;                        \
    if (shouldTrackThreadId)                            \
    {                                                   \
        currentThreadId = GetCurrentThreadId();         \
        if (threadId == currentThreadId)                \
        {                                               \
            enterCount++;                               \
            _ASSERTE(enterCount > 0);                   \
            return true;                                \
        }                                               \
    }                                                   \
                                                        \
    int spin = 0;                                       \
    while(__sync_lock_test_and_set(&lock, 1))           \
    {                                                   \
        while(lock)                                     \
        {                                               \
            if (++spin > 11)                            \
            {                                           \
                TASK;                                   \
                spin = 0;                               \
            }                                           \
        }                                               \
    }                                                   \
                                                        \
    if (shouldTrackThreadId)                            \
    {                                                   \
        threadId = currentThreadId;                     \
        _ASSERTE(enterCount == 0);                      \
        enterCount = 1;                                 \
    }

void CCSpinLock::Enter()
{
    TRY_ENTER_IF_NOT_(sched_yield())
}

bool CCSpinLock::TryEnter()
{
    TRY_ENTER_IF_NOT_(return false)
//  ELSE
    return true;
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
