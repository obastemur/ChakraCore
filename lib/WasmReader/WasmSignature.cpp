//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "WasmReaderPch.h"

#ifdef ENABLE_WASM

namespace Wasm
{

WasmSignature::WasmSignature() :
    m_resultType(WasmTypes::Void),
    m_id(Js::Constants::UninitializedValue),
    m_paramSize(Js::Constants::UninitializedValue),
    m_params(nullptr),
    m_paramsCount(0),
    m_shortSig(Js::Constants::InvalidSignature)
{TRACE_IT(68436);
}

void
WasmSignature::AllocateParams(uint32 count, Recycler * recycler)
{TRACE_IT(68437);
    if (count > 0)
    {TRACE_IT(68438);
        m_params = RecyclerNewArrayLeafZ(recycler, Local, count);
    }
    m_paramsCount = count;
}

void
WasmSignature::SetParam(WasmTypes::WasmType type, uint32 index)
{TRACE_IT(68439);
    if (index >= GetParamCount())
    {TRACE_IT(68440);
        throw WasmCompilationException(_u("Parameter %d out of range (max %d)"), index, GetParamCount());
    }
    m_params[index] = Local(type);
}

void
WasmSignature::SetResultType(WasmTypes::WasmType type)
{TRACE_IT(68441);
    Assert(m_resultType == WasmTypes::Void);
    m_resultType = type;
}

void
WasmSignature::SetSignatureId(uint32 id)
{TRACE_IT(68442);
    Assert(m_id == Js::Constants::UninitializedValue);
    m_id = id;
}

Local
WasmSignature::GetParam(uint index) const
{TRACE_IT(68443);
    if (index >= GetParamCount())
    {TRACE_IT(68444);
        throw WasmCompilationException(_u("Parameter %d out of range (max %d)"), index, GetParamCount());
    }
    return m_params[index];
}

WasmTypes::WasmType
WasmSignature::GetResultType() const
{TRACE_IT(68445);
    return m_resultType;
}

uint32
WasmSignature::GetParamCount() const
{TRACE_IT(68446);
    return m_paramsCount;
}

uint32
WasmSignature::GetSignatureId() const
{TRACE_IT(68447);
    return m_id;
}

size_t
WasmSignature::GetShortSig() const
{TRACE_IT(68448);
    return m_shortSig;
}

bool
WasmSignature::IsEquivalent(const WasmSignature* sig) const
{TRACE_IT(68449);
    if (m_shortSig != Js::Constants::InvalidSignature)
    {TRACE_IT(68450);
        return sig->GetShortSig() == m_shortSig;
    }
    if (GetResultType() == sig->GetResultType() &&
        GetParamCount() == sig->GetParamCount() &&
        GetParamsSize() == sig->GetParamsSize())
    {TRACE_IT(68451);
        return GetParamCount() == 0 || memcmp(m_params, sig->m_params, GetParamCount() * sizeof(Local)) == 0;
    }
    return false;
}

uint32 WasmSignature::GetParamSize(uint index) const
{TRACE_IT(68452);
    switch (GetParam(index))
    {
    case WasmTypes::F32:
    case WasmTypes::I32:
        CompileAssert(sizeof(float) == sizeof(int32));
#ifdef _M_X64
        // on x64, we always alloc (at least) 8 bytes per arguments
        return sizeof(void*);
#elif _M_IX86
        return sizeof(int32);
#else
        Assert(UNREACHED);
#endif
        break;
    case WasmTypes::F64:
    case WasmTypes::I64:
        CompileAssert(sizeof(double) == sizeof(int64));
        return sizeof(int64);
        break;
    default:
        throw WasmCompilationException(_u("Invalid param type"));
    }
}

void
WasmSignature::FinalizeSignature()
{TRACE_IT(68453);
    Assert(m_paramSize == Js::Constants::UninitializedValue);
    Assert(m_shortSig == Js::Constants::InvalidSignature);

    m_paramSize = 0;
    for (uint32 i = 0; i < GetParamCount(); ++i)
    {TRACE_IT(68454);
        m_paramSize += GetParamSize(i);
    }

    CompileAssert(Local::Limit - 1 <= 4);
    CompileAssert(Local::Void == 0);

    // 3 bits for result type, 2 for each arg
    // we don't need to reserve a sentinel bit because there is no result type with value of 7
    int sigSize = 3 + 2 * GetParamCount();
    if (sigSize <= sizeof(m_shortSig) << 3)
    {TRACE_IT(68455);
        m_shortSig = (m_shortSig << 3) | m_resultType;
        for (uint32 i = 0; i < GetParamCount(); ++i)
        {TRACE_IT(68456);
            // we can use 2 bits per arg by dropping void
            m_shortSig = (m_shortSig << 2) | (m_params[i] - 1);
        }
    }
}

uint32
WasmSignature::GetParamsSize() const
{TRACE_IT(68457);
    return m_paramSize;
}

WasmSignature *
WasmSignature::FromIDL(WasmSignatureIDL* sig)
{TRACE_IT(68458);
    // must update WasmSignatureIDL when changing WasmSignature
    CompileAssert(sizeof(Wasm::WasmSignature) == sizeof(WasmSignatureIDL));
    CompileAssert(offsetof(Wasm::WasmSignature, m_resultType) == offsetof(WasmSignatureIDL, resultType));
    CompileAssert(offsetof(Wasm::WasmSignature, m_id) == offsetof(WasmSignatureIDL, id));
    CompileAssert(offsetof(Wasm::WasmSignature, m_paramSize) == offsetof(WasmSignatureIDL, paramSize));
    CompileAssert(offsetof(Wasm::WasmSignature, m_paramsCount) == offsetof(WasmSignatureIDL, paramsCount));
    CompileAssert(offsetof(Wasm::WasmSignature, m_params) == offsetof(WasmSignatureIDL, params));
    CompileAssert(offsetof(Wasm::WasmSignature, m_shortSig) == offsetof(WasmSignatureIDL, shortSig));
    CompileAssert(sizeof(Local) == sizeof(int));

    return reinterpret_cast<WasmSignature*>(sig);
}

void
WasmSignature::Dump()
{TRACE_IT(68459);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    Output::Print(_u("("));
    for(uint32 i = 0; i < this->GetParamCount(); i++)
    {TRACE_IT(68460);
        if(i != 0)
        {TRACE_IT(68461);
            Output::Print(_u(", "));
        }
        switch(this->GetParam(i)) {
            case WasmTypes::WasmType::Void:
                Output::Print(_u("void"));
                break;
            case WasmTypes::WasmType::I32:
                Output::Print(_u("i32"));
                break;
            case WasmTypes::WasmType::I64:
                Output::Print(_u("i64"));
                break;
            case WasmTypes::WasmType::F32:
                Output::Print(_u("f32"));
                break;
            case WasmTypes::WasmType::F64:
                Output::Print(_u("f64"));
                break;
        }
    }
    Output::Print(_u(") -> "));
    switch(this->GetResultType()) {
        case WasmTypes::WasmType::Void:
            Output::Print(_u("void"));
            break;
        case WasmTypes::WasmType::I32:
            Output::Print(_u("i32"));
            break;
        case WasmTypes::WasmType::I64:
            Output::Print(_u("i64"));
            break;
        case WasmTypes::WasmType::F32:
            Output::Print(_u("f32"));
            break;
        case WasmTypes::WasmType::F64:
            Output::Print(_u("f64"));
            break;
    }
#endif
}

} // namespace Wasm

#endif // ENABLE_WASM
