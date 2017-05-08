//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

CompileAssert(sizeof(JITTimeConstructorCache) == sizeof(JITTimeConstructorCacheIDL));

JITTimeConstructorCache::JITTimeConstructorCache(const Js::JavascriptFunction* constructor, Js::ConstructorCache* runtimeCache)
{TRACE_IT(9751);
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
    {TRACE_IT(9752);
        JITType::BuildFromJsType(runtimeCache->content.type, (JITType*)&m_data.type);
    }
}

JITTimeConstructorCache::JITTimeConstructorCache(const JITTimeConstructorCache* other)
{TRACE_IT(9753);
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
{TRACE_IT(9754);
    JITTimeConstructorCache* clone = Anew(allocator, JITTimeConstructorCache, this);
    return clone;
}
BVSparse<JitArenaAllocator>*
JITTimeConstructorCache::GetGuardedPropOps() const
{TRACE_IT(9755);
    return (BVSparse<JitArenaAllocator>*)(m_data.guardedPropOps & ~(intptr_t)1);
}

void
JITTimeConstructorCache::EnsureGuardedPropOps(JitArenaAllocator* allocator)
{TRACE_IT(9756);
    if (GetGuardedPropOps() == nullptr)
    {TRACE_IT(9757);
        m_data.guardedPropOps = (intptr_t)Anew(allocator, BVSparse<JitArenaAllocator>, allocator);
        m_data.guardedPropOps |= 1; // tag it to prevent false positive after the arena address reuse in recycler
    }
}

void
JITTimeConstructorCache::SetGuardedPropOp(uint propOpId)
{TRACE_IT(9758);
    Assert(GetGuardedPropOps() != nullptr);
    GetGuardedPropOps()->Set(propOpId);
}

void
JITTimeConstructorCache::AddGuardedPropOps(const BVSparse<JitArenaAllocator>* propOps)
{TRACE_IT(9759);
    Assert(GetGuardedPropOps() != nullptr);
    GetGuardedPropOps()->Or(propOps);
}

intptr_t
JITTimeConstructorCache::GetRuntimeCacheAddr() const
{TRACE_IT(9760);
    return reinterpret_cast<intptr_t>(PointerValue(m_data.runtimeCacheAddr));
}

intptr_t
JITTimeConstructorCache::GetRuntimeCacheGuardAddr() const
{TRACE_IT(9761);
    return reinterpret_cast<intptr_t>(PointerValue(m_data.runtimeCacheGuardAddr));
}

JITTypeHolder
JITTimeConstructorCache::GetType() const
{TRACE_IT(9762);
    return JITTypeHolder((JITType*)&m_data.type);
}

int
JITTimeConstructorCache::GetSlotCount() const
{TRACE_IT(9763);
    return m_data.slotCount;
}

int16
JITTimeConstructorCache::GetInlineSlotCount() const
{TRACE_IT(9764);
    return m_data.inlineSlotCount;
}

bool
JITTimeConstructorCache::SkipNewScObject() const
{TRACE_IT(9765);
    return m_data.skipNewScObject != FALSE;
}

bool
JITTimeConstructorCache::CtorHasNoExplicitReturnValue() const
{TRACE_IT(9766);
    return m_data.ctorHasNoExplicitReturnValue != FALSE;
}

bool
JITTimeConstructorCache::IsTypeFinal() const
{TRACE_IT(9767);
    return m_data.typeIsFinal != FALSE;
}

bool
JITTimeConstructorCache::IsUsed() const
{TRACE_IT(9768);
    return m_data.isUsed != FALSE;
}

// TODO: OOP JIT, does this need to flow back?
void
JITTimeConstructorCache::SetUsed(bool val)
{TRACE_IT(9769);
    m_data.isUsed = val;
}

JITTimeConstructorCacheIDL *
JITTimeConstructorCache::GetData()
{TRACE_IT(9770);
    return &m_data;
}

