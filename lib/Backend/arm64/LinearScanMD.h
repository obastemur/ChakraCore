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
    LinearScanMD(Func *func) {TRACE_IT(17816); }

    void        Init(LinearScan *linearScan) {TRACE_IT(17817); __debugbreak(); }

    BitVector   FilterRegIntSizeConstraints(BitVector regsBv, BitVector sizeUsageBv) const {TRACE_IT(17818); __debugbreak(); return 0; }
    bool        FitRegIntSizeConstraints(RegNum reg, BitVector sizeUsageBv) const {TRACE_IT(17819); __debugbreak(); return 0; }
    void        InsertOpHelperSpillAndRestores(SList<OpHelperBlock> *opHelperBlockList) {TRACE_IT(17820); __debugbreak(); }
    void        EndOfHelperBlock(uint32 helperSpilledLiveranges) {TRACE_IT(17821); __debugbreak(); }

    uint        UnAllocatableRegCount(Func *func) const
              {TRACE_IT(17822);
                  __debugbreak(); return 0;
//                  return func->GetLocalsPointer() != RegSP ? 5 : 4; //r11(Frame Pointer),r12,sp,pc
              }

    StackSym   *EnsureSpillSymForVFPReg(RegNum reg, Func *func) {TRACE_IT(17823); __debugbreak(); return 0; }

    void        LegalizeDef(IR::Instr * instr) {TRACE_IT(17824); __debugbreak(); }
    void        LegalizeUse(IR::Instr * instr, IR::Opnd * opnd) {TRACE_IT(17825); __debugbreak(); }
    void        LegalizeConstantUse(IR::Instr * instr, IR::Opnd * opnd) {TRACE_IT(17826); __debugbreak(); }

    void        GenerateBailOut(IR::Instr * instr, StackSym ** registerSaveSyms, uint registerSaveSymsCount) {TRACE_IT(17827); __debugbreak(); }
    IR::Instr  *GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo) {TRACE_IT(17828); __debugbreak(); return nullptr; }

public:
    static void SaveAllRegistersAndBailOut(BailOutRecord *const bailOutRecord) {TRACE_IT(17829); __debugbreak(); }
    static void SaveAllRegistersAndBranchBailOut(BranchBailOutRecord *const bailOutRecord, const BOOL condition) {TRACE_IT(17830); __debugbreak(); }
    static RegNum GetParamReg(IR::SymOpnd *symOpnd, Func *func) {TRACE_IT(17831); /* TODO */ return RegNOREG; }

    bool        IsAllocatable(RegNum reg, Func *func) const {TRACE_IT(17832); __debugbreak(); return 0; }
    static uint GetRegisterSaveSlotCount() {TRACE_IT(17833);
        return RegisterSaveSlotCount ;
    }
    static uint GetRegisterSaveIndex(RegNum reg) {TRACE_IT(17834); __debugbreak(); return 0; }
    static RegNum GetRegisterFromSaveIndex(uint offset) {TRACE_IT(17835); __debugbreak(); return RegNum(0); }

    static const uint RegisterSaveSlotCount = RegNumCount + VFP_REGCOUNT;
};
