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
        pdataArray(NULL),
        xdataArray(NULL)
    {LOGMEIN("UnwindInfoManager.h] 26\n");
    }

    void Init(Func * func) {LOGMEIN("UnwindInfoManager.h] 29\n"); this->func = func; }
    void EmitUnwindInfo(PBYTE funcStart, DWORD size, CustomHeap::Allocation* allocation) {LOGMEIN("UnwindInfoManager.h] 30\n"); __debugbreak(); }
    DWORD EmitLongUnwindInfoChunk(DWORD remainingLength) {LOGMEIN("UnwindInfoManager.h] 31\n"); __debugbreak(); }

    void SetFunc(Func *func)
    {LOGMEIN("UnwindInfoManager.h] 34\n");
        Assert(this->func == NULL);
        this->func = func;
    }
    Func * GetFunc() const
    {LOGMEIN("UnwindInfoManager.h] 39\n");
        return this->func;
    }

    void SetFragmentStart(PBYTE pStart)
    {LOGMEIN("UnwindInfoManager.h] 44\n");
        this->fragmentStart = pStart;
    }
    PBYTE GetFragmentStart() const
    {LOGMEIN("UnwindInfoManager.h] 48\n");
        return this->fragmentStart;
    }

    void SetEpilogEndOffset(DWORD offset)
    {LOGMEIN("UnwindInfoManager.h] 53\n");
        Assert(this->epilogEndOffset == 0);
        this->epilogEndOffset = offset;
    }
    DWORD GetEpilogEndOffset() const
    {LOGMEIN("UnwindInfoManager.h] 58\n");
        return this->epilogEndOffset;
    }

    void SetPrologOffset(DWORD offset)
    {LOGMEIN("UnwindInfoManager.h] 63\n");
        Assert(this->prologOffset == 0);
        this->prologOffset = offset;
    }
    DWORD GetPrologOffset() const
    {LOGMEIN("UnwindInfoManager.h] 68\n");
        return this->prologOffset;
    }

    void SetFragmentLength(DWORD length)
    {LOGMEIN("UnwindInfoManager.h] 73\n");
        this->fragmentLength = length;
    }
    DWORD GetFragmentLength() const
    {LOGMEIN("UnwindInfoManager.h] 77\n");
        return this->fragmentLength;
    }

    void SetHomedParamCount(BYTE count)
    {LOGMEIN("UnwindInfoManager.h] 82\n");
        Assert(this->homedParamCount == 0);
        this->homedParamCount = count;
    }
    DWORD GetHomedParamCount() const
    {LOGMEIN("UnwindInfoManager.h] 87\n");
        return this->homedParamCount;
    }

    void SetStackDepth(DWORD depth)
    {LOGMEIN("UnwindInfoManager.h] 92\n");
        Assert(this->stackDepth == 0);
        this->stackDepth = depth;
    }
    DWORD GetStackDepth() const
    {LOGMEIN("UnwindInfoManager.h] 97\n");
        return this->stackDepth;
    }

    void SetHasCalls(bool has)
    {LOGMEIN("UnwindInfoManager.h] 102\n");
        this->hasCalls = has;
    }
    bool GetHasCalls() const
    {LOGMEIN("UnwindInfoManager.h] 106\n");
        return this->hasCalls;
    }

    void SetPrologStartLabel(DWORD id)
    {LOGMEIN("UnwindInfoManager.h] 111\n");
        Assert(this->prologLabelId == 0);
        this->prologLabelId = id;
    }
    DWORD GetPrologStartLabel() const
    {LOGMEIN("UnwindInfoManager.h] 116\n");
        return this->prologLabelId;
    }

    void SetEpilogEndLabel(DWORD id)
    {LOGMEIN("UnwindInfoManager.h] 121\n");
        Assert(this->epilogEndLabelId == 0);
        this->epilogEndLabelId = id;
    }
    DWORD GetEpilogEndLabel() const
    {LOGMEIN("UnwindInfoManager.h] 126\n");
        return this->epilogEndLabelId;
    }

    bool GetHasChkStk() const {LOGMEIN("UnwindInfoManager.h] 130\n"); __debugbreak(); return 0; }
    DWORD GetPDataCount(DWORD length) {LOGMEIN("UnwindInfoManager.h] 131\n"); __debugbreak(); return 0; }
    void SetSavedReg(BYTE reg) {LOGMEIN("UnwindInfoManager.h] 132\n"); __debugbreak(); }
    DWORD ClearSavedReg(DWORD mask, BYTE reg) const {LOGMEIN("UnwindInfoManager.h] 133\n"); __debugbreak(); return 0; }

    void SetDoubleSavedRegList(DWORD doubleRegMask) {LOGMEIN("UnwindInfoManager.h] 135\n"); __debugbreak(); }
    DWORD GetDoubleSavedRegList() const {LOGMEIN("UnwindInfoManager.h] 136\n"); __debugbreak(); return 0; }

    static BYTE GetLastSavedReg(DWORD mask) {LOGMEIN("UnwindInfoManager.h] 138\n"); __debugbreak(); return 0; }
    static BYTE GetFirstSavedReg(DWORD mask) {LOGMEIN("UnwindInfoManager.h] 139\n"); __debugbreak(); return 0; }

private:

    Func * func;
    PBYTE fragmentStart;
    RUNTIME_FUNCTION* pdataArray;
    BYTE* xdataArray;
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
