//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

QueuedFullJitWorkItem::QueuedFullJitWorkItem(CodeGenWorkItem *const workItem) : workItem(workItem)
{LOGMEIN("QueuedFullJitWorkItem.cpp] 7\n");
    Assert(workItem->GetJitMode() == ExecutionMode::FullJit);
}

CodeGenWorkItem *QueuedFullJitWorkItem::WorkItem() const
{LOGMEIN("QueuedFullJitWorkItem.cpp] 12\n");
    return workItem;
}
