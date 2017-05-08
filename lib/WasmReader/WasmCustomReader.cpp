//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "WasmReaderPch.h"

#ifdef ENABLE_WASM
namespace Wasm
{

WasmCustomReader::WasmCustomReader(ArenaAllocator* alloc) : m_nodes(alloc)
{TRACE_IT(68387);

}

void
WasmCustomReader::SeekToFunctionBody(FunctionBodyReaderInfo readerInfo)
{TRACE_IT(68388);
    m_state = 0;
}

bool
WasmCustomReader::IsCurrentFunctionCompleted() const
{TRACE_IT(68389);
    return m_state >= (uint32)m_nodes.Count();
}

WasmOp
WasmCustomReader::ReadExpr()
{TRACE_IT(68390);
    if (m_state < (uint32)m_nodes.Count())
    {TRACE_IT(68391);
        m_currentNode = m_nodes.Item(m_state++);
        return m_currentNode.op;
    }
    return wbEnd;
}

void WasmCustomReader::FunctionEnd()
{TRACE_IT(68392);
}

void WasmCustomReader::AddNode(WasmNode node)
{TRACE_IT(68393);
    m_nodes.Add(node);
}

};
#endif