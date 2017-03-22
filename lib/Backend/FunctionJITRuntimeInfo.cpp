//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

FunctionJITRuntimeInfo::FunctionJITRuntimeInfo(FunctionJITRuntimeIDL * data) : m_data(*data)
{LOGMEIN("FunctionJITRuntimeInfo.cpp] 8\n");
    CompileAssert(sizeof(FunctionJITRuntimeInfo) == sizeof(FunctionJITRuntimeIDL));
}

intptr_t
FunctionJITRuntimeInfo::GetClonedInlineCache(uint index) const
{LOGMEIN("FunctionJITRuntimeInfo.cpp] 14\n");
    Assert(index < m_data.clonedCacheCount);
    return m_data.clonedInlineCaches[index];
}

bool
FunctionJITRuntimeInfo::HasClonedInlineCaches() const
{LOGMEIN("FunctionJITRuntimeInfo.cpp] 21\n");
    return m_data.clonedInlineCaches != nullptr;
}
