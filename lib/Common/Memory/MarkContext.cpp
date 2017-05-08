//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#if defined(_M_IX86) || defined(_M_X64)
// For prefetch
#include <mmintrin.h>
#endif


MarkContext::MarkContext(Recycler * recycler, PagePool * pagePool) :
    recycler(recycler),
    pagePool(pagePool),
    markStack(pagePool),
    trackStack(pagePool)
{TRACE_IT(24669);
}


MarkContext::~MarkContext()
{TRACE_IT(24670);
#ifdef RECYCLER_MARK_TRACK
    this->markMap = nullptr;
#endif
}

#ifdef RECYCLER_MARK_TRACK
void MarkContext::OnObjectMarked(void* object, void* parent)
{TRACE_IT(24671);
    if (!this->markMap->ContainsKey(object))
    {TRACE_IT(24672);
        this->markMap->AddNew(object, parent);
    }
}
#endif

void MarkContext::Init(uint reservedPageCount)
{TRACE_IT(24673);
    markStack.Init(reservedPageCount);
    trackStack.Init();
}

void MarkContext::Clear()
{TRACE_IT(24674);
    markStack.Clear();
    trackStack.Clear();
}

void MarkContext::Abort()
{TRACE_IT(24675);
    markStack.Abort();
    trackStack.Abort();

    pagePool->ReleaseFreePages();
}


void MarkContext::Release()
{TRACE_IT(24676);
    markStack.Release();
    trackStack.Release();

    pagePool->ReleaseFreePages();
}


uint MarkContext::Split(uint targetCount, __in_ecount(targetCount) MarkContext ** targetContexts)
{TRACE_IT(24677);
    Assert(targetCount > 0 && targetCount <= PageStack<MarkCandidate>::MaxSplitTargets);
    __analysis_assume(targetCount <= PageStack<MarkCandidate>::MaxSplitTargets);

    PageStack<MarkCandidate> * targetStacks[PageStack<MarkCandidate>::MaxSplitTargets];

    for (uint i = 0; i < targetCount; i++)
    {TRACE_IT(24678);
        targetStacks[i] = &targetContexts[i]->markStack;
    }

    return this->markStack.Split(targetCount, targetStacks);
}


void MarkContext::ProcessTracked()
{TRACE_IT(24679);
    if (trackStack.IsEmpty())
    {TRACE_IT(24680);
        return;
    }

    FinalizableObject * trackedObject;
    while (trackStack.Pop(&trackedObject))
    {TRACE_IT(24681);
        MarkTrackedObject(trackedObject);
    }

    Assert(trackStack.IsEmpty());

    trackStack.Release();
}



