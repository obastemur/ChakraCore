//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // These function needs to be in INL file for static lib
#if INT32VAR
    inline bool RecyclableObject::Is(Var aValue)
    {TRACE_IT(66998);
        AssertMsg(aValue != nullptr, "RecyclableObject::Is aValue is null");

        return (((uintptr_t)aValue) >> VarTag_Shift) == 0;
    }
#else
    inline bool RecyclableObject::Is(Var aValue)
    {TRACE_IT(66999);
        AssertMsg(aValue != nullptr, "RecyclableObject::Is aValue is null");

        return (((uintptr_t)aValue) & AtomTag) == AtomTag_Object;
    }
#endif

    inline RecyclableObject* RecyclableObject::FromVar(const Js::Var aValue)
    {
        AssertMsg(AtomTag_Object == 0, "Ensure GC objects do not need to be marked");
        AssertMsg(Is(aValue), "Ensure instance is a RecyclableObject");
        AssertMsg(!TaggedNumber::Is(aValue), "Tagged value being used as RecyclableObject");

        return reinterpret_cast<RecyclableObject *>(aValue);
    }

    inline TypeId RecyclableObject::GetTypeId() const
    {TRACE_IT(67001);
        return this->GetType()->GetTypeId();
    }

    inline JavascriptLibrary* RecyclableObject::GetLibrary() const
    {TRACE_IT(67002);
        return this->GetType()->GetLibrary();
    }

    inline ScriptContext* RecyclableObject::GetScriptContext() const
    {TRACE_IT(67003);
        return this->GetLibrary()->GetScriptContext();
    }

    inline BOOL RecyclableObject::CanHaveInterceptors() const
    {TRACE_IT(67004);
#if !defined(USED_IN_STATIC_LIB)
        Assert(this->DbgCanHaveInterceptors() == this->GetType()->CanHaveInterceptors());
#endif
        return this->GetType()->CanHaveInterceptors();
    }
};
