//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

//------------------------------------------------------------------------------
// define the set of ARMNT unwind codes based on "unwindCodes.h"
//------------------------------------------------------------------------------

#undef UNWIND_CODE

#define UNWIND_CODE(name, width, dwTemplate, dwMask, length) \
    UWOP_##name##_##width,

enum UnwindCode
{
#include "UnwindCodes.h"
};

#undef UNWIND_CODE

#define UNWIND_CODE(name, width, dwTemplate, dwMask, length) \
    dwTemplate,

static const DWORD UnwindCodeTemplates[] =
{
#include "UnwindCodes.h"
};

#undef UNWIND_CODE

#define UNWIND_CODE(name, width, dwTemplate, dwMask, length) \
    length,

static const DWORD UnwindCodeLengths[] =
{
#include "UnwindCodes.h"
};

DWORD UnwindInfoManager::XdataTemplate(UnwindCode op) const
{TRACE_IT(17680);
    return UnwindCodeTemplates[op];
}

DWORD UnwindInfoManager::XdataLength(UnwindCode op) const
{TRACE_IT(17681);
    return UnwindCodeLengths[op];
}

DWORD UnwindInfoManager::RelativeRegEncoding(RegNum reg, RegNum baseReg) const
{TRACE_IT(17682);
    // TODO: handle non-int regs here.

    Assert(reg >= baseReg);
    DWORD encode = EncoderMD::GetRegEncode(reg) - EncoderMD::GetRegEncode(baseReg);
    return encode;
}

void UnwindInfoManager::Init(Func * func)
{TRACE_IT(17683);
    this->SetFunc(func);
    this->processHandle = func->GetThreadContextInfo()->GetProcessHandle();
}

DWORD UnwindInfoManager::GetPDataCount(DWORD length)
{TRACE_IT(17684);
    DWORD count = 0;
    // First handle the main function, which we may have to emit in multiple
    // fragments depending on its length.
    DWORD remainingLength = this->GetEpilogEndOffset();

    while (remainingLength > MaxXdataFuncLength)
    {TRACE_IT(17685);
        // The function is too big to be encoded. Encode it in multiple fragments no larger than half
        // the remaining unencoded size (so we don't risk creating a fragment that splits the epilog).
        DWORD currLength = min(MaxXdataFuncLength, (remainingLength >> 1 ) & (0 - INSTR_ALIGNMENT));
        remainingLength -= currLength;
        ++count;
    }

    ++count;

    if (length > this->GetEpilogEndOffset())
    {TRACE_IT(17686);
        remainingLength = length - this->GetEpilogEndOffset();
        while(remainingLength > MaxXdataFuncLength)
        {TRACE_IT(17687);
            DWORD currLength = min(MaxXdataFuncLength, (remainingLength >> 1 ) & (0 - INSTR_ALIGNMENT));
            remainingLength -= currLength;
            ++count;
        }
        ++count;
    }

    return count;
}

void UnwindInfoManager::EmitUnwindInfo(JITOutput *jitOutput, CustomHeap::Allocation * alloc)
{TRACE_IT(17688);
    this->jitOutput = jitOutput;
    this->alloc = alloc;
    this->processHandle = processHandle;

    // First handle the main function, which we may have to emit in multiple
    // fragments depending on its length.
    DWORD remainingLength = this->GetEpilogEndOffset();
    this->fragmentHasProlog = true;
    this->fragmentStart = jitOutput->GetCodeAddress();

    this->pdataIndex = 0;
    this->xdataTotal = 0;
    while (remainingLength > MaxXdataFuncLength)
    {TRACE_IT(17689);
        remainingLength = this->EmitLongUnwindInfoChunk(remainingLength);
    }
    this->fragmentLength = remainingLength;
    this->fragmentHasEpilog = true;
    this->EmitPdata();

    Assert(jitOutput->GetCodeSize() >= (ptrdiff_t)this->GetEpilogEndOffset());

    if (jitOutput->GetCodeSize() > (ptrdiff_t)this->GetEpilogEndOffset())
    {TRACE_IT(17690);
        // Set the start/end pointers to indicate the boundaries of the fragment.
        // Almost identical to the code above, except that all chunks have neither prolog nor epilog.
        this->fragmentStart = jitOutput->GetCodeAddress() + this->GetEpilogEndOffset();
        this->fragmentHasProlog = false;
        remainingLength = jitOutput->GetCodeSize() - this->GetEpilogEndOffset();

        while (remainingLength > MaxXdataFuncLength)
        {TRACE_IT(17691);
            remainingLength = this->EmitLongUnwindInfoChunk(remainingLength);
        }
        this->fragmentLength = remainingLength;
        this->fragmentHasEpilog = false;
        this->EmitPdata();
    }
    AssertMsg(this->pdataIndex == jitOutput->GetPdataCount(), "The length of pdata array is not in sync with the usage");
    AssertMsg(this->xdataTotal <= jitOutput->GetXdataSize(), "We under-allocated the size of the xdata");
}

DWORD UnwindInfoManager::EmitLongUnwindInfoChunk(DWORD remainingLength)
{TRACE_IT(17692);
    // The function is too big to be encoded. Encode it in multiple fragments no larger than half
    // the remaining unencoded size (so we don't risk creating a fragment that splits the epilog).

    // The current chunk has no epilog, and subsequent chunks have no prolog.

    DWORD currLength = min(MaxXdataFuncLength, (remainingLength >> 1) & (0 - INSTR_ALIGNMENT));
    this->fragmentHasEpilog = false;
    this->fragmentLength = currLength;
    this->EmitPdata();

    this->fragmentStart += currLength;
    this->fragmentHasProlog = false;

    return remainingLength - currLength;
}

void UnwindInfoManager::EmitPdata()
{TRACE_IT(17693);
    // Can we emit packed pdata?
    bool fPacked = this->CanEmitPackedPdata();

    // If so, do it.
    if (fPacked)
    {TRACE_IT(17694);
        this->EncodePackedUnwindData();
    }
    else
    {TRACE_IT(17695);
        // Otherwise, emit pdata + xdata records
        this->EncodeExpandedUnwindData();
    }
    this->pdataIndex++;
}

bool UnwindInfoManager::CanEmitPackedPdata() const
{TRACE_IT(17696);
    if (this->GetHasCalls())
    {TRACE_IT(17697);
        //We need to reserve a slot for arguments objects and we can't emit packed pdata.
        return false;
    }

    if (this->homedParamCount > 0 && this->homedParamCount != NUM_INT_ARG_REGS)
    {TRACE_IT(17698);
        // The function homes only some of the params, which can't be represented
        // in packed pdata.
        return false;
    }

    // Is the function too long?
    DWORD length = this->GetFragmentLength();
    if (length > MaxPackedPdataFuncLength)
    {TRACE_IT(17699);
        return false;
    }

    // Is there too much stack?
    DWORD stack = this->GetStackDepth();
    if (stack > MaxPackedPdataStackDepth)
    {TRACE_IT(17700);
        return false;
    }

    // Are the saved regs a contiguous range of the form r4-rN?
    // (Note that we don't care about the frame pointer when we're asking this question here,
    // so tell the API only to consider r11 if this is a leaf. As if.)
    if (!this->IsR4SavedRegRange(!this->hasCalls))
    {TRACE_IT(17701);
        return false;
    }

    return true;
}

void UnwindInfoManager::EncodePackedUnwindData()
{TRACE_IT(17702);
    Assert(!this->hasCalls);    //As of now we don't emit PackedUnwindData for non leaf functions.

    // 2nd DWORD (packed bits):

    // 1. Set the function length
    DWORD length = this->fragmentLength;
    Assert((length & ~PackedFuncLengthMask) == 0);
    DWORD dwFlags = length << PackedFuncLengthShift;

    // 2. Set the packed flag bits
    if (!this->fragmentHasProlog)
    {TRACE_IT(17703);
        // Set the "fragment" flag and the no-epilog bits at once.

        dwFlags |= PackedNoPrologBits;
    }
    else
    {TRACE_IT(17704);
        dwFlags |= PackedNormalFuncBits;
    }

    // Set the Ret bits (return instruction style).
    if (!this->fragmentHasEpilog)
    {TRACE_IT(17705);
        dwFlags |= PackedNoEpilogBits;
    }
    else if (this->hasCalls)
    {TRACE_IT(17706);
        dwFlags |= PackedNonLeafRetBits;
    }
    else
    {TRACE_IT(17707);
        dwFlags |= PackedLeafRetBits;
    }

    // 3. Set L bit (saves LR) and C bit (frame chaining) on non-leaf functions.
    DWORD savedRegMask = this->savedRegMask;
    if (this->hasCalls)
    {TRACE_IT(17708);
        dwFlags |= PackedNonLeafFunctionBits;
        // R11 is the frame pointer, and we don't encode it as part
        // of the saved regs.
        savedRegMask = this->ClearSavedReg(savedRegMask, RegEncode[RegR11]);
    }

    // 4. Set the homed param bit for JS functions if we're doing so.
    if (this->homedParamCount > 0)
    {TRACE_IT(17709);
        Assert(this->homedParamCount == NUM_INT_ARG_REGS);
        dwFlags |= PackedHomedParamsBit;
    }

    // 5. Set the last saved reg num.
    if (savedRegMask == 0)
    {TRACE_IT(17710);
        // Special encoding for this case.
        dwFlags |= PackedNoSavedRegsBits;
    }
    else
    {TRACE_IT(17711);
        // Encode the reg.
        BYTE regEncode = this->GetLastSavedReg(savedRegMask);
        Assert(regEncode <= RegEncode[RegR11]);
        dwFlags |= ((regEncode - RegEncode[RegR4]) << PackedRegShift);
    }

    // 6. Finally, the stack adjustment.
    DWORD depth = this->GetStackDepth();
    Assert((depth & ~PackedStackDepthMask) == 0);
    dwFlags |= depth << PackedStackDepthShift;

    RecordPdataEntry(((DWORD)this->GetFragmentStart()) | 1, dwFlags);
}

void UnwindInfoManager::EncodeExpandedUnwindData()
{TRACE_IT(17712);
    // Temp local storage. This will contain contiguous pdata and xdata.
    BYTE xData[MaxXdataBytes];
    // Pointer to the current unwind code (i.e., point past the pdata and the first xdata DWORD).
    BYTE  *xDataBuffer = xData + 4;

    DWORD xDataByteCount = 0;
    DWORD xDataHeader;
    DWORD epilogCodeWord = 0;

    // We'll fill out the local buffer with the variable-length xdata, then write it out
    // to a permanent buffer with the correct size.

    // First, the bits describing function length, epilog, etc.

    DWORD length = this->fragmentLength;

    AssertMsg((length & ~XdataFuncLengthMask) == 0, "Invalid function length (too large or odd)");

    xDataHeader = length >> XdataFuncLengthAdjust;

    // Describe the epilog. Parent function always has 1 epilog; fragment has none.

    if (!this->fragmentHasProlog)
    {TRACE_IT(17713);
        // Set the F bit.

        xDataHeader |= 1 << XdataFuncFragmentShift;
    }

    // The prolog operations are as follows (in reverse of the lexical order of the instructions):
    // 1. Allocate stack
    // 2. Save double callee-saved registers
    // 3. Save callee-saved registers
    // 4. Set up frame chain pointer
    // 5. Save r11 & lr registers
    // 6. Home parameters

    // 1. Allocate stack
    BOOL hasTry = this->func->HasTry();
    RegNum localsReg = this->func->GetLocalsPointer();
    if (localsReg != RegSP && !hasTry)
    {TRACE_IT(17714);
        // First tell the xdata to restore SP from another reg.
        xDataByteCount = this->EmitXdataLocalsPointer(xDataBuffer, xDataByteCount, RegEncode[localsReg]);
    }

    DWORD stack = this->GetStackDepth();

    if (stack != 0)
    {TRACE_IT(17715);
        xDataByteCount = this->EmitXdataStackAlloc(xDataBuffer, xDataByteCount, stack);
    }

    if (this->GetHasChkStk() && this->fragmentHasProlog)
    {TRACE_IT(17716);
        //This is rare __chkstk call case where before stack allocation there is a call to __chkstk
        // LDIMM RegR4, stackSize/4
        // LDIMM RegR12, HelperCRT_chkstk
        // BLX RegR12
        // SUB SP, SP, RegR4
        xDataByteCount = this->EmitXdataNop16(xDataBuffer, xDataByteCount); //BLX RegR12
        xDataByteCount = this->EmitXdataNop32(xDataBuffer, xDataByteCount); //MOVW RegR12, HelperCRT_chkstk
        xDataByteCount = this->EmitXdataNop32(xDataBuffer, xDataByteCount); //MOVT RegR12, HelperCRT_chkstk
        xDataByteCount = this->EmitXdataNop32(xDataBuffer, xDataByteCount); //MOVW RegR12, stackSize/4
        xDataByteCount = this->EmitXdataNop32(xDataBuffer, xDataByteCount); //MOVT RegR12, stackSize/4
    }

    if (hasTry)
    {TRACE_IT(17717);
        // Encode the following (in reverse order):
        // mov ehreg,sp      ==> save stack: ehreg
        // sub sp,#locals    ==> nop
        // mov r7,sp         ==> nop
        // push ehreg

        xDataByteCount = this->EmitXdataRestoreRegs(xDataBuffer, xDataByteCount, 1 << RegEncode[EH_STACK_SAVE_REG], false);
        if (this->fragmentHasProlog)
        {TRACE_IT(17718);
            xDataByteCount = this->EmitXdataNop32(xDataBuffer, xDataByteCount);
            xDataByteCount = this->EmitXdataNop32(xDataBuffer, xDataByteCount);
        }
        xDataByteCount = this->EmitXdataLocalsPointer(xDataBuffer, xDataByteCount, RegEncode[EH_STACK_SAVE_REG]);
    }

    // 2. Save callee-saved double registers
    xDataByteCount = this->EmitXdataRestoreDoubleRegs(xDataBuffer, xDataByteCount, this->savedDoubleRegMask);

    // 3. Save callee-saved registers
    xDataByteCount = this->EmitXdataRestoreRegs(xDataBuffer, xDataByteCount, this->savedRegMask, false);

    // 4. Set up frame chain pointer. This is a 32-bit NOP, for the purposes of xdata.
    // 5. Save r11 & lr registers
    bool hasCalls = this->GetHasCalls();
    if (hasCalls)
    {TRACE_IT(17719);
        // Note: if the fragment has no prolog, then the boundaries of the prolog need not be computed,
        // so there's no need to encode NOP's.
        if (this->fragmentHasProlog)
        {TRACE_IT(17720);
            xDataByteCount = this->EmitXdataNop32(xDataBuffer, xDataByteCount);
        }

        DWORD r11RegMask = 1 << RegEncode[RegR11];
        xDataByteCount = this->EmitXdataRestoreRegs(xDataBuffer, xDataByteCount, r11RegMask, true); // true to indicate save LR
    }

    // 5. Home parameters
    xDataByteCount = this->EmitXdataHomeParams(xDataBuffer, xDataByteCount);

    // 7. And END.
    xDataByteCount = this->EmitXdataEnd(xDataBuffer, xDataByteCount);

    // If we're emitting a fragment with no epilog, and we're done. If we do have an epilog, then in
    // theory we could optimize the xdata by making prolog and epilog share unwind codes. In practice that
    // doesn't seem to be possible for any of the prolog/epilog idioms we generate.

    if (this->fragmentHasEpilog)
    {TRACE_IT(17721);
        // Tell the runtime where epilog encoding starts.
        epilogCodeWord = xDataByteCount;

        // Note that the first epilog operation need not be encoded (see optimization guidance in "ARM
        // Exception Data"). So we don't typically care about the stack alloc here.

        // Epilog ops:
        // Non-leaf function:
        // 1. Restore callee-saved registers.
        // 2. Restore r11 (but not LR).
        // 3. Return via LDR.
        // 4. END.
        // Leaf function:
        // 1. Restore callee-saved register (but not LR).
        // 2. If there are saved regs, dealloc homed params (because the dealloc was not folded into
        // the dealloc we omitted from this encoding).
        // 3. Return via BX (i.e., END+16).

        if (hasTry)
        {TRACE_IT(17722);
            // Restore the saved SP from the stack.
            xDataByteCount = this->EmitXdataRestoreRegs(xDataBuffer, xDataByteCount, 1 << RegEncode[EH_STACK_SAVE_REG], false);
            xDataByteCount = this->EmitXdataLocalsPointer(xDataBuffer, xDataByteCount, RegEncode[EH_STACK_SAVE_REG]);
        }
        else if (localsReg != RegSP && stack != 0)
        {TRACE_IT(17723);
            // Emit the stack alloc code, since the first epilog instr is the SP restore.
            xDataByteCount = this->EmitXdataStackAlloc(xDataBuffer, xDataByteCount, stack);
        }

        // 1. Save callee-saved  double registers, if there is double register this won't be leaf
        xDataByteCount = this->EmitXdataRestoreDoubleRegs(xDataBuffer, xDataByteCount, this->savedDoubleRegMask);


        // 2. Restore callee-saved registers (but not LR) for both leaf and non-leaf.
        xDataByteCount = this->EmitXdataRestoreRegs(xDataBuffer, xDataByteCount, this->savedRegMask, false);

        if (hasCalls)
        {TRACE_IT(17724);
            // Non-leaf:
            // 3. Restore r11 (but not LR).
            DWORD r11RegMask = 1 << RegEncode[RegR11];
            xDataByteCount = this->EmitXdataRestoreRegs(xDataBuffer, xDataByteCount, r11RegMask, false);

            // 4. Return via LDR.
            xDataByteCount = this->EmitXdataIndirReturn(xDataBuffer, xDataByteCount);

            // 6. END.
            xDataByteCount = this->EmitXdataEnd(xDataBuffer, xDataByteCount);
        }
        else
        {TRACE_IT(17725);
            // Leaf:
            // 3. If there are saved regs, dealloc homed params (because the dealloc was not folded into
            // the dealloc we omitted from this encoding).
            if (this->savedRegMask != 0)
            {TRACE_IT(17726);
                xDataByteCount = this->EmitXdataHomeParams(xDataBuffer, xDataByteCount);
            }

            // 4. Return via BX (i.e., END+16).
            xDataByteCount = this->EmitXdataEndPlus16(xDataBuffer, xDataByteCount);
        }

        // Now complete the header DWORD with the epilog start byte and the total DWORD count.
        // (In the no-epilog case, we'll leave zeroes in this field.)
        AssertMsg(epilogCodeWord <= MaxXdataEpilogCount, "Xdata prolog too long for normal encoding");
        xDataHeader |= (epilogCodeWord << XdataEpilogCountShift) | (1 << XdataSingleEpilogShift);
    }

    DWORD xDataDwordCount = (xDataByteCount + 3) >> 2;
    // Account for the leading DWORD.
    xDataDwordCount += 1;
    AssertMsg(xDataDwordCount <= MaxXdataDwordCount, "Xdata too long for normal encoding");

    xDataHeader |= xDataDwordCount << XdataDwordCountShift;

    *(DWORD*)(xData) = xDataHeader;

    size_t totalSize = (xDataDwordCount * 4);

    size_t xdataFinal = this->jitOutput->RecordUnwindInfo(this->xdataTotal, xData, totalSize, this->alloc->xdata.address);
    // for OOP JIT, we will set UnwindData to be the offset to it. we can fix it up on other side
    DWORD unwindField = (DWORD)(JITManager::GetJITManager()->IsOOPJITEnabled() ? this->xdataTotal : xdataFinal);
    this->xdataTotal += totalSize;
    RecordPdataEntry((DWORD)(this->GetFragmentStart() + this->GetPrologOffset()) | 1, unwindField);
}

DWORD UnwindInfoManager::EmitXdataStackAlloc(BYTE xData[], DWORD byte, DWORD stack)
{TRACE_IT(17727);
    DWORD encoding;
    UnwindCode op;

    stack >>= 2;

    if (stack < 0x80)
    {TRACE_IT(17728);
        op = UWOP_ALLOC_7B_16;
        encoding = stack << 24;
    }
    else if (stack < 0x400)
    {TRACE_IT(17729);
        op = UWOP_ALLOC_10B_32;
        encoding = stack << 16;
    }
    else if (stack < 0x10000)
    {TRACE_IT(17730);
        op = UWOP_ALLOC_16B_32;
        encoding = stack << 8;
    }
    else if (stack < 0x1000000)
    {TRACE_IT(17731);
        op = UWOP_ALLOC_24B_32;
        encoding = stack;
    }
    else
    {TRACE_IT(17732);
        // frame size is too large
        // (it can't be encoded with the unwind codes)
        Assert(UNREACHED);
        return byte;
    }

    encoding |= this->XdataTemplate(op);
    return this->WriteXdataBytes(xData, byte, encoding, this->XdataLength(op));
}

void UnwindInfoManager::RecordPdataEntry(DWORD beginAddress, DWORD unwindData)
{TRACE_IT(17733);
    RUNTIME_FUNCTION *function = this->alloc->xdata.GetPdataArray() + this->pdataIndex;
    function->BeginAddress = beginAddress;
    function->UnwindData = unwindData;
}

DWORD UnwindInfoManager::EmitXdataHomeParams(BYTE xData[], DWORD byte)
{TRACE_IT(17734);
    Assert(this->homedParamCount >= MIN_HOMED_PARAM_REGS &&
           this->homedParamCount <= NUM_INT_ARG_REGS);
    return this->EmitXdataStackAlloc(xData, byte, this->homedParamCount * MachRegInt);
}

DWORD UnwindInfoManager::EmitXdataRestoreRegs(BYTE xData[], DWORD byte, DWORD savedRegMask, bool restoreLR)
{TRACE_IT(17735);
    bool hasCalls = this->GetHasCalls();
    UnwindCode op;
    DWORD encoding;
    DWORD lrShift;

    if (savedRegMask == 0)
    {TRACE_IT(17736);
        // We're actually not saving any regs at all, so we're done.
        return byte;
    }

    BYTE lastSavedReg = this->GetLastSavedReg(savedRegMask);

    if (lastSavedReg > RegEncode[RegR11] || !IsR4SavedRegRange(savedRegMask))
    {TRACE_IT(17737);
        // We don't have a contiguous range that includes all the saved regs.
        if (lastSavedReg <= RegEncode[RegR7])
        {TRACE_IT(17738);
            // This is the 16-bit pop {r0-r7,lr} form.
            op = UWOP_POP_BITMASK_16;
            lrShift = 24;
        }
        else
        {TRACE_IT(17739);
            // This requires the 32-bit pop {r0-r12,lr} form (or the 1-register pop form, but we
            // don't care about the encoding difference here).
            op = UWOP_POP_BITMASK_32;
            lrShift = 29;
        }
        Assert((savedRegMask & 0xFFFF) == savedRegMask);
        encoding = (savedRegMask << 16);
    }
    else
    {TRACE_IT(17740);
        // All the regs are contiguous, so we can use a shorter xdata form in which we only encode
        // the last register in the range.
        // Remember where to set the LR bit.
        lrShift = 26;
        if (lastSavedReg <= RegEncode[RegR7])
        {TRACE_IT(17741);
            // This is the 16-bit pop {r4-r7,lr} form, encoding the reg relative to r4.
            op = UWOP_POP_RANGE_16;
            encoding = (lastSavedReg - RegEncode[RegR4]) << 24;
        }
        else
        {TRACE_IT(17742);
            // This is the 32-bit pop {r4-r11,lr} form, encoding the reg relative to r8.
            op = UWOP_POP_RANGE_32;
            encoding = (lastSavedReg - RegEncode[RegR8]) << 24;
        }
    }

    if (hasCalls && restoreLR)
    {TRACE_IT(17743);
        // Set the LR bit.
        encoding |= 1 << lrShift;
    }

    encoding |= this->XdataTemplate(op);
    return this->WriteXdataBytes(xData, byte, encoding, this->XdataLength(op));
}

DWORD UnwindInfoManager::EmitXdataRestoreDoubleRegs(BYTE xData[], DWORD byte, DWORD savedDoubleRegMask)
{TRACE_IT(17744);
    UnwindCode op;
    DWORD encoding = 0;

    if (savedDoubleRegMask == 0)
    {TRACE_IT(17745);
        // We're actually not saving any regs at all, so we're done.
        return byte;
    }

    BYTE lastSavedReg = this->GetLastSavedReg(savedDoubleRegMask);
    BYTE firstSavedReg = this->GetFirstSavedReg(savedDoubleRegMask);

    // All the double regs are assumed to be contiguous...
    // This is the 32-bit pop {d8-d15} form, encoding the reg relative to d8.

    Assert(firstSavedReg >= 8 && lastSavedReg < 16);
    Assert(firstSavedReg <= lastSavedReg);

    // firstSavedReg == 8 should be by far the most common path
    if (firstSavedReg == 8)
    {TRACE_IT(17746);
        op = UWOP_VPOP_32;
        encoding = ((lastSavedReg - 8) << 24);
    }
    else
    {TRACE_IT(17747);
        op = UWOP_VPOP_RANGE_32;
        encoding = firstSavedReg << 20;
        encoding |= lastSavedReg << 16;
    }
    encoding |= this->XdataTemplate(op);
    return this->WriteXdataBytes(xData, byte, encoding, this->XdataLength(op));
}


DWORD UnwindInfoManager::EmitXdataIndirReturn(BYTE xData[], DWORD byte)
{TRACE_IT(17748);
    // We're doing ldr pc,[sp],N, where N is the size of the homed params plus LR.
    // In the xdata, we encode N/4, so we can just use the register count here.
    UnwindCode op = UWOP_POP_LR_32;
    DWORD encoding = (this->homedParamCount + 1) << 16;

    encoding |= this->XdataTemplate(op);
    return this->WriteXdataBytes(xData, byte, encoding, this->XdataLength(op));
}

DWORD UnwindInfoManager::EmitXdataLocalsPointer(BYTE xData[], DWORD byte, BYTE regEncode)
{TRACE_IT(17749);
    UnwindCode op = UWOP_MOV_SP_16;
    DWORD encoding = this->XdataTemplate(op);
    encoding |= regEncode << 24;
    return this->WriteXdataBytes(xData, byte, encoding, this->XdataLength(op));
}

DWORD UnwindInfoManager::EmitXdataNop32(BYTE xData[], DWORD byte)
{TRACE_IT(17750);
    // A 32-bit NOP (not a real NOP opcode, just some 32-bit instruction that doesn't impact stack unwinding).
    UnwindCode op = UWOP_NOP_32;
    return this->WriteXdataBytes(xData, byte, this->XdataTemplate(op), this->XdataLength(op));
}

DWORD UnwindInfoManager::EmitXdataNop16(BYTE xData[], DWORD byte)
{TRACE_IT(17751);
    // A 16-bit NOP (not a real NOP opcode, just some 16-bit instruction that doesn't impact stack unwinding).
    UnwindCode op = UWOP_NOP_16;
    return this->WriteXdataBytes(xData, byte, this->XdataTemplate(op), this->XdataLength(op));
}

DWORD UnwindInfoManager::EmitXdataEnd(BYTE xData[], DWORD byte)
{TRACE_IT(17752);
    // The end of the prolog/epilog.
    UnwindCode op = UWOP_END_00;
    return this->WriteXdataBytes(xData, byte, this->XdataTemplate(op), this->XdataLength(op));
}

DWORD UnwindInfoManager::EmitXdataEndPlus16(BYTE xData[], DWORD byte)
{TRACE_IT(17753);
    // The end of the prolog/epilog plus a 16-bit unwinding NOP (such as "bx lr").
    UnwindCode op = UWOP_END_EX_16;
    return this->WriteXdataBytes(xData, byte, this->XdataTemplate(op), this->XdataLength(op));
}

DWORD UnwindInfoManager::WriteXdataBytes(BYTE xdata[], DWORD byte, DWORD encoding, DWORD length)
{TRACE_IT(17754);
    // We're required to encode the bytes from most- to least-significant. (The op bits are part of
    // the most significant byte.)

    Assert(length > 0 && length <= 4);

    if (byte + length >= MaxXdataBytes)
    {TRACE_IT(17755);
        Assert(UNREACHED);
        Fatal();
    }

    for (uint i = 0; i < length; i++)
    {TRACE_IT(17756);
        xdata[byte++] = (BYTE)(encoding >> (24 - (i * 8)));
    }

    // Return the new byte offset.
    return byte;
}

void UnwindInfoManager::SetSavedReg(BYTE reg)
{TRACE_IT(17757);
    Assert(reg <= RegEncode[RegR12]);
    this->savedRegMask |= 1 << reg;
}

void UnwindInfoManager::SetDoubleSavedRegList(DWORD doubleRegMask)
{TRACE_IT(17758);
#if DBG
    DWORD lastDoubleReg;
    DWORD firstDoubleReg;
    _BitScanReverse(&lastDoubleReg, doubleRegMask);
    _BitScanForward(&firstDoubleReg, doubleRegMask);
    Assert(lastDoubleReg <= LAST_CALLEE_SAVED_DBL_REG_NUM);
    Assert(firstDoubleReg >= FIRST_CALLEE_SAVED_DBL_REG_NUM && firstDoubleReg <= lastDoubleReg);

#endif
    this->savedDoubleRegMask = doubleRegMask;
}
DWORD UnwindInfoManager::GetDoubleSavedRegList() const
{TRACE_IT(17759);
    return this->savedDoubleRegMask;
}

DWORD UnwindInfoManager::ClearSavedReg(DWORD mask, BYTE reg) const
{TRACE_IT(17760);
    return mask & ~(1 << reg);
}

BYTE UnwindInfoManager::GetLastSavedReg(DWORD savedRegMask)
{TRACE_IT(17761);
    BVUnit32 savedRegs(savedRegMask);
    DWORD encode = savedRegs.GetPrevBit();
    Assert(encode == (BYTE)encode);
    Assert(Math::Log2(savedRegMask) == (BYTE)encode);
    return (BYTE)encode;
}

BYTE UnwindInfoManager::GetFirstSavedReg(DWORD savedRegMask)
{TRACE_IT(17762);
    BVUnit32 savedRegs(savedRegMask);
    DWORD encode = savedRegs.GetNextBit();
    Assert(encode == (BYTE)encode);
    return (BYTE)encode;
}


bool UnwindInfoManager::IsR4SavedRegRange(bool saveR11) const
{TRACE_IT(17763);
    DWORD savedRegMask = this->savedRegMask;
    if (!saveR11)
    {TRACE_IT(17764);
        savedRegMask = this->ClearSavedReg(savedRegMask, RegEncode[RegR11]);
    }
    return IsR4SavedRegRange(savedRegMask);
}

bool UnwindInfoManager::IsR4SavedRegRange(DWORD savedRegMask)
{TRACE_IT(17765);
    return ((savedRegMask + (1 << RegEncode[RegR4])) & savedRegMask) == 0;
}

bool UnwindInfoManager::GetHasChkStk() const
{TRACE_IT(17766);
    return (!LowererMD::IsSmallStack(this->stackDepth));
}
