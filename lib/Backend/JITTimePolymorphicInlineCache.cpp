//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimePolymorphicInlineCache::JITTimePolymorphicInlineCache()
{LOGMEIN("JITTimePolymorphicInlineCache.cpp] 8\n");
    CompileAssert(sizeof(JITTimePolymorphicInlineCache) == sizeof(PolymorphicInlineCacheIDL));
}

intptr_t
JITTimePolymorphicInlineCache::GetAddr() const
{LOGMEIN("JITTimePolymorphicInlineCache.cpp] 14\n");
    return reinterpret_cast<intptr_t>(PointerValue(m_data.addr));
}

intptr_t
JITTimePolymorphicInlineCache::GetInlineCachesAddr() const
{LOGMEIN("JITTimePolymorphicInlineCache.cpp] 20\n");
    return m_data.inlineCachesAddr;
}

uint16
JITTimePolymorphicInlineCache::GetSize() const
{LOGMEIN("JITTimePolymorphicInlineCache.cpp] 26\n");
    return m_data.size;
}

