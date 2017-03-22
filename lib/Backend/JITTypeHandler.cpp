//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTypeHandler::JITTypeHandler(TypeHandlerIDL * data)
{LOGMEIN("JITTypeHandler.cpp] 8\n");
    CompileAssert(sizeof(JITTypeHandler) == sizeof(TypeHandlerIDL));
}

bool
JITTypeHandler::IsObjectHeaderInlinedTypeHandler() const
{LOGMEIN("JITTypeHandler.cpp] 14\n");
    return m_data.isObjectHeaderInlinedTypeHandler != FALSE;
}

bool
JITTypeHandler::IsLocked() const
{LOGMEIN("JITTypeHandler.cpp] 20\n");
    return m_data.isLocked != FALSE;
}

uint16
JITTypeHandler::GetInlineSlotCapacity() const
{LOGMEIN("JITTypeHandler.cpp] 26\n");
    return m_data.inlineSlotCapacity;
}

uint16
JITTypeHandler::GetOffsetOfInlineSlots() const
{LOGMEIN("JITTypeHandler.cpp] 32\n");
    return m_data.offsetOfInlineSlots;
}

int
JITTypeHandler::GetSlotCapacity() const
{LOGMEIN("JITTypeHandler.cpp] 38\n");
    return m_data.slotCapacity;
}

// TODO: OOP JIT, remove copy/paste code
/* static */
bool
JITTypeHandler::IsTypeHandlerCompatibleForObjectHeaderInlining(const JITTypeHandler * oldTypeHandler, const JITTypeHandler * newTypeHandler)
{LOGMEIN("JITTypeHandler.cpp] 46\n");
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
