//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

const Js::ArgSlot StackSym::InvalidSlot = (Js::ArgSlot)-1;

///----------------------------------------------------------------------------
///
/// StackSym::New
///
///     Creates a StackSym
///
///----------------------------------------------------------------------------

StackSym *
StackSym::New(SymID id, IRType type, Js::RegSlot byteCodeRegSlot, Func *func)
{TRACE_IT(15546);
    StackSym * stackSym;

    if (byteCodeRegSlot != Js::Constants::NoRegister)
    {TRACE_IT(15547);
        stackSym = AnewZ(func->m_alloc, ByteCodeStackSym, byteCodeRegSlot, func);
        stackSym->m_hasByteCodeRegSlot = true;
    }
    else
    {TRACE_IT(15548);
        stackSym = AnewZ(func->m_alloc, StackSym);
    }

    stackSym->m_id = id;
    stackSym->m_kind = SymKindStack;

    // Assume SingleDef until proven false.

    stackSym->m_isConst = false;
    stackSym->m_isIntConst = false;
    stackSym->m_isTaggableIntConst = false;
    stackSym->m_isSingleDef = true;
    stackSym->m_isEncodedConstant = false;
    stackSym->m_isFltConst = false;
    stackSym->m_isInt64Const = false;
    stackSym->m_isStrConst = false;
    stackSym->m_isStrEmpty = false;
    stackSym->m_allocated = false;
    stackSym->m_isTypeSpec = false;
    stackSym->m_isArgSlotSym = false;
    stackSym->m_isArgSlotRegSym = false;
    stackSym->m_isParamSym = false;
    stackSym->m_isImplicitParamSym = false;
    stackSym->m_isBailOutReferenced = false;
    stackSym->m_isArgCaptured = false;
    stackSym->m_requiresBailOnNotNumber = false;
    stackSym->m_isCatchObjectSym = false;
    stackSym->m_builtInIndex = Js::BuiltinFunction::None;
    stackSym->m_slotNum = StackSym::InvalidSlot;

    stackSym->m_type = type;
    stackSym->m_equivNext = stackSym;
    stackSym->m_objectInfo = nullptr;

    AssertMsg(func->m_symTable->Find(id) == nullptr, "Trying to add new symbol which already exists.");
    func->m_symTable->Add(stackSym);

    return stackSym;
}

ObjectSymInfo *
ObjectSymInfo::New(Func * func)
{TRACE_IT(15549);
    ObjectSymInfo * objSymInfo = JitAnewZ(func->m_alloc, ObjectSymInfo);
    return objSymInfo;
}

ObjectSymInfo *
ObjectSymInfo::New(StackSym * typeSym, Func * func)
{TRACE_IT(15550);
    ObjectSymInfo * objSymInfo = ObjectSymInfo::New(func);
    objSymInfo->m_typeSym = typeSym;
    return objSymInfo;
}

ObjectSymInfo *
StackSym::EnsureObjectInfo(Func * func)
{TRACE_IT(15551);
    if (this->m_objectInfo == nullptr)
    {TRACE_IT(15552);
        this->m_objectInfo = ObjectSymInfo::New(func);
    }

    return this->m_objectInfo;
}

///----------------------------------------------------------------------------
///
/// StackSym::New
///
///     Creates a StackSym
///
///----------------------------------------------------------------------------

StackSym *
StackSym::New(Func *func)
{TRACE_IT(15553);
    return StackSym::New(func->m_symTable->NewID(), TyVar, Js::Constants::NoRegister, func);
}

StackSym *
StackSym::New(IRType type, Func *func)
{TRACE_IT(15554);
    return StackSym::New(func->m_symTable->NewID(), type, Js::Constants::NoRegister, func);
}

StackSym *
StackSym::NewImplicitParamSym(Js::ArgSlot paramSlotNum, Func * func)
{TRACE_IT(15555);
    return func->m_symTable->GetImplicitParam(paramSlotNum);
}

StackSym *
StackSym::NewParamSlotSym(Js::ArgSlot paramSlotNum, Func * func)
{TRACE_IT(15556);
    return NewParamSlotSym(paramSlotNum, func, TyVar);
}

StackSym *
StackSym::NewParamSlotSym(Js::ArgSlot paramSlotNum, Func * func, IRType type)
{TRACE_IT(15557);
    StackSym * stackSym = StackSym::New(type, func);
    stackSym->m_isParamSym = true;
    stackSym->m_slotNum = paramSlotNum;
    return stackSym;
}

// Represents a temporary sym which is a copy of an arg slot sym.
StackSym *
StackSym::NewArgSlotRegSym(Js::ArgSlot argSlotNum, Func * func, IRType type /* = TyVar */)
{TRACE_IT(15558);
    StackSym * stackSym = StackSym::New(type, func);
    stackSym->m_isArgSlotRegSym = true;
    stackSym->m_slotNum = argSlotNum;

#if defined(_M_X64)
    stackSym->m_argPosition = 0;
#endif

    return stackSym;
}

StackSym *
StackSym::NewArgSlotSym(Js::ArgSlot argSlotNum, Func * func, IRType type /* = TyVar */)
{TRACE_IT(15559);
    StackSym * stackSym = StackSym::New(type, func);
    stackSym->m_isArgSlotSym = true;
    stackSym->m_slotNum = argSlotNum;

#if defined(_M_X64)
    stackSym->m_argPosition = 0;
#endif

    return stackSym;
}

bool
StackSym::IsTempReg(Func *const func) const
{TRACE_IT(15560);
    return !HasByteCodeRegSlot() || GetByteCodeRegSlot() >= func->GetJITFunctionBody()->GetFirstTmpReg();
}

#if DBG
void
StackSym::VerifyConstFlags() const
{TRACE_IT(15561);
    if (m_isConst)
    {TRACE_IT(15562);
        Assert(this->m_isSingleDef);
        Assert(this->m_instrDef);
        if (m_isIntConst)
        {TRACE_IT(15563);
            Assert(!m_isFltConst);
        }
        else
        {TRACE_IT(15564);
            Assert(!m_isTaggableIntConst);
        }
    }
    else
    {TRACE_IT(15565);
        Assert(!m_isIntConst);
        Assert(!m_isTaggableIntConst);
        Assert(!m_isFltConst);
    }
}
#endif

bool
StackSym::IsConst() const
{TRACE_IT(15566);
#if DBG
    VerifyConstFlags();
#endif
    return m_isConst;
}

bool
StackSym::IsIntConst() const
{TRACE_IT(15567);
#if DBG
    VerifyConstFlags();
#endif
    return m_isIntConst;
}

bool
StackSym::IsInt64Const() const
{TRACE_IT(15568);
#if DBG
    VerifyConstFlags();
#endif
    return m_isInt64Const;
}

bool
StackSym::IsTaggableIntConst() const
{TRACE_IT(15569);
#if DBG
    VerifyConstFlags();
#endif
    return m_isTaggableIntConst;
}

bool
StackSym::IsFloatConst() const
{TRACE_IT(15570);
#if DBG
    VerifyConstFlags();
#endif
    return m_isFltConst;
}

bool
StackSym::IsSimd128Const() const
{TRACE_IT(15571);
#if DBG
    VerifyConstFlags();
#endif
    return m_isSimd128Const;
}


void
StackSym::SetIsConst()
{TRACE_IT(15572);
    Assert(this->m_isSingleDef);
    Assert(this->m_instrDef);
    IR::Opnd * src = this->m_instrDef->GetSrc1();

    Assert(src->IsImmediateOpnd() || src->IsFloatConstOpnd() || src->IsSimd128ConstOpnd());

    if (src->IsIntConstOpnd())
    {TRACE_IT(15573);
        Assert(this->m_instrDef->m_opcode == Js::OpCode::Ld_I4 ||  this->m_instrDef->m_opcode == Js::OpCode::LdC_A_I4 || LowererMD::IsAssign(this->m_instrDef));
        this->SetIsIntConst(src->AsIntConstOpnd()->GetValue());
    }
    else if (src->IsInt64ConstOpnd())
    {TRACE_IT(15574);
        Assert(this->m_instrDef->m_opcode == Js::OpCode::Ld_I4 || LowererMD::IsAssign(this->m_instrDef));
        this->SetIsInt64Const();
    }
    else if (src->IsFloatConstOpnd())
    {TRACE_IT(15575);
        Assert(this->m_instrDef->m_opcode == Js::OpCode::LdC_A_R8);
        this->SetIsFloatConst();
    }
    else if (src->IsSimd128ConstOpnd()){TRACE_IT(15576);
        Assert(this->m_instrDef->m_opcode == Js::OpCode::Simd128_LdC);
        this->SetIsSimd128Const();
    }
    else
    {TRACE_IT(15577);
        Assert(this->m_instrDef->m_opcode == Js::OpCode::Ld_A || LowererMD::IsAssign(this->m_instrDef));
        Assert(src->IsAddrOpnd());
        IR::AddrOpnd * addrOpnd = src->AsAddrOpnd();
        this->m_isConst = true;
        if (addrOpnd->IsVar())
        {TRACE_IT(15578);
            Js::Var var = addrOpnd->m_address;
            if (Js::TaggedInt::Is(var))
            {TRACE_IT(15579);
                this->m_isIntConst = true;
                this->m_isTaggableIntConst = true;
            }
            else if (var)
            {TRACE_IT(15580);
#if !FLOATVAR
                if (JITManager::GetJITManager()->IsOOPJITEnabled())
                {TRACE_IT(15581);
                    if (addrOpnd->m_localAddress && Js::JavascriptNumber::Is(addrOpnd->m_localAddress) && Js::JavascriptNumber::IsInt32_NoChecks(addrOpnd->m_localAddress))
                    {TRACE_IT(15582);
                        this->m_isIntConst = true;
                    }
                }
                else
#endif
                {TRACE_IT(15583);
                    if (Js::JavascriptNumber::Is(var) && Js::JavascriptNumber::IsInt32_NoChecks(var))
                    {TRACE_IT(15584);
                        this->m_isIntConst = true;
                    }
                }
            }
        }
    }
}

void
StackSym::SetIsIntConst(IntConstType value)
{TRACE_IT(15585);
    Assert(this->m_isSingleDef);
    Assert(this->m_instrDef);
    this->m_isConst = true;
    this->m_isIntConst = true;
    this->m_isTaggableIntConst = !Js::TaggedInt::IsOverflow(value);
    this->m_isFltConst = false;
}

void StackSym::SetIsInt64Const()
{TRACE_IT(15586);
    Assert(this->m_isSingleDef);
    Assert(this->m_instrDef);
    this->m_isConst = true;
    this->m_isInt64Const = true;
    this->m_isIntConst = false;
    this->m_isTaggableIntConst = false;
    this->m_isFltConst = false;
}

void
StackSym::SetIsFloatConst()
{TRACE_IT(15587);
    Assert(this->m_isSingleDef);
    Assert(this->m_instrDef);
    this->m_isConst = true;
    this->m_isIntConst = false;
    this->m_isTaggableIntConst = false;
    this->m_isFltConst = true;
}

void
StackSym::SetIsSimd128Const()
{TRACE_IT(15588);
    Assert(this->m_isSingleDef);
    Assert(this->m_instrDef);
    this->m_isConst = true;
    this->m_isIntConst = false;
    this->m_isTaggableIntConst = false;
    this->m_isFltConst = false;
    this->m_isSimd128Const = true;
}

Js::RegSlot
StackSym::GetByteCodeRegSlot() const
{TRACE_IT(15589);
    Assert(HasByteCodeRegSlot());
    return ((ByteCodeStackSym *)this)->byteCodeRegSlot;
}



Func *
StackSym::GetByteCodeFunc() const
{TRACE_IT(15590);
    Assert(HasByteCodeRegSlot());
    return ((ByteCodeStackSym *)this)->byteCodeFunc;
}

void
StackSym::IncrementArgSlotNum()
{TRACE_IT(15591);
    Assert(IsArgSlotSym());
    m_slotNum++;
}

void
StackSym::DecrementArgSlotNum()
{TRACE_IT(15592);
    Assert(IsArgSlotSym());
    m_slotNum--;
}

void
StackSym::FixupStackOffset(Func * currentFunc)
{TRACE_IT(15593);
    int offsetFixup = 0;
    Func * parentFunc = currentFunc->GetParentFunc();
    while (parentFunc)
    {TRACE_IT(15594);
        if (parentFunc->callSiteToArgumentsOffsetFixupMap)
        {TRACE_IT(15595);
            parentFunc->callSiteToArgumentsOffsetFixupMap->TryGetValue(currentFunc->callSiteIdInParentFunc, &offsetFixup);
            this->m_offset += offsetFixup * MachPtr;
        }
        parentFunc = parentFunc->GetParentFunc();
        currentFunc = currentFunc->GetParentFunc();
        offsetFixup = 0;
    }
}

///----------------------------------------------------------------------------
///
/// StackSym::FindOrCreate
///
///     Look for a StackSym with the given ID.  If not found, create it.
///
///----------------------------------------------------------------------------

StackSym *
StackSym::FindOrCreate(SymID id, Js::RegSlot byteCodeRegSlot, Func *func, IRType type)
{TRACE_IT(15596);
    StackSym *  stackSym;

    stackSym = func->m_symTable->FindStackSym(id);

    if (stackSym)
    {TRACE_IT(15597);
        Assert(!stackSym->HasByteCodeRegSlot() ||
            (stackSym->GetByteCodeRegSlot() == byteCodeRegSlot && stackSym->GetByteCodeFunc() == func));
        Assert(stackSym->GetType() == type);
        return stackSym;
    }

    return StackSym::New(id, type, byteCodeRegSlot, func);
}

StackSym *
StackSym::CloneDef(Func *func)
{TRACE_IT(15598);
    Cloner * cloner = func->GetCloner();
    StackSym *  newSym = nullptr;

    AssertMsg(cloner, "Use Func::BeginClone to initialize cloner");

    if (!this->m_isSingleDef)
    {TRACE_IT(15599);
        return this;
    }

    switch (this->m_instrDef->m_opcode)
    {
        // Note: we were cloning single-def load constant instr's, but couldn't guarantee
        // that we were cloning all the uses.
    case Js::OpCode::ArgOut_A:
    case Js::OpCode::ArgOut_A_Dynamic:
    case Js::OpCode::ArgOut_A_InlineBuiltIn:
    case Js::OpCode::ArgOut_A_SpreadArg:
    case Js::OpCode::StartCall:
    case Js::OpCode::InlineeMetaArg:
        // Go ahead and clone: we need a single-def sym.
        // Arg/StartCall trees must be single-def.
        // Uses of int-const syms may need to find their defining instr's.
        break;
    default:
        return this;
    }

    if (cloner->symMap == nullptr)
    {TRACE_IT(15600);
        cloner->symMap = HashTable<StackSym*>::New(cloner->alloc, 7);
    }
    else
    {TRACE_IT(15601);
        StackSym **entry = cloner->symMap->Get(m_id);
        if (entry)
        {TRACE_IT(15602);
            newSym = *entry;
        }
    }
    if (newSym == nullptr)
    {TRACE_IT(15603);
        // NOTE: We don't care about the bytecode register information for cloned symbol
        // As those are the float sym that we will convert back to Var before jumping back
        // to the slow path's bailout.  The bailout can just track the original symbol.
        newSym = StackSym::New(func);
        newSym->m_isConst = m_isConst;
        newSym->m_isIntConst = m_isIntConst;
        newSym->m_isTaggableIntConst = m_isTaggableIntConst;
        newSym->m_isArgSlotSym = m_isArgSlotSym;
        newSym->m_isArgSlotRegSym = m_isArgSlotRegSym;
        newSym->m_isParamSym = m_isParamSym;
        newSym->m_isImplicitParamSym = m_isImplicitParamSym;
        newSym->m_isArgCaptured = m_isArgCaptured;
        newSym->m_isBailOutReferenced = m_isBailOutReferenced;
        newSym->m_slotNum = m_slotNum;

#if defined(_M_X64)
        newSym->m_argPosition = m_argPosition;
#endif

        newSym->m_offset = m_offset;
        newSym->m_allocated = m_allocated;
        newSym->m_isInlinedArgSlot = m_isInlinedArgSlot;
        newSym->m_isCatchObjectSym = m_isCatchObjectSym;

        newSym->m_type = m_type;

        newSym->CopySymAttrs(this);

        // NOTE: It is not clear what the right thing to do is here...
        newSym->m_equivNext = newSym;
        cloner->symMap->FindOrInsert(newSym, m_id);
    }

    return newSym;
}

StackSym *
StackSym::CloneUse(Func *func)
{TRACE_IT(15604);
    Cloner * cloner = func->GetCloner();
    StackSym **  newSym;

    AssertMsg(cloner, "Use Func::BeginClone to initialize cloner");

    if (cloner->symMap == nullptr)
    {TRACE_IT(15605);
        return this;
    }

    newSym = cloner->symMap->Get(m_id);

    if (newSym == nullptr)
    {TRACE_IT(15606);
        return this;
    }

    return *newSym;
}

void
StackSym::CopySymAttrs(StackSym *symSrc)
{TRACE_IT(15607);
    m_isNotInt = symSrc->m_isNotInt;
    m_isSafeThis = symSrc->m_isSafeThis;
    m_builtInIndex = symSrc->m_builtInIndex;
}

// StackSym::GetIntConstValue
int32
StackSym::GetIntConstValue() const
{TRACE_IT(15608);
    Assert(this->IsIntConst());
    IR::Instr *defInstr = this->m_instrDef;
    IR::Opnd *src1 = defInstr->GetSrc1();

    if (src1->IsIntConstOpnd())
    {TRACE_IT(15609);
        Assert(defInstr->m_opcode == Js::OpCode::Ld_I4 || defInstr->m_opcode == Js::OpCode::LdC_A_I4 || defInstr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn || LowererMD::IsAssign(defInstr));
        return src1->AsIntConstOpnd()->AsInt32();
    }

    if (src1->IsAddrOpnd())
    {TRACE_IT(15610);
        Assert(defInstr->m_opcode == Js::OpCode::Ld_A || LowererMD::IsAssign(defInstr) || defInstr->m_opcode == Js::OpCode::ArgOut_A || defInstr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn);
        IR::AddrOpnd *addr = src1->AsAddrOpnd();
        Assert(addr->IsVar());
        Js::Var var = src1->AsAddrOpnd()->m_address;
        if (Js::TaggedInt::Is(var))
        {TRACE_IT(15611);
            return Js::TaggedInt::ToInt32(var);
        }
        int32 value;
        const bool isInt32 = Js::JavascriptNumber::TryGetInt32Value(Js::JavascriptNumber::GetValue(var), &value);
        Assert(isInt32);
        return value;
    }
    Assert(src1->IsRegOpnd());
    return src1->AsRegOpnd()->m_sym->GetIntConstValue();
}

Js::Var StackSym::GetFloatConstValueAsVar_PostGlobOpt() const
{TRACE_IT(15612);
    Assert(this->IsConst());
    IR::Instr *defInstr = this->m_instrDef;
    IR::Opnd *src1 = defInstr->GetSrc1();

    StackSym * stackSym = nullptr;

    if (src1->IsRegOpnd())
    {TRACE_IT(15613);
        stackSym = src1->AsRegOpnd()->m_sym;
        Assert(!stackSym->m_isEncodedConstant);
        if (stackSym->m_isSingleDef)
        {TRACE_IT(15614);
            //In ARM constant load is always legalized. Try to get the constant from src def
            defInstr = stackSym->m_instrDef;
            src1 = defInstr->GetSrc1();
        }
    }

    Assert(this->IsFloatConst() || (stackSym && stackSym->IsFloatConst()));

    IR::AddrOpnd *addrOpnd;
    if (src1->IsAddrOpnd())
    {TRACE_IT(15615);
        Assert(defInstr->m_opcode == Js::OpCode::Ld_A);
        addrOpnd = src1->AsAddrOpnd();
    }
    else
    {TRACE_IT(15616);
        Assert(src1->IsFloatConstOpnd());
        Assert(defInstr->m_opcode == Js::OpCode::LdC_A_R8);

        addrOpnd = src1->AsFloatConstOpnd()->GetAddrOpnd(defInstr->m_func);

        // This is just to prevent creating multiple numbers when the sym is used multiple times. We can only do this
        // post-GlobOpt, as otherwise it violates some invariants assumed in GlobOpt.
        defInstr->ReplaceSrc1(addrOpnd);
        defInstr->m_opcode = Js::OpCode::Ld_A;
    }

    const Js::Var address = addrOpnd->m_address;
    Assert(Js::JavascriptNumber::Is(addrOpnd->m_localAddress? addrOpnd->m_localAddress: addrOpnd->m_address));
    return address;
}

void *StackSym::GetConstAddress(bool useLocal /*= false*/) const
{TRACE_IT(15617);
    Assert(this->IsConst());
    IR::Instr *defInstr = this->m_instrDef;
    IR::Opnd *src1 = defInstr->GetSrc1();

    StackSym * stackSym = nullptr;

    if (src1->IsRegOpnd())
    {TRACE_IT(15618);
        stackSym = src1->AsRegOpnd()->m_sym;
        Assert(!stackSym->m_isEncodedConstant);
        if (stackSym->m_isSingleDef)
        {TRACE_IT(15619);
            //In ARM constant load is always legalized. Try to get the constant from src def
            defInstr = stackSym->m_instrDef;
            src1 = defInstr->GetSrc1();
        }
    }

    Assert(src1->IsAddrOpnd());
    Assert(defInstr->m_opcode == Js::OpCode::Ld_A || defInstr->m_opcode == Js::OpCode::LdStr || defInstr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn || LowererMD::IsAssign(defInstr));
    return useLocal ? src1->AsAddrOpnd()->m_localAddress : src1->AsAddrOpnd()->m_address;
}

intptr_t StackSym::GetLiteralConstValue_PostGlobOpt() const
{TRACE_IT(15620);
    Assert(this->IsConst());
    IR::Instr *defInstr = this->m_instrDef;
    IR::Opnd *src1 = defInstr->GetSrc1();

    StackSym * stackSym = nullptr;

    if (src1->IsRegOpnd())
    {TRACE_IT(15621);
        stackSym = src1->AsRegOpnd()->m_sym;
        if (stackSym->m_isEncodedConstant)
        {TRACE_IT(15622);
            Assert(!stackSym->m_isSingleDef);
            Assert(LowererMD::IsAssign(defInstr) || defInstr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn);
            return stackSym->constantValue;
        }

        if (stackSym->m_isSingleDef)
        {TRACE_IT(15623);
            //In ARM constant load is always legalized. Try to get the constant from src def
            defInstr = stackSym->m_instrDef;
            src1 = defInstr->GetSrc1();
        }
    }

    if (src1->IsAddrOpnd())
    {TRACE_IT(15624);
        Assert(defInstr->m_opcode == Js::OpCode::Ld_A || defInstr->m_opcode == Js::OpCode::LdStr || defInstr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn || LowererMD::IsAssign(defInstr));
        return reinterpret_cast<intptr_t>(src1->AsAddrOpnd()->m_address);
    }
    if (src1->IsIntConstOpnd())
    {TRACE_IT(15625);
        Assert(this->IsIntConst() || (stackSym && stackSym->IsIntConst()));
        if (defInstr->m_opcode == Js::OpCode::LdC_A_I4)
        {TRACE_IT(15626);
            IR::AddrOpnd *const addrOpnd = IR::AddrOpnd::NewFromNumber(src1->AsIntConstOpnd()->GetValue(), defInstr->m_func);
            const Js::Var address = addrOpnd->m_address;

            // This is just to prevent creating multiple numbers when the sym is used multiple times. We can only do this
            // post-GlobOpt, as otherwise it violates some invariants assumed in GlobOpt.
            defInstr->ReplaceSrc1(addrOpnd);
            defInstr->m_opcode = Js::OpCode::Ld_A;
            return reinterpret_cast<intptr_t>(address);
        }
        Assert(defInstr->m_opcode == Js::OpCode::Ld_I4 || LowererMD::IsAssign(defInstr) || defInstr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn);
        return src1->AsIntConstOpnd()->GetValue();
    }
    if (src1->IsFloatConstOpnd())
    {TRACE_IT(15627);
        Assert(this->IsFloatConst() || (stackSym && stackSym->IsFloatConst()));
        Assert(defInstr->m_opcode == Js::OpCode::LdC_A_R8);

        IR::AddrOpnd *const addrOpnd = src1->AsFloatConstOpnd()->GetAddrOpnd(defInstr->m_func);
        const Js::Var address = addrOpnd->m_address;

        // This is just to prevent creating multiple numbers when the sym is used multiple times. We can only do this
        // post-GlobOpt, as otherwise it violates some invariants assumed in GlobOpt.
        defInstr->ReplaceSrc1(addrOpnd);
        defInstr->m_opcode = Js::OpCode::Ld_A;
        return reinterpret_cast<intptr_t>(address);
    }

    AssertMsg(UNREACHED, "Unknown const value");
    return 0;
}

IR::Opnd *
StackSym::GetConstOpnd() const
{TRACE_IT(15628);
    Assert(IsConst());
    IR::Instr *defInstr = this->m_instrDef;
    IR::Opnd *src1 = defInstr->GetSrc1();
    if (!src1)
    {TRACE_IT(15629);
#if defined(_M_IX86) || defined(_M_X64)
        Assert(defInstr->m_opcode == Js::OpCode::MOVSD_ZERO);
#else
        Assert(UNREACHED);
#endif
    }
    else if (src1->IsIntConstOpnd())
    {TRACE_IT(15630);
        Assert(this->IsIntConst());
        if (defInstr->m_opcode == Js::OpCode::LdC_A_I4)
        {TRACE_IT(15631);
            src1 = IR::AddrOpnd::NewFromNumber(src1->AsIntConstOpnd()->GetValue(), defInstr->m_func);
            defInstr->ReplaceSrc1(src1);
            defInstr->m_opcode = Js::OpCode::Ld_A;
        }
        else
        {TRACE_IT(15632);
            Assert(defInstr->m_opcode == Js::OpCode::Ld_I4 || LowererMD::IsAssign(defInstr) || defInstr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn);
        }
    }
    else if (src1->IsInt64ConstOpnd())
    {TRACE_IT(15633);
        Assert(this->m_isInt64Const);
        Assert(defInstr->m_opcode == Js::OpCode::Ld_I4 || LowererMD::IsAssign(defInstr));
    }
    else if (src1->IsFloatConstOpnd())
    {TRACE_IT(15634);
        Assert(this->IsFloatConst());
        Assert(defInstr->m_opcode == Js::OpCode::LdC_A_R8);

        src1 = src1->AsFloatConstOpnd()->GetAddrOpnd(defInstr->m_func);
        defInstr->ReplaceSrc1(src1);
        defInstr->m_opcode = Js::OpCode::Ld_A;

    }
    else if (src1->IsAddrOpnd())
    {TRACE_IT(15635);
        Assert(defInstr->m_opcode == Js::OpCode::Ld_A || LowererMD::IsAssign(defInstr) || defInstr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn);
    }
    else if (src1->IsMemRefOpnd())
    {TRACE_IT(15636);
        Assert(this->IsFloatConst() || this->IsSimd128Const());
    }
    else
    {
        AssertMsg(UNREACHED, "Invalid const value");
    }
    return src1;
}

BailoutConstantValue StackSym::GetConstValueForBailout() const
{TRACE_IT(15637);
    Assert(this->IsConst());
    IR::Instr *defInstr = this->m_instrDef;
    IR::Opnd *src1 = defInstr->GetSrc1();

    return src1->GetConstValue();
}


// SIMD_JS
StackSym *
StackSym::GetSimd128EquivSym(IRType type, Func *func)
{TRACE_IT(15638);
    switch (type)
    {
    case TySimd128F4:
        return this->GetSimd128F4EquivSym(func);
        break;
    case TySimd128I4:
        return this->GetSimd128I4EquivSym(func);
        break;
    case TySimd128I16:
        return this->GetSimd128I16EquivSym(func);
        break;
    case TySimd128D2:
        return this->GetSimd128D2EquivSym(func);
        break;
    default:
        Assert(UNREACHED);
        return nullptr;
    }
}

StackSym *
StackSym::GetSimd128F4EquivSym(Func *func)
{TRACE_IT(15639);
    return this->GetTypeEquivSym(TySimd128F4, func);
}

StackSym *
StackSym::GetSimd128I4EquivSym(Func *func)
{TRACE_IT(15640);
    return this->GetTypeEquivSym(TySimd128I4, func);
}

StackSym *
StackSym::GetSimd128I16EquivSym(Func *func)
{TRACE_IT(15641);
    return this->GetTypeEquivSym(TySimd128I16, func);
}

StackSym *
StackSym::GetSimd128D2EquivSym(Func *func)
{TRACE_IT(15642);
    return this->GetTypeEquivSym(TySimd128D2, func);
}

StackSym *
StackSym::GetFloat64EquivSym(Func *func)
{TRACE_IT(15643);
    return this->GetTypeEquivSym(TyFloat64, func);
}

StackSym *
StackSym::GetInt32EquivSym(Func *func)
{TRACE_IT(15644);
    return this->GetTypeEquivSym(TyInt32, func);
}

StackSym *
StackSym::GetVarEquivSym(Func *func)
{TRACE_IT(15645);
    return this->GetTypeEquivSym(TyVar, func);
}

StackSym *
StackSym::GetTypeEquivSym(IRType type, Func *func)
{TRACE_IT(15646);
    Assert(this->m_type != type);

    StackSym *sym = this->m_equivNext;
    int i = 1;
    while (sym != this)
    {TRACE_IT(15647);
        Assert(i <= 5); // circular of at most 6 syms : var, f64, i32, simd128I4, simd128F4, simd128D2
        if (sym->m_type == type)
        {TRACE_IT(15648);
            return sym;
        }
        sym = sym->m_equivNext;
        i++;
    }

    // Don't allocate if func wasn't passed in.
    if (func == nullptr)
    {TRACE_IT(15649);
        return nullptr;
    }

    if (this->HasByteCodeRegSlot())
    {TRACE_IT(15650);
        sym = StackSym::New(func->m_symTable->NewID(), type,
            this->GetByteCodeRegSlot(), this->GetByteCodeFunc());
    }
    else
    {TRACE_IT(15651);
        sym = StackSym::New(type, func);
    }
    sym->m_equivNext = this->m_equivNext;
    this->m_equivNext = sym;
    if (type != TyVar)
    {TRACE_IT(15652);
        sym->m_isTypeSpec = true;
    }

    return sym;
}

StackSym *StackSym::GetVarEquivStackSym_NoCreate(Sym *const sym)
{TRACE_IT(15653);
    Assert(sym);

    if(!sym->IsStackSym())
    {TRACE_IT(15654);
        return nullptr;
    }

    StackSym *stackSym = sym->AsStackSym();
    if(stackSym->IsTypeSpec())
    {TRACE_IT(15655);
        stackSym = stackSym->GetVarEquivSym(nullptr);
    }
    return stackSym;
}

///----------------------------------------------------------------------------
///
/// PropertySym::New
///
///     Creates a PropertySym
///
///----------------------------------------------------------------------------

PropertySym *
PropertySym::New(SymID stackSymID, int32 propertyId, uint32 propertyIdIndex, uint inlineCacheIndex, PropertyKind fieldKind, Func *func)
{TRACE_IT(15656);
    StackSym *  stackSym;

    stackSym = func->m_symTable->FindStackSym(stackSymID);
    AssertMsg(stackSym != nullptr, "Adding propertySym to non-existing stackSym...  Can this happen??");

    return PropertySym::New(stackSym, propertyId, propertyIdIndex, inlineCacheIndex, fieldKind, func);
}

PropertySym *
PropertySym::New(StackSym *stackSym, int32 propertyId, uint32 propertyIdIndex, uint inlineCacheIndex, PropertyKind fieldKind, Func *func)
{TRACE_IT(15657);
    PropertySym *  propertySym;

    propertySym = JitAnewZ(func->m_alloc, PropertySym);

    propertySym->m_func = func;
    propertySym->m_id = func->m_symTable->NewID();
    propertySym->m_kind = SymKindProperty;

    propertySym->m_propertyId = propertyId;

    propertyIdIndex = (uint)-1;
    inlineCacheIndex = (uint)-1;

    propertySym->m_propertyIdIndex = propertyIdIndex;
    propertySym->m_inlineCacheIndex = inlineCacheIndex;
    Assert(propertyIdIndex == (uint)-1 || inlineCacheIndex == (uint)-1);
    propertySym->m_loadInlineCacheIndex = (uint)-1;
    propertySym->m_loadInlineCacheFunc = nullptr;
    propertySym->m_fieldKind = fieldKind;

    propertySym->m_stackSym = stackSym;
    propertySym->m_propertyEquivSet = nullptr;

    // Add to list

    func->m_symTable->Add(propertySym);

    // Keep track of all the property we use from this sym so we can invalidate
    // the value in glob opt
    ObjectSymInfo * objectSymInfo = stackSym->EnsureObjectInfo(func);
    propertySym->m_nextInStackSymList = objectSymInfo->m_propertySymList;
    objectSymInfo->m_propertySymList = propertySym;

    return propertySym;
}

///----------------------------------------------------------------------------
///
/// PropertySym::FindOrCreate
///
///     Look for a PropertySym with the given ID/propertyId.
///
///----------------------------------------------------------------------------

PropertySym *
PropertySym::Find(SymID stackSymID, int32 propertyId, Func *func)
{TRACE_IT(15658);
    return func->m_symTable->FindPropertySym(stackSymID, propertyId);
}

///----------------------------------------------------------------------------
///
/// PropertySym::FindOrCreate
///
///     Look for a PropertySym with the given ID/propertyId.  If not found,
///     create it.
///
///----------------------------------------------------------------------------

PropertySym *
PropertySym::FindOrCreate(SymID stackSymID, int32 propertyId, uint32 propertyIdIndex, uint inlineCacheIndex, PropertyKind fieldKind, Func *func)
{TRACE_IT(15659);
    PropertySym *  propertySym;

    propertySym = Find(stackSymID, propertyId, func);

    if (propertySym)
    {TRACE_IT(15660);
        return propertySym;
    }

    return PropertySym::New(stackSymID, propertyId, propertyIdIndex, inlineCacheIndex, fieldKind, func);
}
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)

///----------------------------------------------------------------------------
///
/// Sym::Dump
///
///----------------------------------------------------------------------------

void
Sym::Dump(IRDumpFlags flags, const ValueType valueType)
{TRACE_IT(15661);
    bool const AsmDumpMode = flags & IRDumpFlags_AsmDumpMode;
    bool const SimpleForm = !!(flags & IRDumpFlags_SimpleForm);

    if (AsmDumpMode)
    {TRACE_IT(15662);
        if (this->IsStackSym() && this->AsStackSym()->IsArgSlotSym())
        {TRACE_IT(15663);
            Output::Print(_u("arg "));
        }
        else if (this->IsStackSym() && this->AsStackSym()->IsParamSlotSym())
        {TRACE_IT(15664);
            Output::Print(_u("param "));
        }
    }
    else if (this->IsStackSym())
    {TRACE_IT(15665);
        StackSym *stackSym = this->AsStackSym();

        if (stackSym->IsArgSlotSym())
        {TRACE_IT(15666);
            if (stackSym->m_isInlinedArgSlot)
            {TRACE_IT(15667);
                Output::Print(_u("iarg%d"), stackSym->GetArgSlotNum());
            }
            else
            {TRACE_IT(15668);
                Output::Print(_u("arg%d"), stackSym->GetArgSlotNum());
            }
            Output::Print(_u("(s%d)"), m_id);
        }
        else if (stackSym->IsParamSlotSym())
        {TRACE_IT(15669);
            if (stackSym->IsImplicitParamSym())
            {TRACE_IT(15670);
                switch (stackSym->GetParamSlotNum())
                {
                case 1:
                    Output::Print(_u("callInfo"));
                    break;
                case 2:
                    Output::Print(_u("funcInfo"));
                    break;
                case 3:
                    Output::Print(_u("genObj"));
                    break;
                case 4:
                    Output::Print(_u("genFrame"));
                    break;
                default:
                    Output::Print(_u("implPrm%d"), stackSym->GetParamSlotNum());
                }
            }
            else
            {TRACE_IT(15671);
                Output::Print(_u("prm%d"), stackSym->GetParamSlotNum());
            }
        }
        else
        {TRACE_IT(15672);
            Output::Print(_u("s%d"), m_id);

            if (Js::Configuration::Global.flags.Debug && stackSym->HasByteCodeRegSlot())
            {TRACE_IT(15673);
                if (!JITManager::GetJITManager()->IsOOPJITEnabled())
                {TRACE_IT(15674);
                    Js::FunctionBody* functionBody = (Js::FunctionBody*)stackSym->GetByteCodeFunc()->GetJITFunctionBody()->GetAddr();
                    if (functionBody->GetPropertyIdOnRegSlotsContainer())
                    {TRACE_IT(15675);
                        if (functionBody->IsNonTempLocalVar(stackSym->GetByteCodeRegSlot()))
                        {TRACE_IT(15676);
                            uint index = stackSym->GetByteCodeRegSlot() - stackSym->GetByteCodeFunc()->GetJITFunctionBody()->GetConstCount();
                            Js::PropertyId propertyId = functionBody->GetPropertyIdOnRegSlotsContainer()->propertyIdsForRegSlots[index];
                            Output::Print(_u("(%s)"), stackSym->GetByteCodeFunc()->GetInProcThreadContext()->GetPropertyRecord(propertyId)->GetBuffer());
                        }
                    }
                }
            }
            if (stackSym->IsVar())
            {TRACE_IT(15677);
                if (stackSym->HasObjectTypeSym() && !SimpleForm)
                {TRACE_IT(15678);
                    Output::Print(_u("<s%d>"), stackSym->GetObjectTypeSym()->m_id);
                }
            }
            else
            {TRACE_IT(15679);
                StackSym *varSym = stackSym->GetVarEquivSym(nullptr);
                if (varSym)
                {TRACE_IT(15680);
                    Output::Print(_u("(s%d)"), varSym->m_id);
                }
            }
            if (!SimpleForm)
            {TRACE_IT(15681);
                if (stackSym->m_builtInIndex != Js::BuiltinFunction::None)
                {TRACE_IT(15682);
                    Output::Print(_u("[ffunc]"));
                }
            }
            IR::Opnd::DumpValueType(valueType);
        }
    }
    else if (this->IsPropertySym())
    {TRACE_IT(15683);
        PropertySym *propertySym = this->AsPropertySym();

        if (!SimpleForm)
        {TRACE_IT(15684);
            Output::Print(_u("s%d("), m_id);
        }

        switch (propertySym->m_fieldKind)
        {
        case PropertyKindData:
            propertySym->m_stackSym->Dump(flags, valueType);
            if (JITManager::GetJITManager()->IsOOPJITEnabled())
            {TRACE_IT(15685);
                Output::Print(_u("->#%d"), propertySym->m_propertyId);
            }
            else
            {TRACE_IT(15686);
                Js::PropertyRecord const* fieldName = propertySym->m_func->GetInProcThreadContext()->GetPropertyRecord(propertySym->m_propertyId);
                Output::Print(_u("->%s"), fieldName->GetBuffer());
            }
            break;
        case PropertyKindSlots:
        case PropertyKindSlotArray:
            propertySym->m_stackSym->Dump(flags, valueType);
            Output::Print(_u("[%d]"), propertySym->m_propertyId);
            break;
        case PropertyKindLocalSlots:
            propertySym->m_stackSym->Dump(flags, valueType);
            Output::Print(_u("l[%d]"), propertySym->m_propertyId);
            break;
        default:
            AssertMsg(0, "Unknown field kind");
            break;
        }

        if (!SimpleForm)
        {TRACE_IT(15687);
            Output::Print(_u(")"));
        }
    }
}


void
Sym::Dump(const ValueType valueType)
{TRACE_IT(15688);
    this->Dump(IRDumpFlags_None, valueType);
}

void
Sym::DumpSimple()
{TRACE_IT(15689);
    this->Dump(IRDumpFlags_SimpleForm);
}
#endif  // DBG_DUMP
