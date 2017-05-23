//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "pal.h"
#include <sched.h>

// do not use PAL internals
#ifdef DEBUG
#define AssertMessage(condition, message) \
  if (!condition) { PRINT_ERROR("%s\n", message); fflush(stderr); abort(); }
#else
#define AssertMessage(_, __)
#endif

#define IF_NOT_LOCKED(TASK)                             \
    size_t currentThreadId = -1;                        \
    if (shouldTrackThreadId)                            \
    {                                                   \
        currentThreadId = GetCurrentThreadId();         \
        if (threadId == currentThreadId) return true;   \
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
    if (shouldTrackThreadId) threadId = currentThreadId;

void CCSpinLock::Enter()
{
    IF_NOT_LOCKED(sched_yield())
}

bool CCSpinLock::TryEnter()
{
    IF_NOT_LOCKED(return false)
//  ELSE
    return true;
}

void CCSpinLock::Leave()
{
    if (shouldTrackThreadId) threadId = 0;
    __sync_lock_release(&lock);
}

bool CCSpinLock::IsLocked()
{
    return threadId == GetCurrentThreadId();
}

bool CCSpinLock::IsLockedByAnyThread()
{
    AssertMessage(shouldTrackThreadId,
      "CCSpinLock::IsLockedByAnyThread Called! shouldTrackThreadId(false)");

    return threadId != 0;
}
