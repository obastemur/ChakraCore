//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#ifndef CC_PAL_INC_CCSPINLOCK_H
#define CC_PAL_INC_CCSPINLOCK_H

template<bool shouldTrackThreadId>
class CCSpinLock
{
    char         mutexPtr[64];
#if DEBUG
    size_t         threadId; // to track IsLocked
#endif

public:
    void Reset(unsigned int _ = 0);
    CCSpinLock(unsigned int _ = 0)
    {
        Reset(_);
    }

    ~CCSpinLock();

    void Enter();
    bool TryEnter();
    void Leave();
    bool IsLocked() const;
};

#endif // CC_PAL_INC_CCSPINLOCK_H
