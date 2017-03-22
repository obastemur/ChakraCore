//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
class OpHelperBlock;
class LinearScan;
class BailOutRecord;
class BranchBailOutRecord;

class LinearScanMD : public LinearScanMDShared
{
public:
    LinearScanMD(Func *func) {LOGMEIN("LinearScanMD.h] 13\n"); }

    void        Init(LinearScan *linearScan) {LOGMEIN("LinearScanMD.h] 15\n"); __debugbreak(); }

    BitVector   FilterRegIntSizeConstraints(BitVector regsBv, BitVector sizeUsageBv) const {LOGMEIN("LinearScanMD.h] 17\n"); __debugbreak(); return 0; }
    bool        FitRegIntSizeConstraints(RegNum reg, BitVector sizeUsageBv) const {LOGMEIN("LinearScanMD.h] 18\n"); __debugbreak(); return 0; }
    void        InsertOpHelperSpillAndRestores(SList<OpHelperBlock> *opHelperBlockList) {LOGMEIN("LinearScanMD.h] 19\n"); __debugbreak(); }
    void        EndOfHelperBlock(uint32 helperSpilledLiveranges) {LOGMEIN("LinearScanMD.h] 20\n"); __debugbreak(); }

    uint        UnAllocatableRegCount(Func *func) const
              {LOGMEIN("LinearScanMD.h] 23\n");
                  __debugbreak(); return 0;
//                  return func->GetLocalsPointer() != RegSP ? 5 : 4; //r11(Frame Pointer),r12,sp,pc
              }

    StackSym   *EnsureSpillSymForVFPReg(RegNum reg, Func *func) {LOGMEIN("LinearScanMD.h] 28\n"); __debugbreak(); return 0; }

    void        LegalizeDef(IR::Instr * instr) {LOGMEIN("LinearScanMD.h] 30\n"); __debugbreak(); }
    void        LegalizeUse(IR::Instr * instr, IR::Opnd * opnd) {LOGMEIN("LinearScanMD.h] 31\n"); __debugbreak(); }
    void        LegalizeConstantUse(IR::Instr * instr, IR::Opnd * opnd) {LOGMEIN("LinearScanMD.h] 32\n"); __debugbreak(); }

    void        GenerateBailOut(IR::Instr * instr, StackSym ** registerSaveSyms, uint registerSaveSymsCount) {LOGMEIN("LinearScanMD.h] 34\n"); __debugbreak(); }
    IR::Instr  *GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo) {LOGMEIN("LinearScanMD.h] 35\n"); __debugbreak(); return nullptr; }

public:
    static void SaveAllRegistersAndBailOut(BailOutRecord *const bailOutRecord) {LOGMEIN("LinearScanMD.h] 38\n"); __debugbreak(); }
    static void SaveAllRegistersAndBranchBailOut(BranchBailOutRecord *const bailOutRecord, const BOOL condition) {LOGMEIN("LinearScanMD.h] 39\n"); __debugbreak(); }
    static RegNum GetParamReg(IR::SymOpnd *symOpnd, Func *func) {LOGMEIN("LinearScanMD.h] 40\n"); /* TODO */ return RegNOREG; }

    bool        IsAllocatable(RegNum reg, Func *func) const {LOGMEIN("LinearScanMD.h] 42\n"); __debugbreak(); return 0; }
    static uint GetRegisterSaveSlotCount() {LOGMEIN("LinearScanMD.h] 43\n");
        return RegisterSaveSlotCount ;
    }
    static uint GetRegisterSaveIndex(RegNum reg) {LOGMEIN("LinearScanMD.h] 46\n"); __debugbreak(); return 0; }
    static RegNum GetRegisterFromSaveIndex(uint offset) {LOGMEIN("LinearScanMD.h] 47\n"); __debugbreak(); return RegNum(0); }

    static const uint RegisterSaveSlotCount = RegNumCount + VFP_REGCOUNT;
};
