//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITOutput::JITOutput(JITOutputIDL * outputData) :
    m_outputData(outputData),
    m_inProcAlloc(nullptr),
    m_func(nullptr)
{TRACE_IT(9712);
}

void
JITOutput::SetHasJITStackClosure()
{TRACE_IT(9713);
    m_outputData->hasJittedStackClosure = true;
}

void
JITOutput::SetVarSlotsOffset(int32 offset)
{TRACE_IT(9714);
    m_outputData->localVarSlotsOffset = offset;
}

void
JITOutput::SetVarChangedOffset(int32 offset)
{TRACE_IT(9715);
    m_outputData->localVarChangedOffset = offset;
}

void
JITOutput::SetHasBailoutInstr(bool val)
{TRACE_IT(9716);
    m_outputData->hasBailoutInstr = val;
}

void
JITOutput::SetArgUsedForBranch(uint8 param)
{TRACE_IT(9717);
    Assert(param > 0);
    Assert(param < Js::Constants::MaximumArgumentCountForConstantArgumentInlining);
    m_outputData->argUsedForBranch |= (1 << (param - 1));
}

void
JITOutput::SetFrameHeight(uint val)
{TRACE_IT(9718);
    m_outputData->frameHeight = val;
}

void
JITOutput::RecordThrowMap(Js::ThrowMapEntry * throwMap, uint mapCount)
{TRACE_IT(9719);
    m_outputData->throwMapOffset = NativeCodeData::GetDataTotalOffset(throwMap);
    m_outputData->throwMapCount = mapCount;
}

uint16
JITOutput::GetArgUsedForBranch() const
{TRACE_IT(9720);
    return m_outputData->argUsedForBranch;
}

intptr_t
JITOutput::GetCodeAddress() const
{TRACE_IT(9721);
    return (intptr_t)m_outputData->codeAddress;
}

void
JITOutput::SetCodeAddress(intptr_t addr)
{TRACE_IT(9722);
    m_outputData->codeAddress = addr;
}

size_t
JITOutput::GetCodeSize() const
{TRACE_IT(9723);
    return (size_t)m_outputData->codeSize;
}

ushort
JITOutput::GetPdataCount() const
{TRACE_IT(9724);
    return m_outputData->pdataCount;
}

ushort
JITOutput::GetXdataSize() const
{TRACE_IT(9725);
    return m_outputData->xdataSize;
}

EmitBufferAllocation<VirtualAllocWrapper, PreReservedVirtualAllocWrapper> *
JITOutput::RecordInProcNativeCodeSize(Func *func, uint32 bytes, ushort pdataCount, ushort xdataSize)
{TRACE_IT(9726);
    m_func = func;

#if defined(_M_ARM32_OR_ARM64)
    bool canAllocInPreReservedHeapPageSegment = false;
#else
    bool canAllocInPreReservedHeapPageSegment = m_func->CanAllocInPreReservedHeapPageSegment();
#endif

    BYTE *buffer = nullptr;
    m_inProcAlloc = m_func->GetInProcCodeGenAllocators()->emitBufferManager.AllocateBuffer(bytes, &buffer, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, true);

    if (buffer == nullptr)
    {TRACE_IT(9727);
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
{TRACE_IT(9728);
    m_func = func;

#if defined(_M_ARM32_OR_ARM64)
    bool canAllocInPreReservedHeapPageSegment = false;
#else
    bool canAllocInPreReservedHeapPageSegment = m_func->CanAllocInPreReservedHeapPageSegment();
#endif

    BYTE *buffer = nullptr;
    m_oopAlloc = m_func->GetOOPCodeGenAllocators()->emitBufferManager.AllocateBuffer(bytes, &buffer, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, true);

    if (buffer == nullptr)
    {TRACE_IT(9729);
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
{TRACE_IT(9730);
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
{TRACE_IT(9731);
    Assert(m_outputData->codeAddress == (intptr_t)allocation->allocation->address);
    if (!codeGenAllocators->emitBufferManager.CommitBuffer(allocation, localCodeAddress, m_outputData->codeSize, sourceBuffer))
    {TRACE_IT(9732);
        Js::Throw::OutOfMemory();
    }

#if DBG_DUMP
    if (m_func->IsLoopBody())
    {TRACE_IT(9733);
        codeGenAllocators->emitBufferManager.totalBytesLoopBody += m_outputData->codeSize;
    }
#endif
}

void
JITOutput::RecordInlineeFrameOffsetsInfo(unsigned int offsetsArrayOffset, unsigned int offsetsArrayCount)
{TRACE_IT(9734);
    m_outputData->inlineeFrameOffsetArrayOffset = offsetsArrayOffset;
    m_outputData->inlineeFrameOffsetArrayCount = offsetsArrayCount;
}

#if _M_X64
void
JITOutput::RecordUnwindInfo(BYTE *unwindInfo, size_t size, BYTE * xdataAddr, BYTE* localXdataAddr)
{TRACE_IT(9735);
    Assert(XDATA_SIZE >= size);
    memcpy_s(localXdataAddr, XDATA_SIZE, unwindInfo, size);
    m_outputData->xdataAddr = (intptr_t)xdataAddr;
}

#elif _M_ARM
size_t
JITOutput::RecordUnwindInfo(size_t offset, BYTE *unwindInfo, size_t size, BYTE * xdataAddr)
{TRACE_IT(9736);
    BYTE *xdataFinal = xdataAddr + offset;

    Assert(xdataFinal);
    Assert(((DWORD)xdataFinal & 0x3) == 0); // 4 byte aligned
    memcpy_s(xdataFinal, size, unwindInfo, size);

    return (size_t)xdataFinal;
}

void
JITOutput::RecordXData(BYTE * xdata)
{TRACE_IT(9737);
    m_outputData->xdataOffset = NativeCodeData::GetDataTotalOffset(xdata);
}
#endif

void
JITOutput::FinalizeNativeCode()
{TRACE_IT(9738);
#if ENABLE_OOP_NATIVE_CODEGEN
    if (JITManager::GetJITManager()->IsJITServer())
    {TRACE_IT(9739);
        m_func->GetOOPCodeGenAllocators()->emitBufferManager.CompletePreviousAllocation(m_oopAlloc);
    }
    else
#endif
    {TRACE_IT(9740);
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
{TRACE_IT(9741);
    return m_outputData;
}
