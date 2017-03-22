//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#ifdef ASMJS_PLAT
AsmJsJITInfo::AsmJsJITInfo(AsmJsDataIDL * data) :
    m_data(*data)
{LOGMEIN("AsmJsJITInfo.cpp] 10\n");
    CompileAssert(sizeof(AsmJsJITInfo) == sizeof(AsmJsDataIDL));
}

WAsmJs::TypedSlotInfo
AsmJsJITInfo::GetTypedSlotInfo(WAsmJs::Types type) const
{LOGMEIN("AsmJsJITInfo.cpp] 16\n");
    WAsmJs::TypedSlotInfo info;
    if (type >= 0 && type < WAsmJs::LIMIT)
    {LOGMEIN("AsmJsJITInfo.cpp] 19\n");
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
{LOGMEIN("AsmJsJITInfo.cpp] 31\n");
    return m_data.totalSizeInBytes;
}

Js::ArgSlot
AsmJsJITInfo::GetArgCount() const
{LOGMEIN("AsmJsJITInfo.cpp] 37\n");
    return m_data.argCount;
}

Js::ArgSlot
AsmJsJITInfo::GetArgByteSize() const
{LOGMEIN("AsmJsJITInfo.cpp] 43\n");
    return m_data.argByteSize;
}

Js::AsmJsRetType::Which
AsmJsJITInfo::GetRetType() const
{LOGMEIN("AsmJsJITInfo.cpp] 49\n");
    return static_cast<Js::AsmJsRetType::Which>(m_data.retType);
}

Js::AsmJsVarType::Which *
AsmJsJITInfo::GetArgTypeArray() const
{LOGMEIN("AsmJsJITInfo.cpp] 55\n");
    return reinterpret_cast<Js::AsmJsVarType::Which *>(m_data.argTypeArray);
}

Js::AsmJsVarType::Which
AsmJsJITInfo::GetArgType(Js::ArgSlot argNum) const
{LOGMEIN("AsmJsJITInfo.cpp] 61\n");
    Assert(argNum < GetArgCount());
    return GetArgTypeArray()[argNum];
}

#ifdef ENABLE_WASM
Wasm::WasmSignature *
AsmJsJITInfo::GetWasmSignature(uint index) const
{LOGMEIN("AsmJsJITInfo.cpp] 69\n");
    Assert(index < m_data.wasmSignatureCount);
    return Wasm::WasmSignature::FromIDL(&m_data.wasmSignatures[index]);
}

intptr_t
AsmJsJITInfo::GetWasmSignatureAddr(uint index) const
{LOGMEIN("AsmJsJITInfo.cpp] 76\n");
    Assert(index < m_data.wasmSignatureCount);
    return m_data.wasmSignaturesBaseAddr + index * sizeof(Wasm::WasmSignature);
}
#endif

bool
AsmJsJITInfo::IsHeapBufferConst() const
{LOGMEIN("AsmJsJITInfo.cpp] 84\n");
    return m_data.isHeapBufferConst != FALSE;
}

bool
AsmJsJITInfo::UsesHeapBuffer() const
{LOGMEIN("AsmJsJITInfo.cpp] 90\n");
    return m_data.usesHeapBuffer != FALSE;
}

bool
AsmJsJITInfo::AccessNeedsBoundCheck(uint offset) const
{LOGMEIN("AsmJsJITInfo.cpp] 96\n");
    // Normally, heap has min size of 0x10000, but if you use ChangeHeap, min heap size is increased to 0x1000000
    return offset >= 0x1000000 || (IsHeapBufferConst() && offset >= 0x10000);
}
#endif