//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTypeHandler::JITTypeHandler(TypeHandlerIDL * data)
{TRACE_IT(10093);
    CompileAssert(sizeof(JITTypeHandler) == sizeof(TypeHandlerIDL));
}

bool
JITTypeHandler::IsObjectHeaderInlinedTypeHandler() const
{TRACE_IT(10094);
    return m_data.isObjectHeaderInlinedTypeHandler != FALSE;
}

bool
JITTypeHandler::IsLocked() const
{TRACE_IT(10095);
    return m_data.isLocked != FALSE;
}

uint16
JITTypeHandler::GetInlineSlotCapacity() const
{TRACE_IT(10096);
    return m_data.inlineSlotCapacity;
}

uint16
JITTypeHandler::GetOffsetOfInlineSlots() const
{TRACE_IT(10097);
    return m_data.offsetOfInlineSlots;
}

int
JITTypeHandler::GetSlotCapacity() const
{TRACE_IT(10098);
    return m_data.slotCapacity;
}

// TODO: OOP JIT, remove copy/paste code
/* static */
bool
JITTypeHandler::IsTypeHandlerCompatibleForObjectHeaderInlining(const JITTypeHandler * oldTypeHandler, const JITTypeHandler * newTypeHandler)
{TRACE_IT(10099);
    Assert(oldTypeHandler);
    Assert(newTypeHandler);

    return
        oldTypeHandler->GetInlineSlotCapacity() == newTypeHandler->GetInlineSlotCapacity() ||
        (
            oldTypeHandler->IsObjectHeaderInlinedTypeHandler() &&
            newTypeHandler->GetInlineSlotCapacity() ==
            oldTypeHandler->GetInlineSlotCapacity() - Js::DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity()
        );
}
