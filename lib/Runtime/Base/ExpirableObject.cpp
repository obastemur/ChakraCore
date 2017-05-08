//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

ExpirableObject::ExpirableObject(ThreadContext* threadContext):
    isUsed(false),
    registrationHandle(nullptr)
{TRACE_IT(33828);
    if (threadContext)
    {TRACE_IT(33829);
        threadContext->RegisterExpirableObject(this);
    }
}

void ExpirableObject::Finalize(bool isShutdown)
{TRACE_IT(33830);
    if (!isShutdown && this->registrationHandle != nullptr)
    {TRACE_IT(33831);
        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();

        threadContext->UnregisterExpirableObject(this);
    }
}

void ExpirableObject::Dispose(bool isShutdown)
{TRACE_IT(33832);
    if (!isShutdown && this->registrationHandle == nullptr)
    {TRACE_IT(33833);
        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();
        threadContext->DisposeExpirableObject(this);
    }
}

void ExpirableObject::EnterExpirableCollectMode()
{TRACE_IT(33834);
    this->isUsed = false;
}

bool ExpirableObject::IsObjectUsed()
{TRACE_IT(33835);
    return this->isUsed;
}

void ExpirableObject::SetIsObjectUsed()
{TRACE_IT(33836);
    this->isUsed = true;
}
