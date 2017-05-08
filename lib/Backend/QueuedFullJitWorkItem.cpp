//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

QueuedFullJitWorkItem::QueuedFullJitWorkItem(CodeGenWorkItem *const workItem) : workItem(workItem)
{TRACE_IT(15120);
    Assert(workItem->GetJitMode() == ExecutionMode::FullJit);
}

CodeGenWorkItem *QueuedFullJitWorkItem::WorkItem() const
{TRACE_IT(15121);
    return workItem;
}
