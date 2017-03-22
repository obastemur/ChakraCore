//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITType::JITType()
{LOGMEIN("JITType.cpp] 8\n");
}

JITType::JITType(TypeIDL * data) :
    m_data(*data)
{LOGMEIN("JITType.cpp] 13\n");
    CompileAssert(sizeof(JITType) == sizeof(TypeIDL));
}

/* static */
void
JITType::BuildFromJsType(__in Js::Type * jsType, __out JITType * jitType)
{LOGMEIN("JITType.cpp] 20\n");
    TypeIDL * data = jitType->GetData();
    data->addr = jsType;
    data->typeId = jsType->GetTypeId();
    data->libAddr = jsType->GetLibrary();
    data->protoAddr = jsType->GetPrototype();
    data->entrypointAddr = (intptr_t)jsType->GetEntryPoint();
    data->propertyCacheAddr = jsType->GetPropertyCache();
    if (Js::DynamicType::Is(jsType->GetTypeId()))
    {LOGMEIN("JITType.cpp] 29\n");
        Js::DynamicType * dynamicType = static_cast<Js::DynamicType*>(jsType);

        data->isShared = dynamicType->GetIsShared();

        Js::DynamicTypeHandler * handler = dynamicType->GetTypeHandler();
        data->handler.isObjectHeaderInlinedTypeHandler = handler->IsObjectHeaderInlinedTypeHandler();
        data->handler.isLocked = handler->GetIsLocked();
        data->handler.inlineSlotCapacity = handler->GetInlineSlotCapacity();
        data->handler.offsetOfInlineSlots = handler->GetOffsetOfInlineSlots();
        data->handler.slotCapacity = handler->GetSlotCapacity();
    }
}

bool
JITType::IsShared() const
{LOGMEIN("JITType.cpp] 45\n");
    Assert(Js::DynamicType::Is(GetTypeId()));
    return m_data.isShared != FALSE;
}

Js::TypeId
JITType::GetTypeId() const
{LOGMEIN("JITType.cpp] 52\n");
    return (Js::TypeId)m_data.typeId;
}

TypeIDL *
JITType::GetData()
{LOGMEIN("JITType.cpp] 58\n");
    return &m_data;
}

intptr_t
JITType::GetAddr() const
{LOGMEIN("JITType.cpp] 64\n");
    return (intptr_t)PointerValue(m_data.addr);
}

intptr_t
JITType::GetPrototypeAddr() const
{LOGMEIN("JITType.cpp] 70\n");
    return (intptr_t)PointerValue(m_data.protoAddr);
}

const JITTypeHandler*
JITType::GetTypeHandler() const
{LOGMEIN("JITType.cpp] 76\n");
    return (const JITTypeHandler*)&m_data.handler;
}


template <class TAllocator>
JITTypeHolderBase<TAllocator>::JITTypeHolderBase() :
    t(nullptr)
{LOGMEIN("JITType.cpp] 84\n");
}

template <class TAllocator>
JITTypeHolderBase<TAllocator>::JITTypeHolderBase(JITType * t) :
    t(t)
{LOGMEIN("JITType.cpp] 90\n");
}

template <class TAllocator>
const JITType *
JITTypeHolderBase<TAllocator>::operator->() const
{LOGMEIN("JITType.cpp] 96\n");
    return this->t;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator==(const JITTypeHolderBase& p) const
{LOGMEIN("JITType.cpp] 103\n");
    if (this->t != nullptr && p != nullptr)
    {LOGMEIN("JITType.cpp] 105\n");
        return this->t->GetAddr() == p->GetAddr();
    }
    return this->t == nullptr && p == nullptr;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator!=(const JITTypeHolderBase& p) const
{LOGMEIN("JITType.cpp] 114\n");
    return !(*this == p);
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator==(const std::nullptr_t &p) const
{LOGMEIN("JITType.cpp] 121\n");
    return this->t == nullptr;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator!=(const std::nullptr_t &p) const
{LOGMEIN("JITType.cpp] 128\n");
    return this->t != nullptr;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator>(const JITTypeHolderBase& p) const
{LOGMEIN("JITType.cpp] 135\n");
    if (this->t != nullptr && p != nullptr)
    {LOGMEIN("JITType.cpp] 137\n");
        return this->t->GetAddr() > p->GetAddr();
    }
    return false;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator<=(const JITTypeHolderBase& p) const
{LOGMEIN("JITType.cpp] 146\n");
    return !(*this > p);
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator>=(const JITTypeHolderBase& p) const
{LOGMEIN("JITType.cpp] 153\n");
    if (this->t != nullptr && p != nullptr)
    {LOGMEIN("JITType.cpp] 155\n");
        return this->t->GetAddr() >= p->GetAddr();
    }
    return false;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator<(const JITTypeHolderBase& p) const
{LOGMEIN("JITType.cpp] 164\n");
    return !(*this >= p);
}

template class JITTypeHolderBase<void>;
template class JITTypeHolderBase<Recycler>;
