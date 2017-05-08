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
{TRACE_IT(6775);
    if (func->GetJITFunctionBody()->IsAsmJsMode() || SIMD128_TYPE_SPEC_FLAG == false)
    {TRACE_IT(6776);
        // no type-spec for ASMJS code or flag is off.
        return false;
    }

    switch (instr->m_opcode)
    {
    case Js::OpCode::ArgOut_A_InlineBuiltIn:
        if (instr->GetSrc1()->IsRegOpnd())
        {TRACE_IT(6777);
            StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
            if (IsSimd128TypeSpecialized(sym, this->currentBlock))
            {TRACE_IT(6778);
                ValueType valueType = (*pSrc1Val)->GetValueInfo()->Type();
                Assert(valueType.IsSimd128());
                ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, GetIRTypeFromValueType(valueType), GetBailOutKindFromValueType(valueType));

                return true;
            }
        }
        return false;

    case Js::OpCode::Ld_A:
        if (instr->GetSrc1()->IsRegOpnd())
        {TRACE_IT(6779);
            StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
            IRType type = TyIllegal;
            if (IsSimd128F4TypeSpecialized(sym, this->currentBlock))
            {TRACE_IT(6780);
                type = TySimd128F4;
            }
            else if (IsSimd128I4TypeSpecialized(sym, this->currentBlock))
            {TRACE_IT(6781);
                type = TySimd128I4;
            }
            else
            {TRACE_IT(6782);
                return false;
            }
            ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, type, IR::BailOutSimd128F4Only /*not used for Ld_A*/);
            TypeSpecializeSimd128Dst(type, instr, *pSrc1Val, *pSrc1Val, pDstVal);
            return true;
        }

        return false;

    case Js::OpCode::ExtendArg_A:

        if (Simd128DoTypeSpec(instr, *pSrc1Val, *pSrc2Val, *pDstVal))
        {TRACE_IT(6783);
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
    {TRACE_IT(6784);
        return false;
    }

    // Simd instr
    if (Simd128DoTypeSpec(instr, *pSrc1Val, *pSrc2Val, *pDstVal))
    {TRACE_IT(6785);
        ThreadContext::SimdFuncSignature simdFuncSignature;
        instr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(instr->m_opcode, simdFuncSignature);
        // type-spec logic

        // special handling for load/store
        // OptArraySrc will type-spec the array and the index. We type-spec the value here.
        if (Js::IsSimd128Load(instr->m_opcode))
        {TRACE_IT(6786);
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
        {TRACE_IT(6787);
            Assert(instr->GetSrc1());
            ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, GetIRTypeFromValueType(simdFuncSignature.args[0]), GetBailOutKindFromValueType(simdFuncSignature.args[0]));

            if (instr->GetSrc2())
            {
                ToTypeSpecUse(instr, instr->GetSrc2(), this->currentBlock, *pSrc2Val, nullptr, GetIRTypeFromValueType(simdFuncSignature.args[1]), GetBailOutKindFromValueType(simdFuncSignature.args[1]));
            }
        }
        if (instr->GetDst())
        {TRACE_IT(6788);
            TypeSpecializeSimd128Dst(GetIRTypeFromValueType(simdFuncSignature.returnType), instr, nullptr, *pSrc1Val, pDstVal);
        }
        return true;
    }
    else
    {TRACE_IT(6789);
        // We didn't type-spec
        if (!IsLoopPrePass())
        {TRACE_IT(6790);
            // Emit bailout if not loop prepass.
            // The inliner inserts bytecodeUses of original args after the instruction. Bailout is safe.
            IR::Instr * bailoutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNoSimdTypeSpec, IR::BailOutNoSimdTypeSpec, instr, instr->m_func);
            bailoutInstr->SetByteCodeOffset(instr);
            instr->InsertAfter(bailoutInstr);

            instr->m_opcode = Js::OpCode::Nop;
            if (instr->GetSrc1())
            {TRACE_IT(6791);
                instr->FreeSrc1();
                if (instr->GetSrc2())
                {TRACE_IT(6792);
                    instr->FreeSrc2();
                }
            }
            if (instr->GetDst())
            {TRACE_IT(6793);
                instr->FreeDst();
            }

            if (this->byteCodeUses)
            {TRACE_IT(6794);
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
{TRACE_IT(6795);
    bool doTypeSpec = true;

    // TODO: Some operations require additional opnd constraints (e.g. shuffle/swizzle).
    if (Js::IsSimd128Opcode(instr->m_opcode))
    {TRACE_IT(6796);
        ThreadContext::SimdFuncSignature simdFuncSignature;
        instr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(instr->m_opcode, simdFuncSignature);
        if (!simdFuncSignature.valid)
        {TRACE_IT(6797);
            // not implemented yet.
            return false;
        }
        // special handling for Load/Store
        if (Js::IsSimd128Load(instr->m_opcode) || Js::IsSimd128Store(instr->m_opcode))
        {TRACE_IT(6798);
            return Simd128DoTypeSpecLoadStore(instr, src1Val, src2Val, dstVal, &simdFuncSignature);
        }

        const uint argCount = simdFuncSignature.argCount;
        switch (argCount)
        {
        case 2:
            Assert(src2Val);
            doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(src2Val->GetValueInfo()->Type(), simdFuncSignature.args[1]) && Simd128ValidateIfLaneIndex(instr, instr->GetSrc2(), 1);
            // fall-through
        case 1:
            Assert(src1Val);
            doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(src1Val->GetValueInfo()->Type(), simdFuncSignature.args[0]) && Simd128ValidateIfLaneIndex(instr, instr->GetSrc1(), 0);
            break;
        default:
        {TRACE_IT(6799);
            // extended args
            Assert(argCount > 2);
            // Check if all args have been type specialized.

            int arg = argCount - 1;
            IR::Instr * eaInstr = GetExtendedArg(instr);

            while (arg>=0)
            {TRACE_IT(6800);
                Assert(eaInstr);
                Assert(eaInstr->m_opcode == Js::OpCode::ExtendArg_A);

                ValueType expectedType = simdFuncSignature.args[arg];
                IR::Opnd * opnd = eaInstr->GetSrc1();
                StackSym * sym = opnd->GetStackSym();

                // In Forward Prepass: Check liveness through liveness bits, not IR type, since in prepass no actual type-spec happens.
                // In the Forward Pass: Check IRType since Sym can be null, because of const prop.
                if (expectedType.IsSimd128Float32x4())
                {TRACE_IT(6801);
                    if (sym && !IsSimd128F4TypeSpecialized(sym, &currentBlock->globOptData) ||
                        !sym && opnd->GetType() != TySimd128F4)
                    {TRACE_IT(6802);
                        return false;
                    }
                }
                else if (expectedType.IsSimd128Int32x4())
                {TRACE_IT(6803);
                    if (sym && !IsSimd128I4TypeSpecialized(sym, &currentBlock->globOptData) ||
                        !sym && opnd->GetType() != TySimd128I4)
                    {TRACE_IT(6804);
                        return false;
                    }
                }
                else if (expectedType.IsFloat())
                {TRACE_IT(6805);
                    if (sym && !IsFloat64TypeSpecialized(sym, &currentBlock->globOptData) ||
                        !sym&& opnd->GetType() != TyFloat64)
                    {TRACE_IT(6806);
                        return false;
                    }

                }
                else if (expectedType.IsInt())
                {TRACE_IT(6807);
                    if ((sym && !IsInt32TypeSpecialized(sym, &currentBlock->globOptData) && !currentBlock->globOptData.liveLossyInt32Syms->Test(sym->m_id)) ||
                        !sym && opnd->GetType() != TyInt32)
                    {TRACE_IT(6808);
                        return false;
                    }
                    // Extra check if arg is a lane index
                    if (!Simd128ValidateIfLaneIndex(instr, opnd, arg))
                    {TRACE_IT(6809);
                        return false;
                    }
                }
                else
                {TRACE_IT(6810);
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
    {TRACE_IT(6811);
        Assert(instr->m_opcode == Js::OpCode::ExtendArg_A);
        // For ExtendArg, the expected type is encoded in the dst(link) operand.
        doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(src1Val->GetValueInfo()->Type(), instr->GetDst()->GetValueType());
    }

    return doTypeSpec;
}

bool
GlobOpt::Simd128DoTypeSpecLoadStore(IR::Instr *instr, const Value *src1Val, const Value *src2Val, const Value *dstVal, const ThreadContext::SimdFuncSignature *simdFuncSignature)
{TRACE_IT(6812);
    IR::Opnd *baseOpnd = nullptr, *indexOpnd = nullptr, *valueOpnd = nullptr;
    IR::Opnd *src, *dst;

    bool doTypeSpec = true;

    // value = Ld [arr + index]
    // [arr + index] = St value
    src = instr->GetSrc1();
    dst = instr->GetDst();
    Assert(dst && src && !instr->GetSrc2());

    if (Js::IsSimd128Load(instr->m_opcode))
    {TRACE_IT(6813);
        Assert(src->IsIndirOpnd());
        baseOpnd = instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd();
        indexOpnd = instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd();
        valueOpnd = instr->GetDst();
    }
    else if (Js::IsSimd128Store(instr->m_opcode))
    {TRACE_IT(6814);
        Assert(dst->IsIndirOpnd());
        baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
        indexOpnd = instr->GetDst()->AsIndirOpnd()->GetIndexOpnd();
        valueOpnd = instr->GetSrc1();

        // St(arr, index, value). Make sure value can be Simd128 type-spec'd
        doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(FindValue(valueOpnd->AsRegOpnd()->m_sym)->GetValueInfo()->Type(), simdFuncSignature->args[2]);
    }
    else
    {TRACE_IT(6815);
        Assert(UNREACHED);
    }

    // array and index operands should have been type-specialized in OptArraySrc: ValueTypes should be definite at this point. If not, don't type-spec.
    // We can be in a loop prepass, where opnd ValueInfo is not set yet. Get the ValueInfo from the Value Table instead.
    ValueType baseOpndType = FindValue(baseOpnd->AsRegOpnd()->m_sym)->GetValueInfo()->Type();
    
    if (IsLoopPrePass())
    {TRACE_IT(6816);
        doTypeSpec = doTypeSpec && (baseOpndType.IsObject() && baseOpndType.IsTypedArray());
        // indexOpnd might be missing if loading from [0]
        if (indexOpnd != nullptr)
        {TRACE_IT(6817);
            ValueType indexOpndType = FindValue(indexOpnd->AsRegOpnd()->m_sym)->GetValueInfo()->Type();
            doTypeSpec = doTypeSpec && indexOpndType.IsLikelyInt();
        }
    }
    else
    {TRACE_IT(6818);
        doTypeSpec = doTypeSpec && (baseOpndType.IsObject() && baseOpndType.IsTypedArray());
        if (indexOpnd != nullptr)
        {TRACE_IT(6819);
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
{TRACE_IT(6820);
    if (!opndType.IsSimd128() && !expectedType.IsSimd128())
    {TRACE_IT(6821);
        // Non-Simd types can be coerced or we bailout by a FromVar.
        return true;
    }

    // Simd type
    if (opndType.HasBeenNull() || opndType.HasBeenUndefined())
    {TRACE_IT(6822);
        return false;
    }

    if (
            (opndType.IsLikelyObject() && opndType.ToDefiniteObject() == expectedType) ||
            (opndType.IsLikelyObject() && opndType.GetObjectType() == ObjectType::Object)
       )
    {TRACE_IT(6823);
        return true;
    }
    return false;
}

/*
Given an instr, opnd and the opnd position. Return true if opnd is a lane index and valid, or not a lane index all-together..
*/
bool GlobOpt::Simd128ValidateIfLaneIndex(const IR::Instr * instr, IR::Opnd * opnd, uint argPos)
{TRACE_IT(6824);
    Assert(instr);
    Assert(opnd);

    uint laneIndex;
    uint argPosLo, argPosHi;
    uint laneIndexLo, laneIndexHi;

    // operation takes a lane index ?
    switch (instr->m_opcode)
    {
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
    {TRACE_IT(6825);
        return true; // not a lane index
    }

    // It is a lane index ...

    // Arg is Int constant (literal or const prop'd) ?
    if (!opnd->IsIntConstOpnd())
    {TRACE_IT(6826);
        return false;
    }
    laneIndex = (uint) opnd->AsIntConstOpnd()->GetValue();

    // In range ?
    if (laneIndex < laneIndexLo|| laneIndex > laneIndexHi)
    {TRACE_IT(6827);
        return false;
    }

    return true;
}

IR::Instr * GlobOpt::GetExtendedArg(IR::Instr *instr)
{TRACE_IT(6828);
    IR::Opnd *src1, *src2;

    src1 = instr->GetSrc1();
    src2 = instr->GetSrc2();

    if (instr->m_opcode == Js::OpCode::ExtendArg_A)
    {TRACE_IT(6829);
        if (src2)
        {TRACE_IT(6830);
            // mid chain
            Assert(src2->GetStackSym()->IsSingleDef());
            return src2->GetStackSym()->GetInstrDef();
        }
        else
        {TRACE_IT(6831);
            // end of chain
            return nullptr;
        }
    }
    else
    {TRACE_IT(6832);
        // start of chain
        Assert(Js::IsSimd128Opcode(instr->m_opcode));
        Assert(src1);
        Assert(src1->GetStackSym()->IsSingleDef());
        return src1->GetStackSym()->GetInstrDef();
    }
}

#endif

IRType GlobOpt::GetIRTypeFromValueType(const ValueType &valueType)
{TRACE_IT(6833);
    if (valueType.IsFloat())
    {TRACE_IT(6834);
        return TyFloat64;
    }
    else if (valueType.IsInt())
    {TRACE_IT(6835);
        return TyInt32;
    }
    else if (valueType.IsSimd128Float32x4())
    {TRACE_IT(6836);
        return TySimd128F4;
    }
    else
    {TRACE_IT(6837);
        Assert(valueType.IsSimd128Int32x4());
        return TySimd128I4;
    }
}

ValueType GlobOpt::GetValueTypeFromIRType(const IRType &type)
{TRACE_IT(6838);
    switch (type)
    {
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
{TRACE_IT(6839);
    if (valueType.IsFloat())
    {TRACE_IT(6840);
        // if required valueType is Float, then allow coercion from any primitive except String.
        return IR::BailOutPrimitiveButString;
    }
    else if (valueType.IsInt())
    {TRACE_IT(6841);
        return IR::BailOutIntOnly;
    }
    else if (valueType.IsSimd128Float32x4())
    {TRACE_IT(6842);
        return IR::BailOutSimd128F4Only;
    }
    else
    {TRACE_IT(6843);
        Assert(valueType.IsSimd128Int32x4());
        return IR::BailOutSimd128I4Only;
    }
}

#ifdef ENABLE_SIMDJS
void
GlobOpt::UpdateBoundCheckHoistInfoForSimd(ArrayUpperBoundCheckHoistInfo &upperHoistInfo, ValueType arrValueType, const IR::Instr *instr)
{TRACE_IT(6844);
    if (!upperHoistInfo.HasAnyInfo())
    {TRACE_IT(6845);
        return;
    }

    int newOffset = GetBoundCheckOffsetForSimd(arrValueType, instr, upperHoistInfo.Offset());
    upperHoistInfo.UpdateOffset(newOffset);
}
#endif
int
GlobOpt::GetBoundCheckOffsetForSimd(ValueType arrValueType, const IR::Instr *instr, const int oldOffset /* = -1 */)
{TRACE_IT(6846);
#ifdef ENABLE_SIMDJS
    if (!(Js::IsSimd128LoadStore(instr->m_opcode)))
    {TRACE_IT(6847);
        return oldOffset;
    }

    if (!arrValueType.IsTypedArray())
    {TRACE_IT(6848);
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
{TRACE_IT(6849);
    switch (opcode)
    {
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
