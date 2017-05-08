//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "WasmReaderPch.h"

#ifdef ENABLE_WASM

namespace Wasm
{
    WasmElementSegment::WasmElementSegment(ArenaAllocator * alloc, const UINT32 index, const WasmNode initExpr, const UINT32 numElem) :
        m_alloc(alloc),
        m_index(index),
        m_offsetExpr(initExpr),
        m_numElem(numElem),
        m_offset(0),
        m_elemIdx(0),
        m_elems(nullptr)
    {TRACE_IT(68398);}

    void
    WasmElementSegment::Init(const Js::WebAssemblyModule& module)
    {TRACE_IT(68399);
        Assert(m_numElem > 0);
        m_elems = AnewArray(m_alloc, UINT32, m_numElem);
        memset(m_elems, Js::Constants::UninitializedValue, m_numElem * sizeof(UINT32));
    }

    void
    WasmElementSegment::AddElement(const UINT32 funcIndex, const Js::WebAssemblyModule& module)
    {TRACE_IT(68400);
        if (m_elems == nullptr)
        {TRACE_IT(68401);
            Init(module);
        }
        Assert(m_elemIdx < m_numElem);
        m_elems[m_elemIdx++] = funcIndex;
    }

    UINT32
    WasmElementSegment::GetElement(const UINT32 tableIndex) const
    {TRACE_IT(68402);
        Assert(m_elems != nullptr);
        return m_elems[tableIndex];
    }
} // namespace Wasm

#endif // ENABLE_WASM
