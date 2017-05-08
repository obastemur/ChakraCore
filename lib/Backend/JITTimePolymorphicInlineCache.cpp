//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimePolymorphicInlineCache::JITTimePolymorphicInlineCache()
{TRACE_IT(9937);
    CompileAssert(sizeof(JITTimePolymorphicInlineCache) == sizeof(PolymorphicInlineCacheIDL));
}

intptr_t
JITTimePolymorphicInlineCache::GetAddr() const
{TRACE_IT(9938);
    return reinterpret_cast<intptr_t>(PointerValue(m_data.addr));
}

intptr_t
JITTimePolymorphicInlineCache::GetInlineCachesAddr() const
{TRACE_IT(9939);
    return m_data.inlineCachesAddr;
}

uint16
JITTimePolymorphicInlineCache::GetSize() const
{TRACE_IT(9940);
    return m_data.size;
}

