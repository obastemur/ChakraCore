//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    // -------------------------------------------------------------------------------------------------------------------------
    // JobProcessor
    // -------------------------------------------------------------------------------------------------------------------------

    template<class TJobManager>
    void JobProcessor::PrioritizeManagerAndWait(TJobManager *const manager, const unsigned int milliseconds)
    {TRACE_IT(19044);
        TemplateParameter::SameOrDerivedFrom<TJobManager, WaitableJobManager> unused;
        Assert(manager);
        Assert(!isClosed);

#if ENABLE_BACKGROUND_JOB_PROCESSOR
        if(processesInBackground)
            static_cast<BackgroundJobProcessor *>(this)->PrioritizeManagerAndWait(manager, milliseconds);
        else
#endif
        {TRACE_IT(19045);
            Assert(!processesInBackground);
            static_cast<ForegroundJobProcessor *>(this)->PrioritizeManagerAndWait(manager, milliseconds);
        }
    }

    template<class TJobManager, class TJobHolder>
    void JobProcessor::AddJobAndProcessProactively(TJobManager *const manager, const TJobHolder holder)
    {TRACE_IT(19046);
        TemplateParameter::SameOrDerivedFrom<TJobManager, JobManager> unused;
        Assert(manager);
        Assert(!isClosed);

#if ENABLE_BACKGROUND_JOB_PROCESSOR
        if(processesInBackground)
            return static_cast<BackgroundJobProcessor *>(this)->AddJobAndProcessProactively(manager, holder);
        else
#endif
        {TRACE_IT(19047);
            Assert(!processesInBackground);
            return static_cast<ForegroundJobProcessor *>(this)->AddJobAndProcessProactively(manager, holder);
        }
    }

    template<class TJobManager, class TJobHolder>
    bool JobProcessor::PrioritizeJob(TJobManager *const manager, const TJobHolder holder, void* function)
    {TRACE_IT(19048);
        TemplateParameter::SameOrDerivedFrom<TJobManager, JobManager> unused;
        Assert(manager);
        Assert(!isClosed);

#if ENABLE_BACKGROUND_JOB_PROCESSOR
        if(processesInBackground)
            return static_cast<BackgroundJobProcessor *>(this)->PrioritizeJob(manager, holder, function);
        else
#endif
        {TRACE_IT(19049);
            Assert(!processesInBackground);
            return static_cast<ForegroundJobProcessor *>(this)->PrioritizeJob(manager, holder, function);
        }
    }

    template<class TJobManager, class TJobHolder>
    void JobProcessor::PrioritizeJobAndWait(TJobManager *const manager, const TJobHolder holder, void* function)
    {TRACE_IT(19050);
        TemplateParameter::SameOrDerivedFrom<TJobManager, WaitableJobManager>();
        Assert(manager);
        Assert(!isClosed);

#if ENABLE_BACKGROUND_JOB_PROCESSOR
        if(processesInBackground)
            static_cast<BackgroundJobProcessor *>(this)->PrioritizeJobAndWait(manager, holder, function);
        else
#endif
        {TRACE_IT(19051);
            Assert(!processesInBackground);
            static_cast<ForegroundJobProcessor *>(this)->PrioritizeJobAndWait(manager, holder, function);
        }
    }

    // -------------------------------------------------------------------------------------------------------------------------
    // ForegroundJobProcessor
    // -------------------------------------------------------------------------------------------------------------------------

    template<class TJobManager, class TJobHolder>
    void ForegroundJobProcessor::AddJobAndProcessProactively(TJobManager *const manager, const TJobHolder holder)
    {TRACE_IT(19052);
        TemplateParameter::SameOrDerivedFrom<TJobManager, JobManager> unused;
        Assert(manager);
        Assert(!IsClosed());

        Job *const job = manager->GetJob(holder);
        Assert(job);

        manager->BeforeWaitForJob(holder);
        JobProcessed(manager, job, Process(job)); // the job may be deleted during this and should not be used afterwards
        if(manager->numJobsAddedToProcessor == 0)
            LastJobProcessed(manager);
        manager->AfterWaitForJob(holder);
    }

    template<class TJobManager>
    void ForegroundJobProcessor::PrioritizeManagerAndWait(TJobManager *const manager, const unsigned int milliseconds)
    {TRACE_IT(19053);
        TemplateParameter::SameOrDerivedFrom<TJobManager, WaitableJobManager> unused;
        Assert(manager);
        Assert(manager->isWaitable);
        Assert(!IsClosed());

        PrioritizeManager(manager);
        if(milliseconds == 0)
            return;

        // We have been given some time to process jobs proactively, so process as many jobs as possible, trying not to exceed
        // the specified amount of time

        Js::Tick startTick = Js::Tick::Now();
        Js::TickDelta endTickDelta = Js::TickDelta::FromMicroseconds((__int64)milliseconds * 1000);
        do
        {TRACE_IT(19054);
            if(manager->numJobsAddedToProcessor != 0)
            {TRACE_IT(19055);
                // Process only jobs from this manager
                Job *job = jobs.Head();
                for(; job && job->Manager() != manager; job = job->Next());
                if(job)
                {TRACE_IT(19056);
                    jobs.Unlink(job);
                    JobProcessed(manager, job, Process(job)); // the job may be deleted during this and should not be used afterwards
                    Assert(manager->numJobsAddedToProcessor != 0);
                    if(--manager->numJobsAddedToProcessor == 0)
                        LastJobProcessed(manager);
                    continue;
                }
            }

            // No jobs in queue, check this job manager for more jobs
            Job *const job = manager->GetJobToProcessProactively();
            if(job)
            {
                JobProcessed(manager, job, Process(job)); // the job may be deleted during this and should not be used afterwards
                continue;
            }

            // No jobs from job managers either
            break;
        } while (milliseconds == INFINITE || Js::Tick::Now() - startTick < endTickDelta);
    }

    template<class TJobManager, class TJobHolder>
    bool ForegroundJobProcessor::PrioritizeJob(TJobManager *const manager, const TJobHolder holder, void* function)
    {TRACE_IT(19057);
        TemplateParameter::SameOrDerivedFrom<TJobManager, JobManager> unused;
        Assert(manager);
        Assert(!IsClosed());

        Job *const job = manager->GetJob(holder);
        if(!job)
            return true; // job was processed
        Assert(job->Manager() == manager);

        if(!manager->WasAddedToJobProcessor(job))
        {TRACE_IT(19058);
            // The job wasn't added for processing, so ask the manager to prioritize it
            manager->Prioritize(job, false, function);
            manager->PrioritizedButNotYetProcessed(job);
            return false;
        }

        jobs.Unlink(job);
        manager->BeforeWaitForJob(holder);
        JobProcessed(manager, job, Process(job)); // the job may be deleted during this and should not be used afterwards
        Assert(manager->numJobsAddedToProcessor != 0);
        if(--manager->numJobsAddedToProcessor == 0)
            LastJobProcessed(manager);
        manager->AfterWaitForJob(holder);
        return true;
    }

    template<class TJobManager, class TJobHolder>
    void ForegroundJobProcessor::PrioritizeJobAndWait(TJobManager *const manager, const TJobHolder holder, void* function)
    {TRACE_IT(19059);
        TemplateParameter::SameOrDerivedFrom<TJobManager, WaitableJobManager>();
        Assert(manager);
        Assert(manager->isWaitable);
        Assert(!IsClosed());

        Job *const job = manager->GetJob(holder);
        if(!job)
            return; // job was processed
        Assert(job->Manager() == manager);

        if(!manager->WasAddedToJobProcessor(job))
        {TRACE_IT(19060);
            // The job wasn't added for processing, so ask the manager to prioritize it and add the job to the job processor
            // since we need to process it and wait for it
            manager->Prioritize(job, /*forceaddToProcessor*/ true, function);
            Assert(manager->WasAddedToJobProcessor(job));
        }

        jobs.Unlink(job);
        manager->BeforeWaitForJob(holder);
        JobProcessed(manager, job, Process(job)); // the job may be deleted during this and should not be used afterwards
        Assert(manager->numJobsAddedToProcessor != 0);
        if(--manager->numJobsAddedToProcessor == 0)
            LastJobProcessed(manager);
        manager->AfterWaitForJob(holder);
    }

    // -------------------------------------------------------------------------------------------------------------------------
    // BackgroundJobProcessor
    // -------------------------------------------------------------------------------------------------------------------------

#if ENABLE_BACKGROUND_JOB_PROCESSOR
    template<class TJobManager, class TJobHolder>
    void BackgroundJobProcessor::AddJobAndProcessProactively(TJobManager *const manager, const TJobHolder holder)
    {TRACE_IT(19061);
        TemplateParameter::SameOrDerivedFrom<TJobManager, JobManager> unused;
        Assert(manager);
        Assert(!IsClosed());

        Job *const job = manager->GetJob(holder);

        AddJob(job, /*prioritize*/ true);
    }

    template<class TJobManager>
    void BackgroundJobProcessor::PrioritizeManagerAndWait(TJobManager *const manager, const unsigned int milliseconds)
    {TRACE_IT(19062);
        TemplateParameter::SameOrDerivedFrom<TJobManager, WaitableJobManager> unused;
        Assert(manager);
        Assert(manager->isWaitable);

        bool waitForQueuedJobs;
        {
            AutoCriticalSection lock(&criticalSection);
            Assert(!IsClosed());

            waitForQueuedJobs = manager->numJobsAddedToProcessor != 0;
            if(waitForQueuedJobs)
            {TRACE_IT(19063);
                PrioritizeManager(manager);

                if (threadService->HasCallback() && AreAllThreadsWaitingForJobs())
                {TRACE_IT(19064);
                    // Thread service denied our background request, so we must process in foreground.
                    waitForQueuedJobs = false;
                }

                Assert(!manager->isWaitingForQueuedJobs);
                manager->isWaitingForQueuedJobs = true;
                manager->queuedJobsProcessed.Reset();
            }
        }

        Js::Tick startTick = Js::Tick::Now();
        Js::TickDelta endTickDelta = Js::TickDelta::FromMicroseconds((__int64)milliseconds * 1000);
        if(waitForQueuedJobs)
        {TRACE_IT(19065);
            // Wait for the event, background thread should be alive
            const bool timeout = !(manager->queuedJobsProcessed.Wait(milliseconds));
            manager->isWaitingForQueuedJobs = false;
            if (timeout)
            {TRACE_IT(19066);
                return;
            }
        }

        if (milliseconds == 0)
        {TRACE_IT(19067);
            return;
        }

        // We have been given some time to process jobs proactively, so process as many jobs as possible, trying not to exceed
        // the specified amount of time
        do
        {TRACE_IT(19068);
            Job * job = NULL;
            if(!waitForQueuedJobs && manager->numJobsAddedToProcessor != 0)
            {TRACE_IT(19069);
                AutoCriticalSection lock(&criticalSection);
                // Process only jobs from this manager
                job = jobs.Head();
                for(; job && job->Manager() != manager; job = job->Next());
                if(job)
                {TRACE_IT(19070);
                    jobs.Unlink(job);
                }
            }

            if (job == NULL)
            {TRACE_IT(19071);
                job = manager->GetJobToProcessProactively();
                if(!job)
                    break;
            }

            const bool succeeded = ForegroundJobProcessor::Process(job);
            {TRACE_IT(19072);
                AutoCriticalSection lock(&criticalSection);
                manager->JobProcessed(job, succeeded); // the job may be deleted during this and should not be used afterwards
                if (!waitForQueuedJobs && manager->numJobsAddedToProcessor != 0)
                {TRACE_IT(19073);
                    Assert(manager->numJobsAddedToProcessor != 0);
                    if(--manager->numJobsAddedToProcessor == 0)
                        LastJobProcessed(manager);
                }
            }
        } while (milliseconds == INFINITE || Js::Tick::Now() - startTick < endTickDelta);
    }

    template<class TJobManager, class TJobHolder>
    bool BackgroundJobProcessor::PrioritizeJob(TJobManager *const manager, const TJobHolder holder, void* function)
    {TRACE_IT(19074);
        TemplateParameter::SameOrDerivedFrom<TJobManager, JobManager> unused;
        Assert(manager);
        Assert(!IsClosed());

        // Fast, nondeterministic check to see if the job was already processed, without using a memory barrier or lock
        Job *job = manager->GetJob(holder);
        if(!job)
        {TRACE_IT(19075);
            // The memory barrier ensures that other state changes made in JobManager::JobProcessed, before nullifying the job
            // in the job holder, are up to date when this function returns
            MemoryBarrier();
            return true; // job was processed
        }

        {
            AutoCriticalSection lock(&criticalSection);
            Assert(!IsClosed());

            // Get the job again inside the lock to deterministically verify whether the job was already processed, and if not,
            // that the job won't be processed until the lock is released
            job = manager->GetJob(holder);
            if (!job)
            {TRACE_IT(19076);
                return true;
            }
            Assert(job->Manager() == manager);

            if(!manager->WasAddedToJobProcessor(job))
            {TRACE_IT(19077);
                // The job wasn't added for processing, so ask the manager to prioritize it
                manager->Prioritize(job, false, function);
                manager->PrioritizedButNotYetProcessed(job);
                return false;
            }

            if (IsBeingProcessed(job))
            {TRACE_IT(19078);
                manager->PrioritizedButNotYetProcessed(job);
                return false;
            }

            // If isWaitingForJobs is true, then we have failed a thread service request.
            // So we want to force processing in-thread here.
            // Otherwise, ask the manager whether to force it in thread or not.
            bool forcedInThread = (threadService->HasCallback() && this->parallelThreadData[0]->isWaitingForJobs);
            if (!forcedInThread && !manager->ShouldProcessInForeground(false, numJobs))
            {TRACE_IT(19079);
                jobs.MoveToBeginning(job);
                manager->PrioritizedButNotYetProcessed(job);
                return false;
            }

            jobs.Unlink(job);
            Assert(numJobs != 0);
            --numJobs;
        }

        manager->BeforeWaitForJob(holder);
        const bool succeeded = ForegroundJobProcessor::Process(job);
        {TRACE_IT(19080);
            AutoCriticalSection lock(&criticalSection);
            JobProcessed(manager, job, succeeded); // the job may be deleted during this and should not be used afterwards
            Assert(manager->numJobsAddedToProcessor != 0);
            if(--manager->numJobsAddedToProcessor == 0)
                LastJobProcessed(manager);
        }
        manager->AfterWaitForJob(holder);
        return true;
    }


    template<class TJobManager, class TJobHolder>
    void BackgroundJobProcessor::PrioritizeJobAndWait(TJobManager *const manager, const TJobHolder holder, void* function)
    {TRACE_IT(19081);
        TemplateParameter::SameOrDerivedFrom<TJobManager, WaitableJobManager>();
        Assert(manager);
        Assert(manager->isWaitable);
        Assert(!IsClosed());

        // Fast, nondeterministic check to see if the job was already processed, without using a memory barrier or lock
        Job *job = manager->GetJob(holder);
        if(!job)
        {TRACE_IT(19082);
            // The memory barrier ensures that other state changes made in JobManager::JobProcessed, before nullifying the job
            // in the job holder, are up to date when this function returns
            MemoryBarrier();
            return; // job was processed
        }

        bool processInForeground = false;
        {
            AutoCriticalSection lock(&criticalSection);
            Assert(!IsClosed());

            // Get the job again inside the lock to deterministically verify whether the job was already processed, and if not,
            // that the job won't be processed until the lock is released
            job = manager->GetJob(holder);
            if(!job)
                return; // job was processed
            Assert(job->Manager() == manager);

            if(!manager->WasAddedToJobProcessor(job))
            {TRACE_IT(19083);
                // The job wasn't added for processing, so ask the manager to prioritize it and add the job to the job processor
                // since we need to process it and wait for it
                manager->Prioritize(job, true /* forceAddJobToProcessor */, function);
                Assert(manager->WasAddedToJobProcessor(job));
                processInForeground = manager->ShouldProcessInForeground(true, numJobs);
                if(processInForeground)
                    jobs.Unlink(job);
            }
            else if (!IsBeingProcessed(job))
            {TRACE_IT(19084);
                // If isWaitingForJobs is true, then we have failed a thread service request.
                // So we want to force processing in-thread here.
                // Otherwise, ask the manager whether to force it in thread or not.
                bool forcedInThread = (threadService->HasCallback() && this->parallelThreadData[0]->isWaitingForJobs);
                if (forcedInThread || manager->ShouldProcessInForeground(true, numJobs))
                {TRACE_IT(19085);
                    jobs.Unlink(job);
                    processInForeground = true;
                }
            }

            if(processInForeground)
            {TRACE_IT(19086);
                Assert(numJobs != 0);
                --numJobs;

                Assert(!jobs.Contains(job));
            }
            else
            {TRACE_IT(19087);
                if (!IsBeingProcessed(job))
                {TRACE_IT(19088);
                    jobs.MoveToBeginning(job);
                }
                Assert(!manager->jobBeingWaitedUpon);
                manager->jobBeingWaitedUpon = job;
                manager->jobBeingWaitedUponProcessed.Reset();
            }
        }

        if(processInForeground)
        {TRACE_IT(19089);
            manager->BeforeWaitForJob(holder);
            const bool succeeded = ForegroundJobProcessor::Process(job);
            {TRACE_IT(19090);
                AutoCriticalSection lock(&criticalSection);
                JobProcessed(manager, job, succeeded); // the job may be deleted during this and should not be used afterwards
                Assert(manager->numJobsAddedToProcessor != 0);
                if(--manager->numJobsAddedToProcessor == 0)
                    LastJobProcessed(manager);
            }
            manager->AfterWaitForJob(holder);
            return;
        }

        manager->BeforeWaitForJob(holder);
        //WaitWithThread(GetThreadDataFromCurrentJob(manager->jobBeingWaitedUpon), manager->jobBeingWaitedUponProcessed);
        manager->jobBeingWaitedUponProcessed.Wait();
        manager->jobBeingWaitedUpon = 0;
        // No need for memory barrier to indicate to the background thread immediately that this job manager is no longer
        // waiting for the job because the wait is indefinite, and a job is processed only once
        manager->AfterWaitForJob(holder);
    }
#endif
}
