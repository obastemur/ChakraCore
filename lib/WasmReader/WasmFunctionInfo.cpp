//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "WasmReaderPch.h"

#ifdef ENABLE_WASM

namespace Wasm
{

WasmFunctionInfo::WasmFunctionInfo(ArenaAllocator * alloc, WasmSignature* signature, uint32 number) : 
    m_alloc(alloc),
    m_signature(signature),
    m_body(nullptr),
    m_name(nullptr),
    m_customReader(nullptr),
    m_nameLength(0),
    m_number(number),
    m_locals(alloc, signature->GetParamCount())
#if DBG_DUMP
    , importedFunctionReference(nullptr)
#endif
{
    for (uint32 i = 0; i < signature->GetParamCount(); ++i)
    {TRACE_IT(68405);
        m_locals.Add(Wasm::Local(signature->GetParam(i)));
    }
}

void
WasmFunctionInfo::AddLocal(WasmTypes::WasmType type, uint count)
{TRACE_IT(68406);
    for (uint i = 0; i < count; ++i)
    {TRACE_IT(68407);
        m_locals.Add(Wasm::Local(type));
    }
}

Local
WasmFunctionInfo::GetLocal(uint index) const
{TRACE_IT(68408);
    if (index < GetLocalCount())
    {TRACE_IT(68409);
        return m_locals.ItemInBuffer(index);
    }
    return WasmTypes::Limit;
}

Local
WasmFunctionInfo::GetParam(uint index) const
{TRACE_IT(68410);
    return m_signature->GetParam(index);
}

WasmTypes::WasmType
WasmFunctionInfo::GetResultType() const
{TRACE_IT(68411);
    return m_signature->GetResultType();
}

uint32
WasmFunctionInfo::GetLocalCount() const
{TRACE_IT(68412);
    return m_locals.Count();
}

uint32
WasmFunctionInfo::GetParamCount() const
{TRACE_IT(68413);
    return m_signature->GetParamCount();
}


} // namespace Wasm
#endif // ENABLE_WASM
