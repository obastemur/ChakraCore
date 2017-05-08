//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#include "Base/ThreadServiceWrapperBase.h"

ThreadServiceWrapperBase::ThreadServiceWrapperBase() :
    threadContext(nullptr),
    needIdleCollect(false),
    inIdleCollect(false),
    hasScheduledIdleCollect(false),
    shouldScheduleIdleCollectOnExitIdle(false),
    forceIdleCollectOnce(false)
{TRACE_IT(37831);
}

bool ThreadServiceWrapperBase::Initialize(ThreadContext *newThreadContext)
{TRACE_IT(37832);
    if (newThreadContext == nullptr)
    {TRACE_IT(37833);
        return false;
    }
    threadContext = newThreadContext;
    threadContext->SetThreadServiceWrapper(this);
    return true;
}

void ThreadServiceWrapperBase::Shutdown()
{TRACE_IT(37834);
    if (hasScheduledIdleCollect)
    {TRACE_IT(37835);
#if DBG
        // Fake the inIdleCollect to get pass asserts in FinishIdleCollect
        inIdleCollect = true;
#endif
        FinishIdleCollect(FinishReason::FinishReasonNormal);
    }
}

bool ThreadServiceWrapperBase::ScheduleIdleCollect(uint ticks, bool scheduleAsTask)
{TRACE_IT(37836);
    Assert(!threadContext->IsInScript());

    // We should schedule have called this in one of two cases:
    //  1) Either needIdleCollect is true- in which case, we should schedule one
    //  2) Or ScheduleNextCollectionOnExit was called when needIdleCollect was true, but we didn't schedule
    //      one because we were at the time in one. Later, as we unwound, we might have set needIdleCollect to false
    //      but because we had noted that we needed to schedule a collect, we would end up coming into this function
    //      so allow for that
    Assert(needIdleCollect || shouldScheduleIdleCollectOnExitIdle || threadContext->GetRecycler()->CollectionInProgress());

    if (!CanScheduleIdleCollect())
    {TRACE_IT(37837);
        return false;
    }

    if (hasScheduledIdleCollect)
    {TRACE_IT(37838);
        return true;
    }

    if (OnScheduleIdleCollect(ticks, scheduleAsTask))
    {TRACE_IT(37839);
        JS_ETW(EventWriteJSCRIPT_GC_IDLE_START(this));
        IDLE_COLLECT_VERBOSE_TRACE(_u("ScheduledIdleCollect- Set hasScheduledIdleCollect\n"));

        hasScheduledIdleCollect = true;
        return true;
    }
    else
    {TRACE_IT(37840);
        IDLE_COLLECT_TRACE(_u("Idle timer setup failed\n"));
        FinishIdleCollect(FinishReason::FinishReasonIdleTimerSetupFailed);
        return false;
    }
}

bool ThreadServiceWrapperBase::IdleCollect()
{TRACE_IT(37841);
    Assert(hasScheduledIdleCollect);
    IDLE_COLLECT_VERBOSE_TRACE(_u("IdleCollect- reset hasScheduledIdleCollect\n"));
    hasScheduledIdleCollect = false;

    // Don't do anything and kill the timer if we are called recursively or if we are in script
    if (inIdleCollect || threadContext->IsInScript())
    {TRACE_IT(37842);
        FinishIdleCollect(FinishReason::FinishReasonNormal);
        return hasScheduledIdleCollect;
    }

    // If during idle collect we determine that we need to schedule another
    // idle collect, this gets flipped to true
    shouldScheduleIdleCollectOnExitIdle = false;

    AutoBooleanToggle autoInIdleCollect(&inIdleCollect);
    Recycler* recycler = threadContext->GetRecycler();
#if ENABLE_CONCURRENT_GC
    // Finish concurrent on timer heart beat if needed
    // We wouldn't try to finish if we need to schedule
    // an idle task to finish the collection
    if (this->ShouldFinishConcurrentCollectOnIdleCallback() && recycler->FinishConcurrent<FinishConcurrentOnIdle>())
    {TRACE_IT(37843);
        IDLE_COLLECT_TRACE(_u("Idle callback: finish concurrent\n"));
        JS_ETW(EventWriteJSCRIPT_GC_IDLE_CALLBACK_FINISH(this));
    }
#endif

    while (true)
    {TRACE_IT(37844);
        // If a GC is still happening, just wait for the next heart beat
        if (recycler->CollectionInProgress())
        {
            ScheduleIdleCollect(IdleTicks, true /* schedule as task */);
            break;
        }

        // If there no more need of idle collect, then cancel the timer
        if (!needIdleCollect)
        {TRACE_IT(37845);
            FinishIdleCollect(FinishReason::FinishReasonNormal);
            break;
        }

        int timeDiff = tickCountNextIdleCollection - GetTickCount();

        // See if we pass the time for the next scheduled Idle GC
        if (timeDiff > 0)
        {
            // Not time yet, wait for the next heart beat
            ScheduleIdleCollect(IdleTicks, false /* not schedule as task */);

            IDLE_COLLECT_TRACE(_u("Idle callback: nop until next collection: %d\n"), timeDiff);
            break;
        }

        // activate an idle collection
        IDLE_COLLECT_TRACE(_u("Idle callback: collection: %d\n"), timeDiff);
        JS_ETW(EventWriteJSCRIPT_GC_IDLE_CALLBACK_NEWCOLLECT(this));

        needIdleCollect = false;
        recycler->CollectNow<CollectOnScriptIdle>();
    }

    if (shouldScheduleIdleCollectOnExitIdle)
    {
        ScheduleIdleCollect(IdleTicks, false /* not schedule as task */);
    }

    return hasScheduledIdleCollect;
}

void ThreadServiceWrapperBase::FinishIdleCollect(ThreadServiceWrapperBase::FinishReason reason)
{TRACE_IT(37846);
    Assert(reason == FinishReason::FinishReasonIdleTimerSetupFailed ||
        reason == FinishReason::FinishReasonTaskComplete ||
        inIdleCollect || threadContext->IsInScript() || !threadContext->GetRecycler()->CollectionInProgress());

    IDLE_COLLECT_VERBOSE_TRACE(_u("FinishIdleCollect- Reset hasScheduledIdleCollect\n"));
    hasScheduledIdleCollect = false;
    needIdleCollect = false;

    OnFinishIdleCollect();

    IDLE_COLLECT_TRACE(_u("Idle timer finished\n"));
    JS_ETW(EventWriteJSCRIPT_GC_IDLE_FINISHED(this));
}

bool ThreadServiceWrapperBase::ScheduleNextCollectOnExit()
{TRACE_IT(37847);
    Assert(!threadContext->IsInScript());
    Assert(!needIdleCollect || hasScheduledIdleCollect);

    Recycler* recycler = threadContext->GetRecycler();
#if ENABLE_CONCURRENT_GC
    recycler->FinishConcurrent<FinishConcurrentOnExitScript>();
#endif

#ifdef RECYCLER_TRACE
    bool oldNeedIdleCollect = needIdleCollect;

    if (forceIdleCollectOnce)
    {TRACE_IT(37848);
        IDLE_COLLECT_VERBOSE_TRACE(_u("Need to force one idle collection\n"));
    }
#endif

    needIdleCollect = forceIdleCollectOnce || recycler->ShouldIdleCollectOnExit();

    if (needIdleCollect)
    {TRACE_IT(37849);
        // Set up when we will do the idle decommit
        tickCountNextIdleCollection = GetTickCount() + IdleTicks;

        IDLE_COLLECT_VERBOSE_TRACE(_u("Idle on exit collect %s: %d\n"), (oldNeedIdleCollect ? _u("rescheduled") : _u("scheduled")),
            tickCountNextIdleCollection - GetTickCount());

        JS_ETW(EventWriteJSCRIPT_GC_IDLE_SCHEDULED(this));
    }
    else
    {TRACE_IT(37850);
        IDLE_COLLECT_VERBOSE_TRACE(_u("Idle on exit collect %s\n"), oldNeedIdleCollect ? _u("cancelled") : _u("not scheduled"));
        if (!recycler->CollectionInProgress())
        {TRACE_IT(37851);
            // We collected and finished, no need to ensure the idle collect call back.
            return true;
        }

        IDLE_COLLECT_VERBOSE_TRACE(_u("Idle on exit collect %s\n"), hasScheduledIdleCollect || oldNeedIdleCollect ? _u("reschedule finish") : _u("schedule finish"));
    }

    // Don't schedule the call back if we are already in idle call back, as we don't do anything on recursive call anyways
    // IdleCollect will schedule one if necessary
    if (inIdleCollect)
    {TRACE_IT(37852);
        shouldScheduleIdleCollectOnExitIdle = true;
        return true;
    }
    else
    {TRACE_IT(37853);
        return ScheduleIdleCollect(IdleTicks, false /* not schedule as task */);
    }
}

void ThreadServiceWrapperBase::ClearForceOneIdleCollection()
{TRACE_IT(37854);
    IDLE_COLLECT_VERBOSE_TRACE(_u("Clearing force idle collect flag\n"));

    this->forceIdleCollectOnce = false;
}

void ThreadServiceWrapperBase::SetForceOneIdleCollection()
{TRACE_IT(37855);
    IDLE_COLLECT_VERBOSE_TRACE(_u("Setting force idle collect flag\n"));

    this->forceIdleCollectOnce = true;
}

void ThreadServiceWrapperBase::ScheduleFinishConcurrent()
{TRACE_IT(37856);
    Assert(!threadContext->IsInScript());
    Assert(threadContext->GetRecycler()->CollectionInProgress());

    if (!this->inIdleCollect)
    {TRACE_IT(37857);
        IDLE_COLLECT_VERBOSE_TRACE(_u("Idle collect %s\n"), needIdleCollect ? _u("reschedule finish") : _u("scheduled finish"));
        this->needIdleCollect = false;
        ScheduleIdleCollect(IdleFinishTicks, true /* schedule as task */);
    }
}
