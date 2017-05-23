//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#ifndef CC_PAL_INC_CCSPINLOCK_H
#define CC_PAL_INC_CCSPINLOCK_H

class CCSpinLock
{
    unsigned int  enterCount;
    bool          shouldTrackThreadId;
    char          lock;
    size_t        threadId;
public:
    void Reset(bool trackThread = false)
    {
        enterCount = 0;
        lock = 0;
        shouldTrackThreadId = trackThread;
        threadId = 0;
    }

    CCSpinLock(DWORD _) { Reset(true); }
    CCSpinLock(bool trackThread = false) { Reset(trackThread); }

    void Enter();
    bool TryEnter();
    void Leave();
    bool IsLocked() const;
    bool IsLockedByAnyThread();
};

#endif // CC_PAL_INC_CCSPINLOCK_H
