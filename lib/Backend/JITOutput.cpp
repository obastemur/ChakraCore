//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITOutput::JITOutput(JITOutputIDL * outputData) :
    m_outputData(outputData),
    m_inProcAlloc(nullptr),
    m_func(nullptr)
{LOGMEIN("JITOutput.cpp] 11\n");
}

void
JITOutput::SetHasJITStackClosure()
{LOGMEIN("JITOutput.cpp] 16\n");
    m_outputData->hasJittedStackClosure = true;
}

void
JITOutput::SetVarSlotsOffset(int32 offset)
{LOGMEIN("JITOutput.cpp] 22\n");
    m_outputData->localVarSlotsOffset = offset;
}

void
JITOutput::SetVarChangedOffset(int32 offset)
{LOGMEIN("JITOutput.cpp] 28\n");
    m_outputData->localVarChangedOffset = offset;
}

void
JITOutput::SetHasBailoutInstr(bool val)
{LOGMEIN("JITOutput.cpp] 34\n");
    m_outputData->hasBailoutInstr = val;
}

void
JITOutput::SetArgUsedForBranch(uint8 param)
{LOGMEIN("JITOutput.cpp] 40\n");
    Assert(param > 0);
    Assert(param < Js::Constants::MaximumArgumentCountForConstantArgumentInlining);
    m_outputData->argUsedForBranch |= (1 << (param - 1));
}

void
JITOutput::SetFrameHeight(uint val)
{LOGMEIN("JITOutput.cpp] 48\n");
    m_outputData->frameHeight = val;
}

void
JITOutput::RecordThrowMap(Js::ThrowMapEntry * throwMap, uint mapCount)
{LOGMEIN("JITOutput.cpp] 54\n");
    m_outputData->throwMapOffset = NativeCodeData::GetDataTotalOffset(throwMap);
    m_outputData->throwMapCount = mapCount;
}

uint16
JITOutput::GetArgUsedForBranch() const
{LOGMEIN("JITOutput.cpp] 61\n");
    return m_outputData->argUsedForBranch;
}

intptr_t
JITOutput::GetCodeAddress() const
{LOGMEIN("JITOutput.cpp] 67\n");
    return (intptr_t)m_outputData->codeAddress;
}

void
JITOutput::SetCodeAddress(intptr_t addr)
{LOGMEIN("JITOutput.cpp] 73\n");
    m_outputData->codeAddress = addr;
}

size_t
JITOutput::GetCodeSize() const
{LOGMEIN("JITOutput.cpp] 79\n");
    return (size_t)m_outputData->codeSize;
}

ushort
JITOutput::GetPdataCount() const
{LOGMEIN("JITOutput.cpp] 85\n");
    return m_outputData->pdataCount;
}

ushort
JITOutput::GetXdataSize() const
{LOGMEIN("JITOutput.cpp] 91\n");
    return m_outputData->xdataSize;
}

EmitBufferAllocation<VirtualAllocWrapper, PreReservedVirtualAllocWrapper> *
JITOutput::RecordInProcNativeCodeSize(Func *func, uint32 bytes, ushort pdataCount, ushort xdataSize)
{LOGMEIN("JITOutput.cpp] 97\n");
    m_func = func;

#if defined(_M_ARM32_OR_ARM64)
    bool canAllocInPreReservedHeapPageSegment = false;
#else
    bool canAllocInPreReservedHeapPageSegment = m_func->CanAllocInPreReservedHeapPageSegment();
#endif

    BYTE *buffer = nullptr;
    m_inProcAlloc = m_func->GetInProcCodeGenAllocators()->emitBufferManager.AllocateBuffer(bytes, &buffer, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, true);

    if (buffer == nullptr)
    {LOGMEIN("JITOutput.cpp] 110\n");
        Js::Throw::OutOfMemory();
    }
    m_outputData->codeAddress = (intptr_t)buffer;
    m_outputData->codeSize = bytes;
    m_outputData->pdataCount = pdataCount;
    m_outputData->xdataSize = xdataSize;
    m_outputData->isInPrereservedRegion = m_inProcAlloc->inPrereservedRegion;
    return m_inProcAlloc;
}

#if ENABLE_OOP_NATIVE_CODEGEN
EmitBufferAllocation<SectionAllocWrapper, PreReservedSectionAllocWrapper> *
JITOutput::RecordOOPNativeCodeSize(Func *func, uint32 bytes, ushort pdataCount, ushort xdataSize)
{LOGMEIN("JITOutput.cpp] 124\n");
    m_func = func;

#if defined(_M_ARM32_OR_ARM64)
    bool canAllocInPreReservedHeapPageSegment = false;
#else
    bool canAllocInPreReservedHeapPageSegment = m_func->CanAllocInPreReservedHeapPageSegment();
#endif

    BYTE *buffer = nullptr;
    m_oopAlloc = m_func->GetOOPCodeGenAllocators()->emitBufferManager.AllocateBuffer(bytes, &buffer, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, true);

    if (buffer == nullptr)
    {LOGMEIN("JITOutput.cpp] 137\n");
        Js::Throw::OutOfMemory();
    }

    m_outputData->codeAddress = (intptr_t)buffer;
    m_outputData->codeSize = bytes;
    m_outputData->pdataCount = pdataCount;
    m_outputData->xdataSize = xdataSize;
    m_outputData->isInPrereservedRegion = m_oopAlloc->inPrereservedRegion;
    return m_oopAlloc;
}
#endif

void
JITOutput::RecordNativeCode(const BYTE* sourceBuffer, BYTE* localCodeAddress)
{LOGMEIN("JITOutput.cpp] 152\n");
#if ENABLE_OOP_NATIVE_CODEGEN
    if (JITManager::GetJITManager()->IsJITServer())
    {
        RecordNativeCode(sourceBuffer, localCodeAddress, m_oopAlloc, m_func->GetOOPCodeGenAllocators());
    }
    else
#endif
    {
        RecordNativeCode(sourceBuffer, localCodeAddress, m_inProcAlloc, m_func->GetInProcCodeGenAllocators());
    }
}

template <typename TEmitBufferAllocation, typename TCodeGenAllocators>
void
JITOutput::RecordNativeCode(const BYTE* sourceBuffer, BYTE* localCodeAddress, TEmitBufferAllocation allocation, TCodeGenAllocators codeGenAllocators)
{LOGMEIN("JITOutput.cpp] 168\n");
    Assert(m_outputData->codeAddress == (intptr_t)allocation->allocation->address);
    if (!codeGenAllocators->emitBufferManager.CommitBuffer(allocation, localCodeAddress, m_outputData->codeSize, sourceBuffer))
    {LOGMEIN("JITOutput.cpp] 171\n");
        Js::Throw::OutOfMemory();
    }

#if DBG_DUMP
    if (m_func->IsLoopBody())
    {LOGMEIN("JITOutput.cpp] 177\n");
        codeGenAllocators->emitBufferManager.totalBytesLoopBody += m_outputData->codeSize;
    }
#endif
}

void
JITOutput::RecordInlineeFrameOffsetsInfo(unsigned int offsetsArrayOffset, unsigned int offsetsArrayCount)
{LOGMEIN("JITOutput.cpp] 185\n");
    m_outputData->inlineeFrameOffsetArrayOffset = offsetsArrayOffset;
    m_outputData->inlineeFrameOffsetArrayCount = offsetsArrayCount;
}

#if _M_X64
void
JITOutput::RecordUnwindInfo(BYTE *unwindInfo, size_t size, BYTE * xdataAddr, BYTE* localXdataAddr)
{LOGMEIN("JITOutput.cpp] 193\n");
    Assert(XDATA_SIZE >= size);
    memcpy_s(localXdataAddr, XDATA_SIZE, unwindInfo, size);
    m_outputData->xdataAddr = (intptr_t)xdataAddr;
}

#elif _M_ARM
size_t
JITOutput::RecordUnwindInfo(size_t offset, BYTE *unwindInfo, size_t size, BYTE * xdataAddr)
{LOGMEIN("JITOutput.cpp] 202\n");
    BYTE *xdataFinal = xdataAddr + offset;

    Assert(xdataFinal);
    Assert(((DWORD)xdataFinal & 0x3) == 0); // 4 byte aligned
    memcpy_s(xdataFinal, size, unwindInfo, size);

    return (size_t)xdataFinal;
}

void
JITOutput::RecordXData(BYTE * xdata)
{LOGMEIN("JITOutput.cpp] 214\n");
    m_outputData->xdataOffset = NativeCodeData::GetDataTotalOffset(xdata);
}
#endif

void
JITOutput::FinalizeNativeCode()
{LOGMEIN("JITOutput.cpp] 221\n");
#if ENABLE_OOP_NATIVE_CODEGEN
    if (JITManager::GetJITManager()->IsJITServer())
    {LOGMEIN("JITOutput.cpp] 224\n");
        m_func->GetOOPCodeGenAllocators()->emitBufferManager.CompletePreviousAllocation(m_oopAlloc);
    }
    else
#endif
    {
        m_func->GetInProcCodeGenAllocators()->emitBufferManager.CompletePreviousAllocation(m_inProcAlloc);

        m_func->GetInProcJITEntryPointInfo()->SetInProcJITNativeCodeData(m_func->GetNativeCodeDataAllocator()->Finalize());
        m_func->GetInProcJITEntryPointInfo()->GetJitTransferData()->SetRawData(m_func->GetTransferDataAllocator()->Finalize());
#if !FLOATVAR
        CodeGenNumberChunk * numberChunks = m_func->GetNumberAllocator()->Finalize();
        m_func->GetInProcJITEntryPointInfo()->SetNumberChunks(numberChunks);
#endif
    }
}

JITOutputIDL *
JITOutput::GetOutputData()
{LOGMEIN("JITOutput.cpp] 243\n");
    return m_outputData;
}
