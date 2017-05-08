//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "JsrtPch.h"
#include "jsrtHelper.h"
#include "JsrtExternalObject.h"
#include "Types/PathTypeHandler.h"

JsrtExternalType::JsrtExternalType(Js::ScriptContext* scriptContext, JsFinalizeCallback finalizeCallback)
    : Js::DynamicType(
        scriptContext,
        Js::TypeIds_Object,
        scriptContext->GetLibrary()->GetObjectPrototype(),
        nullptr,
        Js::SimplePathTypeHandler::New(scriptContext, scriptContext->GetLibrary()->GetRootPath(), 0, 0, 0, true, true),
        true,
        true)
        , jsFinalizeCallback(finalizeCallback)
{TRACE_IT(28384);
    this->flags |= TypeFlagMask_JsrtExternal;
}

JsrtExternalObject::JsrtExternalObject(JsrtExternalType * type, void *data) :
    slot(data),
    Js::DynamicObject(type, false/* initSlots*/)
{TRACE_IT(28385);
}

/* static */
JsrtExternalObject* JsrtExternalObject::Create(void *data, JsFinalizeCallback finalizeCallback, Js::ScriptContext *scriptContext)
{TRACE_IT(28386);
    Js::DynamicType * dynamicType = scriptContext->GetLibrary()->GetCachedJsrtExternalType(reinterpret_cast<uintptr_t>(finalizeCallback));

    if (dynamicType == nullptr)
    {TRACE_IT(28387);
        dynamicType = RecyclerNew(scriptContext->GetRecycler(), JsrtExternalType, scriptContext, finalizeCallback);
        scriptContext->GetLibrary()->CacheJsrtExternalType(reinterpret_cast<uintptr_t>(finalizeCallback), dynamicType);
    }

    Assert(dynamicType->IsJsrtExternal());
    Assert(dynamicType->GetIsShared());

    return RecyclerNewFinalized(scriptContext->GetRecycler(), JsrtExternalObject, static_cast<JsrtExternalType*>(dynamicType), data);
}

bool JsrtExternalObject::Is(Js::Var value)
{TRACE_IT(28388);
    if (Js::TaggedNumber::Is(value))
    {TRACE_IT(28389);
        return false;
    }

    return (VirtualTableInfo<JsrtExternalObject>::HasVirtualTable(value)) ||
        (VirtualTableInfo<Js::CrossSiteObject<JsrtExternalObject>>::HasVirtualTable(value));
}

JsrtExternalObject * JsrtExternalObject::FromVar(Js::Var value)
{TRACE_IT(28390);
    Assert(Is(value));
    return static_cast<JsrtExternalObject *>(value);
}

void JsrtExternalObject::Finalize(bool isShutdown)
{TRACE_IT(28391);
    JsFinalizeCallback finalizeCallback = this->GetExternalType()->GetJsFinalizeCallback();
    if (nullptr != finalizeCallback)
    {TRACE_IT(28392);
        JsrtCallbackState scope(nullptr);
        finalizeCallback(this->slot);
    }
}

void JsrtExternalObject::Dispose(bool isShutdown)
{TRACE_IT(28393);
}

void * JsrtExternalObject::GetSlotData() const
{TRACE_IT(28394);
    return this->slot;
}

void JsrtExternalObject::SetSlotData(void * data)
{TRACE_IT(28395);
    this->slot = data;
}

Js::DynamicType* JsrtExternalObject::DuplicateType()
{TRACE_IT(28396);
    return RecyclerNew(this->GetScriptContext()->GetRecycler(), JsrtExternalType,
        this->GetExternalType());
}

#if ENABLE_TTD
TTD::NSSnapObjects::SnapObjectType JsrtExternalObject::GetSnapTag_TTD() const
{TRACE_IT(28397);
    return TTD::NSSnapObjects::SnapObjectType::SnapExternalObject;
}

void JsrtExternalObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
{TRACE_IT(28398);
    TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapExternalObject>(objData, nullptr);
}
#endif
