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
    {TRACE_IT(17836);\
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

    LowererMD(Func *func) {TRACE_IT(17837); }

    static  bool            IsUnconditionalBranch(const IR::Instr *instr) {TRACE_IT(17838); __debugbreak(); return 0; }
    static  bool            IsAssign(const IR::Instr *instr) {TRACE_IT(17839); __debugbreak(); return 0; }
    static  bool            IsCall(const IR::Instr *instr) {TRACE_IT(17840); __debugbreak(); return 0; }
    static  bool            IsIndirectBranch(const IR::Instr *instr) {TRACE_IT(17841); __debugbreak(); return 0; }
    static  bool            IsReturnInstr(const IR::Instr *instr) {TRACE_IT(17842); __debugbreak(); return 0; }
    static  void            InvertBranch(IR::BranchInstr *instr) {TRACE_IT(17843); __debugbreak(); }
    static Js::OpCode       MDBranchOpcode(Js::OpCode opcode) {TRACE_IT(17844); __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static Js::OpCode       MDUnsignedBranchOpcode(Js::OpCode opcode) {TRACE_IT(17845); __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static Js::OpCode       MDCompareWithZeroBranchOpcode(Js::OpCode opcode) {TRACE_IT(17846); __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static Js::OpCode       MDConvertFloat64ToInt32Opcode(const bool roundTowardZero) {TRACE_IT(17847); __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static void             ChangeToAdd(IR::Instr *const instr, const bool needFlags) {TRACE_IT(17848); __debugbreak(); }
    static void             ChangeToSub(IR::Instr *const instr, const bool needFlags) {TRACE_IT(17849); __debugbreak(); }
    static void             ChangeToShift(IR::Instr *const instr, const bool needFlags) {TRACE_IT(17850); __debugbreak(); }
    static const uint16     GetFormalParamOffset() {TRACE_IT(17851); __debugbreak(); return 0; }

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
            void            Init(Lowerer *lowerer) {TRACE_IT(17852); __debugbreak(); }
            void            FinalLower(){TRACE_IT(17853); __debugbreak(); }
            bool            FinalLowerAssign(IR::Instr* instr){TRACE_IT(17854); __debugbreak(); return 0;  };
            IR::Opnd *      GenerateMemRef(intptr_t addr, IRType type, IR::Instr *instr, bool dontEncode = false) {TRACE_IT(17855); __debugbreak(); return 0; }
            IR::Instr *     ChangeToHelperCall(IR::Instr * instr, IR::JnHelperMethod helperMethod, IR::LabelInstr *labelBailOut = NULL,
                            IR::Opnd *opndInstance = NULL, IR::PropertySymOpnd * propSymOpnd = nullptr, bool isHelperContinuation = false) {TRACE_IT(17856); __debugbreak(); return 0; }
            IR::Instr *     ChangeToHelperCallMem(IR::Instr * instr, IR::JnHelperMethod helperMethod) {TRACE_IT(17857); __debugbreak(); return 0; }
    static  IR::Instr *     CreateAssign(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsertPt) {TRACE_IT(17858); __debugbreak(); return 0; }
    static  IR::Instr *     ChangeToAssign(IR::Instr * instr) {TRACE_IT(17859); __debugbreak(); return 0; }
    static  IR::Instr *     ChangeToAssign(IR::Instr * instr, IRType type) {TRACE_IT(17860); __debugbreak(); return 0; }
    static  IR::Instr *     ChangeToLea(IR::Instr *const instr, bool postRegAlloc = false) {TRACE_IT(17861); __debugbreak(); return 0; }
    static  IR::Instr *     ForceDstToReg(IR::Instr *instr) {TRACE_IT(17862); __debugbreak(); return 0; }
    static  void            ImmedSrcToReg(IR::Instr * instr, IR::Opnd * newOpnd, int srcNum) {TRACE_IT(17863); __debugbreak(); }

            IR::Instr *     LoadArgumentCount(IR::Instr * instr) {TRACE_IT(17864); __debugbreak(); return 0; }
            IR::Instr *     LoadStackArgPtr(IR::Instr * instr) {TRACE_IT(17865); __debugbreak(); return 0; }
              IR::Instr *     LoadHeapArguments(IR::Instr * instrArgs) {TRACE_IT(17866); __debugbreak(); return 0; }
              IR::Instr *     LoadHeapArgsCached(IR::Instr * instr) {TRACE_IT(17867); __debugbreak(); return 0; }
              IR::Instr *     LoadInputParamCount(IR::Instr * instr, int adjust = 0, bool needFlags = false) {TRACE_IT(17868); __debugbreak(); return 0; }
              IR::Instr *     LoadArgumentsFromFrame(IR::Instr * instr) {TRACE_IT(17869); __debugbreak(); return 0; }
              IR::Instr *     LoadFuncExpression(IR::Instr * instr) {TRACE_IT(17870); __debugbreak(); return 0; }
              IR::Instr *     LowerRet(IR::Instr * instr) {TRACE_IT(17871); __debugbreak(); return 0; }
      static  IR::Instr *     LowerUncondBranch(IR::Instr * instr) {TRACE_IT(17872); __debugbreak(); return 0; }
      static  IR::Instr *     LowerMultiBranch(IR::Instr * instr) {TRACE_IT(17873); __debugbreak(); return 0; }
              IR::Instr *     LowerCondBranch(IR::Instr * instr) {TRACE_IT(17874); __debugbreak(); return 0; }
              IR::Instr *     LoadFunctionObjectOpnd(IR::Instr *instr, IR::Opnd *&functionObjOpnd) {TRACE_IT(17875); __debugbreak(); return 0; }
              IR::Instr *     LowerLdSuper(IR::Instr * instr, IR::JnHelperMethod helperOpCode) {TRACE_IT(17876); __debugbreak(); return 0; }
              IR::Instr *     GenerateSmIntPairTest(IR::Instr * instrInsert, IR::Opnd * opndSrc1, IR::Opnd * opndSrc2, IR::LabelInstr * labelFail) {TRACE_IT(17877); __debugbreak(); return 0; }
              void            GenerateTaggedZeroTest( IR::Opnd * opndSrc, IR::Instr * instrInsert, IR::LabelInstr * labelHelper = NULL) {TRACE_IT(17878); __debugbreak(); }
              void            GenerateObjectPairTest(IR::Opnd * opndSrc1, IR::Opnd * opndSrc2, IR::Instr * insertInstr, IR::LabelInstr * labelTarget) {TRACE_IT(17879); __debugbreak(); }
              bool            GenerateObjectTest(IR::Opnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel = false) {TRACE_IT(17880); __debugbreak(); return false; }
              bool            GenerateFastBrOrCmString(IR::Instr* instr) {TRACE_IT(17881); __debugbreak(); return 0; }
              bool            GenerateFastStringCheck(IR::Instr* instr, IR::RegOpnd *srcReg1, IR::RegOpnd *srcReg2, bool isEqual, bool isStrict, IR::LabelInstr *labelHelper, IR::LabelInstr *labelBranchSuccess, IR::LabelInstr *labelBranchFail) {TRACE_IT(17882); __debugbreak(); return 0; }
              bool            GenerateFastCmSrEqConst(IR::Instr *instr) {TRACE_IT(17883); __debugbreak(); return 0; }
              bool            GenerateFastCmXxI4(IR::Instr *instr) {TRACE_IT(17884); __debugbreak(); return 0; }
              bool            GenerateFastCmXxR8(IR::Instr *instr) {TRACE_IT(17885); Assert(UNREACHED); return NULL; }
              bool            GenerateFastCmXxTaggedInt(IR::Instr *instr) {TRACE_IT(17886); __debugbreak(); return 0; }
              IR::Instr *     GenerateConvBool(IR::Instr *instr) {TRACE_IT(17887); __debugbreak(); return 0; }

              void            GenerateClz(IR::Instr * instr) {TRACE_IT(17888); __debugbreak(); }
              void            GenerateCtz(IR::Instr * instr) {TRACE_IT(17889); __debugbreak(); }
              void            GeneratePopCnt(IR::Instr * instr) {TRACE_IT(17890); __debugbreak(); }
              void            GenerateFastDivByPow2(IR::Instr *instr) {TRACE_IT(17891); __debugbreak(); }
              bool            GenerateFastAdd(IR::Instr * instrAdd) {TRACE_IT(17892); __debugbreak(); return 0; }
              bool            GenerateFastSub(IR::Instr * instrSub) {TRACE_IT(17893); __debugbreak(); return 0; }
              bool            GenerateFastMul(IR::Instr * instrMul) {TRACE_IT(17894); __debugbreak(); return 0; }
              bool            GenerateFastAnd(IR::Instr * instrAnd) {TRACE_IT(17895); __debugbreak(); return 0; }
              bool            GenerateFastXor(IR::Instr * instrXor) {TRACE_IT(17896); __debugbreak(); return 0; }
              bool            GenerateFastOr(IR::Instr * instrOr) {TRACE_IT(17897); __debugbreak(); return 0; }
              bool            GenerateFastNot(IR::Instr * instrNot) {TRACE_IT(17898); __debugbreak(); return 0; }
              bool            GenerateFastNeg(IR::Instr * instrNeg) {TRACE_IT(17899); __debugbreak(); return 0; }
              bool            GenerateFastShiftLeft(IR::Instr * instrShift) {TRACE_IT(17900); __debugbreak(); return 0; }
              bool            GenerateFastShiftRight(IR::Instr * instrShift) {TRACE_IT(17901); __debugbreak(); return 0; }
              void            GenerateFastBrS(IR::BranchInstr *brInstr) {TRACE_IT(17902); __debugbreak(); }
              IR::IndirOpnd * GenerateFastElemIStringIndexCommon(IR::Instr * instr, bool isStore, IR::IndirOpnd *indirOpnd, IR::LabelInstr * labelHelper) {TRACE_IT(17903); __debugbreak(); return 0; }
              void            GenerateFastInlineBuiltInCall(IR::Instr* instr, IR::JnHelperMethod helperMethod) {TRACE_IT(17904); __debugbreak(); }
              void            HelperCallForAsmMathBuiltin(IR::Instr* instr, IR::JnHelperMethod helperMethodFloat, IR::JnHelperMethod helperMethodDouble) {TRACE_IT(17905); __debugbreak(); }
              IR::Opnd *      CreateStackArgumentsSlotOpnd() {TRACE_IT(17906); __debugbreak(); return 0; }
              void            GenerateSmIntTest(IR::Opnd *opndSrc, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::Instr **instrFirst = nullptr, bool fContinueLabel = false) {TRACE_IT(17907); __debugbreak(); }
              IR::RegOpnd *   LoadNonnegativeIndex(IR::RegOpnd *indexOpnd, const bool skipNegativeCheck, IR::LabelInstr *const notTaggedIntLabel, IR::LabelInstr *const negativeLabel, IR::Instr *const insertBeforeInstr) {TRACE_IT(17908); __debugbreak(); return nullptr; }
              IR::RegOpnd *   GenerateUntagVar(IR::RegOpnd * opnd, IR::LabelInstr * labelFail, IR::Instr * insertBeforeInstr, bool generateTagCheck = true) {TRACE_IT(17909); __debugbreak(); return 0; }
              bool            GenerateFastLdMethodFromFlags(IR::Instr * instrLdFld) {TRACE_IT(17910); __debugbreak(); return 0; }
              void            GenerateInt32ToVarConversion( IR::Opnd * opndSrc, IR::Instr * insertInstr ) {TRACE_IT(17911); __debugbreak(); }
              IR::Instr *     GenerateFastScopedFld(IR::Instr * instrScopedFld, bool isLoad) {TRACE_IT(17912); __debugbreak(); return 0; }
              IR::Instr *     GenerateFastScopedLdFld(IR::Instr * instrLdFld) {TRACE_IT(17913); __debugbreak(); return 0; }
              IR::Instr *     GenerateFastScopedStFld(IR::Instr * instrStFld) {TRACE_IT(17914); __debugbreak(); return 0; }
              bool            GenerateJSBooleanTest(IR::RegOpnd * regSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel = false) {TRACE_IT(17915); __debugbreak(); return 0; }
              bool            TryGenerateFastFloatOp(IR::Instr * instr, IR::Instr ** pInsertHelper, bool *pfNoLower) {TRACE_IT(17916); __debugbreak(); return 0; }
              bool            GenerateFastFloatCall(IR::Instr * instr, IR::Instr ** pInsertHelper, bool noFieldFastPath, bool *pfNoLower, IR::Instr **pInstrPrev) {TRACE_IT(17917); __debugbreak(); return 0; }
              bool            GenerateFastFloatBranch(IR::BranchInstr * instr, IR::Instr ** pInsertHelper, bool *pfNoLower) {TRACE_IT(17918); __debugbreak(); return 0; }
              void            GenerateFastAbs(IR::Opnd *dst, IR::Opnd *src, IR::Instr *callInstr, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel) {TRACE_IT(17919); __debugbreak(); }
              bool            GenerateFastCharAt(Js::BuiltinFunction index, IR::Opnd *dst, IR::Opnd *srcStr, IR::Opnd *srcIndex, IR::Instr *callInstr, IR::Instr *insertInstr,
                  IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel) {TRACE_IT(17920); __debugbreak(); return 0; }
              bool            TryGenerateFastMulAdd(IR::Instr * instrAdd, IR::Instr ** pInstrPrev) {TRACE_IT(17921); __debugbreak(); return 0; }
              void            GenerateIsDynamicObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool fContinueLabel = false) {TRACE_IT(17922); __debugbreak(); }
              void            GenerateIsRecyclableObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool checkObjectAndDynamicObject = true) {TRACE_IT(17923); __debugbreak(); }
              bool            GenerateLdThisCheck(IR::Instr * instr) {TRACE_IT(17924); __debugbreak(); return 0; }
              bool            GenerateLdThisStrict(IR::Instr* instr) {TRACE_IT(17925); __debugbreak(); return 0; }
              void            GenerateFloatTest(IR::RegOpnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr* labelHelper) {TRACE_IT(17926); __debugbreak(); }
              void            GenerateFunctionObjectTest(IR::Instr * callInstr, IR::RegOpnd  *functionObjOpnd, bool isHelper, IR::LabelInstr* afterCallLabel = nullptr) {TRACE_IT(17927); __debugbreak(); }

       static void            EmitInt4Instr(IR::Instr *instr) {TRACE_IT(17928); __debugbreak(); }
       static void            EmitPtrInstr(IR::Instr *instr) {TRACE_IT(17929); __debugbreak(); }
              void            EmitLoadVar(IR::Instr *instr, bool isFromUint32 = false, bool isHelper = false) {TRACE_IT(17930); __debugbreak(); }
              bool            EmitLoadInt32(IR::Instr *instr, bool conversionFromObjectAllowed, bool bailOutOnHelper = false, IR::LabelInstr * labelBailOut = nullptr) {TRACE_IT(17931); __debugbreak(); return 0; }
              IR::Instr *     LowerInt64Assign(IR::Instr * instr) {TRACE_IT(17932); __debugbreak(); return nullptr; }

       static void            LowerInt4NegWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {TRACE_IT(17933); __debugbreak(); }
       static void            LowerInt4AddWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {TRACE_IT(17934); __debugbreak(); }
       static void            LowerInt4SubWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {TRACE_IT(17935); __debugbreak(); }
       static void            LowerInt4MulWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {TRACE_IT(17936); __debugbreak(); }
       static void            LowerInt4RemWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) {TRACE_IT(17937); __debugbreak(); }
              void            MarkOneFltTmpSym(StackSym *sym, BVSparse<ArenaAllocator> *bvTmps, bool fFltPrefOp) {TRACE_IT(17938); __debugbreak(); }
              void            GenerateNumberAllocation(IR::RegOpnd * opndDst, IR::Instr * instrInsert, bool isHelper) {TRACE_IT(17939); __debugbreak(); }
              void            GenerateFastRecyclerAlloc(size_t allocSize, IR::RegOpnd* newObjDst, IR::Instr* insertionPointInstr, IR::LabelInstr* allocHelperLabel, IR::LabelInstr* allocDoneLabel) {TRACE_IT(17940); __debugbreak(); }
              void            SaveDoubleToVar(IR::RegOpnd * dstOpnd, IR::RegOpnd *opndFloat, IR::Instr *instrOrig, IR::Instr *instrInsert, bool isHelper = false) {TRACE_IT(17941); __debugbreak(); }
              IR::RegOpnd *   EmitLoadFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr) {TRACE_IT(17942); __debugbreak(); return 0; }
              IR::Instr *     LoadCheckedFloat(IR::RegOpnd *opndOrig, IR::RegOpnd *opndFloat, IR::LabelInstr *labelInline, IR::LabelInstr *labelHelper, IR::Instr *instrInsert) {TRACE_IT(17943); __debugbreak(); return 0; }

              void LoadFloatValue(IR::RegOpnd * javascriptNumber, IR::RegOpnd * opndFloat, IR::LabelInstr * labelHelper, IR::Instr * instrInsert) {TRACE_IT(17944); __debugbreak(); }

              IR::Instr *     LoadStackAddress(StackSym *sym, IR::RegOpnd* regDst = nullptr) {TRACE_IT(17945); __debugbreak(); return 0; }
              IR::Instr *     LowerCatch(IR::Instr *instr) {TRACE_IT(17946); __debugbreak(); return 0; }

              IR::Instr *     LowerGetCachedFunc(IR::Instr *instr) {TRACE_IT(17947); __debugbreak(); return 0; }
              IR::Instr *     LowerCommitScope(IR::Instr *instr) {TRACE_IT(17948); __debugbreak(); return 0; }

              IR::Instr *     LowerCallHelper(IR::Instr *instrCall) {TRACE_IT(17949); __debugbreak(); return 0; }

              IR::LabelInstr *GetBailOutStackRestoreLabel(BailOutInfo * bailOutInfo, IR::LabelInstr * exitTargetInstr) {TRACE_IT(17950); __debugbreak(); return 0; }
              bool            AnyFloatTmps(void) {TRACE_IT(17951); __debugbreak(); return 0; }
              IR::LabelInstr* InsertBeforeRecoveryForFloatTemps(IR::Instr * insertBefore, IR::LabelInstr * labelRecover) {TRACE_IT(17952); __debugbreak(); return 0; }
              StackSym *      GetImplicitParamSlotSym(Js::ArgSlot argSlot) {TRACE_IT(17953); __debugbreak(); return 0; }
       static StackSym *      GetImplicitParamSlotSym(Js::ArgSlot argSlot, Func * func) {TRACE_IT(17954); __debugbreak(); return 0; }
              bool            GenerateFastIsInst(IR::Instr * instr, Js::ScriptContext * scriptContext) {TRACE_IT(17955); __debugbreak(); return 0; }

              IR::Instr *     LowerDivI4AndBailOnReminder(IR::Instr * instr, IR::LabelInstr * bailOutLabel) {TRACE_IT(17956); __debugbreak(); return NULL; }
              bool            GenerateFastIsInst(IR::Instr * instr) {TRACE_IT(17957); __debugbreak(); return false; }
  public:
              IR::Instr *         LowerCall(IR::Instr * callInstr, Js::ArgSlot argCount) {TRACE_IT(17958); __debugbreak(); return 0; }
              IR::Instr *         LowerCallI(IR::Instr * callInstr, ushort callFlags, bool isHelper = false, IR::Instr* insertBeforeInstrForCFG = nullptr) {TRACE_IT(17959); __debugbreak(); return 0; }
              IR::Instr *         LowerCallPut(IR::Instr * callInstr) {TRACE_IT(17960); __debugbreak(); return 0; }
              int32               LowerCallArgs(IR::Instr * callInstr, IR::Instr * stackParamInsert, ushort callFlags) {TRACE_IT(17961); __debugbreak(); return 0; }
              int32               LowerCallArgs(IR::Instr * callInstr, ushort callFlags, Js::ArgSlot extraParams = 1 /* for function object */, IR::IntConstOpnd **callInfoOpndRef = nullptr) {TRACE_IT(17962); __debugbreak(); return 0; }
              IR::Instr *         LowerStartCall(IR::Instr * instr) {TRACE_IT(17963); __debugbreak(); return 0; }
              IR::Instr *         LowerAsmJsCallI(IR::Instr * callInstr) {TRACE_IT(17964); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerAsmJsCallE(IR::Instr * callInstr) {TRACE_IT(17965); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerAsmJsLdElemHelper(IR::Instr * callInstr) {TRACE_IT(17966); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerAsmJsStElemHelper(IR::Instr * callInstr) {TRACE_IT(17967); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerCallIDynamic(IR::Instr *callInstr, IR::Instr*saveThisArgOutInstr, IR::Opnd *argsLength, ushort callFlags, IR::Instr * insertBeforeInstrForCFG = nullptr) {TRACE_IT(17968); __debugbreak(); return 0; }
              IR::Instr *         LoadHelperArgument(IR::Instr * instr, IR::Opnd * opndArg) {TRACE_IT(17969); __debugbreak(); return 0; }
              IR::Instr *         LoadDynamicArgument(IR::Instr * instr, uint argNumber = 1) {TRACE_IT(17970); __debugbreak(); return 0; }
              IR::Instr *         LoadDynamicArgumentUsingLength(IR::Instr *instr) {TRACE_IT(17971); __debugbreak(); return 0; }
              IR::Instr *         LoadDoubleHelperArgument(IR::Instr * instr, IR::Opnd * opndArg) {TRACE_IT(17972); __debugbreak(); return 0; }
              IR::Instr *         LoadFloatHelperArgument(IR::Instr * instr, IR::Opnd * opndArg) {TRACE_IT(17973); __debugbreak(); return 0; }
              IR::Instr *         LowerToFloat(IR::Instr *instr) {TRACE_IT(17974); __debugbreak(); return 0; }
              IR::Instr *         LowerReinterpretPrimitive(IR::Instr* instr) {TRACE_IT(17975); __debugbreak(); return 0; }
       static IR::BranchInstr *   LowerFloatCondBranch(IR::BranchInstr *instrBranch, bool ignoreNaN = false) {TRACE_IT(17976); __debugbreak(); return 0; }
              void                ConvertFloatToInt32(IR::Opnd* intOpnd, IR::Opnd* floatOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelDone, IR::Instr * instInsert) {TRACE_IT(17977); __debugbreak(); }
              void                CheckOverflowOnFloatToInt32(IR::Instr* instr, IR::Opnd* intOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelDone) {TRACE_IT(17978); __debugbreak(); }
              void                EmitLoadVarNoCheck(IR::RegOpnd * dst, IR::RegOpnd * src, IR::Instr *instrLoad, bool isFromUint32, bool isHelper) {TRACE_IT(17979); __debugbreak(); }
              void                EmitIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {TRACE_IT(17980); __debugbreak(); }
              void                EmitUIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {TRACE_IT(17981); __debugbreak(); }
              void                EmitFloatToInt(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {TRACE_IT(17982); __debugbreak(); }
              void                EmitFloat32ToFloat64(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {TRACE_IT(17983); __debugbreak(); }
              void                EmitInt64toFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {TRACE_IT(17984); __debugbreak(); }
              void                EmitIntToLong(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {TRACE_IT(17985); __debugbreak(); }
              void                EmitUIntToLong(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {TRACE_IT(17986); __debugbreak(); }
              void                EmitLongToInt(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) {TRACE_IT(17987); __debugbreak(); }
              void                GenerateTruncWithCheck(IR::Instr * instr) {TRACE_IT(17988); __debugbreak(); }
              static IR::Instr *  InsertConvertFloat64ToInt32(const RoundMode roundMode, IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr) {TRACE_IT(17989); __debugbreak(); return 0; }
              void                EmitLoadFloatFromNumber(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr) {TRACE_IT(17990); __debugbreak(); }
              IR::LabelInstr*     EmitLoadFloatCommon(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr, bool needHelperLabel) {TRACE_IT(17991); __debugbreak(); return 0; }
              static IR::Instr *  LoadFloatZero(IR::Opnd * opndDst, IR::Instr * instrInsert) {TRACE_IT(17992); __debugbreak(); return 0; }
              static IR::Instr *  LoadFloatValue(IR::Opnd * opndDst, double value, IR::Instr * instrInsert) {TRACE_IT(17993); __debugbreak(); return 0; }

              IR::Instr *         LowerEntryInstr(IR::EntryInstr * entryInstr) {TRACE_IT(17994); __debugbreak(); return 0; }
              IR::Instr *         LowerExitInstr(IR::ExitInstr * exitInstr) {TRACE_IT(17995); __debugbreak(); return 0; }
              IR::Instr *         LowerEntryInstrAsmJs(IR::EntryInstr * entryInstr) {TRACE_IT(17996); Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerExitInstrAsmJs(IR::ExitInstr * exitInstr) {TRACE_IT(17997); Assert(UNREACHED); return NULL; }
              IR::Instr *         LoadNewScObjFirstArg(IR::Instr * instr, IR::Opnd * dst, ushort extraArgs = 0) {TRACE_IT(17998); __debugbreak(); return 0; }
              IR::Instr *         LowerTry(IR::Instr *instr, IR::JnHelperMethod helperMethod) {TRACE_IT(17999); __debugbreak(); return 0; }
              IR::Instr *         LowerLeave(IR::Instr *instr, IR::LabelInstr * targetInstr, bool fromFinalLower, bool isOrphanedLeave = false) {TRACE_IT(18000); __debugbreak(); return 0; }
              IR::Instr *         LowerLeaveNull(IR::Instr *instr) {TRACE_IT(18001); __debugbreak(); return 0; }
              IR::LabelInstr *    EnsureEpilogLabel() {TRACE_IT(18002); __debugbreak(); return 0; }
              IR::Instr *         LowerEHRegionReturn(IR::Instr * retInstr, IR::Opnd * targetOpnd) {TRACE_IT(18003); __debugbreak(); return 0; }
              void                FinishArgLowering() {TRACE_IT(18004); __debugbreak(); }
              IR::Opnd *          GetOpndForArgSlot(Js::ArgSlot argSlot, bool isDoubleArgument = false) {TRACE_IT(18005); __debugbreak(); return 0; }
              void                GenerateStackAllocation(IR::Instr *instr, uint32 allocSize, uint32 probeSize) {TRACE_IT(18006); __debugbreak(); }
              void                GenerateStackDeallocation(IR::Instr *instr, uint32 allocSize) {TRACE_IT(18007); __debugbreak(); }
              void                GenerateStackProbe(IR::Instr *instr, bool afterProlog) {TRACE_IT(18008); __debugbreak(); }
              IR::Opnd*           GenerateArgOutForStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr) {TRACE_IT(18009); __debugbreak(); return 0; }

              template <bool verify = false>
              static void         Legalize(IR::Instr *const instr, bool fPostRegAlloc = false) {TRACE_IT(18010); __debugbreak(); }

              IR::Opnd*           IsOpndNegZero(IR::Opnd* opnd, IR::Instr* instr) {TRACE_IT(18011); __debugbreak(); return 0; }
              void                GenerateFastInlineBuiltInMathAbs(IR::Instr *callInstr) {TRACE_IT(18012); __debugbreak(); }
              void                GenerateFastInlineBuiltInMathFloor(IR::Instr *callInstr) {TRACE_IT(18013); __debugbreak(); }
              void                GenerateFastInlineBuiltInMathCeil(IR::Instr *callInstr) {TRACE_IT(18014); __debugbreak(); }
              void                GenerateFastInlineBuiltInMathRound(IR::Instr *callInstr) {TRACE_IT(18015); __debugbreak(); }
              static RegNum       GetRegStackPointer() {TRACE_IT(18016); return RegSP; }
              static RegNum       GetRegFramePointer() {TRACE_IT(18017); return RegFP; }
              static RegNum       GetRegReturn(IRType type) {TRACE_IT(18018); return IRType_IsFloat(type) ? RegNOREG : RegX0; }
              static Js::OpCode   GetLoadOp(IRType type) {TRACE_IT(18019); __debugbreak(); return Js::OpCode::InvalidOpCode; }
              static Js::OpCode   GetStoreOp(IRType type) {TRACE_IT(18020); __debugbreak(); return Js::OpCode::InvalidOpCode; }
              static Js::OpCode   GetMoveOp(IRType type) {TRACE_IT(18021); __debugbreak(); return Js::OpCode::InvalidOpCode; }
              static RegNum       GetRegArgI4(int32 argNum) {TRACE_IT(18022); return RegNOREG; }
              static RegNum       GetRegArgR8(int32 argNum) {TRACE_IT(18023); return RegNOREG; }

              static BYTE         GetDefaultIndirScale() {TRACE_IT(18024); return IndirScale4; }

              // -4 is to avoid alignment issues popping up, we are conservative here.
              // We might check for IsSmallStack first to push R4 register & then align.
              static bool         IsSmallStack(uint32 size)   {TRACE_IT(18025); return (size < (PAGESIZE - 4)); }

              static void GenerateLoadTaggedType(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndTaggedType) {TRACE_IT(18026); __debugbreak(); }
              static void GenerateLoadPolymorphicInlineCacheSlot(IR::Instr * instrLdSt, IR::RegOpnd * opndInlineCache, IR::RegOpnd * opndType, uint polymorphicInlineCacheSize) {TRACE_IT(18027); __debugbreak(); }
              static IR::BranchInstr * GenerateLocalInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext, bool checkTypeWithoutProperty = false) {TRACE_IT(18028); __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateProtoInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) {TRACE_IT(18029); __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateFlagInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) {TRACE_IT(18030); __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateFlagInlineCacheCheckForNoGetterSetter(IR::Instr * instrLdSt, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) {TRACE_IT(18031); __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateFlagInlineCacheCheckForLocal(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) {TRACE_IT(18032); __debugbreak(); return 0; }
              static void GenerateLdFldFromLocalInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) {TRACE_IT(18033); __debugbreak(); }
              static void GenerateLdFldFromProtoInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) {TRACE_IT(18034); __debugbreak(); }
              static void GenerateLdLocalFldFromFlagInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) {TRACE_IT(18035); __debugbreak(); }
              static void GenerateStFldFromLocalInlineCache(IR::Instr * instrStFld, IR::RegOpnd * opndBase, IR::Opnd * opndSrc, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) {TRACE_IT(18036); __debugbreak(); }

              static IR::Instr * ChangeToWriteBarrierAssign(IR::Instr * assignInstr, const Func* func) {TRACE_IT(18037); __debugbreak(); }

              int                 GetHelperArgsCount() {TRACE_IT(18038); __debugbreak(); return 0; }
              void                ResetHelperArgsCount() {TRACE_IT(18039); __debugbreak(); }

              void                LowerInlineSpreadArgOutLoop(IR::Instr *callInstr, IR::RegOpnd *indexOpnd, IR::RegOpnd *arrayElementsStartOpnd) {TRACE_IT(18040); __debugbreak(); }
              void                LowerTypeof(IR::Instr * typeOfInstr) {TRACE_IT(18041); __debugbreak(); }
public:
    static IR::Instr * InsertCmovCC(const Js::OpCode opCode, IR::Opnd * dst, IR::Opnd* src1, IR::Instr* insertBeforeInstr, bool postRegAlloc) {TRACE_IT(18042); __debugbreak(); }
};
