//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ARMEncode.h"

class Encoder;

enum RelocType {
    RelocTypeBranch26,
    RelocTypeBranch19,
    RelocTypeBranch14,
    RelocTypeLabel
};

enum InstructionType {
    None    = 0,
    Integer = 1,
    Vfp     = 2
};

#define FRAME_REG           RegFP

///---------------------------------------------------------------------------
///
/// class EncoderReloc
///
///---------------------------------------------------------------------------

class EncodeReloc
{
public:
    static void     New(EncodeReloc **pHead, RelocType relocType, BYTE *offset, IR::Instr *relocInstr, ArenaAllocator *alloc) {LOGMEIN("EncoderMD.h] 32\n"); __debugbreak(); }

public:
    EncodeReloc *   m_next;
    RelocType       m_relocType;
    BYTE *          m_consumerOffset;  // offset in instruction stream
    IR::Instr *     m_relocInstr;
};



///---------------------------------------------------------------------------
///
/// class EncoderMD
///
///---------------------------------------------------------------------------

class EncoderMD
{
public:
    EncoderMD(Func * func) {LOGMEIN("EncoderMD.h] 52\n"); }
    ptrdiff_t       Encode(IR::Instr * instr, BYTE *pc, BYTE* beginCodeAddress = nullptr) {LOGMEIN("EncoderMD.h] 53\n"); __debugbreak(); return 0; }
    void            Init(Encoder *encoder) {LOGMEIN("EncoderMD.h] 54\n"); __debugbreak(); }
    void            ApplyRelocs(size_t codeBufferAddress, size_t codeSize, uint* bufferCRC, BOOL isBrShorteningSucceeded, bool isFinalBufferValidation = false) {LOGMEIN("EncoderMD.h] 55\n"); __debugbreak(); }
    static bool     TryConstFold(IR::Instr *instr, IR::RegOpnd *regOpnd) {LOGMEIN("EncoderMD.h] 56\n"); __debugbreak(); return 0; }
    static bool     TryFold(IR::Instr *instr, IR::RegOpnd *regOpnd) {LOGMEIN("EncoderMD.h] 57\n"); __debugbreak(); return 0; }
    const BYTE      GetRegEncode(IR::RegOpnd *regOpnd) {LOGMEIN("EncoderMD.h] 58\n"); __debugbreak(); return 0; }
    const BYTE      GetFloatRegEncode(IR::RegOpnd *regOpnd) {LOGMEIN("EncoderMD.h] 59\n"); __debugbreak(); return 0; }
    static const BYTE GetRegEncode(RegNum reg) {LOGMEIN("EncoderMD.h] 60\n"); __debugbreak(); return 0; }
    static uint32   GetOpdope(IR::Instr *instr) {LOGMEIN("EncoderMD.h] 61\n"); __debugbreak(); return 0; }
    static uint32   GetOpdope(Js::OpCode op) {LOGMEIN("EncoderMD.h] 62\n"); __debugbreak(); return 0; }

    static bool     IsLoad(IR::Instr *instr) {LOGMEIN("EncoderMD.h] 64\n"); __debugbreak(); return 0; }
    static bool     IsStore(IR::Instr *instr) {LOGMEIN("EncoderMD.h] 65\n"); __debugbreak(); return 0; }
    static bool     IsShifterUpdate(IR::Instr *instr) {LOGMEIN("EncoderMD.h] 66\n"); __debugbreak(); return 0; }
    static bool     IsShifterSub(IR::Instr *instr) {LOGMEIN("EncoderMD.h] 67\n"); __debugbreak(); return 0; }
    static bool     IsShifterPost(IR::Instr *instr) {LOGMEIN("EncoderMD.h] 68\n"); __debugbreak(); return 0; }
    static bool     SetsSBit(IR::Instr *instr) {LOGMEIN("EncoderMD.h] 69\n"); __debugbreak(); return 0; }

    void            AddLabelReloc(BYTE* relocAddress) {LOGMEIN("EncoderMD.h] 71\n"); __debugbreak(); }

    static bool     CanEncodeModConst12(DWORD constant) {LOGMEIN("EncoderMD.h] 73\n"); __debugbreak(); return 0; }
    static bool     CanEncodeLoadStoreOffset(int32 offset) {LOGMEIN("EncoderMD.h] 74\n"); __debugbreak(); return 0; }
    static void     BaseAndOffsetFromSym(IR::SymOpnd *symOpnd, RegNum *pBaseReg, int32 *pOffset, Func * func) {LOGMEIN("EncoderMD.h] 75\n"); __debugbreak(); }
    static bool     EncodeImmediate16(int32 constant, DWORD * result);

    void            EncodeInlineeCallInfo(IR::Instr *instr, uint32 offset) {LOGMEIN("EncoderMD.h] 78\n"); __debugbreak(); }

    static ENCODE_32 BranchOffset_26(int64 x);
};
