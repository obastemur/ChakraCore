//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "JsrtPch.h"
#include "JsrtThreadService.h"

//
// JsrtThreadService
//

JsrtThreadService::JsrtThreadService() :
    ThreadServiceWrapperBase(),
    nextIdleTick(UINT_MAX)
{TRACE_IT(28533);
}

JsrtThreadService::~JsrtThreadService()
{TRACE_IT(28534);
    Shutdown();
}

bool JsrtThreadService::Initialize(ThreadContext *threadContext)
{TRACE_IT(28535);
    return ThreadServiceWrapperBase::Initialize(threadContext);
}

unsigned int JsrtThreadService::Idle()
{TRACE_IT(28536);
    unsigned int currentTicks = GetTickCount();

    if (currentTicks >= nextIdleTick)
    {TRACE_IT(28537);
        IdleCollect();
    }

    return nextIdleTick;
}

bool JsrtThreadService::OnScheduleIdleCollect(uint ticks, bool /* canScheduleAsTask */)
{TRACE_IT(28538);
    nextIdleTick = GetTickCount() + ticks;
    return true;
}

bool JsrtThreadService::ShouldFinishConcurrentCollectOnIdleCallback()
{TRACE_IT(28539);
    // For the JsrtThreadService, there is no idle task host
    // so we should always try to finish concurrent on entering
    // the idle callback
    return true;
}

void JsrtThreadService::OnFinishIdleCollect()
{TRACE_IT(28540);
    nextIdleTick = UINT_MAX;
}
