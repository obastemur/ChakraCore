//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// SIMD_JS
// GlobOpt bits related to SIMD.js

#include "Backend.h"

#ifdef ENABLE_SIMDJS

/*
Handles all Simd128 type-spec of an instr, if possible.
*/
bool
GlobOpt::TypeSpecializeSimd128(
IR::Instr *instr,
Value **pSrc1Val,
Value **pSrc2Val,
Value **pDstVal
)
{LOGMEIN("GlobOptSimd128.cpp] 21\n");
    if (func->GetJITFunctionBody()->IsAsmJsMode() || SIMD128_TYPE_SPEC_FLAG == false)
    {LOGMEIN("GlobOptSimd128.cpp] 23\n");
        // no type-spec for ASMJS code or flag is off.
        return false;
    }

    switch (instr->m_opcode)
    {LOGMEIN("GlobOptSimd128.cpp] 29\n");
    case Js::OpCode::ArgOut_A_InlineBuiltIn:
        if (instr->GetSrc1()->IsRegOpnd())
        {LOGMEIN("GlobOptSimd128.cpp] 32\n");
            StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
            if (IsSimd128TypeSpecialized(sym, this->currentBlock))
            {LOGMEIN("GlobOptSimd128.cpp] 35\n");
                ValueType valueType = (*pSrc1Val)->GetValueInfo()->Type();
                Assert(valueType.IsSimd128());
                ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, GetIRTypeFromValueType(valueType), GetBailOutKindFromValueType(valueType));

                return true;
            }
        }
        return false;

    case Js::OpCode::Ld_A:
        if (instr->GetSrc1()->IsRegOpnd())
        {LOGMEIN("GlobOptSimd128.cpp] 47\n");
            StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
            IRType type = TyIllegal;
            if (IsSimd128F4TypeSpecialized(sym, this->currentBlock))
            {LOGMEIN("GlobOptSimd128.cpp] 51\n");
                type = TySimd128F4;
            }
            else if (IsSimd128I4TypeSpecialized(sym, this->currentBlock))
            {LOGMEIN("GlobOptSimd128.cpp] 55\n");
                type = TySimd128I4;
            }
            else
            {
                return false;
            }
            ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, type, IR::BailOutSimd128F4Only /*not used for Ld_A*/);
            TypeSpecializeSimd128Dst(type, instr, *pSrc1Val, *pSrc1Val, pDstVal);
            return true;
        }

        return false;

    case Js::OpCode::ExtendArg_A:

        if (Simd128DoTypeSpec(instr, *pSrc1Val, *pSrc2Val, *pDstVal))
        {LOGMEIN("GlobOptSimd128.cpp] 72\n");
            Assert(instr->m_opcode == Js::OpCode::ExtendArg_A);
            Assert(instr->GetDst()->GetType() == TyVar);
            ValueType valueType = instr->GetDst()->GetValueType();

            // Type-spec src1 only based on dst type. Dst type is set by the inliner based on func signature.
            ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, GetIRTypeFromValueType(valueType), GetBailOutKindFromValueType(valueType), true /*lossy*/);
            ToVarRegOpnd(instr->GetDst()->AsRegOpnd(), this->currentBlock);
            return true;
        }
        return false;
    }

    if (!Js::IsSimd128Opcode(instr->m_opcode))
    {LOGMEIN("GlobOptSimd128.cpp] 86\n");
        return false;
    }

    // Simd instr
    if (Simd128DoTypeSpec(instr, *pSrc1Val, *pSrc2Val, *pDstVal))
    {LOGMEIN("GlobOptSimd128.cpp] 92\n");
        ThreadContext::SimdFuncSignature simdFuncSignature;
        instr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(instr->m_opcode, simdFuncSignature);
        // type-spec logic

        // special handling for load/store
        // OptArraySrc will type-spec the array and the index. We type-spec the value here.
        if (Js::IsSimd128Load(instr->m_opcode))
        {LOGMEIN("GlobOptSimd128.cpp] 100\n");
            TypeSpecializeSimd128Dst(GetIRTypeFromValueType(simdFuncSignature.returnType), instr, nullptr, *pSrc1Val, pDstVal);
            Simd128SetIndirOpndType(instr->GetSrc1()->AsIndirOpnd(), instr->m_opcode);
            return true;
        }
        if (Js::IsSimd128Store(instr->m_opcode))
        {
            ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, GetIRTypeFromValueType(simdFuncSignature.args[2]), GetBailOutKindFromValueType(simdFuncSignature.args[2]));
            Simd128SetIndirOpndType(instr->GetDst()->AsIndirOpnd(), instr->m_opcode);
            return true;
        }

        // For op with ExtendArg. All sources are already type-specialized, just type-specialize dst
        if (simdFuncSignature.argCount <= 2)
        {LOGMEIN("GlobOptSimd128.cpp] 114\n");
            Assert(instr->GetSrc1());
            ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, GetIRTypeFromValueType(simdFuncSignature.args[0]), GetBailOutKindFromValueType(simdFuncSignature.args[0]));

            if (instr->GetSrc2())
            {
                ToTypeSpecUse(instr, instr->GetSrc2(), this->currentBlock, *pSrc2Val, nullptr, GetIRTypeFromValueType(simdFuncSignature.args[1]), GetBailOutKindFromValueType(simdFuncSignature.args[1]));
            }
        }
        if (instr->GetDst())
        {LOGMEIN("GlobOptSimd128.cpp] 124\n");
            TypeSpecializeSimd128Dst(GetIRTypeFromValueType(simdFuncSignature.returnType), instr, nullptr, *pSrc1Val, pDstVal);
        }
        return true;
    }
    else
    {
        // We didn't type-spec
        if (!IsLoopPrePass())
        {LOGMEIN("GlobOptSimd128.cpp] 133\n");
            // Emit bailout if not loop prepass.
            // The inliner inserts bytecodeUses of original args after the instruction. Bailout is safe.
            IR::Instr * bailoutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNoSimdTypeSpec, IR::BailOutNoSimdTypeSpec, instr, instr->m_func);
            bailoutInstr->SetByteCodeOffset(instr);
            instr->InsertAfter(bailoutInstr);

            instr->m_opcode = Js::OpCode::Nop;
            if (instr->GetSrc1())
            {LOGMEIN("GlobOptSimd128.cpp] 142\n");
                instr->FreeSrc1();
                if (instr->GetSrc2())
                {LOGMEIN("GlobOptSimd128.cpp] 145\n");
                    instr->FreeSrc2();
                }
            }
            if (instr->GetDst())
            {LOGMEIN("GlobOptSimd128.cpp] 150\n");
                instr->FreeDst();
            }

            if (this->byteCodeUses)
            {LOGMEIN("GlobOptSimd128.cpp] 155\n");
                // All inlined SIMD ops have jitOptimizedReg srcs
                Assert(this->byteCodeUses->IsEmpty());
                JitAdelete(this->alloc, this->byteCodeUses);
                this->byteCodeUses = nullptr;
            }
            RemoveCodeAfterNoFallthroughInstr(bailoutInstr);
            return true;
        }
    }
    return false;
}

bool
GlobOpt::Simd128DoTypeSpec(IR::Instr *instr, const Value *src1Val, const Value *src2Val, const Value *dstVal)
{LOGMEIN("GlobOptSimd128.cpp] 170\n");
    bool doTypeSpec = true;

    // TODO: Some operations require additional opnd constraints (e.g. shuffle/swizzle).
    if (Js::IsSimd128Opcode(instr->m_opcode))
    {LOGMEIN("GlobOptSimd128.cpp] 175\n");
        ThreadContext::SimdFuncSignature simdFuncSignature;
        instr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(instr->m_opcode, simdFuncSignature);
        if (!simdFuncSignature.valid)
        {LOGMEIN("GlobOptSimd128.cpp] 179\n");
            // not implemented yet.
            return false;
        }
        // special handling for Load/Store
        if (Js::IsSimd128Load(instr->m_opcode) || Js::IsSimd128Store(instr->m_opcode))
        {LOGMEIN("GlobOptSimd128.cpp] 185\n");
            return Simd128DoTypeSpecLoadStore(instr, src1Val, src2Val, dstVal, &simdFuncSignature);
        }

        const uint argCount = simdFuncSignature.argCount;
        switch (argCount)
        {LOGMEIN("GlobOptSimd128.cpp] 191\n");
        case 2:
            Assert(src2Val);
            doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(src2Val->GetValueInfo()->Type(), simdFuncSignature.args[1]) && Simd128ValidateIfLaneIndex(instr, instr->GetSrc2(), 1);
            // fall-through
        case 1:
            Assert(src1Val);
            doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(src1Val->GetValueInfo()->Type(), simdFuncSignature.args[0]) && Simd128ValidateIfLaneIndex(instr, instr->GetSrc1(), 0);
            break;
        default:
        {LOGMEIN("GlobOptSimd128.cpp] 201\n");
            // extended args
            Assert(argCount > 2);
            // Check if all args have been type specialized.

            int arg = argCount - 1;
            IR::Instr * eaInstr = GetExtendedArg(instr);

            while (arg>=0)
            {LOGMEIN("GlobOptSimd128.cpp] 210\n");
                Assert(eaInstr);
                Assert(eaInstr->m_opcode == Js::OpCode::ExtendArg_A);

                ValueType expectedType = simdFuncSignature.args[arg];
                IR::Opnd * opnd = eaInstr->GetSrc1();
                StackSym * sym = opnd->GetStackSym();

                // In Forward Prepass: Check liveness through liveness bits, not IR type, since in prepass no actual type-spec happens.
                // In the Forward Pass: Check IRType since Sym can be null, because of const prop.
                if (expectedType.IsSimd128Float32x4())
                {LOGMEIN("GlobOptSimd128.cpp] 221\n");
                    if (sym && !IsSimd128F4TypeSpecialized(sym, &currentBlock->globOptData) ||
                        !sym && opnd->GetType() != TySimd128F4)
                    {LOGMEIN("GlobOptSimd128.cpp] 224\n");
                        return false;
                    }
                }
                else if (expectedType.IsSimd128Int32x4())
                {LOGMEIN("GlobOptSimd128.cpp] 229\n");
                    if (sym && !IsSimd128I4TypeSpecialized(sym, &currentBlock->globOptData) ||
                        !sym && opnd->GetType() != TySimd128I4)
                    {LOGMEIN("GlobOptSimd128.cpp] 232\n");
                        return false;
                    }
                }
                else if (expectedType.IsFloat())
                {LOGMEIN("GlobOptSimd128.cpp] 237\n");
                    if (sym && !IsFloat64TypeSpecialized(sym, &currentBlock->globOptData) ||
                        !sym&& opnd->GetType() != TyFloat64)
                    {LOGMEIN("GlobOptSimd128.cpp] 240\n");
                        return false;
                    }

                }
                else if (expectedType.IsInt())
                {LOGMEIN("GlobOptSimd128.cpp] 246\n");
                    if ((sym && !IsInt32TypeSpecialized(sym, &currentBlock->globOptData) && !currentBlock->globOptData.liveLossyInt32Syms->Test(sym->m_id)) ||
                        !sym && opnd->GetType() != TyInt32)
                    {LOGMEIN("GlobOptSimd128.cpp] 249\n");
                        return false;
                    }
                    // Extra check if arg is a lane index
                    if (!Simd128ValidateIfLaneIndex(instr, opnd, arg))
                    {LOGMEIN("GlobOptSimd128.cpp] 254\n");
                        return false;
                    }
                }
                else
                {
                    Assert(UNREACHED);
                }

                eaInstr = GetExtendedArg(eaInstr);
                arg--;
            }
            // all args are type-spec'd
            doTypeSpec = true;
        }
        }
    }
    else
    {
        Assert(instr->m_opcode == Js::OpCode::ExtendArg_A);
        // For ExtendArg, the expected type is encoded in the dst(link) operand.
        doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(src1Val->GetValueInfo()->Type(), instr->GetDst()->GetValueType());
    }

    return doTypeSpec;
}

bool
GlobOpt::Simd128DoTypeSpecLoadStore(IR::Instr *instr, const Value *src1Val, const Value *src2Val, const Value *dstVal, const ThreadContext::SimdFuncSignature *simdFuncSignature)
{LOGMEIN("GlobOptSimd128.cpp] 283\n");
    IR::Opnd *baseOpnd = nullptr, *indexOpnd = nullptr, *valueOpnd = nullptr;
    IR::Opnd *src, *dst;

    bool doTypeSpec = true;

    // value = Ld [arr + index]
    // [arr + index] = St value
    src = instr->GetSrc1();
    dst = instr->GetDst();
    Assert(dst && src && !instr->GetSrc2());

    if (Js::IsSimd128Load(instr->m_opcode))
    {LOGMEIN("GlobOptSimd128.cpp] 296\n");
        Assert(src->IsIndirOpnd());
        baseOpnd = instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd();
        indexOpnd = instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd();
        valueOpnd = instr->GetDst();
    }
    else if (Js::IsSimd128Store(instr->m_opcode))
    {LOGMEIN("GlobOptSimd128.cpp] 303\n");
        Assert(dst->IsIndirOpnd());
        baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
        indexOpnd = instr->GetDst()->AsIndirOpnd()->GetIndexOpnd();
        valueOpnd = instr->GetSrc1();

        // St(arr, index, value). Make sure value can be Simd128 type-spec'd
        doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(FindValue(valueOpnd->AsRegOpnd()->m_sym)->GetValueInfo()->Type(), simdFuncSignature->args[2]);
    }
    else
    {
        Assert(UNREACHED);
    }

    // array and index operands should have been type-specialized in OptArraySrc: ValueTypes should be definite at this point. If not, don't type-spec.
    // We can be in a loop prepass, where opnd ValueInfo is not set yet. Get the ValueInfo from the Value Table instead.
    ValueType baseOpndType = FindValue(baseOpnd->AsRegOpnd()->m_sym)->GetValueInfo()->Type();
    
    if (IsLoopPrePass())
    {LOGMEIN("GlobOptSimd128.cpp] 322\n");
        doTypeSpec = doTypeSpec && (baseOpndType.IsObject() && baseOpndType.IsTypedArray());
        // indexOpnd might be missing if loading from [0]
        if (indexOpnd != nullptr)
        {LOGMEIN("GlobOptSimd128.cpp] 326\n");
            ValueType indexOpndType = FindValue(indexOpnd->AsRegOpnd()->m_sym)->GetValueInfo()->Type();
            doTypeSpec = doTypeSpec && indexOpndType.IsLikelyInt();
        }
    }
    else
    {
        doTypeSpec = doTypeSpec && (baseOpndType.IsObject() && baseOpndType.IsTypedArray());
        if (indexOpnd != nullptr)
        {LOGMEIN("GlobOptSimd128.cpp] 335\n");
            ValueType indexOpndType = FindValue(indexOpnd->AsRegOpnd()->m_sym)->GetValueInfo()->Type();
            doTypeSpec = doTypeSpec && indexOpndType.IsInt();
        }
    }

    return doTypeSpec;
}


// We can type spec an opnd if:
// Both profiled/propagated and expected types are not Simd128. e.g. expected type is f64/f32/i32 where there is a conversion logic from the incoming type.
// Opnd type is (Likely) SIMD128 and matches expected type.
// Opnd type is Object. e.g. possibly result of merging different SIMD types. We specialize because we don't know which pass is dynamically taken.

bool GlobOpt::Simd128CanTypeSpecOpnd(const ValueType opndType, ValueType expectedType)
{LOGMEIN("GlobOptSimd128.cpp] 351\n");
    if (!opndType.IsSimd128() && !expectedType.IsSimd128())
    {LOGMEIN("GlobOptSimd128.cpp] 353\n");
        // Non-Simd types can be coerced or we bailout by a FromVar.
        return true;
    }

    // Simd type
    if (opndType.HasBeenNull() || opndType.HasBeenUndefined())
    {LOGMEIN("GlobOptSimd128.cpp] 360\n");
        return false;
    }

    if (
            (opndType.IsLikelyObject() && opndType.ToDefiniteObject() == expectedType) ||
            (opndType.IsLikelyObject() && opndType.GetObjectType() == ObjectType::Object)
       )
    {LOGMEIN("GlobOptSimd128.cpp] 368\n");
        return true;
    }
    return false;
}

/*
Given an instr, opnd and the opnd position. Return true if opnd is a lane index and valid, or not a lane index all-together..
*/
bool GlobOpt::Simd128ValidateIfLaneIndex(const IR::Instr * instr, IR::Opnd * opnd, uint argPos)
{LOGMEIN("GlobOptSimd128.cpp] 378\n");
    Assert(instr);
    Assert(opnd);

    uint laneIndex;
    uint argPosLo, argPosHi;
    uint laneIndexLo, laneIndexHi;

    // operation takes a lane index ?
    switch (instr->m_opcode)
    {LOGMEIN("GlobOptSimd128.cpp] 388\n");
    case Js::OpCode::Simd128_Swizzle_F4:
    case Js::OpCode::Simd128_Swizzle_I4:
        argPosLo = 1; argPosHi = 4;
        laneIndexLo = 0; laneIndexHi = 3;
        break;
    case Js::OpCode::Simd128_Shuffle_F4:
    case Js::OpCode::Simd128_Shuffle_I4:
        argPosLo = 2; argPosHi = 5;
        laneIndexLo = 0; laneIndexHi = 7;
        break;
    case Js::OpCode::Simd128_ReplaceLane_F4:
    case Js::OpCode::Simd128_ReplaceLane_I4:
    case Js::OpCode::Simd128_ExtractLane_F4:
    case Js::OpCode::Simd128_ExtractLane_I4:
        argPosLo = argPosHi = 1;
        laneIndexLo = 0;  laneIndexHi = 3;
        break;
    default:
        return true; // not a lane index
    }

    // arg in lane index pos of operation ?
    if (argPos < argPosLo || argPos > argPosHi)
    {LOGMEIN("GlobOptSimd128.cpp] 412\n");
        return true; // not a lane index
    }

    // It is a lane index ...

    // Arg is Int constant (literal or const prop'd) ?
    if (!opnd->IsIntConstOpnd())
    {LOGMEIN("GlobOptSimd128.cpp] 420\n");
        return false;
    }
    laneIndex = (uint) opnd->AsIntConstOpnd()->GetValue();

    // In range ?
    if (laneIndex < laneIndexLo|| laneIndex > laneIndexHi)
    {LOGMEIN("GlobOptSimd128.cpp] 427\n");
        return false;
    }

    return true;
}

IR::Instr * GlobOpt::GetExtendedArg(IR::Instr *instr)
{LOGMEIN("GlobOptSimd128.cpp] 435\n");
    IR::Opnd *src1, *src2;

    src1 = instr->GetSrc1();
    src2 = instr->GetSrc2();

    if (instr->m_opcode == Js::OpCode::ExtendArg_A)
    {LOGMEIN("GlobOptSimd128.cpp] 442\n");
        if (src2)
        {LOGMEIN("GlobOptSimd128.cpp] 444\n");
            // mid chain
            Assert(src2->GetStackSym()->IsSingleDef());
            return src2->GetStackSym()->GetInstrDef();
        }
        else
        {
            // end of chain
            return nullptr;
        }
    }
    else
    {
        // start of chain
        Assert(Js::IsSimd128Opcode(instr->m_opcode));
        Assert(src1);
        Assert(src1->GetStackSym()->IsSingleDef());
        return src1->GetStackSym()->GetInstrDef();
    }
}

#endif

IRType GlobOpt::GetIRTypeFromValueType(const ValueType &valueType)
{LOGMEIN("GlobOptSimd128.cpp] 468\n");
    if (valueType.IsFloat())
    {LOGMEIN("GlobOptSimd128.cpp] 470\n");
        return TyFloat64;
    }
    else if (valueType.IsInt())
    {LOGMEIN("GlobOptSimd128.cpp] 474\n");
        return TyInt32;
    }
    else if (valueType.IsSimd128Float32x4())
    {LOGMEIN("GlobOptSimd128.cpp] 478\n");
        return TySimd128F4;
    }
    else
    {
        Assert(valueType.IsSimd128Int32x4());
        return TySimd128I4;
    }
}

ValueType GlobOpt::GetValueTypeFromIRType(const IRType &type)
{LOGMEIN("GlobOptSimd128.cpp] 489\n");
    switch (type)
    {LOGMEIN("GlobOptSimd128.cpp] 491\n");
    case TyInt32:
        return ValueType::GetInt(false);
    case TyFloat64:
        return ValueType::Float;
    case TySimd128F4:
        return ValueType::GetSimd128(ObjectType::Simd128Float32x4);
    case TySimd128I4:
        return ValueType::GetSimd128(ObjectType::Simd128Int32x4);
    default:
        Assert(UNREACHED);

    }
    return ValueType::UninitializedObject;

}

IR::BailOutKind GlobOpt::GetBailOutKindFromValueType(const ValueType &valueType)
{LOGMEIN("GlobOptSimd128.cpp] 509\n");
    if (valueType.IsFloat())
    {LOGMEIN("GlobOptSimd128.cpp] 511\n");
        // if required valueType is Float, then allow coercion from any primitive except String.
        return IR::BailOutPrimitiveButString;
    }
    else if (valueType.IsInt())
    {LOGMEIN("GlobOptSimd128.cpp] 516\n");
        return IR::BailOutIntOnly;
    }
    else if (valueType.IsSimd128Float32x4())
    {LOGMEIN("GlobOptSimd128.cpp] 520\n");
        return IR::BailOutSimd128F4Only;
    }
    else
    {
        Assert(valueType.IsSimd128Int32x4());
        return IR::BailOutSimd128I4Only;
    }
}

#ifdef ENABLE_SIMDJS
void
GlobOpt::UpdateBoundCheckHoistInfoForSimd(ArrayUpperBoundCheckHoistInfo &upperHoistInfo, ValueType arrValueType, const IR::Instr *instr)
{LOGMEIN("GlobOptSimd128.cpp] 533\n");
    if (!upperHoistInfo.HasAnyInfo())
    {LOGMEIN("GlobOptSimd128.cpp] 535\n");
        return;
    }

    int newOffset = GetBoundCheckOffsetForSimd(arrValueType, instr, upperHoistInfo.Offset());
    upperHoistInfo.UpdateOffset(newOffset);
}
#endif
int
GlobOpt::GetBoundCheckOffsetForSimd(ValueType arrValueType, const IR::Instr *instr, const int oldOffset /* = -1 */)
{LOGMEIN("GlobOptSimd128.cpp] 545\n");
#ifdef ENABLE_SIMDJS
    if (!(Js::IsSimd128LoadStore(instr->m_opcode)))
    {LOGMEIN("GlobOptSimd128.cpp] 548\n");
        return oldOffset;
    }

    if (!arrValueType.IsTypedArray())
    {LOGMEIN("GlobOptSimd128.cpp] 553\n");
        // no need to adjust for other array types, we will not type-spec (see Simd128DoTypeSpecLoadStore)
        return oldOffset;
    }

    Assert(instr->dataWidth == 4 || instr->dataWidth == 8 || instr->dataWidth == 12 || instr->dataWidth == 16);

    int numOfElems = Lowerer::SimdGetElementCountFromBytes(arrValueType, instr->dataWidth);

    // we want to make bound checks more conservative. We compute how many extra elements we need to add to the bound check
    // e.g. if original bound check is value <= Length + offset, and dataWidth is 16 bytes on Float32 array, then we need room for 4 elements. The bound check guarantees room for 1 element.
    // Hence, we need to ensure 3 more: value <= Length + offset - 3
    // We round up since dataWidth may span a partial lane (e.g. dataWidth = 12, bpe = 8 bytes)

    int offsetBias = -(numOfElems - 1);
    // we should always make an existing bound-check more conservative.
    Assert(offsetBias <= 0);
    return oldOffset + offsetBias;
#else
    return oldOffset;
#endif
}

#ifdef ENABLE_SIMDJS
void
GlobOpt::Simd128SetIndirOpndType(IR::IndirOpnd *indirOpnd, Js::OpCode opcode)
{LOGMEIN("GlobOptSimd128.cpp] 579\n");
    switch (opcode)
    {LOGMEIN("GlobOptSimd128.cpp] 581\n");
    case Js::OpCode::Simd128_LdArr_F4:
    case Js::OpCode::Simd128_StArr_F4:
        indirOpnd->SetType(TySimd128F4);
        indirOpnd->SetValueType(ValueType::GetSimd128(ObjectType::Simd128Float32x4));
        break;
    case Js::OpCode::Simd128_LdArr_I4:
    case Js::OpCode::Simd128_StArr_I4:
        indirOpnd->SetType(TySimd128I4);
        indirOpnd->SetValueType(ValueType::GetSimd128(ObjectType::Simd128Int32x4));
        break;
    default:
        Assert(UNREACHED);
    }

}
#endif
