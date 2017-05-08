//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

void
Security::EncodeLargeConstants()
{TRACE_IT(15272);
#pragma prefast(suppress:6236 6285, "logical-or of constants is by design")
    if (PHASE_OFF(Js::EncodeConstantsPhase, this->func) || CONFIG_ISENABLED(Js::DebugFlag) || !MD_ENCODE_LG_CONSTS)
    {TRACE_IT(15273);
        return;
    }

    FOREACH_REAL_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {TRACE_IT(15274);
        if (!instr->IsRealInstr())
        {TRACE_IT(15275);
            continue;
        }
        IR::Opnd *dst = instr->GetDst();
        if (dst)
        {TRACE_IT(15276);
            this->EncodeOpnd(instr, dst);
        }
        IR::Opnd *src1 = instr->GetSrc1();
        if (src1)
        {TRACE_IT(15277);
            this->EncodeOpnd(instr, src1);

            IR::Opnd *src2 = instr->GetSrc2();
            if (src2)
            {TRACE_IT(15278);
                this->EncodeOpnd(instr, src2);
            }
        }
    } NEXT_REAL_INSTR_IN_FUNC_EDITING;
}

int
Security::GetNextNOPInsertPoint()
{TRACE_IT(15279);
    uint frequency = (1 << CONFIG_FLAG(NopFrequency)) - 1;
    return (Math::Rand() & frequency) + 1;
}

void
Security::InsertRandomFunctionPad(IR::Instr * instrBeforeInstr)
{TRACE_IT(15280);
    if (PHASE_OFF(Js::InsertNOPsPhase, instrBeforeInstr->m_func->GetTopFunc())
        || CONFIG_ISENABLED(Js::DebugFlag) || CONFIG_ISENABLED(Js::BenchmarkFlag))
    {TRACE_IT(15281);
        return;
    }
    DWORD randomPad = Math::Rand() & ((0 - INSTR_ALIGNMENT) & 0xF);
#ifndef _M_ARM
    if (randomPad == 1)
    {
        InsertSmallNOP(instrBeforeInstr, 1);
        return;
    }
    if (randomPad & 1)
    {
        InsertSmallNOP(instrBeforeInstr, 3);
        randomPad -= 3;
    }
#endif
    Assert((randomPad & 1) == 0);
    while (randomPad >= 4)
    {
        InsertSmallNOP(instrBeforeInstr, 4);
        randomPad -= 4;
    }
    Assert(randomPad == 2 || randomPad == 0);
    if (randomPad == 2)
    {
        InsertSmallNOP(instrBeforeInstr, 2);
    }
}


void
Security::InsertNOPs()
{TRACE_IT(15282);
    if (PHASE_OFF(Js::InsertNOPsPhase, this->func) || CONFIG_ISENABLED(Js::DebugFlag) || CONFIG_ISENABLED(Js::BenchmarkFlag))
    {TRACE_IT(15283);
        return;
    }

    int count = 0;
    IR::Instr *instr = this->func->m_headInstr;

    while(true)
    {TRACE_IT(15284);
        count = this->GetNextNOPInsertPoint();
        while(instr && count--)
        {TRACE_IT(15285);
            instr = instr->GetNextRealInstr();
        }
        if (instr == nullptr)
        {TRACE_IT(15286);
            break;
        }
        this->InsertNOPBefore(instr);
    };
}

void
Security::InsertNOPBefore(IR::Instr *instr)
{
    InsertSmallNOP(instr, (Math::Rand() & 0x3) + 1);
}

void
Security::InsertSmallNOP(IR::Instr * instr, DWORD nopSize)
{TRACE_IT(15287);
#if defined(_M_IX86) || defined(_M_X64)
#ifdef _M_IX86
    if (AutoSystemInfo::Data.SSE2Available())
    {TRACE_IT(15288);   // on x86 system that has SSE2, encode fast NOPs as x64 does
#endif
        Assert(nopSize >= 1 || nopSize <= 4);
        IR::Instr *nop = IR::Instr::New(Js::OpCode::NOP, instr->m_func);

        // Let the encoder know what the size of the NOP needs to be.
        if (nopSize > 1)
        {TRACE_IT(15289);
            // 2, 3 or 4 byte NOP.
            IR::IntConstOpnd *nopSizeOpnd = IR::IntConstOpnd::New(nopSize, TyInt8, instr->m_func);
            nop->SetSrc1(nopSizeOpnd);
        }

        instr->InsertBefore(nop);
#ifdef _M_IX86
    }
    else
    {TRACE_IT(15290);
        IR::Instr *nopInstr = nullptr;
        IR::RegOpnd *regOpnd;
        IR::IndirOpnd *indirOpnd;
        switch (nopSize)
        {
        case 1:
            // nop
            nopInstr = IR::Instr::New(Js::OpCode::NOP, instr->m_func);
            break;
        case 2:
            // mov edi, edi         ; 2 bytes
            regOpnd = IR::RegOpnd::New(nullptr, RegEDI, TyInt32, instr->m_func);
            nopInstr = IR::Instr::New(Js::OpCode::MOV, regOpnd, regOpnd, instr->m_func);
            break;
        case 3:
            // lea ecx, [ecx+00]    ; 3 bytes
            regOpnd = IR::RegOpnd::New(nullptr, RegECX, TyInt32, instr->m_func);
            indirOpnd = IR::IndirOpnd::New(regOpnd, (int32)0, TyInt32, instr->m_func);
            nopInstr = IR::Instr::New(Js::OpCode::LEA, regOpnd, indirOpnd, instr->m_func);
            break;
        case 4:
            // lea esp, [esp+00]    ; 4 bytes
            regOpnd = IR::RegOpnd::New(nullptr, RegESP, TyInt32, instr->m_func);
            indirOpnd = IR::IndirOpnd::New(regOpnd, (int32)0, TyInt32, instr->m_func);
            nopInstr = IR::Instr::New(Js::OpCode::LEA, regOpnd, indirOpnd, instr->m_func);
            break;
        default:
            Assert(false);
            break;
        }
        instr->InsertBefore(nopInstr);
    }
#endif
#elif defined(_M_ARM)
    // Can't insert 3 bytes, must choose between 2 and 4.

    IR::Instr *nopInstr = nullptr;

    switch(nopSize)
    {
    case 1:
    case 2:
        nopInstr = IR::Instr::New(Js::OpCode::NOP, instr->m_func);
        break;
    case 3:
    case 4:
        nopInstr = IR::Instr::New(Js::OpCode::NOP_W, instr->m_func);
        break;
    default:
        Assert(false);
        break;
    }

    instr->InsertBefore(nopInstr);
#else
    AssertMsg(false, "Unimplemented");
#endif
}

bool
Security::DontEncode(IR::Opnd *opnd)
{TRACE_IT(15291);
    switch (opnd->GetKind())
    {
    case IR::OpndKindIntConst:
    {TRACE_IT(15292);
        IR::IntConstOpnd *intConstOpnd = opnd->AsIntConstOpnd();
        return intConstOpnd->m_dontEncode;
    }

    case IR::OpndKindAddr:
    {TRACE_IT(15293);
        IR::AddrOpnd *addrOpnd = opnd->AsAddrOpnd();
        return (addrOpnd->m_dontEncode ||
                !addrOpnd->IsVar() ||
                addrOpnd->m_address == nullptr ||
                !Js::TaggedNumber::Is(addrOpnd->m_address));
    }

    case IR::OpndKindHelperCall:
        // Never encode helper call addresses, as these are always internal constants.
        return true;
    }

    return false;
}

void
Security::EncodeOpnd(IR::Instr *instr, IR::Opnd *opnd)
{TRACE_IT(15294);
    IR::RegOpnd *newOpnd;
    bool isSrc2 = false;

    if (Security::DontEncode(opnd))
    {TRACE_IT(15295);
        return;
    }

    switch(opnd->GetKind())
    {
    case IR::OpndKindIntConst:
    {TRACE_IT(15296);
        IR::IntConstOpnd *intConstOpnd = opnd->AsIntConstOpnd();

        if (
#if TARGET_64
            IRType_IsInt64(intConstOpnd->GetType()) ? !this->IsLargeConstant(intConstOpnd->GetValue()) :
#endif
            !this->IsLargeConstant(intConstOpnd->AsInt32()))
        {TRACE_IT(15297);
            return;
        }

        if (opnd != instr->GetSrc1())
        {TRACE_IT(15298);
            Assert(opnd == instr->GetSrc2());
            isSrc2 = true;
            instr->UnlinkSrc2();
        }
        else
        {TRACE_IT(15299);
            instr->UnlinkSrc1();
        }

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
        intConstOpnd->decodedValue = intConstOpnd->GetValue();
#endif

        intConstOpnd->SetValue(EncodeValue(instr, intConstOpnd, intConstOpnd->GetValue(), &newOpnd));
    }
    break;

    case IR::OpndKindAddr:
    {TRACE_IT(15300);
        IR::AddrOpnd *addrOpnd = opnd->AsAddrOpnd();

        // Only encode large constants.  Small ones don't allow control of enough bits
        if (Js::TaggedInt::Is(addrOpnd->m_address) && !this->IsLargeConstant(Js::TaggedInt::ToInt32(addrOpnd->m_address)))
        {TRACE_IT(15301);
            return;
        }

        if (opnd != instr->GetSrc1())
        {TRACE_IT(15302);
            Assert(opnd == instr->GetSrc2());
            isSrc2 = true;
            instr->UnlinkSrc2();
        }
        else
        {TRACE_IT(15303);
            instr->UnlinkSrc1();
        }

        addrOpnd->SetEncodedValue((Js::Var)this->EncodeValue(instr, addrOpnd, (IntConstType)addrOpnd->m_address, &newOpnd), addrOpnd->GetAddrOpndKind());
    }
    break;

    case IR::OpndKindIndir:
    {TRACE_IT(15304);
        IR::IndirOpnd *indirOpnd = opnd->AsIndirOpnd();

        if (!this->IsLargeConstant(indirOpnd->GetOffset()) || indirOpnd->m_dontEncode)
        {TRACE_IT(15305);
            return;
        }
        AssertMsg(indirOpnd->GetIndexOpnd() == nullptr, "Code currently doesn't support indir with offset and indexOpnd");

        IR::IntConstOpnd *indexOpnd = IR::IntConstOpnd::New(indirOpnd->GetOffset(), TyInt32, instr->m_func);
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
        indexOpnd->decodedValue = indexOpnd->GetValue();
#endif

        indexOpnd->SetValue(EncodeValue(instr, indexOpnd, indexOpnd->GetValue(), &newOpnd));
        indirOpnd->SetOffset(0);
        indirOpnd->SetIndexOpnd(newOpnd);
    }
    return;

    default:
        return;
    }

    IR::Opnd *dst = instr->GetDst();

    if (dst)
    {TRACE_IT(15306);
#if _M_X64
        // Ensure the left and right operand has the same type (that might not be true for constants on x64)
        newOpnd = (IR::RegOpnd *)newOpnd->UseWithNewType(dst->GetType(), instr->m_func);
#endif
        if (dst->IsRegOpnd())
        {TRACE_IT(15307);
            IR::RegOpnd *dstRegOpnd = dst->AsRegOpnd();
            StackSym *dstSym = dstRegOpnd->m_sym;

            if (dstSym)
            {TRACE_IT(15308);
                dstSym->m_isConst = false;
                dstSym->m_isIntConst = false;
                dstSym->m_isInt64Const = false;
                dstSym->m_isTaggableIntConst = false;
                dstSym->m_isFltConst = false;
            }
        }
    }

     LowererMD::ImmedSrcToReg(instr, newOpnd, isSrc2 ? 2 : 1);
}

IntConstType
Security::EncodeValue(IR::Instr *instr, IR::Opnd *opnd, IntConstType constValue, IR::RegOpnd **pNewOpnd)
{TRACE_IT(15309);
    if (opnd->GetType() == TyInt32 || opnd->GetType() == TyInt16 || opnd->GetType() == TyInt8
#if _M_IX86
        || opnd->GetType() == TyVar
#endif
        )
    {TRACE_IT(15310);
        int32 cookie = (int32)Math::Rand();
        IR::RegOpnd *regOpnd = IR::RegOpnd::New(StackSym::New(TyInt32, instr->m_func), TyInt32, instr->m_func);
        IR::Instr * instrNew = LowererMD::CreateAssign(regOpnd, opnd, instr);

        IR::IntConstOpnd * cookieOpnd = IR::IntConstOpnd::New(cookie, TyInt32, instr->m_func);

#if DBG_DUMP
        cookieOpnd->name = _u("cookie");
#endif

        instrNew = IR::Instr::New(Js::OpCode::Xor_I4, regOpnd, regOpnd, cookieOpnd, instr->m_func);
        instr->InsertBefore(instrNew);

        LowererMD::EmitInt4Instr(instrNew);

        StackSym * stackSym = regOpnd->m_sym;
        Assert(!stackSym->m_isSingleDef);
        Assert(stackSym->m_instrDef == nullptr);
        stackSym->m_isEncodedConstant = true;
        stackSym->constantValue = (int32)constValue;

        *pNewOpnd = regOpnd;

        int32 value = (int32)constValue;
        value = value ^ cookie;
        return value;
    }
    else if (opnd->GetType() == TyUint32 || opnd->GetType() == TyUint16 || opnd->GetType() == TyUint8)
    {TRACE_IT(15311);
        uint32 cookie = (uint32)Math::Rand();
        IR::RegOpnd *regOpnd = IR::RegOpnd::New(StackSym::New(TyUint32, instr->m_func), TyUint32, instr->m_func);
        IR::Instr * instrNew = LowererMD::CreateAssign(regOpnd, opnd, instr);

        IR::IntConstOpnd * cookieOpnd = IR::IntConstOpnd::New(cookie, TyUint32, instr->m_func);

#if DBG_DUMP
        cookieOpnd->name = _u("cookie");
#endif

        instrNew = IR::Instr::New(Js::OpCode::Xor_I4, regOpnd, regOpnd, cookieOpnd, instr->m_func);
        instr->InsertBefore(instrNew);

        LowererMD::EmitInt4Instr(instrNew);

        StackSym * stackSym = regOpnd->m_sym;
        Assert(!stackSym->m_isSingleDef);
        Assert(stackSym->m_instrDef == nullptr);
        stackSym->m_isEncodedConstant = true;
        stackSym->constantValue = (uint32)constValue;

        *pNewOpnd = regOpnd;

        uint32 value = (uint32)constValue;
        value = value ^ cookie;
        return (IntConstType)value;
    }
    else
    {TRACE_IT(15312);
#ifdef _M_X64
        return this->EncodeAddress(instr, opnd, constValue, pNewOpnd);
#else
        Assert(false);
        return 0;
#endif
    }
}

#ifdef _M_X64
size_t
Security::EncodeAddress(IR::Instr *instr, IR::Opnd *opnd, size_t value, IR::RegOpnd **pNewOpnd)
{TRACE_IT(15313);
    IR::Instr   *instrNew = nullptr;
    IR::RegOpnd *regOpnd  = IR::RegOpnd::New(TyMachReg, instr->m_func);

    instrNew = LowererMD::CreateAssign(regOpnd, opnd, instr);

    size_t cookie = (size_t)Math::Rand();
    IR::IntConstOpnd *cookieOpnd = IR::IntConstOpnd::New(cookie, TyMachReg, instr->m_func);
    instrNew = IR::Instr::New(Js::OpCode::XOR, regOpnd, regOpnd, cookieOpnd, instr->m_func);
    instr->InsertBefore(instrNew);
    LowererMD::Legalize(instrNew);

    StackSym * stackSym = regOpnd->m_sym;
    Assert(!stackSym->m_isSingleDef);
    Assert(stackSym->m_instrDef == nullptr);
    stackSym->m_isEncodedConstant = true;
    stackSym->constantValue = value;

    *pNewOpnd = regOpnd;
    return value ^ cookie;
}
#endif
