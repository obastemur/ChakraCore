//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
    FunctionInfo::FunctionInfo(JavascriptMethod entryPoint, Attributes attributes, LocalFunctionId functionId, FunctionProxy* functionBodyImpl)
        : originalEntryPoint(entryPoint), attributes(attributes), functionBodyImpl(functionBodyImpl), functionId(functionId), compileCount(0)
    {
#if !DYNAMIC_INTERPRETER_THUNK
        Assert(entryPoint != nullptr);
#endif
    }

    FunctionInfo::FunctionInfo(JavascriptMethod entryPoint, _no_write_barrier_tag, Attributes attributes, LocalFunctionId functionId, FunctionProxy* functionBodyImpl)
        : originalEntryPoint(entryPoint), attributes(attributes), functionBodyImpl(FORCE_NO_WRITE_BARRIER_TAG(functionBodyImpl)), functionId(functionId), compileCount(0)
    {
#if !DYNAMIC_INTERPRETER_THUNK
        Assert(entryPoint != nullptr);
#endif
    }

    FunctionInfo::FunctionInfo(FunctionInfo& that)
        : originalEntryPoint(that.originalEntryPoint), attributes(that.attributes),
        functionBodyImpl(FORCE_NO_WRITE_BARRIER_TAG(that.functionBodyImpl)), functionId(that.functionId), compileCount(that.compileCount)
    {

    }

    bool FunctionInfo::Is(void* ptr)
    {
        if(!ptr)
        {TRACE_IT(35726);
            return false;
        }
        return VirtualTableInfo<FunctionInfo>::HasVirtualTable(ptr);
    }

    void FunctionInfo::VerifyOriginalEntryPoint() const
    {TRACE_IT(35727);
        Assert(!this->HasBody() || this->IsDeferredParseFunction() || this->IsDeferredDeserializeFunction() || this->GetFunctionProxy()->HasValidEntryPoint());
    }

    JavascriptMethod
    FunctionInfo::GetOriginalEntryPoint() const
    {TRACE_IT(35728);
        VerifyOriginalEntryPoint();
        return GetOriginalEntryPoint_Unchecked();
    }

    JavascriptMethod FunctionInfo::GetOriginalEntryPoint_Unchecked() const
    {TRACE_IT(35729);
        return originalEntryPoint;
    }

    void FunctionInfo::SetOriginalEntryPoint(const JavascriptMethod originalEntryPoint)
    {TRACE_IT(35730);
        Assert(originalEntryPoint);
        this->originalEntryPoint = originalEntryPoint;
    }

    FunctionBody *
    FunctionInfo::GetFunctionBody() const
    {TRACE_IT(35731);
        return functionBodyImpl == nullptr ? nullptr : functionBodyImpl->GetFunctionBody();
    }

    FunctionInfo::Attributes FunctionInfo::GetAttributes(Js::RecyclableObject * function)
    {TRACE_IT(35732);
        return function->GetTypeId() == Js::TypeIds_Function ?
            Js::JavascriptFunction::FromVar(function)->GetFunctionInfo()->GetAttributes() : Js::FunctionInfo::None;
    }
}
