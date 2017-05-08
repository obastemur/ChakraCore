//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#ifdef ASMJS_PLAT
AsmJsJITInfo::AsmJsJITInfo(AsmJsDataIDL * data) :
    m_data(*data)
{TRACE_IT(26);
    CompileAssert(sizeof(AsmJsJITInfo) == sizeof(AsmJsDataIDL));
}

WAsmJs::TypedSlotInfo
AsmJsJITInfo::GetTypedSlotInfo(WAsmJs::Types type) const
{TRACE_IT(27);
    WAsmJs::TypedSlotInfo info;
    if (type >= 0 && type < WAsmJs::LIMIT)
    {TRACE_IT(28);
        info.byteOffset = m_data.typedSlotInfos[type].byteOffset;
        info.constCount = m_data.typedSlotInfos[type].constCount;
        info.constSrcByteOffset = m_data.typedSlotInfos[type].constSrcByteOffset;
        info.tmpCount = m_data.typedSlotInfos[type].tmpCount;
        info.varCount = m_data.typedSlotInfos[type].varCount;
    }
    return info;
}

int
AsmJsJITInfo::GetTotalSizeInBytes() const
{TRACE_IT(29);
    return m_data.totalSizeInBytes;
}

Js::ArgSlot
AsmJsJITInfo::GetArgCount() const
{TRACE_IT(30);
    return m_data.argCount;
}

Js::ArgSlot
AsmJsJITInfo::GetArgByteSize() const
{TRACE_IT(31);
    return m_data.argByteSize;
}

Js::AsmJsRetType::Which
AsmJsJITInfo::GetRetType() const
{TRACE_IT(32);
    return static_cast<Js::AsmJsRetType::Which>(m_data.retType);
}

Js::AsmJsVarType::Which *
AsmJsJITInfo::GetArgTypeArray() const
{TRACE_IT(33);
    return reinterpret_cast<Js::AsmJsVarType::Which *>(m_data.argTypeArray);
}

Js::AsmJsVarType::Which
AsmJsJITInfo::GetArgType(Js::ArgSlot argNum) const
{TRACE_IT(34);
    Assert(argNum < GetArgCount());
    return GetArgTypeArray()[argNum];
}

#ifdef ENABLE_WASM
Wasm::WasmSignature *
AsmJsJITInfo::GetWasmSignature(uint index) const
{TRACE_IT(35);
    Assert(index < m_data.wasmSignatureCount);
    return Wasm::WasmSignature::FromIDL(&m_data.wasmSignatures[index]);
}

intptr_t
AsmJsJITInfo::GetWasmSignatureAddr(uint index) const
{TRACE_IT(36);
    Assert(index < m_data.wasmSignatureCount);
    return m_data.wasmSignaturesBaseAddr + index * sizeof(Wasm::WasmSignature);
}
#endif

bool
AsmJsJITInfo::IsHeapBufferConst() const
{TRACE_IT(37);
    return m_data.isHeapBufferConst != FALSE;
}

bool
AsmJsJITInfo::UsesHeapBuffer() const
{TRACE_IT(38);
    return m_data.usesHeapBuffer != FALSE;
}

bool
AsmJsJITInfo::AccessNeedsBoundCheck(uint offset) const
{TRACE_IT(39);
    // Normally, heap has min size of 0x10000, but if you use ChangeHeap, min heap size is increased to 0x1000000
    return offset >= 0x1000000 || (IsHeapBufferConst() && offset >= 0x10000);
}
#endif