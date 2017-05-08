//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITType::JITType()
{TRACE_IT(10067);
}

JITType::JITType(TypeIDL * data) :
    m_data(*data)
{TRACE_IT(10068);
    CompileAssert(sizeof(JITType) == sizeof(TypeIDL));
}

/* static */
void
JITType::BuildFromJsType(__in Js::Type * jsType, __out JITType * jitType)
{TRACE_IT(10069);
    TypeIDL * data = jitType->GetData();
    data->addr = jsType;
    data->typeId = jsType->GetTypeId();
    data->libAddr = jsType->GetLibrary();
    data->protoAddr = jsType->GetPrototype();
    data->entrypointAddr = (intptr_t)jsType->GetEntryPoint();
    data->propertyCacheAddr = jsType->GetPropertyCache();
    if (Js::DynamicType::Is(jsType->GetTypeId()))
    {TRACE_IT(10070);
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
{TRACE_IT(10071);
    Assert(Js::DynamicType::Is(GetTypeId()));
    return m_data.isShared != FALSE;
}

Js::TypeId
JITType::GetTypeId() const
{TRACE_IT(10072);
    return (Js::TypeId)m_data.typeId;
}

TypeIDL *
JITType::GetData()
{TRACE_IT(10073);
    return &m_data;
}

intptr_t
JITType::GetAddr() const
{TRACE_IT(10074);
    return (intptr_t)PointerValue(m_data.addr);
}

intptr_t
JITType::GetPrototypeAddr() const
{TRACE_IT(10075);
    return (intptr_t)PointerValue(m_data.protoAddr);
}

const JITTypeHandler*
JITType::GetTypeHandler() const
{TRACE_IT(10076);
    return (const JITTypeHandler*)&m_data.handler;
}


template <class TAllocator>
JITTypeHolderBase<TAllocator>::JITTypeHolderBase() :
    t(nullptr)
{TRACE_IT(10077);
}

template <class TAllocator>
JITTypeHolderBase<TAllocator>::JITTypeHolderBase(JITType * t) :
    t(t)
{TRACE_IT(10078);
}

template <class TAllocator>
const JITType *
JITTypeHolderBase<TAllocator>::operator->() const
{TRACE_IT(10079);
    return this->t;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator==(const JITTypeHolderBase& p) const
{TRACE_IT(10080);
    if (this->t != nullptr && p != nullptr)
    {TRACE_IT(10081);
        return this->t->GetAddr() == p->GetAddr();
    }
    return this->t == nullptr && p == nullptr;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator!=(const JITTypeHolderBase& p) const
{TRACE_IT(10082);
    return !(*this == p);
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator==(const std::nullptr_t &p) const
{TRACE_IT(10083);
    return this->t == nullptr;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator!=(const std::nullptr_t &p) const
{TRACE_IT(10084);
    return this->t != nullptr;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator>(const JITTypeHolderBase& p) const
{TRACE_IT(10085);
    if (this->t != nullptr && p != nullptr)
    {TRACE_IT(10086);
        return this->t->GetAddr() > p->GetAddr();
    }
    return false;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator<=(const JITTypeHolderBase& p) const
{TRACE_IT(10087);
    return !(*this > p);
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator>=(const JITTypeHolderBase& p) const
{TRACE_IT(10088);
    if (this->t != nullptr && p != nullptr)
    {TRACE_IT(10089);
        return this->t->GetAddr() >= p->GetAddr();
    }
    return false;
}

template <class TAllocator>
bool
JITTypeHolderBase<TAllocator>::operator<(const JITTypeHolderBase& p) const
{TRACE_IT(10090);
    return !(*this >= p);
}

template class JITTypeHolderBase<void>;
template class JITTypeHolderBase<Recycler>;
