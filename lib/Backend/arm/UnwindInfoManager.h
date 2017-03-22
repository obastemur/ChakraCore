//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum UnwindCode;

class UnwindInfoManager
{
public:
    UnwindInfoManager() :
        func(NULL),
        fragmentStart(NULL),
        fragmentLength(0),
        epilogEndOffset(0),
        prologOffset(0),
        savedRegMask(0),
        savedDoubleRegMask(0),
        homedParamCount(0),
        stackDepth(0),
        hasCalls(false),
        prologLabelId(0),
        epilogEndLabelId(0),
        jitOutput(nullptr),
        alloc(nullptr),
        xdataTotal(0),
        pdataIndex(0),
        savedScratchReg(false)
    {LOGMEIN("UnwindInfoManager.h] 29\n");
    }

    void Init(Func * func);
    void EmitUnwindInfo(JITOutput *jitOutput, CustomHeap::Allocation * alloc);
    DWORD EmitLongUnwindInfoChunk(DWORD remainingLength);

    void SetFunc(Func *func)
    {LOGMEIN("UnwindInfoManager.h] 37\n");
        Assert(this->func == NULL);
        this->func = func;
    }
    Func * GetFunc() const
    {LOGMEIN("UnwindInfoManager.h] 42\n");
        return this->func;
    }

    void SetFragmentStart(size_t pStart)
    {LOGMEIN("UnwindInfoManager.h] 47\n");
        this->fragmentStart = pStart;
    }
    size_t GetFragmentStart() const
    {LOGMEIN("UnwindInfoManager.h] 51\n");
        return this->fragmentStart;
    }

    void SetEpilogEndOffset(DWORD offset)
    {LOGMEIN("UnwindInfoManager.h] 56\n");
        Assert(this->epilogEndOffset == 0);
        this->epilogEndOffset = offset;
    }
    DWORD GetEpilogEndOffset() const
    {LOGMEIN("UnwindInfoManager.h] 61\n");
        return this->epilogEndOffset;
    }

    void SetPrologOffset(DWORD offset)
    {LOGMEIN("UnwindInfoManager.h] 66\n");
        Assert(this->prologOffset == 0);
        this->prologOffset = offset;
    }
    DWORD GetPrologOffset() const
    {LOGMEIN("UnwindInfoManager.h] 71\n");
        return this->prologOffset;
    }

    void SetFragmentLength(DWORD length)
    {LOGMEIN("UnwindInfoManager.h] 76\n");
        this->fragmentLength = length;
    }
    DWORD GetFragmentLength() const
    {LOGMEIN("UnwindInfoManager.h] 80\n");
        return this->fragmentLength;
    }

    void SetHomedParamCount(BYTE count)
    {LOGMEIN("UnwindInfoManager.h] 85\n");
        Assert(this->homedParamCount == 0);
        this->homedParamCount = count;
    }
    DWORD GetHomedParamCount() const
    {LOGMEIN("UnwindInfoManager.h] 90\n");
        return this->homedParamCount;
    }

    void SetStackDepth(DWORD depth)
    {LOGMEIN("UnwindInfoManager.h] 95\n");
        Assert(this->stackDepth == 0);
        this->stackDepth = depth;
    }
    DWORD GetStackDepth() const
    {LOGMEIN("UnwindInfoManager.h] 100\n");
        return this->stackDepth;
    }

    void SetHasCalls(bool has)
    {LOGMEIN("UnwindInfoManager.h] 105\n");
        this->hasCalls = has;
    }
    bool GetHasCalls() const
    {LOGMEIN("UnwindInfoManager.h] 109\n");
        return this->hasCalls;
    }

    void SetPrologStartLabel(DWORD id)
    {LOGMEIN("UnwindInfoManager.h] 114\n");
        Assert(this->prologLabelId == 0);
        this->prologLabelId = id;
    }
    DWORD GetPrologStartLabel() const
    {LOGMEIN("UnwindInfoManager.h] 119\n");
        return this->prologLabelId;
    }

    void SetEpilogEndLabel(DWORD id)
    {LOGMEIN("UnwindInfoManager.h] 124\n");
        Assert(this->epilogEndLabelId == 0);
        this->epilogEndLabelId = id;
    }
    DWORD GetEpilogEndLabel() const
    {LOGMEIN("UnwindInfoManager.h] 129\n");
        return this->epilogEndLabelId;
    }

    bool GetHasChkStk() const;
    DWORD GetPDataCount(DWORD length);
    void SetSavedReg(BYTE reg);
    DWORD ClearSavedReg(DWORD mask, BYTE reg) const;

    void SetDoubleSavedRegList(DWORD doubleRegMask);
    DWORD GetDoubleSavedRegList() const;

    static BYTE GetLastSavedReg(DWORD mask);
    static BYTE GetFirstSavedReg(DWORD mask);

    void SetSavedScratchReg(bool value) {LOGMEIN("UnwindInfoManager.h] 144\n"); savedScratchReg = value; }
    bool GetSavedScratchReg() {LOGMEIN("UnwindInfoManager.h] 145\n"); return savedScratchReg; }

private:

    Func * func;
    size_t fragmentStart;
    int pdataIndex;
    int xdataTotal;
    HANDLE processHandle;
    JITOutput *jitOutput;
    CustomHeap::Allocation * alloc;
    DWORD fragmentLength;
    DWORD prologOffset;
    DWORD prologLabelId;
    DWORD epilogEndLabelId;
    DWORD epilogEndOffset;
    DWORD savedRegMask;
    DWORD savedDoubleRegMask;
    DWORD stackDepth;
    BYTE homedParamCount;
    bool hasCalls;
    bool fragmentHasProlog;
    bool fragmentHasEpilog;
    bool savedScratchReg;

    void EmitPdata();
    bool CanEmitPackedPdata() const;
    void EncodePackedUnwindData();
    void EncodeExpandedUnwindData();
    BYTE * GetBaseAddress();

    bool IsR4SavedRegRange(bool saveR11) const;
    static bool IsR4SavedRegRange(DWORD saveRegMask);

    DWORD XdataTemplate(UnwindCode op) const;
    DWORD XdataLength(UnwindCode op) const;

    DWORD EmitXdataStackAlloc(BYTE xData[], DWORD byte, DWORD stack);
    DWORD EmitXdataHomeParams(BYTE xData[], DWORD byte);
    DWORD EmitXdataRestoreRegs(BYTE xData[], DWORD byte, DWORD savedRegMask, bool restoreLR);
    DWORD EmitXdataRestoreDoubleRegs(BYTE xData[], DWORD byte, DWORD savedDoubleRegMask);
    DWORD EmitXdataIndirReturn(BYTE xData[], DWORD byte);
    DWORD EmitXdataNop32(BYTE xData[], DWORD byte);
    DWORD EmitXdataNop16(BYTE xData[], DWORD byte);
    DWORD EmitXdataEnd(BYTE xData[], DWORD byte);
    DWORD EmitXdataEndPlus16(BYTE xData[], DWORD byte);
    DWORD EmitXdataLocalsPointer(BYTE xData[], DWORD byte, BYTE regEncode);
    DWORD RelativeRegEncoding(RegNum reg, RegNum baseReg) const;
    DWORD WriteXdataBytes(BYTE xdata[], DWORD byte, DWORD encoding, DWORD length);

    void RecordPdataEntry(DWORD beginAddress, DWORD unwindData);
    // Constants defined in the ABI.

    static const DWORD MaxPackedPdataFuncLength = 0xFFE;
    static const DWORD MaxPackedPdataStackDepth = 0xFCC;

    static const DWORD PackedPdataFlagMask = 3;
    static const DWORD ExpandedPdataFlag = 0;

    // Function length is required to have only these bits set.
    static const DWORD PackedFuncLengthMask = 0xFFE;
    // Bit offset of length within pdata dword, combined with right-shift of encoded length.
    static const DWORD PackedFuncLengthShift = 1;

    static const DWORD PackedNoPrologBits = 2;
    static const DWORD PackedNormalFuncBits = 1;

    static const DWORD PackedNonLeafRetBits = 0;
    static const DWORD PackedLeafRetBits = (1 << 13);
    static const DWORD PackedNoEpilogBits = (3 << 13);

    // C (frame chaining) and L (save LR) bits.
    static const DWORD PackedNonLeafFunctionBits = (1 << 20) | (1 << 21);

    static const DWORD PackedHomedParamsBit = (1 << 15);

    static const DWORD PackedRegShift = 16;
    static const DWORD PackedRegMask = 7;
    // Indicate no saved regs with a Reg field of 0x111 and the R bit set.
    static const DWORD PackedNoSavedRegsBits = (7 << PackedRegShift) | (1 << 19);

    // Stack depth is required to have only these bits set.
    static const DWORD PackedStackDepthMask = 0xFFC;
    // Bit offset of stack depth within pdata dword, combined with right-shift of encoded value.
    static const DWORD PackedStackDepthShift = 20;

    static const DWORD MaxXdataFuncLength = 0x7FFFE;
    static const DWORD XdataFuncLengthMask = 0x7FFFE;
    static const DWORD XdataFuncLengthAdjust = 1;

    static const DWORD XdataSingleEpilogShift = 21;
    static const DWORD XdataFuncFragmentShift = 22;
    static const DWORD XdataEpilogCountShift = 23;

    static const DWORD MaxXdataEpilogCount = 0x1F;
    static const DWORD MaxXdataDwordCount = 0xF;
    static const DWORD XdataDwordCountShift = 28;

public:
    // Xdata constants.
    static const DWORD MaxXdataBytes = 40; //buffer of 4 for any future additions
    //
    // 28 == 4 (header DWORD) +
    //      (4 (max stack alloc code) +
    //       1 (locals pointer setup) +
    //       5 (NOP for _chkstk case) +
    //       1 (double reg saves) +
    //       2 (reg saves) +
    //       1 (r11 setup) +
    //       2 (r11,lr saves) +
    //       1 (home params) +
    //       1 (NOP) +
    //       1 (end prolog) +
    //       4 (max stack alloc code, in case of locals pointer setup) +
    //       1 (double reg saves) +
    //       2 (reg saves) +
    //       2 (r11 save) +
    //       2 (indir return) +
    //       1 (end epilog)) rounded up to a DWORD boundary

};
