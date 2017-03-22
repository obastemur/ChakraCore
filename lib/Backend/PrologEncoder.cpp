//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"
#include "PrologEncoderMD.h"

#ifdef _WIN32
// ----------------------------------------------------------------------------
//  _WIN32 x64 unwind uses PDATA
// ----------------------------------------------------------------------------

void PrologEncoder::RecordNonVolRegSave()
{LOGMEIN("PrologEncoder.cpp] 13\n");
    requiredUnwindCodeNodeCount++;
}

void PrologEncoder::RecordXmmRegSave()
{LOGMEIN("PrologEncoder.cpp] 18\n");
    requiredUnwindCodeNodeCount += 2;
}

void PrologEncoder::RecordAlloca(size_t size)
{LOGMEIN("PrologEncoder.cpp] 23\n");
    Assert(size);

    requiredUnwindCodeNodeCount += PrologEncoderMD::GetRequiredNodeCountForAlloca(size);
}

DWORD PrologEncoder::SizeOfPData()
{LOGMEIN("PrologEncoder.cpp] 30\n");
    return sizeof(PData) + (sizeof(UNWIND_CODE) * requiredUnwindCodeNodeCount);
}

void PrologEncoder::EncodeSmallProlog(uint8 prologSize, size_t allocaSize)
{LOGMEIN("PrologEncoder.cpp] 35\n");
    Assert(allocaSize <= 128);
    Assert(requiredUnwindCodeNodeCount == 0);

    // Increment requiredUnwindCodeNodeCount to ensure we do the Alloc for the correct size
    currentUnwindCodeNodeIndex = ++requiredUnwindCodeNodeCount;

    currentInstrOffset = prologSize;

    UnwindCode* unwindCode = GetUnwindCode(1);
    size_t slots = (allocaSize - MachPtr) / MachPtr;
    uint8 unwindCodeOpInfo = TO_UNIBBLE(slots);

    unwindCode->SetOffset(prologSize);
    unwindCode->SetOp(TO_UNIBBLE(UWOP_ALLOC_SMALL));
    unwindCode->SetOpInfo(unwindCodeOpInfo);
}

void PrologEncoder::EncodeInstr(IR::Instr *instr, unsigned __int8 size)
{LOGMEIN("PrologEncoder.cpp] 54\n");
    Assert(instr);
    Assert(size);

    UnwindCode       *unwindCode       = nullptr;
    unsigned __int8   unwindCodeOp     = PrologEncoderMD::GetOp(instr);
    unsigned __int8   unwindCodeOpInfo = 0;
    unsigned __int16  uint16Val        = 0;
    unsigned __int32  uint32Val        = 0;

    if (!currentInstrOffset)
        currentUnwindCodeNodeIndex = requiredUnwindCodeNodeCount;

    Assert((currentInstrOffset + size) > currentInstrOffset);
    currentInstrOffset += size;

    switch (unwindCodeOp)
    {LOGMEIN("PrologEncoder.cpp] 71\n");
    case UWOP_PUSH_NONVOL:
    {LOGMEIN("PrologEncoder.cpp] 73\n");
        unwindCode = GetUnwindCode(1);
        unwindCodeOpInfo = PrologEncoderMD::GetNonVolRegToSave(instr);
        break;
    }

    case UWOP_SAVE_XMM128:
    {LOGMEIN("PrologEncoder.cpp] 80\n");
        unwindCode       = GetUnwindCode(2);
        unwindCodeOpInfo = PrologEncoderMD::GetXmmRegToSave(instr, &uint16Val);

        *((unsigned __int16 *)&((UNWIND_CODE *)unwindCode)[1]) = uint16Val;
        break;
    }

    case UWOP_ALLOC_SMALL:
    {LOGMEIN("PrologEncoder.cpp] 89\n");
        unwindCode = GetUnwindCode(1);
        size_t allocaSize = PrologEncoderMD::GetAllocaSize(instr);
        Assert((allocaSize - MachPtr) % MachPtr == 0);
        size_t slots = (allocaSize - MachPtr) / MachPtr;
        Assert(IS_UNIBBLE(slots));
        unwindCodeOpInfo = TO_UNIBBLE(slots);
        break;
    }

    case UWOP_ALLOC_LARGE:
    {LOGMEIN("PrologEncoder.cpp] 100\n");
        size_t allocaSize = PrologEncoderMD::GetAllocaSize(instr);
        Assert(allocaSize > 0x80);
        Assert(allocaSize % 8 == 0);
        Assert(IS_UNIBBLE(unwindCodeOp));

        size_t slots = allocaSize / MachPtr;
        if (allocaSize > 0x7FF8)
        {LOGMEIN("PrologEncoder.cpp] 108\n");
            unwindCode       = GetUnwindCode(3);
            uint32Val        = TO_UINT32(allocaSize);
            unwindCodeOpInfo = 1;

            unwindCode->SetOp(TO_UNIBBLE(unwindCodeOp));
            *((unsigned __int32 *)&((UNWIND_CODE *)unwindCode)[1]) = uint32Val;
        }
        else
        {
            unwindCode       = GetUnwindCode(2);
            uint16Val        = TO_UINT16(slots);
            unwindCodeOpInfo = 0;

            unwindCode->SetOp(TO_UNIBBLE(unwindCodeOp));
            *((unsigned __int16 *)&((UNWIND_CODE *)unwindCode)[1]) = uint16Val;
        }
        break;
    }

    case UWOP_IGNORE:
    {LOGMEIN("PrologEncoder.cpp] 129\n");
        return;
    }

    default:
    {
        AssertMsg(false, "PrologEncoderMD returned unsupported UnwindCodeOp.");
    }
    }

    Assert(unwindCode);

    unwindCode->SetOffset(currentInstrOffset);

    Assert(IS_UNIBBLE(unwindCodeOp));
    unwindCode->SetOp(TO_UNIBBLE(unwindCodeOp));

    Assert(IS_UNIBBLE(unwindCodeOpInfo));
    unwindCode->SetOpInfo(TO_UNIBBLE(unwindCodeOpInfo));
}

BYTE *PrologEncoder::Finalize(BYTE *functionStart,
                              DWORD codeSize,
                              BYTE *pdataBuffer)
{LOGMEIN("PrologEncoder.cpp] 153\n");
    Assert(pdataBuffer > functionStart);
    Assert((size_t)pdataBuffer % sizeof(DWORD) == 0);

    pdata.runtimeFunction.BeginAddress = 0;
    pdata.runtimeFunction.EndAddress   = codeSize;
    pdata.runtimeFunction.UnwindData   = (DWORD)((pdataBuffer + sizeof(RUNTIME_FUNCTION)) - functionStart);

    FinalizeUnwindInfo(functionStart, codeSize);

    return (BYTE *)&pdata.runtimeFunction;
}

void PrologEncoder::FinalizeUnwindInfo(BYTE *functionStart, DWORD codeSize)
{LOGMEIN("PrologEncoder.cpp] 167\n");
    pdata.unwindInfo.Version           = 1;
    pdata.unwindInfo.Flags             = 0;
    pdata.unwindInfo.SizeOfProlog      = currentInstrOffset;
    pdata.unwindInfo.CountOfCodes      = requiredUnwindCodeNodeCount;

    // We don't use the frame pointer in the standard way, and since we don't do dynamic stack allocation, we don't change the
    // stack pointer except during calls. From the perspective of the unwind info, it needs to restore information relative to
    // the stack pointer, so don't register the frame pointer.
    pdata.unwindInfo.FrameRegister     = 0;
    pdata.unwindInfo.FrameOffset       = 0;

    AssertMsg(requiredUnwindCodeNodeCount <= MaxRequiredUnwindCodeNodeCount, "We allocate 72 bytes for xdata - 34 (UnwindCodes) * 2 + 4 (UnwindInfo)");
}

PrologEncoder::UnwindCode *PrologEncoder::GetUnwindCode(unsigned __int8 nodeCount)
{LOGMEIN("PrologEncoder.cpp] 183\n");
    Assert(nodeCount && ((currentUnwindCodeNodeIndex - nodeCount) >= 0));
    currentUnwindCodeNodeIndex -= nodeCount;

    return static_cast<UnwindCode *>(&pdata.unwindInfo.unwindCodes[currentUnwindCodeNodeIndex]);
}

DWORD PrologEncoder::SizeOfUnwindInfo()
{LOGMEIN("PrologEncoder.cpp] 191\n");
    return (sizeof(UNWIND_INFO) + (sizeof(UNWIND_CODE) * requiredUnwindCodeNodeCount));
}

BYTE *PrologEncoder::GetUnwindInfo()
{LOGMEIN("PrologEncoder.cpp] 196\n");
    return (BYTE *)&pdata.unwindInfo;
}

#else  // !_WIN32
// ----------------------------------------------------------------------------
//  !_WIN32 x64 unwind uses .eh_frame
// ----------------------------------------------------------------------------

void PrologEncoder::EncodeSmallProlog(uint8 prologSize, size_t size)
{LOGMEIN("PrologEncoder.cpp] 206\n");
    auto fde = ehFrame.GetFDE();

    // prolog: push rbp
    fde->cfi_advance_loc(1);                    // DW_CFA_advance_loc: 1
    fde->cfi_def_cfa_offset(MachPtr * 2);       // DW_CFA_def_cfa_offset: 16
    fde->cfi_offset(GetDwarfRegNum(LowererMDArch::GetRegFramePointer()), 2); // DW_CFA_offset: r6 (rbp) at cfa-16

    ehFrame.End();
}

DWORD PrologEncoder::SizeOfPData()
{LOGMEIN("PrologEncoder.cpp] 218\n");
    return ehFrame.Count();
}

BYTE* PrologEncoder::Finalize(BYTE *functionStart, DWORD codeSize, BYTE *pdataBuffer)
{LOGMEIN("PrologEncoder.cpp] 223\n");
    auto fde = ehFrame.GetFDE();
    fde->UpdateAddressRange(functionStart, codeSize);
    return ehFrame.Buffer();
}

void PrologEncoder::Begin(size_t prologStartOffset)
{LOGMEIN("PrologEncoder.cpp] 230\n");
    Assert(currentInstrOffset == 0);

    currentInstrOffset = prologStartOffset;
}

void PrologEncoder::End()
{LOGMEIN("PrologEncoder.cpp] 237\n");
    ehFrame.End();
}

void PrologEncoder::FinalizeUnwindInfo(BYTE *functionStart, DWORD codeSize)
{LOGMEIN("PrologEncoder.cpp] 242\n");
    auto fde = ehFrame.GetFDE();
    fde->UpdateAddressRange(functionStart, codeSize);
}

void PrologEncoder::EncodeInstr(IR::Instr *instr, unsigned __int8 size)
{LOGMEIN("PrologEncoder.cpp] 248\n");
    auto fde = ehFrame.GetFDE();

    uint8 unwindCodeOp = PrologEncoderMD::GetOp(instr);

    Assert((currentInstrOffset + size) > currentInstrOffset);
    currentInstrOffset += size;

    switch (unwindCodeOp)
    {LOGMEIN("PrologEncoder.cpp] 257\n");
    case UWOP_PUSH_NONVOL:
    {LOGMEIN("PrologEncoder.cpp] 259\n");
        const uword advance = currentInstrOffset - cfiInstrOffset;
        cfiInstrOffset = currentInstrOffset;
        cfaWordOffset++;

        fde->cfi_advance(advance);                              // DW_CFA_advance_loc: ?
        fde->cfi_def_cfa_offset(cfaWordOffset * MachPtr);       // DW_CFA_def_cfa_offset: ??

        const ubyte reg = PrologEncoderMD::GetNonVolRegToSave(instr) + 1;
        fde->cfi_offset(GetDwarfRegNum(reg), cfaWordOffset);    // DW_CFA_offset: r? at cfa-??
        break;
    }

    case UWOP_SAVE_XMM128:
    {LOGMEIN("PrologEncoder.cpp] 273\n");
        // TODO
        break;
    }

    case UWOP_ALLOC_SMALL:
    case UWOP_ALLOC_LARGE:
    {LOGMEIN("PrologEncoder.cpp] 280\n");
        size_t allocaSize = PrologEncoderMD::GetAllocaSize(instr);
        Assert(allocaSize % MachPtr == 0);

        size_t slots = allocaSize / MachPtr;
        Assert(cfaWordOffset + slots > cfaWordOffset);

        const uword advance = currentInstrOffset - cfiInstrOffset;
        cfiInstrOffset = currentInstrOffset;
        cfaWordOffset += slots;

        fde->cfi_advance(advance);                          // DW_CFA_advance_loc: ?
        fde->cfi_def_cfa_offset(cfaWordOffset * MachPtr);   // DW_CFA_def_cfa_offset: ??
        break;
    }

    case UWOP_IGNORE:
    {LOGMEIN("PrologEncoder.cpp] 297\n");
        return;
    }

    default:
    {
        AssertMsg(false, "PrologEncoderMD returned unsupported UnwindCodeOp.");
    }
    }
}

#endif  // !_WIN32
