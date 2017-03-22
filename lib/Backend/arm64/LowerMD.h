//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Lowerer;

///---------------------------------------------------------------------------
///
/// class LowererMD
///
///---------------------------------------------------------------------------

#ifdef DBG
#define INSERTDEBUGBREAK(instrInsert)\
    {LOGMEIN("LowerMD.h] 16\n");\
        IR::Instr *int3 = IR::Instr::New(Js::OpCode::DEBUGBREAK, m_func);\
        instrInsert->InsertBefore(int3);\
    }
#else
#define INSERTDEBUGBREAK(instrInsert)
#endif

class LowererMD
{
public:
    static const int    MaxArgumentsToHelper = 16;

    LowererMD(Func *func) {LOGMEIN("LowerMD.h] 29\n"); }

    static  bool            IsUnconditionalBranch(const IR::Instr *instr) {LOGMEIN("LowerMD.h] 31\n"); __debugbreak(); return 0; }
    static  bool            IsAssign(const IR::Instr *instr) {LOGMEIN("LowerMD.h] 32\n"); __debugbreak(); return 0; }
    static  bool            IsCall(const IR::Instr *instr) {LOGMEIN("LowerMD.h] 33\n"); __debugbreak(); return 0; }
    static  bool            IsIndirectBranch(const IR::Instr *instr) {LOGMEIN("LowerMD.h] 34\n"); __debugbreak(); return 0; }
    static  bool            IsReturnInstr(const IR::Instr *instr) {LOGMEIN("LowerMD.h] 35\n"); __debugbreak(); return 0; }
    static  void            InvertBranch(IR::BranchInstr *instr) {LOGMEIN("LowerMD.h] 36\n"); __debugbreak(); }
    static Js::OpCode       MDBranchOpcode(Js::OpCode opcode) {LOGMEIN("LowerMD.h] 37\n"); __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static Js::OpCode       MDUnsignedBranchOpcode(Js::OpCode opcode) {LOGMEIN("LowerMD.h] 38\n"); __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static Js::OpCode       MDCompareWithZeroBranchOpcode(Js::OpCode opcode) {LOGMEIN("LowerMD.h] 39\n"); __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static Js::OpCode       MDConvertFloat64ToInt32Opcode(const bool roundTowardZero) {LOGMEIN("LowerMD.h] 40\n"); __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static void             ChangeToAdd(IR::Instr *const instr, const bool needFlags) {LOGMEIN("LowerMD.h] 41\n"); __debugbreak(); }
    static void             ChangeToSub(IR::Instr *const instr, const bool needFlags) {LOGMEIN("LowerMD.h] 42\n"); __debugbreak(); }
    static void             ChangeToShift(IR::Instr *const instr, const bool needFlags) {LOGMEIN("LowerMD.h] 43\n"); __debugbreak(); }
    static const uint16     GetFormalParamOffset() {LOGMEIN("LowerMD.h] 44\n"); __debugbreak(); return 0; }

    static const Js::OpCode MDUncondBranchOpcode;
    static const Js::OpCode MDTestOpcode;
    static const Js::OpCode MDOrOpcode;
    static const Js::OpCode MDXorOpcode;
    static const Js::OpCode MDOverflowBranchOpcode;
    static const Js::OpCode MDNotOverflowBranchOpcode;
    static const Js::OpCode MDConvertFloat32ToFloat64Opcode;
    static const Js::OpCode MDConvertFloat64ToFloat32Opcode;
    static const Js::OpCode MDCallOpcode;
    static const Js::OpCode MDImulOpcode;

public:
            void            Init(Lowerer *lowerer) {LOGMEIN("LowerMD.h] 58\n"); __debugbreak(); }
            void            FinalLower(){LOGMEIN("LowerMD.h] 59\n"); __debugbreak(); }
            bool            FinalLowerAssign(IR::Instr* instr){LOGMEIN("LowerMD.h] 60\n"); __debugbreak(); return 0;  };
            IR::Opnd *      GenerateMemRef(intptr_t addr, IRType type, IR::Instr *instr, bool dontEncode = false) {LOGMEIN("LowerMD.h] 61\n"); __debugbreak(); return 0; }
            IR::Instr *     ChangeToHelperCall(IR::Instr * instr, IR::JnHelperMethod helperMethod, IR::LabelInstr *labelBailOut = NULL,
                            IR::Opnd *opndInstance = NULL, IR::PropertySymOpnd * propSymOpnd = nullptr, bool isHelperContinuation = false) {LOGMEIN("LowerMD.h] 63\n"); __debugbreak(); return 0; }
            IR::Instr *     ChangeToHelperCallMem(IR::Instr * instr, IR::JnHelperMethod helperMethod) {LOGMEIN("LowerMD.h] 64\n"); __debugbreak(); return 0; }
    static  IR::Instr *     CreateAssign(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsertPt) {LOGMEIN("LowerMD.h] 65\n"); __debugbreak(); return 0; }
    static  IR::Instr *     ChangeToAssign(IR::Instr * instr) {LOGMEIN("LowerMD.h] 66\n"); __debugbreak(); return 0; }
    static  IR::Instr *     ChangeToAssign(IR::Instr * instr, IRType type) {LOGMEIN("LowerMD.h] 67\n"); __debugbreak(); return 0; }
    static  IR::Instr *     ChangeToLea(IR::Instr *const instr, bool postRegAlloc = false) {LOGMEIN("LowerMD.h] 68\n"); __debugbreak(); return 0; }
    static  IR::Instr *     ForceDstToReg(IR::Instr *instr) {LOGMEIN("LowerMD.h] 69\n"); __debugbreak(); return 0; }
    static  void            ImmedSrcToReg(IR::Instr * instr, IR::Opnd * newOpnd, int srcNum) {LOGMEIN("LowerMD.h] 70\n"); __debugbreak(); }

            IR::Instr *     LoadArgumentCount(IR::Instr * instr) {LOGMEIN("LowerMD.h] 72\n"); __debugbreak(); return 0; }
            IR::Instr *     LoadStackArgPtr(IR::Instr * instr) {LOGMEIN("LowerMD.h] 73\n"); __debugbreak(); return 0; }
              IR::Instr *     LoadHeapArguments(IR::Instr * instrArgs) {LOGMEIN("LowerMD.h] 74\n"); __debugbreak(); return 0; }
              IR::Instr *     LoadHeapArgsCached(IR::Instr * instr) {LOGMEIN("LowerMD.h] 75\n"); __debugbreak(); return 0; }
              IR::Instr *     LoadInputParamCount(IR::Instr * instr, int adjust = 0, bool needFlags = false) {LOGMEIN("LowerMD.h] 76\n"); __debugbreak(); return 0; }
              IR::Instr *     LoadArgumentsFromFrame(IR::Instr * instr) {LOGMEIN("LowerMD.h] 77\n"); __debugbreak(); return 0; }
              IR::Instr *     LoadFuncExpression(IR::Instr * instr) {LOGMEIN("LowerMD.h] 78\n"); __debugbreak(); return 0; }
              IR::Instr *     LowerRet(IR::Instr * instr) {LOGMEIN("LowerMD.h] 79\n"); __debugbreak(); return 0; }
      static  IR::Instr *     LowerUncondBranch(IR::Instr * instr) {LOGMEIN("LowerMD.h] 80\n"); __debugbreak(); return 0; }
      static  IR::Instr *     LowerMultiBranch(IR::Instr * instr) {LOGMEIN("LowerMD.h] 81\n"); __debugbreak(); return 0; }
              IR::Instr *     LowerCondBranch(IR::Instr * instr) {LOGMEIN("LowerMD.h] 82\n"); __debugbreak(); return 0; }
              IR::Instr *     LoadFunctionObjectOpnd(IR::Instr *instr, IR::Opnd *&functionObjOpnd) {LOGMEIN("LowerMD.h] 83\n"); __debugbreak(); return 0; }
              IR::Instr *     LowerLdSuper(IR::Instr * instr, IR::JnHelperMethod helperOpCode) {LOGMEIN("LowerMD.h] 84\n"); __debugbreak(); return 0; }
              IR::Instr *     GenerateSmIntPairTest(IR::Instr * instrInsert, IR::Opnd * opndSrc1, IR::Opnd * opndSrc2, IR::LabelInstr * labelFail) {LOGMEIN("LowerMD.h] 85\n"); __debugbreak(); return 0; }
              void            GenerateTaggedZeroTest( IR::Opnd * opndSrc, IR::Instr * instrInsert, IR::LabelInstr * labelHelper = NULL) {LOGMEIN("LowerMD.h] 86\n"); __debugbreak(); }
              void            GenerateObjectPairTest(IR::Opnd * opndSrc1, IR::Opnd * opndSrc2, IR::Instr * insertInstr, IR::LabelInstr * labelTarget) {LOGMEIN("LowerMD.h] 87\n"); __debugbreak(); }
              bool            GenerateObjectTest(IR::Opnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel = false) {LOGMEIN("LowerMD.h] 88\n"); __debugbreak(); return false; }
              bool            GenerateFastBrOrCmString(IR::Instr* instr) {LOGMEIN("LowerMD.h] 89\n"); __debugbreak(); return 0; }
              bool            GenerateFastStringCheck(IR::Instr* instr, IR::RegOpnd *srcReg1, IR::RegOpnd *srcReg2, bool isEqual, bool isStrict, IR::LabelInstr *labelHelper, IR::LabelInstr *labelBranchSuccess, IR::LabelInstr *labelBranchFail) {LOGMEIN("LowerMD.h] 90\n"); __debugbreak(); return 0; }
              bool            GenerateFastCmSrEqConst(IR::Instr *instr) {LOGMEIN("LowerMD.h] 91\n"); __debugbreak(); return 0; }
              bool            GenerateFastCmXxI4(IR::Instr *instr) {LOGMEIN("LowerMD.h] 92\n"); __debugbreak(); return 0; }
              bool            GenerateFastCmXxR8(IR::Instr *instr) {LOGMEIN("LowerMD.h] 93\n"); Assert(UNREACHED); return NULL; }
              bool            GenerateFastCmXxTaggedInt(IR::Instr *instr) {LOGMEIN("LowerMD.h] 94\n"); __debugbreak(); return 0; }
              IR::Instr *     GenerateConvBool(IR::Instr *instr) {LOGMEIN("LowerMD.h] 95\n"); __debugbreak(); return 0; }

              void            GenerateClz(IR::Instr * instr) {LOGMEIN("LowerMD.h] 97\n"); __debugbreak(); }
              void            GenerateCtz(IR::Instr * instr) {LOGMEIN("LowerMD.h] 98\n"); __debugbreak(); }
              void            GeneratePopCnt(IR::Instr * instr) {LOGMEIN("LowerMD.h] 99\n"); __debugbreak(); }
              void            GenerateFastDivByPow2(IR::Instr *instr) {LOGMEIN("LowerMD.h] 100\n"); __debugbreak(); }
              bool            GenerateFastAdd(IR::Instr * instrAdd) {LOGMEIN("LowerMD.h] 101\n"); __debugbreak(); return 0; }
              bool            GenerateFastSub(IR::Instr * instrSub) {LOGMEIN("LowerMD.h] 102\n"); __debugbreak(); return 0; }
              bool            GenerateFastMul(IR::Instr * instrMul) {LOGMEIN("LowerMD.h] 103\n"); __debugbreak(); return 0; }
              bool            GenerateFastAnd(IR::Instr * instrAnd) {LOGMEIN("LowerMD.h] 104\n"); __debugbreak(); return 0; }
              bool            GenerateFastXor(IR::Instr * instrXor) {LOGMEIN("LowerMD.h] 105\n"); __debugbreak(); return 0; }
              bool            GenerateFastOr(IR::Instr * instrOr) {LOGMEIN("LowerMD.h] 106\n"); __debugbreak(); return 0; }
              bool            GenerateFastNot(IR::Instr * instrNot) {LOGMEIN("LowerMD.h] 107\n"); __debugbreak(); return 0; }
              bool            GenerateFastNeg(IR::Instr * instrNeg) {LOGMEIN("LowerMD.h] 108\n"); __debugbreak(); return 0; }
              bool            GenerateFastShiftLeft(IR::Instr * instrShift) {LOGMEIN("LowerMD.h] 109\n"); __debugbreak(); return 0; }
              bool            GenerateFastShiftRight(IR::Instr * instrShift) {LOGMEIN("LowerMD.h] 110\n"); __debugbreak(); return 0; }
              void            GenerateFastBrS(IR::BranchInstr *brInstr) {LOGMEIN("LowerMD.h] 111\n"); __debugbreak(); }
              IR::IndirOpnd * GenerateFastElemIStringIndexCommon(IR::Instr * instr, bool isStore, IR::IndirOpnd *indirOpnd, IR::LabelInstr * labelHelper) {LOGMEIN("LowerMD.h] 112\n"); __debugbreak(); return 0; }
              void            GenerateFastInlineBuiltInCall(IR::Instr* instr, IR::JnHelperMethod helperMethod) {LOGMEIN("LowerMD.h] 113\n"); __debugbreak(); }
              void            HelperCallForAsmMathBuiltin(IR::Instr* instr, IR::JnHelperMethod helperMethodFloat, IR::JnHelperMethod helperMethodDouble) {LOGMEIN("LowerMD.h] 114\n"); __debugbreak(); }
              IR::Opnd *      CreateStackArgumentsSlotOpnd() {LOGMEIN("LowerMD.h] 115\n"); __debugbreak(); return 0; }
              void            GenerateSmIntTest(IR::Opnd *opndSrc, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::Instr **instrFirst = nullptr, bool fContinueLabel = false) {LOGMEIN("LowerMD.h] 116\n"); __debugbreak(); }
              IR::RegOpnd *   LoadNonnegativeIndex(IR::RegOpnd *indexOpnd, const bool skipNegativeCheck, IR::LabelInstr *const notTaggedIntLabel, IR::LabelInstr *const negativeLabel, IR::Instr *const insertBeforeInstr) {LOGMEIN("LowerMD.h] 117\n"); __debugbreak(); return nullptr; }
              IR::RegOpnd *   GenerateUntagVar(IR::RegOpnd * opnd, IR::LabelInstr * labelFail, IR::Instr * insertBeforeInstr, bool generateTagCheck = true) {LOGMEIN("LowerMD.h] 118\n"); __debugbreak(); return 0; }
              bool            GenerateFastLdMethodFromFlags(IR::Instr * instrLdFld) {LOGMEIN("LowerMD.h] 119\n"); __debugbreak(); return 0; }
              void            GenerateInt32ToVarConversion( IR::Opnd * opndSrc, IR::Instr * insertInstr ) {LOGMEIN("LowerMD.h] 120\n"); __debugbreak(); }
              IR::Instr *     GenerateFastScopedFld(IR::Instr * instrScopedFld, bool isLoad) {LOGMEIN("LowerMD.h] 121\n"); __debugbreak(); return 0; }
              IR::Instr *     GenerateFastScopedLdFld(IR::Instr * instrLdFld) {LOGMEIN("LowerMD.h] 122\n"); __debugbreak(); return 0; }
              IR::Instr *     GenerateFastScopedStFld(IR::Instr * instrStFld) {LOGMEIN("LowerMD.h] 123\n"); __debugbreak(); return 0; }
              bool            GenerateJSBooleanTest(IR::RegOpnd * regSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel = false) {LOGMEIN("LowerMD.h] 124\n"); __debugbreak(); return 0; }
              bool            TryGenerateFastFloatOp(IR::Instr * instr, IR::Instr ** pInsertHelper, bool *pfNoLower) {LOGMEIN("LowerMD.h] 125\n"); __debugbreak(); return 0; }
              bool            GenerateFastFloatCall(IR::Instr * instr, IR::Instr ** pInsertHelper, bool noFieldFastPath, bool *pfNoLower, IR::Instr **pInstrPrev) {LOGMEIN("LowerMD.h] 126\n"); __debugbreak(); return 0; }
              bool            GenerateFastFloatBranch(IR::BranchInstr * instr, IR::Instr ** pInsertHelper, bool *pfNoLower) {LOGMEIN("LowerMD.h] 127\n"); __debugbreak(); return 0; }
              void            GenerateFastAbs(IR::Opnd *dst, IR::Opnd *src, IR::Instr *callInstr, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel) {LOGMEIN("LowerMD.h] 128\n"); __debugbreak(); }
              bool            GenerateFastCharAt(Js::BuiltinFunction index, IR::Opnd *dst, IR::Opnd *srcStr, IR::Opnd *srcIndex, IR::Instr *callInstr, IR::Instr *insertInstr,
                  IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel) {LOGMEIN("LowerMD.h] 130\n"); __debugbreak(); return 0; }
              bool            TryGenerateFastMulAdd(IR::Instr * instrAdd, IR::Instr ** pInstrPrev) {LOGMEIN("LowerMD.h] 131\n"); __debugbreak(); return 0; }
              void            GenerateIsDynamicObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool fContinueLabel = false) {LOGMEIN("LowerMD.h] 132\n"); __debugbreak(); }
              void            GenerateIsRecyclableObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool checkObjectAndDynamicObject = true) {LOGMEIN("LowerMD.h] 133\n"); __debugbreak(); }
              bool            GenerateLdThisCheck(IR::Instr * instr) {LOGMEIN("LowerMD.h] 134\n"); __debugbreak(); return 0; }
              bool            GenerateLdThisStrict(IR::Instr* instr) {LOGMEIN("LowerMD.h] 135\n"); __debugbreak(); return 0; }
              void            GenerateFloatTest(IR::RegOpnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr* labelHelper) {LOGMEIN("LowerMD.h] 136\n"); __debugbreak(); }
              void            GenerateFunctionObjectTest(IR::Instr * callInstr, IR::RegOpnd  *functionObjOpnd, bool isHelper, IR::LabelInstr* afterCallLabel = nullptr) {LOGMEIN("LowerMD.h] 137\n"); __debugbreak(); }

       static void            EmitInt4Instr(IR::Instr *instr) {LOGMEIN("LowerMD.h] 139\n"); __debugbreak(); }
       static void            EmitPtrInstr(IR::Instr *instr) {LOGMEIN("LowerMD.h] 140\n"); __debugbreak(); }
              void            EmitLoadVar(IR::Instr *instr, bool isFromUint32 = false, bool isHelper = false) {LOGMEIN("LowerMD.h] 141\n"); __debugbreak(); }
              bool            EmitLoadInt32(IR::Instr *instr, bool conversionFromObjectAllowed) {LOGMEIN("LowerMD.h] 142\n"); __debugbreak(); return 0; }
              IR::Instr *     LowerInt64Assign(IR::Instr * instr) {LOGMEIN("LowerMD.h] 143\n"); __debugbreak(); return nullptr; }

       static void            LowerInt4NegWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {LOGMEIN("LowerMD.h] 145\n"); __debugbreak(); }
       static void            LowerInt4AddWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {LOGMEIN("LowerMD.h] 146\n"); __debugbreak(); }
       static void            LowerInt4SubWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {LOGMEIN("LowerMD.h] 147\n"); __debugbreak(); }
       static void            LowerInt4MulWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {LOGMEIN("LowerMD.h] 148\n"); __debugbreak(); }
       static void            LowerInt4RemWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {LOGMEIN("LowerMD.h] 149\n"); __debugbreak(); }
              void            MarkOneFltTmpSym(StackSym *sym, BVSparse<ArenaAllocator> *bvTmps, bool fFltPrefOp) {LOGMEIN("LowerMD.h] 150\n"); __debugbreak(); }
              void            GenerateNumberAllocation(IR::RegOpnd * opndDst, IR::Instr * instrInsert, bool isHelper) {LOGMEIN("LowerMD.h] 151\n"); __debugbreak(); }
              void            GenerateFastRecyclerAlloc(size_t allocSize, IR::RegOpnd* newObjDst, IR::Instr* insertionPointInstr, IR::LabelInstr* allocHelperLabel, IR::LabelInstr* allocDoneLabel) {LOGMEIN("LowerMD.h] 152\n"); __debugbreak(); }
              void            SaveDoubleToVar(IR::RegOpnd * dstOpnd, IR::RegOpnd *opndFloat, IR::Instr *instrOrig, IR::Instr *instrInsert, bool isHelper = false) {LOGMEIN("LowerMD.h] 153\n"); __debugbreak(); }
              IR::RegOpnd *   EmitLoadFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr) {LOGMEIN("LowerMD.h] 154\n"); __debugbreak(); return 0; }
              IR::Instr *     LoadCheckedFloat(IR::RegOpnd *opndOrig, IR::RegOpnd *opndFloat, IR::LabelInstr *labelInline, IR::LabelInstr *labelHelper, IR::Instr *instrInsert) {LOGMEIN("LowerMD.h] 155\n"); __debugbreak(); return 0; }

              void LoadFloatValue(IR::RegOpnd * javascriptNumber, IR::RegOpnd * opndFloat, IR::LabelInstr * labelHelper, IR::Instr * instrInsert) {LOGMEIN("LowerMD.h] 157\n"); __debugbreak(); }

              IR::Instr *     LoadStackAddress(StackSym *sym, IR::RegOpnd* regDst = nullptr) {LOGMEIN("LowerMD.h] 159\n"); __debugbreak(); return 0; }
              IR::Instr *     LowerCatch(IR::Instr *instr) {LOGMEIN("LowerMD.h] 160\n"); __debugbreak(); return 0; }

              IR::Instr *     LowerGetCachedFunc(IR::Instr *instr) {LOGMEIN("LowerMD.h] 162\n"); __debugbreak(); return 0; }
              IR::Instr *     LowerCommitScope(IR::Instr *instr) {LOGMEIN("LowerMD.h] 163\n"); __debugbreak(); return 0; }

              IR::Instr *     LowerCallHelper(IR::Instr *instrCall) {LOGMEIN("LowerMD.h] 165\n"); __debugbreak(); return 0; }

              IR::LabelInstr *GetBailOutStackRestoreLabel(BailOutInfo * bailOutInfo, IR::LabelInstr * exitTargetInstr) {LOGMEIN("LowerMD.h] 167\n"); __debugbreak(); return 0; }
              bool            AnyFloatTmps(void) {LOGMEIN("LowerMD.h] 168\n"); __debugbreak(); return 0; }
              IR::LabelInstr* InsertBeforeRecoveryForFloatTemps(IR::Instr * insertBefore, IR::LabelInstr * labelRecover) {LOGMEIN("LowerMD.h] 169\n"); __debugbreak(); return 0; }
              StackSym *      GetImplicitParamSlotSym(Js::ArgSlot argSlot) {LOGMEIN("LowerMD.h] 170\n"); __debugbreak(); return 0; }
       static StackSym *      GetImplicitParamSlotSym(Js::ArgSlot argSlot, Func * func) {LOGMEIN("LowerMD.h] 171\n"); __debugbreak(); return 0; }
              bool            GenerateFastIsInst(IR::Instr * instr, Js::ScriptContext * scriptContext) {LOGMEIN("LowerMD.h] 172\n"); __debugbreak(); return 0; }

              IR::Instr *     LowerDivI4AndBailOnReminder(IR::Instr * instr, IR::LabelInstr * bailOutLabel) {LOGMEIN("LowerMD.h] 174\n"); __debugbreak(); return NULL; }
              bool            GenerateFastIsInst(IR::Instr * instr) {LOGMEIN("LowerMD.h] 175\n"); __debugbreak(); return false; }
  public:
              IR::Instr *         LowerCall(IR::Instr * callInstr, Js::ArgSlot argCount) {LOGMEIN("LowerMD.h] 177\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerCallI(IR::Instr * callInstr, ushort callFlags, bool isHelper = false, IR::Instr* insertBeforeInstrForCFG = nullptr) {LOGMEIN("LowerMD.h] 178\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerCallPut(IR::Instr * callInstr) {LOGMEIN("LowerMD.h] 179\n"); __debugbreak(); return 0; }
              int32               LowerCallArgs(IR::Instr * callInstr, IR::Instr * stackParamInsert, ushort callFlags) {LOGMEIN("LowerMD.h] 180\n"); __debugbreak(); return 0; }
              int32               LowerCallArgs(IR::Instr * callInstr, ushort callFlags, Js::ArgSlot extraParams = 1 /* for function object */, IR::IntConstOpnd **callInfoOpndRef = nullptr) {LOGMEIN("LowerMD.h] 181\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerStartCall(IR::Instr * instr) {LOGMEIN("LowerMD.h] 182\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerAsmJsCallI(IR::Instr * callInstr) {LOGMEIN("LowerMD.h] 183\n"); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerAsmJsCallE(IR::Instr * callInstr) {LOGMEIN("LowerMD.h] 184\n"); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerAsmJsLdElemHelper(IR::Instr * callInstr) {LOGMEIN("LowerMD.h] 185\n"); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerAsmJsStElemHelper(IR::Instr * callInstr) {LOGMEIN("LowerMD.h] 186\n"); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerCallIDynamic(IR::Instr *callInstr, IR::Instr*saveThisArgOutInstr, IR::Opnd *argsLength, ushort callFlags, IR::Instr * insertBeforeInstrForCFG = nullptr) {LOGMEIN("LowerMD.h] 187\n"); __debugbreak(); return 0; }
              IR::Instr *         LoadHelperArgument(IR::Instr * instr, IR::Opnd * opndArg) {LOGMEIN("LowerMD.h] 188\n"); __debugbreak(); return 0; }
              IR::Instr *         LoadDynamicArgument(IR::Instr * instr, uint argNumber = 1) {LOGMEIN("LowerMD.h] 189\n"); __debugbreak(); return 0; }
              IR::Instr *         LoadDynamicArgumentUsingLength(IR::Instr *instr) {LOGMEIN("LowerMD.h] 190\n"); __debugbreak(); return 0; }
              IR::Instr *         LoadDoubleHelperArgument(IR::Instr * instr, IR::Opnd * opndArg) {LOGMEIN("LowerMD.h] 191\n"); __debugbreak(); return 0; }
              IR::Instr *         LoadFloatHelperArgument(IR::Instr * instr, IR::Opnd * opndArg) {LOGMEIN("LowerMD.h] 192\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerToFloat(IR::Instr *instr) {LOGMEIN("LowerMD.h] 193\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerReinterpretPrimitive(IR::Instr* instr) {LOGMEIN("LowerMD.h] 194\n"); __debugbreak(); return 0; }
       static IR::BranchInstr *   LowerFloatCondBranch(IR::BranchInstr *instrBranch, bool ignoreNaN = false) {LOGMEIN("LowerMD.h] 195\n"); __debugbreak(); return 0; }
              void                ConvertFloatToInt32(IR::Opnd* intOpnd, IR::Opnd* floatOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelDone, IR::Instr * instInsert) {LOGMEIN("LowerMD.h] 196\n"); __debugbreak(); }
              void                CheckOverflowOnFloatToInt32(IR::Instr* instr, IR::Opnd* intOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelDone) {LOGMEIN("LowerMD.h] 197\n"); __debugbreak(); }
              void                EmitLoadVarNoCheck(IR::RegOpnd * dst, IR::RegOpnd * src, IR::Instr *instrLoad, bool isFromUint32, bool isHelper) {LOGMEIN("LowerMD.h] 198\n"); __debugbreak(); }
              void                EmitIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {LOGMEIN("LowerMD.h] 199\n"); __debugbreak(); }
              void                EmitUIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {LOGMEIN("LowerMD.h] 200\n"); __debugbreak(); }
              void                EmitFloatToInt(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {LOGMEIN("LowerMD.h] 201\n"); __debugbreak(); }
              void                EmitFloat32ToFloat64(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {LOGMEIN("LowerMD.h] 202\n"); __debugbreak(); }
              void                EmitInt64toFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {LOGMEIN("LowerMD.h] 203\n"); __debugbreak(); }
              void                EmitIntToLong(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {LOGMEIN("LowerMD.h] 204\n"); __debugbreak(); }
              void                EmitUIntToLong(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {LOGMEIN("LowerMD.h] 205\n"); __debugbreak(); }
              void                EmitLongToInt(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {LOGMEIN("LowerMD.h] 206\n"); __debugbreak(); }
              void                GenerateTruncWithCheck(IR::Instr * instr) {LOGMEIN("LowerMD.h] 207\n"); __debugbreak(); }
              static IR::Instr *  InsertConvertFloat64ToInt32(const RoundMode roundMode, IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr) {LOGMEIN("LowerMD.h] 208\n"); __debugbreak(); return 0; }
              void                EmitLoadFloatFromNumber(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr) {LOGMEIN("LowerMD.h] 209\n"); __debugbreak(); }
              IR::LabelInstr*     EmitLoadFloatCommon(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr, bool needHelperLabel) {LOGMEIN("LowerMD.h] 210\n"); __debugbreak(); return 0; }
              static IR::Instr *  LoadFloatZero(IR::Opnd * opndDst, IR::Instr * instrInsert) {LOGMEIN("LowerMD.h] 211\n"); __debugbreak(); return 0; }
              static IR::Instr *  LoadFloatValue(IR::Opnd * opndDst, double value, IR::Instr * instrInsert) {LOGMEIN("LowerMD.h] 212\n"); __debugbreak(); return 0; }

              IR::Instr *         LowerEntryInstr(IR::EntryInstr * entryInstr) {LOGMEIN("LowerMD.h] 214\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerExitInstr(IR::ExitInstr * exitInstr) {LOGMEIN("LowerMD.h] 215\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerEntryInstrAsmJs(IR::EntryInstr * entryInstr) {LOGMEIN("LowerMD.h] 216\n"); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerExitInstrAsmJs(IR::ExitInstr * exitInstr) {LOGMEIN("LowerMD.h] 217\n"); Assert(UNREACHED); return NULL; }
              IR::Instr *         LoadNewScObjFirstArg(IR::Instr * instr, IR::Opnd * dst, ushort extraArgs = 0) {LOGMEIN("LowerMD.h] 218\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerTry(IR::Instr *instr, IR::JnHelperMethod helperMethod) {LOGMEIN("LowerMD.h] 219\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerLeave(IR::Instr *instr, IR::LabelInstr * targetInstr, bool fromFinalLower, bool isOrphanedLeave = false) {LOGMEIN("LowerMD.h] 220\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerLeaveNull(IR::Instr *instr) {LOGMEIN("LowerMD.h] 221\n"); __debugbreak(); return 0; }
              IR::LabelInstr *    EnsureEpilogLabel() {LOGMEIN("LowerMD.h] 222\n"); __debugbreak(); return 0; }
              IR::Instr *         LowerEHRegionReturn(IR::Instr * retInstr, IR::Opnd * targetOpnd) {LOGMEIN("LowerMD.h] 223\n"); __debugbreak(); return 0; }
              void                FinishArgLowering() {LOGMEIN("LowerMD.h] 224\n"); __debugbreak(); }
              IR::Opnd *          GetOpndForArgSlot(Js::ArgSlot argSlot, bool isDoubleArgument = false) {LOGMEIN("LowerMD.h] 225\n"); __debugbreak(); return 0; }
              void                GenerateStackAllocation(IR::Instr *instr, uint32 allocSize, uint32 probeSize) {LOGMEIN("LowerMD.h] 226\n"); __debugbreak(); }
              void                GenerateStackDeallocation(IR::Instr *instr, uint32 allocSize) {LOGMEIN("LowerMD.h] 227\n"); __debugbreak(); }
              void                GenerateStackProbe(IR::Instr *instr, bool afterProlog) {LOGMEIN("LowerMD.h] 228\n"); __debugbreak(); }
              IR::Opnd*           GenerateArgOutForStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr) {LOGMEIN("LowerMD.h] 229\n"); __debugbreak(); return 0; }

              template <bool verify = false>
              static void         Legalize(IR::Instr *const instr, bool fPostRegAlloc = false) {LOGMEIN("LowerMD.h] 232\n"); __debugbreak(); }

              IR::Opnd*           IsOpndNegZero(IR::Opnd* opnd, IR::Instr* instr) {LOGMEIN("LowerMD.h] 234\n"); __debugbreak(); return 0; }
              void                GenerateFastInlineBuiltInMathAbs(IR::Instr *callInstr) {LOGMEIN("LowerMD.h] 235\n"); __debugbreak(); }
              void                GenerateFastInlineBuiltInMathFloor(IR::Instr *callInstr) {LOGMEIN("LowerMD.h] 236\n"); __debugbreak(); }
              void                GenerateFastInlineBuiltInMathCeil(IR::Instr *callInstr) {LOGMEIN("LowerMD.h] 237\n"); __debugbreak(); }
              void                GenerateFastInlineBuiltInMathRound(IR::Instr *callInstr) {LOGMEIN("LowerMD.h] 238\n"); __debugbreak(); }
              static RegNum       GetRegStackPointer() {LOGMEIN("LowerMD.h] 239\n"); return RegSP; }
              static RegNum       GetRegFramePointer() {LOGMEIN("LowerMD.h] 240\n"); return RegFP; }
              static RegNum       GetRegReturn(IRType type) {LOGMEIN("LowerMD.h] 241\n"); return IRType_IsFloat(type) ? RegNOREG : RegX0; }
              static Js::OpCode   GetLoadOp(IRType type) {LOGMEIN("LowerMD.h] 242\n"); __debugbreak(); return Js::OpCode::InvalidOpCode; }
              static Js::OpCode   GetStoreOp(IRType type) {LOGMEIN("LowerMD.h] 243\n"); __debugbreak(); return Js::OpCode::InvalidOpCode; }
              static Js::OpCode   GetMoveOp(IRType type) {LOGMEIN("LowerMD.h] 244\n"); __debugbreak(); return Js::OpCode::InvalidOpCode; }
              static RegNum       GetRegArgI4(int32 argNum) {LOGMEIN("LowerMD.h] 245\n"); return RegNOREG; }
              static RegNum       GetRegArgR8(int32 argNum) {LOGMEIN("LowerMD.h] 246\n"); return RegNOREG; }

              static BYTE         GetDefaultIndirScale() {LOGMEIN("LowerMD.h] 248\n"); return IndirScale4; }

              // -4 is to avoid alignment issues popping up, we are conservative here.
              // We might check for IsSmallStack first to push R4 register & then align.
              static bool         IsSmallStack(uint32 size)   {LOGMEIN("LowerMD.h] 252\n"); return (size < (PAGESIZE - 4)); }

              static void GenerateLoadTaggedType(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndTaggedType) {LOGMEIN("LowerMD.h] 254\n"); __debugbreak(); }
              static void GenerateLoadPolymorphicInlineCacheSlot(IR::Instr * instrLdSt, IR::RegOpnd * opndInlineCache, IR::RegOpnd * opndType, uint polymorphicInlineCacheSize) {LOGMEIN("LowerMD.h] 255\n"); __debugbreak(); }
              static IR::BranchInstr * GenerateLocalInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext, bool checkTypeWithoutProperty = false) {LOGMEIN("LowerMD.h] 256\n"); __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateProtoInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) {LOGMEIN("LowerMD.h] 257\n"); __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateFlagInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) {LOGMEIN("LowerMD.h] 258\n"); __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateFlagInlineCacheCheckForNoGetterSetter(IR::Instr * instrLdSt, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) {LOGMEIN("LowerMD.h] 259\n"); __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateFlagInlineCacheCheckForLocal(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) {LOGMEIN("LowerMD.h] 260\n"); __debugbreak(); return 0; }
              static void GenerateLdFldFromLocalInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) {LOGMEIN("LowerMD.h] 261\n"); __debugbreak(); }
              static void GenerateLdFldFromProtoInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) {LOGMEIN("LowerMD.h] 262\n"); __debugbreak(); }
              static void GenerateLdLocalFldFromFlagInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) {LOGMEIN("LowerMD.h] 263\n"); __debugbreak(); }
              static void GenerateStFldFromLocalInlineCache(IR::Instr * instrStFld, IR::RegOpnd * opndBase, IR::Opnd * opndSrc, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) {LOGMEIN("LowerMD.h] 264\n"); __debugbreak(); }

              static IR::Instr * ChangeToWriteBarrierAssign(IR::Instr * assignInstr, const Func* func) {LOGMEIN("LowerMD.h] 266\n"); __debugbreak(); }

              int                 GetHelperArgsCount() {LOGMEIN("LowerMD.h] 268\n"); __debugbreak(); return 0; }
              void                ResetHelperArgsCount() {LOGMEIN("LowerMD.h] 269\n"); __debugbreak(); }

              void                LowerInlineSpreadArgOutLoop(IR::Instr *callInstr, IR::RegOpnd *indexOpnd, IR::RegOpnd *arrayElementsStartOpnd) {LOGMEIN("LowerMD.h] 271\n"); __debugbreak(); }
              void                LowerTypeof(IR::Instr * typeOfInstr) {LOGMEIN("LowerMD.h] 272\n"); __debugbreak(); }
public:
    static IR::Instr * InsertCmovCC(const Js::OpCode opCode, IR::Opnd * dst, IR::Opnd* src1, IR::Instr* insertBeforeInstr, bool postRegAlloc) {LOGMEIN("LowerMD.h] 274\n"); __debugbreak(); }
};
