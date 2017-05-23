//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#ifndef CC_PAL_INC_CCSPINLOCK_H
#define CC_PAL_INC_CCSPINLOCK_H

class CCSpinLock
{
    long   enterCount;
    bool   shouldTrackThreadId;
    size_t threadId;
    char   lock;
public:
    CCSpinLock(bool trackThreadId = false):
      enterCount(0),
      lock(0),
      shouldTrackThreadId(trackThreadId),
      threadId(0) { }

    void Enter();
    bool TryEnter();
    void Leave();
    bool IsLocked() const;
    bool IsLockedByAnyThread();
};

#endif // CC_PAL_INC_CCSPINLOCK_H
