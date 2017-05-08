//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

IdleDecommitPageAllocator::IdleDecommitPageAllocator(AllocationPolicyManager * policyManager, PageAllocatorType type,
#ifndef JD_PRIVATE
    Js::ConfigFlagsTable& flagTable,
#endif
    uint maxFreePageCount, uint maxIdleFreePageCount,
    bool zeroPages,
#if ENABLE_BACKGROUND_PAGE_FREEING 
    BackgroundPageQueue *  backgroundPageQueue,
#endif
    uint maxAllocPageCount, bool enableWriteBarrier) :
#ifdef IDLE_DECOMMIT_ENABLED
    idleDecommitTryEnterWaitFactor(0),
    hasDecommitTimer(false),
    hadDecommitTimer(false),
#endif
    PageAllocator(policyManager,
#ifndef JD_PRIVATE
        flagTable,
#endif
    type, maxFreePageCount, zeroPages,
#if ENABLE_BACKGROUND_PAGE_FREEING
    backgroundPageQueue,
#endif        
    maxAllocPageCount, 0, false, false, GetCurrentProcess(), enableWriteBarrier),
    maxIdleDecommitFreePageCount(maxIdleFreePageCount),
    maxNonIdleDecommitFreePageCount(maxFreePageCount)
{TRACE_IT(24266);
    // if maxIdle is the same as max free, disable idleDecommit but setting the entry count to 1
    this->idleDecommitEnterCount = (maxIdleFreePageCount == maxFreePageCount);
#ifdef IDLE_DECOMMIT_ENABLED
#if DBG_DUMP
    idleDecommitCount = 0;
#endif
#endif
}


void
IdleDecommitPageAllocator::EnterIdleDecommit()
{TRACE_IT(24267);
    this->idleDecommitEnterCount++;
    if (this->idleDecommitEnterCount != 1)
    {TRACE_IT(24268);
        return;
    }
#ifdef IDLE_DECOMMIT_ENABLED
    cs.Enter();

    this->isUsed = false;
    this->hadDecommitTimer = hasDecommitTimer;
    PAGE_ALLOC_VERBOSE_TRACE(_u("EnterIdleDecommit"));
    if (hasDecommitTimer)
    {TRACE_IT(24269);
        // Cancel the decommit timer
        Assert(this->maxFreePageCount == maxIdleDecommitFreePageCount);
        hasDecommitTimer = false;
        PAGE_ALLOC_TRACE(_u("Cancel Decommit Timer"));
    }
    else
    {TRACE_IT(24270);
        // Switch to maxIdleDecommitFreePageCount
        Assert(this->maxFreePageCount == maxNonIdleDecommitFreePageCount);
        Assert(minFreePageCount == 0);
        this->maxFreePageCount = maxIdleDecommitFreePageCount;
    }

    cs.Leave();

    Assert(!hasDecommitTimer);
#else
    Assert(this->maxFreePageCount == maxNonIdleDecommitFreePageCount);
    this->maxFreePageCount = maxIdleDecommitFreePageCount;
#endif
}

IdleDecommitSignal
IdleDecommitPageAllocator::LeaveIdleDecommit(bool allowTimer)
{TRACE_IT(24271);

    Assert(this->idleDecommitEnterCount > 0);
    Assert(this->maxFreePageCount == maxIdleDecommitFreePageCount);

#ifdef IDLE_DECOMMIT_ENABLED
    Assert(!hasDecommitTimer);
#endif

    this->idleDecommitEnterCount--;
    if (this->idleDecommitEnterCount != 0)
    {TRACE_IT(24272);
        return IdleDecommitSignal_None;
    }

#ifdef IDLE_DECOMMIT_ENABLED
    if (allowTimer)
    {TRACE_IT(24273);
        cs.Enter();

        PAGE_ALLOC_VERBOSE_TRACE(_u("LeaveIdleDecommit"));
        Assert(maxIdleDecommitFreePageCount != maxNonIdleDecommitFreePageCount);

        IdleDecommitSignal idleDecommitSignal = IdleDecommitSignal_None;
        if (freePageCount == 0 && !isUsed && !hadDecommitTimer)
        {TRACE_IT(24274);
            Assert(minFreePageCount == 0);
            Assert(minFreePageCount == debugMinFreePageCount);

            // Nothing to decommit, it isn't used, and there was no timer before.
            // Just switch it back to non idle decommit mode
            this->maxFreePageCount = maxNonIdleDecommitFreePageCount;
        }
        else
        {TRACE_IT(24275);
            UpdateMinFreePageCount();

            hasDecommitTimer = true;
            idleDecommitSignal = IdleDecommitSignal_NeedTimer;

            if (isUsed)
            {TRACE_IT(24276);
                // Reschedule the timer
                decommitTime = ::GetTickCount() + IdleDecommitTimeout;
                PAGE_ALLOC_TRACE( _u("Schedule idle decommit at %d (%d)"), decommitTime, IdleDecommitTimeout);
            }
            else
            {TRACE_IT(24277);
                int timeDiff = (int)decommitTime - ::GetTickCount();
                if (timeDiff < 20)
                {TRACE_IT(24278);
                    idleDecommitSignal = IdleDecommitSignal_NeedSignal;
                }

                PAGE_ALLOC_TRACE(_u("Reschedule idle decommit at %d (%d)"), decommitTime, decommitTime - ::GetTickCount());
            }

        }
        cs.Leave();
        return idleDecommitSignal;
    }
#endif
    this->maxFreePageCount = maxNonIdleDecommitFreePageCount;
    __super::DecommitNow();
    ClearMinFreePageCount();
    return IdleDecommitSignal_None;
}

#ifdef IDLE_DECOMMIT_ENABLED
void
IdleDecommitPageAllocator::DecommitNow(bool all)
{TRACE_IT(24279);
    SuspendIdleDecommit();

    // If we are in non-idle-decommit mode, then always decommit all.
    // Otherwise, we will end up with some un-decommitted pages and get confused later.
    if (maxFreePageCount == maxNonIdleDecommitFreePageCount)
        all = true;

    __super::DecommitNow(all);

    if (all)
    {TRACE_IT(24280);
        if (this->hasDecommitTimer)
        {TRACE_IT(24281);
            Assert(idleDecommitEnterCount == 0);
            Assert(this->maxFreePageCount == maxIdleDecommitFreePageCount);
            this->hasDecommitTimer = false;
            this->maxFreePageCount = maxNonIdleDecommitFreePageCount;
        }
        else
        {TRACE_IT(24282);
            Assert((idleDecommitEnterCount > 0? maxIdleDecommitFreePageCount : maxNonIdleDecommitFreePageCount)
                == this->maxFreePageCount);
        }
        ClearMinFreePageCount();
    }
    else
    {TRACE_IT(24283);
        ResetMinFreePageCount();
    }

    ResumeIdleDecommit();
}

DWORD
IdleDecommitPageAllocator::IdleDecommit()
{TRACE_IT(24284);
    // We can check hasDecommitTimer outside of the lock because when it change to true
    // the Recycler::concurrentIdleDecommitEvent will signal and we try to IdleDecommit again
    // If it change to false, we check again when we acquired the lock
    if (!hasDecommitTimer)
    {TRACE_IT(24285);
        return INFINITE;
    }
    if (!cs.TryEnter())
    {TRACE_IT(24286);
        // Failed to acquire the lock, wait for a variable time.
        PAGE_ALLOC_TRACE(_u("IdleDecommit Retry"));

        // Varies the wait time between 11 - 99
        idleDecommitTryEnterWaitFactor++;
        if (idleDecommitTryEnterWaitFactor >= 10)
        {TRACE_IT(24287);
            idleDecommitTryEnterWaitFactor = 1;
        }
        DWORD waitTime = 11 * idleDecommitTryEnterWaitFactor;
        return waitTime;      // Retry time
    }
    idleDecommitTryEnterWaitFactor = 0;
    DWORD waitTime = INFINITE;
    if (hasDecommitTimer)
    {TRACE_IT(24288);
        Assert(this->maxFreePageCount == maxIdleDecommitFreePageCount);
        int timediff = (int)(decommitTime - ::GetTickCount());
        if (timediff >= 20)   // Ignore time diff is it is < 20 since the system timer doesn't have that high of precision anyways
        {TRACE_IT(24289);
            waitTime = (DWORD)timediff;
        }
        else
        {TRACE_IT(24290);
            // Do the decommit in normal priority so that we don't block the main thread for too long
            PAGE_ALLOC_TRACE(_u("IdleDecommit"));
#if DBG_DUMP
            idleDecommitCount++;
#endif
            __super::DecommitNow();
            hasDecommitTimer = false;
            ClearMinFreePageCount();
            this->maxFreePageCount = maxNonIdleDecommitFreePageCount;
        }
    }
    cs.Leave();
    return waitTime;
}

#endif

void
IdleDecommitPageAllocator::Prime(uint primePageCount)
{TRACE_IT(24291);
    while (this->freePageCount < primePageCount)
    {TRACE_IT(24292);
        PageSegment * segment = AddPageSegment(emptySegments);
        if (segment == nullptr)
        {TRACE_IT(24293);
            return;
        }
        segment->Prime();
    }
}


#if DBG
bool
IdleDecommitPageAllocator::HasMultiThreadAccess() const
{TRACE_IT(24294);
#ifdef IDLE_DECOMMIT_ENABLED
    return this->hasDecommitTimer && !cs.IsLocked();
#else
    return false;
#endif
}

void
IdleDecommitPageAllocator::ShutdownIdleDecommit()
{TRACE_IT(24295);
    // The recycler thread should have died already
    // Just set the state
    idleDecommitEnterCount = 1;
#ifdef IDLE_DECOMMIT_ENABLED
    hasDecommitTimer = false;
#endif
}
#endif

#ifdef IDLE_DECOMMIT_ENABLED
#if DBG_DUMP
void
IdleDecommitPageAllocator::DumpStats() const
{TRACE_IT(24296);
    __super::DumpStats();
    Output::Print(_u("  Idle Decommit Count       : %4d\n"),
        this->idleDecommitCount);
}
#endif
#endif
