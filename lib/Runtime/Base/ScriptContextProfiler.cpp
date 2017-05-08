//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#ifdef PROFILE_EXEC
#include "Base/ScriptContextProfiler.h"

namespace Js
{
    ULONG
    ScriptContextProfiler::AddRef()
    {TRACE_IT(36840);
        return refcount++;
    }

    ULONG
    ScriptContextProfiler::Release()
    {TRACE_IT(36841);
        ULONG count = --refcount;
        if (count == 0)
        {TRACE_IT(36842);
            if (recycler != nullptr && this->profiler == recycler->GetProfiler())
            {TRACE_IT(36843);
                recycler->SetProfiler(nullptr, nullptr);
            }
            NoCheckHeapDelete(this);
        }
        return count;
    }

    ScriptContextProfiler::ScriptContextProfiler() :
        refcount(1), profilerArena(nullptr), profiler(nullptr), backgroundRecyclerProfilerArena(nullptr), backgroundRecyclerProfiler(nullptr), recycler(nullptr), pageAllocator(nullptr), next(nullptr)
    {TRACE_IT(36844);
    }

    void
    ScriptContextProfiler::Initialize(PageAllocator * pageAllocator, Recycler * recycler)
    {TRACE_IT(36845);
        Assert(!IsInitialized());
        profilerArena = HeapNew(ArenaAllocator, _u("Profiler"), pageAllocator, Js::Throw::OutOfMemory);
        profiler = Anew(profilerArena, Profiler, profilerArena);
        if (recycler)
        {TRACE_IT(36846);
            backgroundRecyclerProfilerArena = recycler->AddBackgroundProfilerArena();
            backgroundRecyclerProfiler = Anew(profilerArena, Profiler, backgroundRecyclerProfilerArena);

#if DBG
            //backgroundRecyclerProfiler is allocated from background and its guaranteed to assert below if we don't disable thread access check.
            backgroundRecyclerProfiler->alloc->GetPageAllocator()->SetDisableThreadAccessCheck();
#endif

            backgroundRecyclerProfiler->Begin(Js::AllPhase);

#if DBG
            backgroundRecyclerProfiler->alloc->GetPageAllocator()->SetEnableThreadAccessCheck();
#endif
        }
        profiler->Begin(Js::AllPhase);

        this->recycler = recycler;
    }

    void
    ScriptContextProfiler::ProfilePrint(Js::Phase phase)
    {TRACE_IT(36847);
        if (!IsInitialized())
        {TRACE_IT(36848);
            return;
        }
        profiler->End(Js::AllPhase);
        profiler->Print(phase);
        if (this->backgroundRecyclerProfiler)
        {TRACE_IT(36849);
            this->backgroundRecyclerProfiler->End(Js::AllPhase);
            this->backgroundRecyclerProfiler->Print(phase);
            this->backgroundRecyclerProfiler->Begin(Js::AllPhase);
        }
        profiler->Begin(Js::AllPhase);
    }

    ScriptContextProfiler::~ScriptContextProfiler()
    {TRACE_IT(36850);
        if (profilerArena)
        {TRACE_IT(36851);
            HeapDelete(profilerArena);
        }

        if (recycler && backgroundRecyclerProfilerArena)
        {TRACE_IT(36852);
#if DBG
            //We are freeing from main thread, disable thread check assert.
            backgroundRecyclerProfilerArena->GetPageAllocator()->SetDisableThreadAccessCheck();
#endif
            recycler->ReleaseBackgroundProfilerArena(backgroundRecyclerProfilerArena);
        }
    }

    void
    ScriptContextProfiler::ProfileBegin(Js::Phase phase)
    {TRACE_IT(36853);
        Assert(IsInitialized());
        this->profiler->Begin(phase);
    }

    void
    ScriptContextProfiler::ProfileEnd(Js::Phase phase)
    {TRACE_IT(36854);
        Assert(IsInitialized());
        this->profiler->End(phase);
    }

    void
    ScriptContextProfiler::ProfileSuspend(Js::Phase phase, Js::Profiler::SuspendRecord * suspendRecord)
    {TRACE_IT(36855);
        Assert(IsInitialized());
        this->profiler->Suspend(phase, suspendRecord);
    }

    void
    ScriptContextProfiler::ProfileResume(Js::Profiler::SuspendRecord * suspendRecord)
    {TRACE_IT(36856);
        Assert(IsInitialized());
        this->profiler->Resume(suspendRecord);
    }

    void
    ScriptContextProfiler::ProfileMerge(ScriptContextProfiler * profiler)
    {TRACE_IT(36857);
        Assert(IsInitialized());
        this->profiler->Merge(profiler->profiler);
    }
}
#endif
