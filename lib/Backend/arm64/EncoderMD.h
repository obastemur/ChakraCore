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
    static void     New(EncodeReloc **pHead, RelocType relocType, BYTE *offset, IR::Instr *relocInstr, ArenaAllocator *alloc) {TRACE_IT(17793); __debugbreak(); }

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
    EncoderMD(Func * func) {TRACE_IT(17794); }
    ptrdiff_t       Encode(IR::Instr * instr, BYTE *pc, BYTE* beginCodeAddress = nullptr) {TRACE_IT(17795); __debugbreak(); return 0; }
    void            Init(Encoder *encoder) {TRACE_IT(17796); __debugbreak(); }
    void            ApplyRelocs(size_t codeBufferAddress, size_t codeSize, uint* bufferCRC, BOOL isBrShorteningSucceeded, bool isFinalBufferValidation = false) {TRACE_IT(17797); __debugbreak(); }
    static bool     TryConstFold(IR::Instr *instr, IR::RegOpnd *regOpnd) {TRACE_IT(17798); __debugbreak(); return 0; }
    static bool     TryFold(IR::Instr *instr, IR::RegOpnd *regOpnd) {TRACE_IT(17799); __debugbreak(); return 0; }
    const BYTE      GetRegEncode(IR::RegOpnd *regOpnd) {TRACE_IT(17800); __debugbreak(); return 0; }
    const BYTE      GetFloatRegEncode(IR::RegOpnd *regOpnd) {TRACE_IT(17801); __debugbreak(); return 0; }
    static const BYTE GetRegEncode(RegNum reg) {TRACE_IT(17802); __debugbreak(); return 0; }
    static uint32   GetOpdope(IR::Instr *instr) {TRACE_IT(17803); __debugbreak(); return 0; }
    static uint32   GetOpdope(Js::OpCode op) {TRACE_IT(17804); __debugbreak(); return 0; }

    static bool     IsLoad(IR::Instr *instr) {TRACE_IT(17805); __debugbreak(); return 0; }
    static bool     IsStore(IR::Instr *instr) {TRACE_IT(17806); __debugbreak(); return 0; }
    static bool     IsShifterUpdate(IR::Instr *instr) {TRACE_IT(17807); __debugbreak(); return 0; }
    static bool     IsShifterSub(IR::Instr *instr) {TRACE_IT(17808); __debugbreak(); return 0; }
    static bool     IsShifterPost(IR::Instr *instr) {TRACE_IT(17809); __debugbreak(); return 0; }
    static bool     SetsSBit(IR::Instr *instr) {TRACE_IT(17810); __debugbreak(); return 0; }

    void            AddLabelReloc(BYTE* relocAddress) {TRACE_IT(17811); __debugbreak(); }

    static bool     CanEncodeModConst12(DWORD constant) {TRACE_IT(17812); __debugbreak(); return 0; }
    static bool     CanEncodeLoadStoreOffset(int32 offset) {TRACE_IT(17813); __debugbreak(); return 0; }
    static void     BaseAndOffsetFromSym(IR::SymOpnd *symOpnd, RegNum *pBaseReg, int32 *pOffset, Func * func) {TRACE_IT(17814); __debugbreak(); }
    static bool     EncodeImmediate16(int32 constant, DWORD * result);

    void            EncodeInlineeCallInfo(IR::Instr *instr, uint32 offset) {TRACE_IT(17815); __debugbreak(); }

    static ENCODE_32 BranchOffset_26(int64 x);
};
