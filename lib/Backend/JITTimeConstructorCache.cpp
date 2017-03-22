//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

CompileAssert(sizeof(JITTimeConstructorCache) == sizeof(JITTimeConstructorCacheIDL));

JITTimeConstructorCache::JITTimeConstructorCache(const Js::JavascriptFunction* constructor, Js::ConstructorCache* runtimeCache)
{LOGMEIN("JITTimeConstructorCache.cpp] 10\n");
    Assert(constructor != nullptr);
    Assert(runtimeCache != nullptr);
    m_data.runtimeCacheAddr = runtimeCache;
    m_data.runtimeCacheGuardAddr = const_cast<void*>(runtimeCache->GetAddressOfGuardValue());
    m_data.slotCount = runtimeCache->content.slotCount;
    m_data.inlineSlotCount = runtimeCache->content.inlineSlotCount;
    m_data.skipNewScObject = runtimeCache->content.skipDefaultNewObject;
    m_data.ctorHasNoExplicitReturnValue = runtimeCache->content.ctorHasNoExplicitReturnValue;
    m_data.typeIsFinal = runtimeCache->content.typeIsFinal;
    m_data.isUsed = false;
    m_data.guardedPropOps = 0;
    if (runtimeCache->IsNormal())
    {LOGMEIN("JITTimeConstructorCache.cpp] 23\n");
        JITType::BuildFromJsType(runtimeCache->content.type, (JITType*)&m_data.type);
    }
}

JITTimeConstructorCache::JITTimeConstructorCache(const JITTimeConstructorCache* other)
{LOGMEIN("JITTimeConstructorCache.cpp] 29\n");
    Assert(other != nullptr);
    Assert(other->GetRuntimeCacheAddr() != 0);
    m_data.runtimeCacheAddr = reinterpret_cast<void*>(other->GetRuntimeCacheAddr());
    m_data.runtimeCacheGuardAddr = reinterpret_cast<void*>(other->GetRuntimeCacheGuardAddr());
    m_data.type = *(TypeIDL*)PointerValue(other->GetType().t);
    m_data.slotCount = other->GetSlotCount();
    m_data.inlineSlotCount = other->GetInlineSlotCount();
    m_data.skipNewScObject = other->SkipNewScObject();
    m_data.ctorHasNoExplicitReturnValue = other->CtorHasNoExplicitReturnValue();
    m_data.typeIsFinal = other->IsTypeFinal();
    m_data.isUsed = false;
    m_data.guardedPropOps = 0; // REVIEW: OOP JIT should we copy these when cloning?
}

JITTimeConstructorCache*
JITTimeConstructorCache::Clone(JitArenaAllocator* allocator) const
{LOGMEIN("JITTimeConstructorCache.cpp] 46\n");
    JITTimeConstructorCache* clone = Anew(allocator, JITTimeConstructorCache, this);
    return clone;
}
BVSparse<JitArenaAllocator>*
JITTimeConstructorCache::GetGuardedPropOps() const
{LOGMEIN("JITTimeConstructorCache.cpp] 52\n");
    return (BVSparse<JitArenaAllocator>*)(m_data.guardedPropOps & ~(intptr_t)1);
}

void
JITTimeConstructorCache::EnsureGuardedPropOps(JitArenaAllocator* allocator)
{LOGMEIN("JITTimeConstructorCache.cpp] 58\n");
    if (GetGuardedPropOps() == nullptr)
    {LOGMEIN("JITTimeConstructorCache.cpp] 60\n");
        m_data.guardedPropOps = (intptr_t)Anew(allocator, BVSparse<JitArenaAllocator>, allocator);
        m_data.guardedPropOps |= 1; // tag it to prevent false positive after the arena address reuse in recycler
    }
}

void
JITTimeConstructorCache::SetGuardedPropOp(uint propOpId)
{LOGMEIN("JITTimeConstructorCache.cpp] 68\n");
    Assert(GetGuardedPropOps() != nullptr);
    GetGuardedPropOps()->Set(propOpId);
}

void
JITTimeConstructorCache::AddGuardedPropOps(const BVSparse<JitArenaAllocator>* propOps)
{LOGMEIN("JITTimeConstructorCache.cpp] 75\n");
    Assert(GetGuardedPropOps() != nullptr);
    GetGuardedPropOps()->Or(propOps);
}

intptr_t
JITTimeConstructorCache::GetRuntimeCacheAddr() const
{LOGMEIN("JITTimeConstructorCache.cpp] 82\n");
    return reinterpret_cast<intptr_t>(PointerValue(m_data.runtimeCacheAddr));
}

intptr_t
JITTimeConstructorCache::GetRuntimeCacheGuardAddr() const
{LOGMEIN("JITTimeConstructorCache.cpp] 88\n");
    return reinterpret_cast<intptr_t>(PointerValue(m_data.runtimeCacheGuardAddr));
}

JITTypeHolder
JITTimeConstructorCache::GetType() const
{LOGMEIN("JITTimeConstructorCache.cpp] 94\n");
    return JITTypeHolder((JITType*)&m_data.type);
}

int
JITTimeConstructorCache::GetSlotCount() const
{LOGMEIN("JITTimeConstructorCache.cpp] 100\n");
    return m_data.slotCount;
}

int16
JITTimeConstructorCache::GetInlineSlotCount() const
{LOGMEIN("JITTimeConstructorCache.cpp] 106\n");
    return m_data.inlineSlotCount;
}

bool
JITTimeConstructorCache::SkipNewScObject() const
{LOGMEIN("JITTimeConstructorCache.cpp] 112\n");
    return m_data.skipNewScObject != FALSE;
}

bool
JITTimeConstructorCache::CtorHasNoExplicitReturnValue() const
{LOGMEIN("JITTimeConstructorCache.cpp] 118\n");
    return m_data.ctorHasNoExplicitReturnValue != FALSE;
}

bool
JITTimeConstructorCache::IsTypeFinal() const
{LOGMEIN("JITTimeConstructorCache.cpp] 124\n");
    return m_data.typeIsFinal != FALSE;
}

bool
JITTimeConstructorCache::IsUsed() const
{LOGMEIN("JITTimeConstructorCache.cpp] 130\n");
    return m_data.isUsed != FALSE;
}

// TODO: OOP JIT, does this need to flow back?
void
JITTimeConstructorCache::SetUsed(bool val)
{LOGMEIN("JITTimeConstructorCache.cpp] 137\n");
    m_data.isUsed = val;
}

JITTimeConstructorCacheIDL *
JITTimeConstructorCache::GetData()
{LOGMEIN("JITTimeConstructorCache.cpp] 143\n");
    return &m_data;
}

