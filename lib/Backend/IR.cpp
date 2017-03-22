//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

namespace IR
{
void
Instr::Init(Js::OpCode opcode, IRKind kind, Func * func)
{LOGMEIN("IR.cpp] 10\n");
    Assert(!OpCodeAttr::ByteCodeOnly(opcode));
    this->m_opcode = opcode;
    this->m_kind = kind;
    this->m_func = func;
#ifdef BAILOUT_INJECTION
    this->bailOutByteCodeLocation = (uint)-1;
#endif
}

uint32
Instr::GetByteCodeOffset() const
{LOGMEIN("IR.cpp] 22\n");
    Assert(m_func->HasByteCodeOffset());
    return m_number;
}

void
Instr::SetByteCodeOffset(uint32 offset)
{LOGMEIN("IR.cpp] 29\n");
    Assert(m_func->HasByteCodeOffset());
    Assert(m_number == Js::Constants::NoByteCodeOffset);
    m_number = offset;
}

void
Instr::SetByteCodeOffset(IR::Instr * instr)
{LOGMEIN("IR.cpp] 37\n");
    SetByteCodeOffset(instr->GetByteCodeOffset());
}

void
Instr::ClearByteCodeOffset()
{LOGMEIN("IR.cpp] 43\n");
    Assert(m_func->HasByteCodeOffset());
    m_number = Js::Constants::NoByteCodeOffset;
}

uint32
Instr::GetNumber() const
{LOGMEIN("IR.cpp] 50\n");
    Assert(m_func->HasInstrNumber());
    return m_number;
}

void
Instr::SetNumber(uint32 number)
{LOGMEIN("IR.cpp] 57\n");
    Assert(m_func->HasInstrNumber());
    m_number = number;
}

bool
Instr::IsPlainInstr() const
{LOGMEIN("IR.cpp] 64\n");
    return this->GetKind() == IR::InstrKindInstr;
}

bool
Instr::DoStackArgsOpt(Func *topFunc) const
{LOGMEIN("IR.cpp] 70\n");
    return this->usesStackArgumentsObject && m_func->IsStackArgsEnabled();
}

bool
Instr::HasTypeCheckBailOut() const
{LOGMEIN("IR.cpp] 76\n");
    return this->HasBailOutInfo() && IR::IsTypeCheckBailOutKind(this->GetBailOutKind());
}

bool
Instr::HasEquivalentTypeCheckBailOut() const
{LOGMEIN("IR.cpp] 82\n");
    return this->HasBailOutInfo() && IR::IsEquivalentTypeCheckBailOutKind(this->GetBailOutKind());
}

void
Instr::ChangeEquivalentToMonoTypeCheckBailOut()
{LOGMEIN("IR.cpp] 88\n");
    Assert(this->HasEquivalentTypeCheckBailOut());

    this->SetBailOutKind(IR::EquivalentToMonoTypeCheckBailOutKind(this->GetBailOutKind()));
}

intptr_t
Instr::TryOptimizeInstrWithFixedDataProperty(IR::Instr **pInstr, GlobOpt * globopt)
{LOGMEIN("IR.cpp] 96\n");
    IR::Instr *&instr = *pInstr;
    Assert(OpCodeAttr::CanLoadFixedFields(instr->m_opcode));
    IR::Opnd * src1 = instr->GetSrc1();
    Assert(src1 && src1->IsSymOpnd() && src1->AsSymOpnd()->IsPropertySymOpnd());

    IR::PropertySymOpnd * propSymOpnd = src1->AsSymOpnd()->AsPropertySymOpnd();
    if (propSymOpnd->HasFixedValue() && !propSymOpnd->IsPoly())
    {LOGMEIN("IR.cpp] 104\n");
        intptr_t fixedValue = propSymOpnd->GetFieldValueAsFixedData();
        Assert(instr->IsProfiledInstr());
        ValueType valType = instr->AsProfiledInstr()->u.FldInfo().valueType;
        if (fixedValue && ((Js::TaggedInt::Is(fixedValue) && (valType.IsUninitialized() || valType.IsLikelyInt())) || PHASE_ON1(Js::FixDataVarPropsPhase)))
        {LOGMEIN("IR.cpp] 109\n");
            // Change Ld[Root]Fld to CheckFixedFld, which doesn't need a dst.
            instr->m_opcode = Js::OpCode::CheckFixedFld;
            IR::RegOpnd* dataValueDstOpnd = instr->UnlinkDst()->AsRegOpnd();
            if (globopt)
            {LOGMEIN("IR.cpp] 114\n");
                globopt->GenerateBailAtOperation(&instr, !propSymOpnd->HasEquivalentTypeSet() ? IR::BailOutFailedFixedFieldTypeCheck : IR::BailOutFailedEquivalentFixedFieldTypeCheck);
            }
            else
            {
                instr = instr->ConvertToBailOutInstr(instr, !propSymOpnd->HasEquivalentTypeSet() ? IR::BailOutFailedFixedFieldTypeCheck : IR::BailOutFailedEquivalentFixedFieldTypeCheck);
            }

            IR::Instr* loadInstr = IR::Instr::NewConstantLoad(dataValueDstOpnd, (intptr_t)fixedValue, valType, instr->m_func);

            OUTPUT_VERBOSE_TRACE(Js::UseFixedDataPropsPhase,
                _u("FixedFields: Replacing the source (fixed Data prop) with property id %u with 0x%x .\n"),
                propSymOpnd->GetPropertyId(), fixedValue);

            instr->InsertAfter(loadInstr);
            propSymOpnd->SetUsesFixedValue(true);
            return fixedValue;
        }
    }
    return 0;
}

///----------------------------------------------------------------------------
///
/// Instr::IsEqual
///     Check if this instruction is the same instruction as compareInstr. Two
/// instructions are equal if kind, opcode, dst, src1 and src2 from both instrs
/// are the same.
///
///----------------------------------------------------------------------------
bool
Instr::IsEqual(IR::Instr *compareInstr) const
{LOGMEIN("IR.cpp] 146\n");
    Assert(compareInstr);
    if (this->GetKind() == compareInstr->GetKind()
        && this->m_opcode == compareInstr->m_opcode)
    {LOGMEIN("IR.cpp] 150\n");
        IR::Opnd *dst = this->GetDst();
        IR::Opnd *src1 = this->GetSrc1();
        IR::Opnd *src2 = this->GetSrc2();
        IR::Opnd *compareDst = compareInstr->GetDst();
        IR::Opnd *compareSrc1 = compareInstr->GetSrc1();
        IR::Opnd *compareSrc2 = compareInstr->GetSrc2();

        // when both dst and compareDst are null, they are equal, same applies to src1, src2
        if ((dst != compareDst) && (!dst || !compareDst || !dst->IsEqual(compareDst)))
        {LOGMEIN("IR.cpp] 160\n");
            return false;
        }
        if ((src1 != compareSrc1) && (!src1 || !compareSrc1 || !src1->IsEqual(compareSrc1)))
        {LOGMEIN("IR.cpp] 164\n");
            return false;
        }
        if ((src2 != compareSrc2) && (!src2 || !compareSrc2 || !src2->IsEqual(compareSrc2)))
        {LOGMEIN("IR.cpp] 168\n");
            return false;
        }

        return true;
    }
    else
    {
        return false;
    }
}

///----------------------------------------------------------------------------
///
/// Instr::InsertBefore
///
///     Insert 'instr' before 'this' instruction.
///
///----------------------------------------------------------------------------

void
Instr::InsertBefore(Instr *instr)
{LOGMEIN("IR.cpp] 190\n");
    Assert(!instr->IsLinked());

    Instr * prevInstr = this->m_prev;
    instr->m_prev = prevInstr;
    this->m_prev = instr;

    if (prevInstr)
    {LOGMEIN("IR.cpp] 198\n");
        prevInstr->m_next = instr;
    }
    instr->m_next = this;
}

///----------------------------------------------------------------------------
///
/// Instr::InsertAfter
///
///     Insert 'instr' after 'this' instruction.
///
///----------------------------------------------------------------------------

void
Instr::InsertAfter(Instr *instr)
{LOGMEIN("IR.cpp] 214\n");
    Assert(!instr->IsLinked());

    Instr * nextInstr = this->m_next;
    instr->m_next = nextInstr;
    this->m_next = instr;

    if (nextInstr)
    {LOGMEIN("IR.cpp] 222\n");
        nextInstr->m_prev = instr;
    }
    instr->m_prev = this;
}

///----------------------------------------------------------------------------
///
/// Instr::InsertRangeBefore
///
///----------------------------------------------------------------------------
void
Instr::InsertRangeBefore(Instr *startInstr, Instr *endInstr)
{LOGMEIN("IR.cpp] 235\n");
    Instr * prevInstr = this->m_prev;

    startInstr->m_prev = prevInstr;
    this->m_prev = endInstr;

    if (prevInstr)
    {LOGMEIN("IR.cpp] 242\n");
        prevInstr->m_next = startInstr;
    }
    endInstr->m_next = this;
}

///----------------------------------------------------------------------------
///
/// Instr::InsertMultipleBefore - Inserts multiple instr
///
///----------------------------------------------------------------------------
void
Instr::InsertMultipleBefore(Instr *endInstr)
{LOGMEIN("IR.cpp] 255\n");
    Instr *startInstr = endInstr->m_prev;

    if (startInstr) // more than one instruction to insert
    {LOGMEIN("IR.cpp] 259\n");
        while (startInstr->m_prev)
        {LOGMEIN("IR.cpp] 261\n");
            startInstr = startInstr->m_prev;
        }
        return this->InsertRangeBefore(startInstr, endInstr);
    }
    return this->InsertBefore(endInstr);
}

///----------------------------------------------------------------------------
///
/// Instr::InsertRangeAfter
///
///----------------------------------------------------------------------------
void
Instr::InsertRangeAfter(Instr *startInstr, Instr *endInstr)
{LOGMEIN("IR.cpp] 276\n");
    Instr * nextInstr = this->m_next;

    endInstr->m_next = nextInstr;
    this->m_next = startInstr;

    if (nextInstr)
    {LOGMEIN("IR.cpp] 283\n");
        nextInstr->m_prev = endInstr;
    }
    startInstr->m_prev = this;
}

///----------------------------------------------------------------------------
///
/// Instr::InsertMultipleAfter - Inserts multiple instr
///
///----------------------------------------------------------------------------
void
Instr::InsertMultipleAfter(Instr *endInstr)
{LOGMEIN("IR.cpp] 296\n");
    Instr *startInstr = endInstr->m_prev;

    if (startInstr) //more than one instruction to insert
    {LOGMEIN("IR.cpp] 300\n");
        while (startInstr->m_prev)
        {LOGMEIN("IR.cpp] 302\n");
            startInstr = startInstr->m_prev;
        }
        return this->InsertRangeAfter(startInstr, endInstr);
    }
    return this->InsertAfter(endInstr);
}


///----------------------------------------------------------------------------
///
/// Instr::Free
///
///     Free this instruction by putting it on a free list.
///
///----------------------------------------------------------------------------

void
Instr::Free()
{LOGMEIN("IR.cpp] 321\n");
    AssertMsg(!this->IsLabelInstr() || !this->AsLabelInstr()->m_hasNonBranchRef,
        "Cannot free label with non-branch reference");

    switch (this->GetKind())
    {LOGMEIN("IR.cpp] 326\n");
    case InstrKindBranch:
        {LOGMEIN("IR.cpp] 328\n");
            IR::BranchInstr *branchInstr = this->AsBranchInstr();
            branchInstr->ClearTarget();
            break;
        }
    }

    IR::Opnd * dstOpnd = this->GetDst();
    if (dstOpnd)
    {LOGMEIN("IR.cpp] 337\n");
        StackSym * stackSym = dstOpnd->GetStackSym();
        if (stackSym)
        {LOGMEIN("IR.cpp] 340\n");
            if (stackSym->m_isSingleDef)
            {LOGMEIN("IR.cpp] 342\n");
                Assert(!stackSym->m_isEncodedConstant);
                if (stackSym->m_instrDef == this)
                {LOGMEIN("IR.cpp] 345\n");
                    Assert(!dstOpnd->isFakeDst);
                    if (stackSym->IsConst())
                    {LOGMEIN("IR.cpp] 348\n");
                        // keep the instruction around so we can get the constant value
                        // from the symbol
                        return;
                    }
                    Assert(this->m_func->GetTopFunc()->allowRemoveBailOutArgInstr || !stackSym->m_isBailOutReferenced);
                    stackSym->m_instrDef = nullptr;
                }
                else
                {
                    Assert(dstOpnd->isFakeDst);
                }
            }
            else
            {
                // Encoded constants are not single-defs anymore, and therefore not isConst.
                Assert((!stackSym->m_isConst && stackSym->constantValue == 0)
                    || (stackSym->m_isEncodedConstant && stackSym->constantValue != 0));
            }
        }
    }

    ClearBailOutInfo();
    JitAdelete(this->m_func->m_alloc, this);
}

///----------------------------------------------------------------------------
///
/// Instr::Unlink
///
///     Unlink this instr from the instr list.
///
///----------------------------------------------------------------------------

void
Instr::Unlink()
{LOGMEIN("IR.cpp] 384\n");
    m_prev->m_next = m_next;
    if (m_next)
    {LOGMEIN("IR.cpp] 387\n");
        m_next->m_prev = m_prev;
    }
    else
    {
        Assert(this == this->m_func->m_tailInstr);
    }

#if DBG_DUMP
    // Transferring the globOptInstrString to the next non-Label Instruction
    if(this->globOptInstrString != nullptr && m_next && m_next->globOptInstrString == nullptr && !m_next->IsLabelInstr())
    {LOGMEIN("IR.cpp] 398\n");
        m_next->globOptInstrString = this->globOptInstrString;
    }
#endif

#if DBG
    m_prev = nullptr;
    m_next = nullptr;
#endif
}

///----------------------------------------------------------------------------
///
/// Instr::Remove
///
///     Unlink and free this instr.
///
///----------------------------------------------------------------------------

void
Instr::Remove()
{LOGMEIN("IR.cpp] 419\n");
    this->Unlink();
    this->Free();
}

void
Instr::SwapOpnds()
{LOGMEIN("IR.cpp] 426\n");
    IR::Opnd *opndTemp = m_src1;
    m_src1 = m_src2;
    m_src2 = opndTemp;
}

// Copy a vanilla instruction.
Instr *
Instr::Copy()
{LOGMEIN("IR.cpp] 435\n");
    Instr * instrCopy;

    if (this->HasBailOutInfo() || this->HasAuxBailOut())
    {LOGMEIN("IR.cpp] 439\n");
        instrCopy = BailOutInstr::New(this->m_opcode, this->GetBailOutKind(), this->GetBailOutInfo(), this->m_func);
        instrCopy->SetByteCodeOffset(this->GetByteCodeOffset());
        if (this->HasAuxBailOut())
        {LOGMEIN("IR.cpp] 443\n");
            instrCopy->hasAuxBailOut = true;
            instrCopy->SetAuxBailOutKind(this->GetAuxBailOutKind());
        }
    }
    else
    {
        switch (this->GetKind())
        {LOGMEIN("IR.cpp] 451\n");
        case InstrKindInstr:
            instrCopy = Instr::New(this->m_opcode, this->m_func);
            break;

        case InstrKindProfiled:
            instrCopy = this->AsProfiledInstr()->CopyProfiledInstr();
            break;

        case InstrKindJitProfiling:
            instrCopy = this->AsJitProfilingInstr()->CopyJitProfiling();
            break;

        case InstrKindPragma:
            instrCopy = this->AsPragmaInstr()->CopyPragma();
            break;

        default:
            instrCopy = nullptr;
            AnalysisAssertMsg(UNREACHED, "Copy of other instr kinds NYI");
        }
    }

    Opnd * opnd = this->GetDst();
    if (opnd)
    {LOGMEIN("IR.cpp] 476\n");
        instrCopy->SetDst(opnd->Copy(this->m_func));
    }
    opnd = this->GetSrc1();
    if (opnd)
    {LOGMEIN("IR.cpp] 481\n");
        instrCopy->SetSrc1(opnd->Copy(this->m_func));
        opnd = this->GetSrc2();
        if (opnd)
        {LOGMEIN("IR.cpp] 485\n");
            instrCopy->SetSrc2(opnd->Copy(this->m_func));
        }
    }

    instrCopy->isInlineeEntryInstr = this->isInlineeEntryInstr;

    if (this->m_func->DoMaintainByteCodeOffset())
    {LOGMEIN("IR.cpp] 493\n");
        instrCopy->SetByteCodeOffset(this->GetByteCodeOffset());
    }
    instrCopy->usesStackArgumentsObject = this->usesStackArgumentsObject;
    return instrCopy;
}

LabelInstr *
LabelInstr::CloneLabel(BOOL fCreate)
{LOGMEIN("IR.cpp] 502\n");
    Func * func = this->m_func;
    Cloner * cloner = func->GetCloner();
    IR::LabelInstr * instrLabel = nullptr;

    AssertMsg(cloner, "Use Func::BeginClone to initialize cloner");

    if (cloner->labelMap == nullptr)
    {LOGMEIN("IR.cpp] 510\n");
        if (!fCreate)
        {LOGMEIN("IR.cpp] 512\n");
            return nullptr;
        }
        cloner->labelMap = HashTable<LabelInstr*>::New(cloner->alloc, 7);
    }
    else
    {
        IR::LabelInstr ** map = cloner->labelMap->Get(this->m_id);
        if (map)
        {LOGMEIN("IR.cpp] 521\n");
            instrLabel = *map;
        }
    }

    if (instrLabel == nullptr)
    {LOGMEIN("IR.cpp] 527\n");
        if (!fCreate)
        {LOGMEIN("IR.cpp] 529\n");
            return nullptr;
        }
        if (this->IsProfiledLabelInstr())
        {LOGMEIN("IR.cpp] 533\n");
            instrLabel = IR::ProfiledLabelInstr::New(this->m_opcode, func, this->AsProfiledLabelInstr()->loopImplicitCallFlags, this->AsProfiledLabelInstr()->loopFlags);
#if DBG
            instrLabel->AsProfiledLabelInstr()->loopNum = this->AsProfiledLabelInstr()->loopNum;
#endif
        }
        else
        {
            instrLabel = IR::LabelInstr::New(this->m_opcode, func, this->isOpHelper);
        }
        instrLabel->m_isLoopTop = this->m_isLoopTop;
        cloner->labelMap->FindOrInsert(instrLabel, this->m_id);
    }

    return instrLabel;
}

ProfiledLabelInstr::ProfiledLabelInstr(JitArenaAllocator * allocator)
    : LabelInstr(allocator)
{LOGMEIN("IR.cpp] 552\n");
}

ProfiledLabelInstr *
ProfiledLabelInstr::New(Js::OpCode opcode, Func *func, Js::ImplicitCallFlags flags, Js::LoopFlags loopFlags)
{LOGMEIN("IR.cpp] 557\n");
    ProfiledLabelInstr * profiledLabelInstr = JitAnew(func->m_alloc, ProfiledLabelInstr, func->m_alloc);
    profiledLabelInstr->Init(opcode, InstrKindProfiledLabel, func, false);
    profiledLabelInstr->loopImplicitCallFlags = flags;
    profiledLabelInstr->loopFlags = loopFlags;
    return profiledLabelInstr;
}

void
BranchInstr::RetargetClonedBranch()
{LOGMEIN("IR.cpp] 567\n");
    IR::LabelInstr * instrLabel = this->m_branchTarget->CloneLabel(false);
    if (instrLabel == nullptr)
    {LOGMEIN("IR.cpp] 570\n");
        // Jumping outside the cloned range. No retarget.
        return;
    }

    this->SetTarget(instrLabel);
}

PragmaInstr *
PragmaInstr::ClonePragma()
{LOGMEIN("IR.cpp] 580\n");
    return this->CopyPragma();
}

PragmaInstr *
PragmaInstr::CopyPragma()
{LOGMEIN("IR.cpp] 586\n");
    IR::PragmaInstr * instrPragma = IR::PragmaInstr::New(this->m_opcode, 0, this->m_func);
    return instrPragma;
}

Instr *
Instr::CloneInstr() const
{LOGMEIN("IR.cpp] 593\n");
    if (this->HasBailOutInfo() || this->HasAuxBailOut())
    {LOGMEIN("IR.cpp] 595\n");
        return ((BailOutInstr *)this)->CloneBailOut();
    }

    IR::Instr *clone = IR::Instr::New(this->m_opcode, this->m_func);
    clone->isInlineeEntryInstr = this->isInlineeEntryInstr;

    return clone;
}

// Clone a vanilla instruction, replacing single-def StackSym's with new syms where appropriate.
Instr *
Instr::Clone()
{LOGMEIN("IR.cpp] 608\n");
    Func * func = this->m_func;
    Cloner *cloner = func->GetCloner();
    IR::Instr * instrClone;
    IR::Opnd * opnd;

    switch (this->GetKind())
    {LOGMEIN("IR.cpp] 615\n");
    case InstrKindInstr:
        instrClone = this->CloneInstr();
        break;
    case InstrKindBranch:
        instrClone = this->AsBranchInstr()->CloneBranchInstr();
        break;
    case InstrKindProfiled:
        instrClone = this->AsProfiledInstr()->CloneProfiledInstr();
        break;
    case InstrKindLabel:
    case InstrKindProfiledLabel:
        instrClone = this->AsLabelInstr()->CloneLabel(true);
        break;
    case InstrKindPragma:
        instrClone = this->AsPragmaInstr()->ClonePragma();
        break;
    case InstrKindJitProfiling:
        instrClone = this->AsJitProfilingInstr()->CloneJitProfiling();
        break;
    default:
        AssertMsg(0, "Clone of this instr kind NYI");
        return nullptr;
    }

    opnd = this->GetDst();
    if (opnd)
    {LOGMEIN("IR.cpp] 642\n");
        instrClone->SetDst(opnd->CloneDef(func));
    }
    opnd = this->GetSrc1();
    if (opnd)
    {LOGMEIN("IR.cpp] 647\n");
        instrClone->SetSrc1(opnd->CloneUse(func));
        opnd = this->GetSrc2();
        if (opnd)
        {LOGMEIN("IR.cpp] 651\n");
            instrClone->SetSrc2(opnd->CloneUse(func));
        }
    }
    if (this->m_func->DoMaintainByteCodeOffset())
    {LOGMEIN("IR.cpp] 656\n");
        instrClone->SetByteCodeOffset(this->GetByteCodeOffset());
    }
    instrClone->usesStackArgumentsObject = this->usesStackArgumentsObject;
    cloner->AddInstr(this, instrClone);

    return instrClone;
}

// Clone a range of instructions.
Instr *
Instr::CloneRange(
    Instr * instrStart, Instr * instrLast, Instr * instrAfter, Lowerer *lowerer, JitArenaAllocator * alloc, bool (*fMapTest)(IR::Instr*), bool clonedInstrGetOrigArgSlotSym)
{LOGMEIN("IR.cpp] 669\n");
    IR::Instr * instrReturn = instrAfter;

    Func * topFunc = instrStart->m_func->GetTopFunc();
    topFunc->BeginClone(lowerer, alloc);
    topFunc->GetCloner()->clonedInstrGetOrigArgSlotSym = clonedInstrGetOrigArgSlotSym;

    FOREACH_INSTR_IN_RANGE(instr, instrStart, instrLast)
    {LOGMEIN("IR.cpp] 677\n");
        Instr * instrClone = instr->Clone();
        instrAfter->InsertAfter(instrClone);
        instrAfter = instrClone;
        instr->isCloned = true;
        if (fMapTest(instrClone))
        {LOGMEIN("IR.cpp] 683\n");
            IR::LabelInstr *instrLabel = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func);
            instrClone->InsertBefore(instrLabel);
            topFunc->GetCloneMap()->Item(instr, instrLabel);
        }
    }
    NEXT_INSTR_IN_RANGE;

    topFunc->EndClone();

    return instrReturn;
}

///----------------------------------------------------------------------------
///
/// Instr::MoveRangeAfter
///
///     Move a range of instruction after another instruction
///
///----------------------------------------------------------------------------

void
Instr::MoveRangeAfter(Instr * instrStart, Instr * instrLast, Instr * instrAfter)
{LOGMEIN("IR.cpp] 706\n");
    if (instrLast->m_next != nullptr)
    {LOGMEIN("IR.cpp] 708\n");
        instrLast->m_next->m_prev = instrStart->m_prev;
    }
    else
    {
        instrLast->m_func->m_tailInstr = instrStart->m_prev;
    }

    if (instrStart->m_prev != nullptr)
    {LOGMEIN("IR.cpp] 717\n");
        instrStart->m_prev->m_next = instrLast->m_next;
    }
    else
    {
        instrStart->m_func->m_headInstr = instrLast->m_next;
    }

    instrStart->m_prev = instrAfter;
    instrLast->m_next = instrAfter->m_next;
    if (instrAfter->m_next != nullptr)
    {LOGMEIN("IR.cpp] 728\n");
        instrAfter->m_next->m_prev = instrLast;
    }
    else
    {
        instrAfter->m_func->m_tailInstr = instrLast;
    }
    instrAfter->m_next = instrStart;
}

JitProfilingInstr *
JitProfilingInstr::New(Js::OpCode opcode, Opnd *dstOpnd, Opnd *src1Opnd, Opnd *src2Opnd, Func * func)
{LOGMEIN("IR.cpp] 740\n");
    JitProfilingInstr * profiledInstr = JitProfilingInstr::New(opcode, dstOpnd, src1Opnd, func);
    profiledInstr->SetSrc2(src2Opnd);

    return profiledInstr;
}

JitProfilingInstr *
JitProfilingInstr::New(Js::OpCode opcode, Opnd *dstOpnd, Opnd *src1Opnd, Func * func)
{LOGMEIN("IR.cpp] 749\n");
    Assert(func->DoSimpleJitDynamicProfile());

    JitProfilingInstr * profiledInstr = JitAnew(func->m_alloc, IR::JitProfilingInstr);
    profiledInstr->Init(opcode, InstrKindJitProfiling, func);

    if (dstOpnd)
    {LOGMEIN("IR.cpp] 756\n");
        profiledInstr->SetDst(dstOpnd);
    }
    if (src1Opnd)
    {LOGMEIN("IR.cpp] 760\n");
        profiledInstr->SetSrc1(src1Opnd);
    }

#if DBG
    profiledInstr->profileId = Js::Constants::NoProfileId;
    profiledInstr->arrayProfileId = Js::Constants::NoProfileId;
    profiledInstr->inlineCacheIndex = Js::Constants::NoInlineCacheIndex;
    Assert(profiledInstr->loopNumber == 0u - 1);
#endif

    // default these to false.
    profiledInstr->isProfiledReturnCall = false;
    profiledInstr->isBeginSwitch = false;
    profiledInstr->isNewArray = false;
    profiledInstr->isLoopHelper = false;

    return profiledInstr;
}

JitProfilingInstr*
JitProfilingInstr::CloneJitProfiling() const
{LOGMEIN("IR.cpp] 782\n");
    // Adapted from Profiled::CloneProfiledInstr. Note that the dst and srcs are not set.

    Assert(!(this->HasBailOutInfo() || this->HasAuxBailOut())); // Shouldn't have bailout info in a jitprofiling instr

    return this->CopyJitProfiling();
}


JitProfilingInstr*
JitProfilingInstr::CopyJitProfiling() const
{LOGMEIN("IR.cpp] 793\n");
    // Adapted from Profiled::CopyProfiledInstr. Note that the dst and srcs are not set.

    IR::JitProfilingInstr * jitProfInstr;

    jitProfInstr = JitAnew(this->m_func->m_alloc, IR::JitProfilingInstr);
    jitProfInstr->Init(this->m_opcode, InstrKindProfiled, this->m_func);

    jitProfInstr->isProfiledReturnCall = this->isProfiledReturnCall;
    jitProfInstr->isBeginSwitch = this->isBeginSwitch;
    jitProfInstr->isNewArray = this->isNewArray;
    jitProfInstr->isLoopHelper = this->isLoopHelper;

    jitProfInstr->profileId = this->profileId;
    jitProfInstr->arrayProfileId = this->arrayProfileId;
    jitProfInstr->inlineCacheIndex = this->inlineCacheIndex;
    Assert(jitProfInstr->loopNumber == this->loopNumber);

    return jitProfInstr;
}

ProfiledInstr *
ProfiledInstr::New(Js::OpCode opcode, Opnd *dstOpnd, Opnd *src1Opnd, Opnd *src2Opnd, Func * func)
{LOGMEIN("IR.cpp] 816\n");
    ProfiledInstr * profiledInstr = ProfiledInstr::New(opcode, dstOpnd, src1Opnd, func);
    profiledInstr->SetSrc2(src2Opnd);

    return profiledInstr;
}

ProfiledInstr *
ProfiledInstr::New(Js::OpCode opcode, Opnd *dstOpnd, Opnd *src1Opnd, Func * func)
{LOGMEIN("IR.cpp] 825\n");
    ProfiledInstr * profiledInstr = JitAnew(func->m_alloc, IR::ProfiledInstr);
    profiledInstr->Init(opcode, InstrKindProfiled, func);

    if (dstOpnd)
    {LOGMEIN("IR.cpp] 830\n");
        profiledInstr->SetDst(dstOpnd);
    }
    if (src1Opnd)
    {LOGMEIN("IR.cpp] 834\n");
        profiledInstr->SetSrc1(src1Opnd);
    }

    profiledInstr->u.ldElemInfo = nullptr;
    return profiledInstr;
}

ProfiledInstr *
ProfiledInstr::CloneProfiledInstr() const
{LOGMEIN("IR.cpp] 844\n");
    IR::ProfiledInstr * profiledInstr;
    if (this->HasBailOutInfo() || this->HasAuxBailOut())
    {LOGMEIN("IR.cpp] 847\n");
        profiledInstr = ((ProfiledBailOutInstr *)this)->CloneBailOut();
        profiledInstr->u = this->u;
    }
    else
    {
        profiledInstr = this->CopyProfiledInstr();
    }

    return profiledInstr;
}

ProfiledInstr *
ProfiledInstr::CopyProfiledInstr() const
{LOGMEIN("IR.cpp] 861\n");
    IR::ProfiledInstr * profiledInstr;

    profiledInstr = JitAnew(this->m_func->m_alloc, IR::ProfiledInstr);
    profiledInstr->Init(this->m_opcode, InstrKindProfiled, this->m_func);
    profiledInstr->u = this->u;

    return profiledInstr;
}

ByteCodeUsesInstr *
ByteCodeUsesInstr::New(IR::Instr * originalBytecodeInstr)
{LOGMEIN("IR.cpp] 873\n");
    Func* func = originalBytecodeInstr->m_func;
    ByteCodeUsesInstr * byteCodeUses = JitAnew(func->m_alloc, IR::ByteCodeUsesInstr);
    byteCodeUses->Init(Js::OpCode::ByteCodeUses, InstrKindByteCodeUses, func);
    byteCodeUses->byteCodeUpwardExposedUsed = nullptr;
    byteCodeUses->propertySymUse = nullptr;
    byteCodeUses->SetByteCodeOffset(originalBytecodeInstr);
    return byteCodeUses;
}

ByteCodeUsesInstr *
ByteCodeUsesInstr::New(Func * func, uint32 offset)
{LOGMEIN("IR.cpp] 885\n");
    ByteCodeUsesInstr * byteCodeUses = JitAnew(func->m_alloc, IR::ByteCodeUsesInstr);
    byteCodeUses->Init(Js::OpCode::ByteCodeUses, InstrKindByteCodeUses, func);
    byteCodeUses->byteCodeUpwardExposedUsed = nullptr;
    byteCodeUses->propertySymUse = nullptr;
    byteCodeUses->SetByteCodeOffset(offset);
    return byteCodeUses;
}

const BVSparse<JitArenaAllocator> * ByteCodeUsesInstr::GetByteCodeUpwardExposedUsed() const
{LOGMEIN("IR.cpp] 895\n");
    return this->byteCodeUpwardExposedUsed;
}

// In the case of instances where you would like to add a ByteCodeUses to some sym,
// which doesn't have an operand associated with it (like a block closure sym), use
// this to set it without needing to pass the check for JIT-Optimized registers.
void ByteCodeUsesInstr::SetNonOpndSymbol(uint symId)
{LOGMEIN("IR.cpp] 903\n");
    if (!this->byteCodeUpwardExposedUsed)
    {LOGMEIN("IR.cpp] 905\n");
        this->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
    }
    this->byteCodeUpwardExposedUsed->Set(symId);
}

// In cases where the operand you're working on may be changed between when you get
// access to it and when you determine that you can set it in the ByteCodeUsesInstr
// set method, cache the values and use this caller.
void ByteCodeUsesInstr::SetRemovedOpndSymbol(bool isJITOptimizedReg, uint symId)
{LOGMEIN("IR.cpp] 915\n");
    if (isJITOptimizedReg)
    {
        AssertMsg(false, "Tried to add a jit-optimized register to a ByteCodeUses instruction!");
        // Although we assert on debug builds, we should actually be ok with release builds
        // if we ignore the operand; not ignoring it, however, can cause us to introduce an
        // inconsistency in bytecode register lifetimes.
        return;
    }
    if(!this->byteCodeUpwardExposedUsed)
    {LOGMEIN("IR.cpp] 925\n");
        this->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
    }
    this->byteCodeUpwardExposedUsed->Set(symId);
}

void ByteCodeUsesInstr::Set(IR::Opnd * originalOperand)
{LOGMEIN("IR.cpp] 932\n");
    Assert(originalOperand && originalOperand->GetStackSym());
    bool isJITOptimizedReg = originalOperand->GetIsJITOptimizedReg();
    SymID symId = originalOperand->GetStackSym()->m_id;
    if (isJITOptimizedReg)
    {
        AssertMsg(false, "Tried to add a jit-optimized register to a ByteCodeUses instruction!");
        // Although we assert on debug builds, we should actually be ok with release builds
        // if we ignore the operand; not ignoring it, however, can cause us to introduce an
        // inconsistency in bytecode register lifetimes.
        return;
    }
    if (!this->byteCodeUpwardExposedUsed)
    {LOGMEIN("IR.cpp] 945\n");
        this->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
    }
    this->byteCodeUpwardExposedUsed->Set(symId);
}

void ByteCodeUsesInstr::Clear(uint symId)
{LOGMEIN("IR.cpp] 952\n");
    Assert(byteCodeUpwardExposedUsed != nullptr);
    this->byteCodeUpwardExposedUsed->Clear(symId);
}

void ByteCodeUsesInstr::SetBV(BVSparse<JitArenaAllocator>* newbv)
{LOGMEIN("IR.cpp] 958\n");
    Assert(byteCodeUpwardExposedUsed == nullptr && newbv != nullptr);
    byteCodeUpwardExposedUsed = newbv;
}

// If possible, we want to aggregate with subsequent ByteCodeUses Instructions, so
// that we can do some optimizations in other places where we can simplify args in
// a compare, but still need to generate them for bailouts. Without this, we cause
// problems because we end up with an instruction losing atomicity in terms of its
// bytecode use and generation lifetimes.
void ByteCodeUsesInstr::Aggregate()
{LOGMEIN("IR.cpp] 969\n");
    IR::Instr* scanner = this->m_next;
    while (scanner && scanner->m_opcode == Js::OpCode::ByteCodeUses && scanner->GetByteCodeOffset() == this->GetByteCodeOffset() && scanner->GetDst() == nullptr)
    {LOGMEIN("IR.cpp] 972\n");
        IR::ByteCodeUsesInstr* target = scanner->AsByteCodeUsesInstr();
        Assert(this->m_func == target->m_func);
        if (target->byteCodeUpwardExposedUsed)
        {LOGMEIN("IR.cpp] 976\n");
            if (this->byteCodeUpwardExposedUsed)
            {LOGMEIN("IR.cpp] 978\n");
                this->byteCodeUpwardExposedUsed->Or(target->byteCodeUpwardExposedUsed);
                JitAdelete(target->byteCodeUpwardExposedUsed->GetAllocator(), target->byteCodeUpwardExposedUsed);
                target->byteCodeUpwardExposedUsed = nullptr;
            }
            else
            {
                this->byteCodeUpwardExposedUsed = target->byteCodeUpwardExposedUsed;
                target->byteCodeUpwardExposedUsed = nullptr;
            }
        }
        scanner = scanner->m_next;
    }
}

BailOutInfo *
Instr::GetBailOutInfo() const
{LOGMEIN("IR.cpp] 995\n");
    Assert(this->HasBailOutInfo() || this->HasAuxBailOut());
    switch (this->m_kind)
    {LOGMEIN("IR.cpp] 998\n");
    case InstrKindInstr:
        return ((BailOutInstr const *)this)->bailOutInfo;
    case InstrKindProfiled:
        return ((ProfiledBailOutInstr const *)this)->bailOutInfo;
    case InstrKindBranch:
        return ((BranchBailOutInstr const *)this)->bailOutInfo;
    default:
        Assert(false);
        __assume(false);
    }
}

BailOutKind
Instr::GetBailOutKind() const
{LOGMEIN("IR.cpp] 1013\n");
    Assert(this->HasBailOutInfo());
    switch (this->m_kind)
    {LOGMEIN("IR.cpp] 1016\n");
    case InstrKindInstr:
        return ((BailOutInstr const *)this)->bailOutKind;
    case InstrKindProfiled:
        return ((ProfiledBailOutInstr const *)this)->bailOutKind;
    case InstrKindBranch:
        return ((BranchBailOutInstr const *)this)->bailOutKind;
    default:
        Assert(false);
        return BailOutInvalid;
    }
}

BailOutKind
Instr::GetBailOutKindNoBits() const
{LOGMEIN("IR.cpp] 1031\n");
    return GetBailOutKind() & ~IR::BailOutKindBits;
}

BailOutKind
Instr::GetAuxBailOutKind() const
{LOGMEIN("IR.cpp] 1037\n");
    Assert(this->HasAuxBailOut());
    switch (this->m_kind)
    {LOGMEIN("IR.cpp] 1040\n");
    case InstrKindInstr:
        return ((BailOutInstr const *)this)->auxBailOutKind;
    case InstrKindProfiled:
        return ((ProfiledBailOutInstr const *)this)->auxBailOutKind;
    case InstrKindBranch:
        return ((BranchBailOutInstr const *)this)->auxBailOutKind;
    default:
        Assert(false);
        return BailOutInvalid;
    }
}

void Instr::SetBailOutKind(const IR::BailOutKind bailOutKind)
{LOGMEIN("IR.cpp] 1054\n");
    Assert(this->HasBailOutInfo());
    Assert(bailOutKind != IR::BailOutInvalid);
    this->SetBailOutKind_NoAssert(bailOutKind);
}

// Helper to set bail out kind, doesn't assert.
void Instr::SetBailOutKind_NoAssert(const IR::BailOutKind bailOutKind)
{LOGMEIN("IR.cpp] 1062\n");
    Assert(IsValidBailOutKindAndBits(bailOutKind));
    switch (this->m_kind)
    {LOGMEIN("IR.cpp] 1065\n");
    case InstrKindInstr:
        ((BailOutInstr *)this)->bailOutKind = bailOutKind;
        break;
    case InstrKindProfiled:
        ((ProfiledBailOutInstr *)this)->bailOutKind = bailOutKind;
        break;
    case InstrKindBranch:
        ((BranchBailOutInstr *)this)->bailOutKind = bailOutKind;
        break;
    default:
        Assert(false);
        __assume(false);
    }
}

void Instr::SetAuxBailOutKind(const IR::BailOutKind bailOutKind)
{LOGMEIN("IR.cpp] 1082\n");
    switch (this->m_kind)
    {LOGMEIN("IR.cpp] 1084\n");
    case InstrKindInstr:
        ((BailOutInstr *)this)->auxBailOutKind = bailOutKind;
        break;
    case InstrKindProfiled:
        ((ProfiledBailOutInstr *)this)->auxBailOutKind = bailOutKind;
        break;
    case InstrKindBranch:
        ((BranchBailOutInstr *)this)->auxBailOutKind = bailOutKind;
        break;
    default:
        Assert(false);
        __assume(false);
    }
}

BailOutInfo *
Instr::UnlinkBailOutInfo()
{LOGMEIN("IR.cpp] 1102\n");
    BailOutInfo *bailOutInfo;
    Assert(this->HasBailOutInfo() || this->HasAuxBailOut());

    switch (this->m_kind)
    {LOGMEIN("IR.cpp] 1107\n");
    case InstrKindInstr:
        bailOutInfo = ((BailOutInstr const *)this)->bailOutInfo;
        ((BailOutInstr *)this)->bailOutInfo = nullptr;
        break;
    case InstrKindProfiled:
        bailOutInfo = ((ProfiledBailOutInstr const *)this)->bailOutInfo;
        ((ProfiledBailOutInstr *)this)->bailOutInfo = nullptr;
        break;
    case InstrKindBranch:
        bailOutInfo = ((BranchBailOutInstr const *)this)->bailOutInfo;
        ((BranchBailOutInstr *)this)->bailOutInfo = nullptr;
        break;
    default:
        Assert(false);
        return nullptr;
    }
    Assert(bailOutInfo);
#if 0
    if (bailOutInfo->bailOutInstr == this)
    {LOGMEIN("IR.cpp] 1127\n");
        bailOutInfo->bailOutInstr = nullptr;
    }
#endif
    this->hasBailOutInfo = false;
    this->hasAuxBailOut = false;

    return bailOutInfo;
}

bool
Instr::ReplaceBailOutInfo(BailOutInfo *newBailOutInfo)
{LOGMEIN("IR.cpp] 1139\n");
    BailOutInfo *oldBailOutInfo;
    bool deleteOld = false;

#if DBG
    newBailOutInfo->wasCopied = true;
#endif

    Assert(this->HasBailOutInfo() || this->HasAuxBailOut());
    switch (this->m_kind)
    {LOGMEIN("IR.cpp] 1149\n");
    case InstrKindInstr:
        oldBailOutInfo = ((BailOutInstr *)this)->bailOutInfo;
        ((BailOutInstr *)this)->bailOutInfo = newBailOutInfo;
        break;
    case InstrKindProfiled:
        oldBailOutInfo = ((ProfiledBailOutInstr *)this)->bailOutInfo;
        ((ProfiledBailOutInstr *)this)->bailOutInfo = newBailOutInfo;
        break;
    case InstrKindBranch:
        AssertMsg(!this->HasBailOutInfo() && this->HasAuxBailOut(), "ReplaceBailOutInfo is not used with InstrKindBranch for non-aux bailout");
        oldBailOutInfo = ((BranchBailOutInstr *)this)->bailOutInfo;
        ((BranchBailOutInstr *)this)->bailOutInfo = newBailOutInfo;
        break;
    default:
        Assert(false);
        __assume(UNREACHED);
    }
    Assert(!oldBailOutInfo->wasCloned && !oldBailOutInfo->wasCopied);
    if (oldBailOutInfo->bailOutInstr == this)
    {LOGMEIN("IR.cpp] 1169\n");
        JitArenaAllocator * alloc = this->m_func->m_alloc;
        oldBailOutInfo->Clear(alloc);
        JitAdelete(alloc, oldBailOutInfo);
        deleteOld = true;
    }

    return deleteOld;
}

IR::Instr *Instr::ShareBailOut()
{LOGMEIN("IR.cpp] 1180\n");
    BailOutInfo *const bailOutInfo = GetBailOutInfo();
    bailOutInfo->bailOutInstr = nullptr;
#if DBG
    bailOutInfo->wasCopied = true;
#endif
    IR::Instr *const sharedBail =
        IR::BailOutInstr::New(Js::OpCode::BailTarget, IR::BailOutShared, bailOutInfo, bailOutInfo->bailOutFunc);
    sharedBail->SetByteCodeOffset(this);
    InsertAfter(sharedBail);
    Assert(bailOutInfo->bailOutInstr == sharedBail);
    return sharedBail;
}

void
Instr::UnlinkStartCallFromBailOutInfo(IR::Instr *endInstr) const
{LOGMEIN("IR.cpp] 1196\n");
#ifdef _M_IX86
    // The StartCall instruction is being deleted, or is being moved and may later be deleted,
    // so remove its references from bailouts in the given range.
    // This only happens during cloning, which is rare, and only across the range of instructions
    // that evaluate outgoing arguments, which is long only in synthetic cases.

    Assert(this->m_opcode == Js::OpCode::StartCall);

    if (!this->m_func->hasBailout)
    {LOGMEIN("IR.cpp] 1206\n");
        return;
    }

    FOREACH_INSTR_IN_RANGE(instr, this->m_next, endInstr)
    {LOGMEIN("IR.cpp] 1211\n");
        if (instr->HasBailOutInfo())
        {LOGMEIN("IR.cpp] 1213\n");
            BailOutInfo *bailOutInfo = instr->GetBailOutInfo();
            bailOutInfo->UnlinkStartCall(this);
        }
    }
    NEXT_INSTR_IN_RANGE;
#endif
}

Opnd *Instr::FindCallArgumentOpnd(const Js::ArgSlot argSlot, IR::Instr * *const ownerInstrRef)
{LOGMEIN("IR.cpp] 1223\n");
    Assert(OpCodeAttr::CallInstr(m_opcode));
    Assert(argSlot != static_cast<Js::ArgSlot>(0));

    IR::Instr *argInstr = this;
    Assert(argInstr->GetSrc2());
    Assert(argInstr->GetSrc2()->IsSymOpnd());
    do
    {LOGMEIN("IR.cpp] 1231\n");
        StackSym *const linkSym = argInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym();
        Assert(linkSym->IsSingleDef());
        Assert(linkSym->IsArgSlotSym());

        argInstr = linkSym->m_instrDef;
        Assert(argInstr->GetSrc2());
        if(argInstr->m_opcode == Js::OpCode::ArgOut_A_InlineSpecialized)
        {LOGMEIN("IR.cpp] 1239\n");
            // This is a fake ArgOut, skip it
            continue;
        }

        if(linkSym->GetArgSlotNum() == argSlot)
        {LOGMEIN("IR.cpp] 1245\n");
            if(ownerInstrRef)
            {LOGMEIN("IR.cpp] 1247\n");
                *ownerInstrRef = argInstr;
            }
            return argInstr->GetSrc1();
        }
    } while(argInstr->GetSrc2()->IsSymOpnd());
    return nullptr;
}


bool
Instr::FetchOperands(_Out_writes_(argsOpndLength) IR::Opnd **argsOpnd, uint argsOpndLength)
{LOGMEIN("IR.cpp] 1259\n");
    return this->ForEachCallDirectArgOutInstrBackward([&](IR::Instr *argOutInstr, uint argNum)
    {
        argsOpnd[argNum] = argOutInstr->GetSrc1();
        return argNum == 0;
    }, argsOpndLength);
}

bool Instr::ShouldCheckForNegativeZero() const
{LOGMEIN("IR.cpp] 1268\n");
    return !ignoreNegativeZero;
}

bool Instr::IsDstNotAlwaysConvertedToInt32() const
{LOGMEIN("IR.cpp] 1273\n");
    return !dstIsAlwaysConvertedToInt32;
}

bool Instr::IsDstNotAlwaysConvertedToNumber() const
{LOGMEIN("IR.cpp] 1278\n");
    return !dstIsAlwaysConvertedToNumber;
}

bool Instr::ShouldCheckForIntOverflow() const
{LOGMEIN("IR.cpp] 1283\n");
    return ShouldCheckFor32BitOverflow() || ShouldCheckForNon32BitOverflow();
}

bool Instr::ShouldCheckFor32BitOverflow() const
{LOGMEIN("IR.cpp] 1288\n");
    return !(ignoreIntOverflow || ignoreIntOverflowInRange);
}

bool Instr::ShouldCheckForNon32BitOverflow() const
{LOGMEIN("IR.cpp] 1293\n");
    return ignoreOverflowBitCount != 32;
}

template <typename InstrType> struct IRKindMap;

template <> struct IRKindMap<IR::Instr> { static const IRKind InstrKind = InstrKindInstr; };
template <> struct IRKindMap<IR::ProfiledInstr> { static const IRKind InstrKind = InstrKindProfiled; };
template <> struct IRKindMap<IR::BranchInstr> { static const IRKind InstrKind = InstrKindBranch; };

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::New(Js::OpCode opcode, BailOutKind kind, IR::Instr * bailOutTarget, Func * func)
{LOGMEIN("IR.cpp] 1306\n");
    Assert(func == bailOutTarget->m_func);
    BailOutInfo * bailOutInfo = JitAnew(func->m_alloc, BailOutInfo, bailOutTarget->GetByteCodeOffset(), func);
#if ENABLE_DEBUG_CONFIG_OPTIONS
    bailOutInfo->bailOutOpcode = opcode;
#endif
    return BailOutInstrTemplate::New(opcode, kind, bailOutInfo, func);
}

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::New(Js::OpCode opcode, IR::Opnd *dst, BailOutKind kind, IR::Instr * bailOutTarget, Func * func)
{LOGMEIN("IR.cpp] 1318\n");
    BailOutInstrTemplate *instr = BailOutInstrTemplate::New(opcode, kind, bailOutTarget, func);
    instr->SetDst(dst);

    return instr;
}

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::New(Js::OpCode opcode, IR::Opnd *dst, IR::Opnd *src1, BailOutKind kind, IR::Instr * bailOutTarget, Func * func)
{LOGMEIN("IR.cpp] 1328\n");
    BailOutInstrTemplate *instr = BailOutInstrTemplate::New(opcode, dst, kind, bailOutTarget, func);
    instr->SetSrc1(src1);

    return instr;
}

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::New(Js::OpCode opcode, IR::Opnd *dst, IR::Opnd *src1, IR::Opnd *src2, BailOutKind kind, IR::Instr * bailOutTarget, Func * func)
{LOGMEIN("IR.cpp] 1338\n");
    BailOutInstrTemplate *instr = BailOutInstrTemplate::New(opcode, dst, src1, kind, bailOutTarget, func);
    instr->SetSrc2(src2);

    return instr;
}

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::New(Js::OpCode opcode, BailOutKind kind, BailOutInfo * bailOutInfo, Func * func)
{LOGMEIN("IR.cpp] 1348\n");
    Assert(func == bailOutInfo->bailOutFunc);
    Assert(IsValidBailOutKindAndBits(kind));
    BailOutInstrTemplate * bailOutInstr = JitAnew(func->m_alloc, BailOutInstrTemplate);
    bailOutInstr->Init(opcode, IRKindMap<InstrType>::InstrKind, func);
#if ENABLE_DEBUG_CONFIG_OPTIONS
    bailOutInfo->bailOutOpcode = opcode;
#endif
    bailOutInstr->bailOutInfo = bailOutInfo;
    bailOutInstr->bailOutKind = kind;
    bailOutInstr->auxBailOutKind = BailOutInvalid;

    if (bailOutInfo->bailOutInstr == nullptr)
    {LOGMEIN("IR.cpp] 1361\n");
        bailOutInfo->bailOutInstr = bailOutInstr;
    }
    else if (bailOutInfo->sharedBailOutKind)
    {LOGMEIN("IR.cpp] 1365\n");
        if (bailOutInfo->bailOutInstr->HasBailOutInfo())
        {LOGMEIN("IR.cpp] 1367\n");
            bailOutInfo->sharedBailOutKind = bailOutInfo->bailOutInstr->GetBailOutKind() ==  kind;
        }
        else
        {
            // Rare cases where we have already generated the bailout record. Unlikely they share the same bailout kind as this is hit only when we try to
            // share bailout in lowerer. See Instr::ShareBailOut.
            bailOutInfo->sharedBailOutKind = false;
        }
    }

    func->hasBailout = true;

    // Indicate that the function has bailout instructions
    // This information is used to determine whether to free jitted loop bodies
    // If the function has bailout instructions, we keep the loop bodies alive
    // in case we bail out to the interpreter, so that we can reuse the jitted
    // loop bodies
    func->GetJITOutput()->SetHasBailoutInstr(true);

    return bailOutInstr;
}

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::CloneBailOut() const
{LOGMEIN("IR.cpp] 1393\n");
    Assert(this->m_func->hasBailout);
    Assert(!this->bailOutInfo->wasCloned);

    BailOutInstrTemplate * bailOutInstr = BailOutInstrTemplate::New(this->m_opcode, this->bailOutKind, this->bailOutInfo, this->bailOutInfo->bailOutFunc);
    bailOutInstr->hasAuxBailOut = this->hasAuxBailOut;
    bailOutInstr->auxBailOutKind = this->auxBailOutKind;
    bailOutInstr->bailOutInfo->wasCloned = true;

    // the new copy is in the slow path and generate the real bailout
    bailOutInstr->bailOutInfo->bailOutInstr = bailOutInstr;

    return bailOutInstr;
}

template class BailOutInstrTemplate<IR::Instr>;

///----------------------------------------------------------------------------
///
/// EntryInstr::New
///
///     Create an EntryInstr.
///
///----------------------------------------------------------------------------

EntryInstr *
EntryInstr::New(Js::OpCode opcode, Func *func)
{LOGMEIN("IR.cpp] 1420\n");
    EntryInstr * entryInstr;

    entryInstr = JitAnew(func->m_alloc, IR::EntryInstr);
    entryInstr->Init(opcode, InstrKindEntry, func);
    return entryInstr;
}

///----------------------------------------------------------------------------
///
/// ExitInstr::New
///
///     Create an ExitInstr.
///
///----------------------------------------------------------------------------

ExitInstr *
ExitInstr::New(Js::OpCode opcode, Func *func)
{LOGMEIN("IR.cpp] 1438\n");
    ExitInstr * exitInstr;

    exitInstr = JitAnew(func->m_alloc, IR::ExitInstr);
    exitInstr->Init(opcode, InstrKindExit, func);
    return exitInstr;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::New
///
///     Create a label.
///
///----------------------------------------------------------------------------

LabelInstr *
LabelInstr::New(Js::OpCode opcode, Func *func, bool isOpHelper)
{LOGMEIN("IR.cpp] 1456\n");
    LabelInstr * labelInstr;

    labelInstr = JitAnew(func->m_alloc, IR::LabelInstr, func->m_alloc);
    labelInstr->Init(opcode, InstrKindLabel, func, isOpHelper);
    return labelInstr;
}

void
LabelInstr::Init(Js::OpCode opcode, IRKind kind, Func *func, bool isOpHelper)
{LOGMEIN("IR.cpp] 1466\n");
    // Pass in the region when this is called from anywhere between the Lowerer and EHBailoutPatchUp code?
    __super::Init(opcode, kind, func);
    this->isOpHelper = isOpHelper;

    this->m_pc.pc = nullptr;
    this->m_id = ++(func->GetTopFunc()->m_labelCount);
    AssertMsg(this->m_id != 0, "Label numbers wrapped around?");
}

///----------------------------------------------------------------------------
///
/// LabelInstr::AddLabelRef
///
///     Add a branch to the list of label references.
///
///----------------------------------------------------------------------------

void
LabelInstr::AddLabelRef(BranchInstr *branchRef)
{LOGMEIN("IR.cpp] 1486\n");
    this->labelRefs.Prepend(branchRef);
}

///----------------------------------------------------------------------------
///
/// LabelInstr::RemoveLabelRef
///
///     Remove a branch from the list of label references.
///
///----------------------------------------------------------------------------

void
LabelInstr::RemoveLabelRef(BranchInstr *branchRef)
{
    FOREACH_SLISTCOUNTED_ENTRY_EDITING(BranchInstr*, branchEntry, &this->labelRefs, iter)
    {LOGMEIN("IR.cpp] 1502\n");
        if (branchEntry == branchRef)
        {LOGMEIN("IR.cpp] 1504\n");
            iter.RemoveCurrent();
            return;
        }
    } NEXT_SLISTCOUNTED_ENTRY_EDITING;

    AssertMsg(UNREACHED, "Branch not found on labelRef list");
}

///----------------------------------------------------------------------------
///
/// BranchInstr::New
///
///     Create a Br (unconditional) BranchInstr.
///
///----------------------------------------------------------------------------

BranchInstr *
BranchInstr::New(Js::OpCode opcode, LabelInstr * branchTarget, Func *func)
{LOGMEIN("IR.cpp] 1523\n");
    BranchInstr * branchInstr;

    branchInstr = JitAnew(func->m_alloc, IR::BranchInstr);
    branchInstr->Init(opcode, InstrKindBranch, func);
    branchInstr->SetTarget(branchTarget);
    branchInstr->m_dst = nullptr;
    branchInstr->m_src1 = nullptr;
    branchInstr->m_src2 = nullptr;
    branchInstr->m_byteCodeReg = Js::Constants::NoRegister;
#if DBG
    branchInstr->m_isHelperToNonHelperBranch = false;
#endif

    return branchInstr;
}

///----------------------------------------------------------------------------
///
/// BranchInstr::New
///
///     Create a BrB BranchInstr (1-operand conditional branch).
///
///----------------------------------------------------------------------------

BranchInstr *
BranchInstr::New(Js::OpCode opcode, LabelInstr * branchTarget, Opnd *srcOpnd, Func *func)
{LOGMEIN("IR.cpp] 1550\n");
    BranchInstr * branchInstr;

    branchInstr = BranchInstr::New(opcode, branchTarget, func);

    branchInstr->SetSrc1(srcOpnd);

    return branchInstr;
}


///----------------------------------------------------------------------------
///
/// BranchInstr::New
///
///     Create a BrBReturn BranchInstr (1-operand conditional branch. If condition fails return the result of the condition).
///
///----------------------------------------------------------------------------

BranchInstr *
BranchInstr::New(Js::OpCode opcode, Opnd* destOpnd, LabelInstr * branchTarget, Opnd *srcOpnd, Func *func)
{LOGMEIN("IR.cpp] 1571\n");
    BranchInstr * branchInstr;

    branchInstr = BranchInstr::New(opcode, branchTarget, func);

    branchInstr->SetSrc1(srcOpnd);
    branchInstr->SetDst(destOpnd);

    return branchInstr;
}

///----------------------------------------------------------------------------
///
/// BranchInstr::New
///
///     Create a BrReg2 BranchInstr (2-operand conditional branch).
///
///----------------------------------------------------------------------------

BranchInstr *
BranchInstr::New(Js::OpCode opcode, LabelInstr * branchTarget, Opnd *src1Opnd, Opnd *src2Opnd, Func *func)
{LOGMEIN("IR.cpp] 1592\n");
    BranchInstr * branchInstr;

    branchInstr = BranchInstr::New(opcode, branchTarget, src1Opnd, func);

    branchInstr->SetSrc2(src2Opnd);

    return branchInstr;
}

///----------------------------------------------------------------------------
///
/// MultiBranchInstr::New
///
///     Create a MultiBr BranchInstr (unconditional multi branch).
///
///----------------------------------------------------------------------------

MultiBranchInstr *
MultiBranchInstr::New(Js::OpCode opcode, IR::Opnd * srcOpnd, Func * func)
{LOGMEIN("IR.cpp] 1612\n");
   MultiBranchInstr * multiBranchInstr;

   multiBranchInstr = MultiBranchInstr::New(opcode, func);

   multiBranchInstr->SetSrc1(srcOpnd);

   return multiBranchInstr;
}

MultiBranchInstr *
MultiBranchInstr::New(Js::OpCode opcode, Func * func)
{LOGMEIN("IR.cpp] 1624\n");
   JitArenaAllocator * m_funcAlloc = func->m_alloc;

   MultiBranchInstr * multiBranchInstr;

   multiBranchInstr = JitAnew(m_funcAlloc, IR::MultiBranchInstr);

   multiBranchInstr->Init(opcode, InstrKindBranch, func);

   return multiBranchInstr;
}

bool
BranchInstr::ReplaceTarget(IR::LabelInstr * oldLabelInstr, IR::LabelInstr * newLabelInstr)
{LOGMEIN("IR.cpp] 1638\n");
    if (this->IsMultiBranch())
    {LOGMEIN("IR.cpp] 1640\n");
        return this->AsMultiBrInstr()->ReplaceTarget(oldLabelInstr, newLabelInstr);
    }
    if (this->GetTarget() == oldLabelInstr)
    {LOGMEIN("IR.cpp] 1644\n");
        this->SetTarget(newLabelInstr);
        return true;
    }
    return false;
}

bool
MultiBranchInstr::ReplaceTarget(IR::LabelInstr * oldLabelInstr, IR::LabelInstr * newLabelInstr)
{LOGMEIN("IR.cpp] 1653\n");
    Assert(this->IsMultiBranch());
    bool remapped = false;
    this->UpdateMultiBrLabels([=, &remapped](IR::LabelInstr * targetLabel) -> IR::LabelInstr *
    {
        if (targetLabel == oldLabelInstr)
        {LOGMEIN("IR.cpp] 1659\n");
            this->ChangeLabelRef(targetLabel, newLabelInstr);
            remapped = true;
            return newLabelInstr;
        }
        return targetLabel;
    });
    return remapped;
}

void
MultiBranchInstr::ClearTarget()
{LOGMEIN("IR.cpp] 1671\n");
    Assert(IsMultiBranch());

    MapMultiBrLabels([&](LabelInstr *const targetLabel)
    {
        ChangeLabelRef(targetLabel, nullptr);
    });
    m_branchTargets = nullptr;
}

BranchInstr *
BranchInstr::CloneBranchInstr() const
{LOGMEIN("IR.cpp] 1683\n");
    AssertMsg(!this->IsMultiBranch(),"Cloning Not supported for MultiBranchInstr");
    Func * func = this->m_func;
    // See if the target has already been cloned.
    IR::LabelInstr * instrLabel = this->GetTarget()->CloneLabel(false);
    if (instrLabel == nullptr)
    {LOGMEIN("IR.cpp] 1689\n");
        // We didn't find a clone for this label.
        // We'll go back and retarget the cloned branch if the target turns up in the cloned range.
        instrLabel = this->GetTarget();
        func->GetCloner()->fRetargetClonedBranch = TRUE;
    }
    return IR::BranchInstr::New(this->m_opcode, instrLabel, func);
}

void
BranchInstr::Invert()
{LOGMEIN("IR.cpp] 1700\n");
    /*
     * If one of the operands to a relational operator is 'undefined', the result
     * is always false. Don't invert such branches as they result in a jump to
     * the wrong target.
     */

    switch (this->m_opcode)
    {LOGMEIN("IR.cpp] 1708\n");
    case Js::OpCode::BrGt_A:
        this->m_opcode = Js::OpCode::BrNotGt_A;
        break;

    case Js::OpCode::BrNotGt_A:
        this->m_opcode = Js::OpCode::BrGt_A;
        break;

    case Js::OpCode::BrGe_A:
        this->m_opcode = Js::OpCode::BrNotGe_A;
        break;

    case Js::OpCode::BrNotGe_A:
        this->m_opcode = Js::OpCode::BrGe_A;
        break;

    case Js::OpCode::BrLt_A:
        this->m_opcode = Js::OpCode::BrNotLt_A;
        break;

    case Js::OpCode::BrNotLt_A:
        this->m_opcode = Js::OpCode::BrLt_A;
        break;

    case Js::OpCode::BrLe_A:
        this->m_opcode = Js::OpCode::BrNotLe_A;
        break;

    case Js::OpCode::BrNotLe_A:
        this->m_opcode = Js::OpCode::BrLe_A;
        break;

    case Js::OpCode::BrEq_A:
        this->m_opcode = Js::OpCode::BrNotEq_A;
        break;

    case Js::OpCode::BrNotEq_A:
        this->m_opcode = Js::OpCode::BrEq_A;
        break;

    case Js::OpCode::BrNeq_A:
        this->m_opcode = Js::OpCode::BrNotNeq_A;
        break;

    case Js::OpCode::BrNotNeq_A:
        this->m_opcode = Js::OpCode::BrNeq_A;
        break;

    case Js::OpCode::Br:
        break;

    case Js::OpCode::BrFalse_A:
        this->m_opcode = Js::OpCode::BrTrue_A;
        break;

    case Js::OpCode::BrTrue_A:
        this->m_opcode = Js::OpCode::BrFalse_A;
        break;

    case Js::OpCode::BrSrEq_A:
        this->m_opcode = Js::OpCode::BrSrNotEq_A;
        break;

    case Js::OpCode::BrSrNotEq_A:
        this->m_opcode = Js::OpCode::BrSrEq_A;
        break;

    case Js::OpCode::BrSrNeq_A:
        this->m_opcode = Js::OpCode::BrSrNotNeq_A;
        break;

    case Js::OpCode::BrSrNotNeq_A:
        this->m_opcode = Js::OpCode::BrSrNeq_A;
        break;

    case Js::OpCode::BrOnHasProperty:
        this->m_opcode = Js::OpCode::BrOnNoProperty;
        break;

    case Js::OpCode::BrOnNoProperty:
        this->m_opcode = Js::OpCode::BrOnHasProperty;
        break;

    case Js::OpCode::BrTrue_I4:
        this->m_opcode = Js::OpCode::BrFalse_I4;
        break;

    case Js::OpCode::BrFalse_I4:
        this->m_opcode = Js::OpCode::BrTrue_I4;
        break;

    case Js::OpCode::BrEq_I4:
        this->m_opcode = Js::OpCode::BrNeq_I4;
        break;

    case Js::OpCode::BrNeq_I4:
        this->m_opcode = Js::OpCode::BrEq_I4;
        break;

    case Js::OpCode::BrGe_I4:
        this->m_opcode = Js::OpCode::BrLt_I4;
        break;

    case Js::OpCode::BrGt_I4:
        this->m_opcode = Js::OpCode::BrLe_I4;
        break;

    case Js::OpCode::BrLe_I4:
        this->m_opcode = Js::OpCode::BrGt_I4;
        break;

    case Js::OpCode::BrLt_I4:
        this->m_opcode = Js::OpCode::BrGe_I4;
        break;

    case Js::OpCode::BrUnGe_A:
        this->m_opcode = Js::OpCode::BrUnLt_A;
        break;

    case Js::OpCode::BrUnGt_A:
        this->m_opcode = Js::OpCode::BrUnLe_A;
        break;

    case Js::OpCode::BrUnLe_A:
        this->m_opcode = Js::OpCode::BrUnGt_A;
        break;

    case Js::OpCode::BrUnLt_A:
        this->m_opcode = Js::OpCode::BrUnGe_A;
        break;
    case Js::OpCode::BrUnGe_I4:
        this->m_opcode = Js::OpCode::BrUnLt_I4;
        break;

    case Js::OpCode::BrUnGt_I4:
        this->m_opcode = Js::OpCode::BrUnLe_I4;
        break;

    case Js::OpCode::BrUnLe_I4:
        this->m_opcode = Js::OpCode::BrUnGt_I4;
        break;

    case Js::OpCode::BrUnLt_I4:
        this->m_opcode = Js::OpCode::BrUnGe_I4;
        break;
    case Js::OpCode::BrOnEmpty:
        this->m_opcode = Js::OpCode::BrOnNotEmpty;
        break;

    case Js::OpCode::BrOnNotEmpty:
        this->m_opcode = Js::OpCode::BrOnEmpty;
        break;
    case Js::OpCode::BrHasSideEffects:
        this->m_opcode = Js::OpCode::BrNotHasSideEffects;
        break;
    case Js::OpCode::BrFncEqApply:
        this->m_opcode = Js::OpCode::BrFncNeqApply;
        break;
    case Js::OpCode::BrFncNeqApply:
        this->m_opcode = Js::OpCode::BrFncEqApply;
        break;
    case Js::OpCode::BrNotHasSideEffects:
        this->m_opcode = Js::OpCode::BrHasSideEffects;
        break;
    case Js::OpCode::BrNotAddr_A:
        this->m_opcode = Js::OpCode::BrAddr_A;
        break;
    case Js::OpCode::BrAddr_A:
        this->m_opcode = Js::OpCode::BrNotAddr_A;
        break;
    case Js::OpCode::BrFncCachedScopeEq:
        this->m_opcode = Js::OpCode::BrFncCachedScopeNeq;
        break;
    case Js::OpCode::BrFncCachedScopeNeq:
        this->m_opcode = Js::OpCode::BrFncCachedScopeEq;
        break;
    case Js::OpCode::BrOnException:
        this->m_opcode = Js::OpCode::BrOnNoException;
        break;

    default:
        AssertMsg(UNREACHED, "Unhandled branch");
    }
}

bool
BranchInstr::IsLoopTail(Func * func)
{LOGMEIN("IR.cpp] 1896\n");
    Assert(func->isPostLower);
    IR::LabelInstr * target = this->GetTarget();
    if (!target->m_isLoopTop)
    {LOGMEIN("IR.cpp] 1900\n");
        return false;
    }

    IR::BranchInstr * lastBranchInstr = nullptr;
    uint32 lastBranchNum = 0;
    FOREACH_SLISTCOUNTED_ENTRY(IR::BranchInstr *, ref, &target->labelRefs)
    {LOGMEIN("IR.cpp] 1907\n");
        if (ref->GetNumber() > lastBranchNum)
        {LOGMEIN("IR.cpp] 1909\n");
            lastBranchInstr = ref;
            lastBranchNum = lastBranchInstr->GetNumber();
        }
    }
    NEXT_SLISTCOUNTED_ENTRY;

    if (this == lastBranchInstr)
    {LOGMEIN("IR.cpp] 1917\n");
        return true;
    }
    return false;
}

///----------------------------------------------------------------------------
///
/// PragmaInstr::New
///
///     Create a PragmaInstr.
///
///----------------------------------------------------------------------------

PragmaInstr *
PragmaInstr::New(Js::OpCode opcode, uint32 index, Func *func)
{LOGMEIN("IR.cpp] 1933\n");
    PragmaInstr * pragmaInstr;

    pragmaInstr = JitAnew(func->m_alloc, IR::PragmaInstr);
    pragmaInstr->Init(opcode, InstrKindPragma, func);
    pragmaInstr->m_statementIndex = index;

    return pragmaInstr;
}

///----------------------------------------------------------------------------
///
/// PragmaInstr::Instr
///
///     Record the information encoded in the pragma
///
///----------------------------------------------------------------------------

#if DBG_DUMP | defined(VTUNE_PROFILING)
void
PragmaInstr::Record(uint32 nativeBufferOffset)
{LOGMEIN("IR.cpp] 1954\n");
    // Currently the only pragma instructions are for Source Info
    Assert(this->m_func->GetTopFunc()->DoRecordNativeMap());
    if (!m_func->IsOOPJIT())
    {LOGMEIN("IR.cpp] 1958\n");
        m_func->GetTopFunc()->GetInProcJITEntryPointInfo()->RecordNativeMap(nativeBufferOffset, m_statementIndex);
    }
}
#endif

///----------------------------------------------------------------------------
///
/// Instr::New
///
///     Create an Instr.
///
///----------------------------------------------------------------------------

Instr *
Instr::New(Js::OpCode opcode, Func *func)
{LOGMEIN("IR.cpp] 1974\n");
    Instr * instr;

    instr = JitAnew(func->m_alloc, IR::Instr);
    instr->Init(opcode, InstrKindInstr, func);
    return instr;
}

///----------------------------------------------------------------------------
///
/// Instr::New
///
///     Create an Instr with dst.
///
///----------------------------------------------------------------------------

Instr *
Instr::New(Js::OpCode opcode, Opnd *dstOpnd, Func *func)
{LOGMEIN("IR.cpp] 1992\n");
    Instr * instr;

    instr = Instr::New(opcode, func);
    instr->SetDst(dstOpnd);

    return instr;
}

///----------------------------------------------------------------------------
///
/// Instr::New
///
///     Create an Instr with dst and a src.
///
///----------------------------------------------------------------------------

Instr *
Instr::New(Js::OpCode opcode, Opnd *dstOpnd, Opnd *src1Opnd, Func *func)
{LOGMEIN("IR.cpp] 2011\n");
    Instr * instr;

    instr = Instr::New(opcode, dstOpnd, func);
    instr->SetSrc1(src1Opnd);

    return instr;
}

///----------------------------------------------------------------------------
///
/// Instr::New
///
///     Create an Instr with dst and 2 srcs.
///
///----------------------------------------------------------------------------

Instr *
Instr::New(Js::OpCode opcode, Opnd *dstOpnd, Opnd *src1Opnd, Opnd *src2Opnd, Func *func)
{LOGMEIN("IR.cpp] 2030\n");
    Instr * instr;

    instr = Instr::New(opcode, dstOpnd, src1Opnd, func);
    instr->SetSrc2(src2Opnd);

    return instr;
}

///----------------------------------------------------------------------------
///
/// Instr::SetDst
///
///     Set the dst for 'this' instruction.  Automatically maintain isSingleDef
///     and instrDef of stackSyms.
///
///----------------------------------------------------------------------------

Opnd *
Instr::SetDst(Opnd * newDst)
{LOGMEIN("IR.cpp] 2050\n");
    AssertMsg(newDst != nullptr, "Calling SetDst with a NULL dst");
    AssertMsg(this->m_dst == nullptr, "Calling SetDst without unlinking/freeing the current dst");
    Assert(!(newDst->IsRegOpnd() && newDst->AsRegOpnd()->IsSymValueFrozen()));

    newDst = newDst->Use(m_func);
    this->m_dst = newDst;

    // If newDst isSingleDef, set instrDef

    StackSym *stackSym;

    if (newDst->IsRegOpnd() && newDst->AsRegOpnd()->m_sym)
    {LOGMEIN("IR.cpp] 2063\n");
        stackSym = newDst->AsRegOpnd()->m_sym->AsStackSym();
    }
    else if (newDst->IsSymOpnd() && newDst->AsSymOpnd()->m_sym->IsStackSym())
    {LOGMEIN("IR.cpp] 2067\n");
        stackSym = newDst->AsSymOpnd()->m_sym->AsStackSym();
    }
    else
    {
        stackSym = nullptr;
    }

    if (stackSym && stackSym->m_isSingleDef)
    {LOGMEIN("IR.cpp] 2076\n");
        if (stackSym->m_instrDef)
        {LOGMEIN("IR.cpp] 2078\n");
            AssertMsg(!stackSym->IsArgSlotSym(), "Arg Slot sym needs to be single def to maintain the StartCall arg links");

            // Multiple defs, clear isSingleDef flag
            stackSym->m_isSingleDef = false;
            stackSym->m_instrDef    = nullptr;
            stackSym->m_isConst     = false;
            stackSym->m_isIntConst  = false;
            stackSym->m_isInt64Const= false;
            stackSym->m_isTaggableIntConst  = false;
            stackSym->m_isNotInt    = false;
            stackSym->m_isStrConst  = false;
            stackSym->m_isStrEmpty  = false;
            stackSym->m_isFltConst  = false;
        }
        else
        {
            stackSym->m_instrDef = this;
        }
    }

    return newDst;
}

Opnd *
Instr::SetFakeDst(Opnd * newDst)
{LOGMEIN("IR.cpp] 2104\n");
    AssertMsg(newDst != nullptr, "Calling SetDst with a NULL dst");
    AssertMsg(this->m_dst == nullptr, "Calling SetDst without unlinking/freeing the current dst");
    Assert(!(newDst->IsRegOpnd() && newDst->AsRegOpnd()->IsSymValueFrozen()));

    newDst = newDst->Use(m_func);
    this->m_dst = newDst;

#if DBG
    newDst->isFakeDst = true;
#endif
    return newDst;
}

///----------------------------------------------------------------------------
///
/// Instr::UnlinkDst
///
///     Unlinks the dst for 'this' instruction.  Automatically maintains
///     instrDef of stackSyms.
///
///----------------------------------------------------------------------------

Opnd *
Instr::UnlinkDst()
{LOGMEIN("IR.cpp] 2129\n");
    Opnd * oldDst = this->m_dst;
    StackSym *stackSym = nullptr;

    // If oldDst isSingleDef, clear instrDef

    if (oldDst->IsRegOpnd())
    {LOGMEIN("IR.cpp] 2136\n");
        stackSym = oldDst->AsRegOpnd()->m_sym;
    }
    else if (oldDst->IsSymOpnd())
    {LOGMEIN("IR.cpp] 2140\n");
        Sym *sym = oldDst->AsSymOpnd()->m_sym;
        if (sym->IsStackSym())
        {LOGMEIN("IR.cpp] 2143\n");
            stackSym = sym->AsStackSym();
        }
    }

#if DBG
    if (oldDst->isFakeDst)
    {LOGMEIN("IR.cpp] 2150\n");
        oldDst->isFakeDst = false;
    }
#endif
    if (stackSym && stackSym->m_isSingleDef)
    {LOGMEIN("IR.cpp] 2155\n");
        AssertMsg(stackSym->m_instrDef == this, "m_instrDef incorrectly set");
        stackSym->m_instrDef = nullptr;
    }

    oldDst->UnUse();
    this->m_dst = nullptr;

    return oldDst;
}

///----------------------------------------------------------------------------
///
/// Instr::FreeDst
///
///     Unlinks and free the dst for 'this' instruction.
///
///----------------------------------------------------------------------------

void
Instr::FreeDst()
{LOGMEIN("IR.cpp] 2176\n");
    Opnd * unlinkedDst;
    unlinkedDst = this->UnlinkDst();
    unlinkedDst->Free(this->m_func);
}

///----------------------------------------------------------------------------
///
/// Instr::ReplaceDst
///
///     Unlink this dst from this instr, free it, and replace it with newDst.
///     The new dst is returned.
///
///----------------------------------------------------------------------------

Opnd *
Instr::ReplaceDst(Opnd * newDst)
{LOGMEIN("IR.cpp] 2193\n");
    this->FreeDst();
    return this->SetDst(newDst);
}

///----------------------------------------------------------------------------
///
/// Instr::SinkDst
///
///     Replace current dst with new symbol, and assign new symbol using the
///     given opcode to the previous dst.
///
///----------------------------------------------------------------------------

Instr *
Instr::SinkDst(Js::OpCode assignOpcode, RegNum regNum, IR::Instr *insertAfterInstr)
{LOGMEIN("IR.cpp] 2209\n");
    return SinkDst(assignOpcode, StackSym::New(TyVar, m_func), regNum, insertAfterInstr);
}

Instr *
Instr::SinkDst(Js::OpCode assignOpcode, StackSym * stackSym, RegNum regNum, IR::Instr *insertAfterInstr)
{LOGMEIN("IR.cpp] 2215\n");
    if(!insertAfterInstr)
    {LOGMEIN("IR.cpp] 2217\n");
        insertAfterInstr = this;
    }

    Opnd *oldDst, *newDst;
    Instr * newInstr;
    IRType type;

    oldDst = this->UnlinkDst();
    type = oldDst->GetType();
    newDst = this->SetDst(RegOpnd::New(stackSym, regNum, type, m_func));
    newInstr = Instr::New(assignOpcode, oldDst, newDst, m_func);
    insertAfterInstr->InsertAfter(newInstr);

    return newInstr;
}

IR::Instr *
Instr::SinkInstrBefore(IR::Instr * instrTarget)
{LOGMEIN("IR.cpp] 2236\n");
    // Move this instruction down to the target location, preserving
    // the use(s), if necessary, from redefinition between the original
    // location and the new one.

    if (this->m_next == instrTarget)
    {LOGMEIN("IR.cpp] 2242\n");
        return this->m_prev;
    }

    StackSym *sym;
    if (this->m_src1)
    {LOGMEIN("IR.cpp] 2248\n");
        sym = this->m_src1->GetStackSym();
        if (sym && !sym->m_isSingleDef)
        {LOGMEIN("IR.cpp] 2251\n");
            this->HoistSrc1(Js::OpCode::Ld_A);
        }

        if (this->m_src2)
        {LOGMEIN("IR.cpp] 2256\n");
            sym = this->m_src2->GetStackSym();
            if (sym && !sym->m_isSingleDef)
            {LOGMEIN("IR.cpp] 2259\n");
                this->HoistSrc2(Js::OpCode::Ld_A);
            }
        }
    }

    // Move the instruction down to the target. Return the instruction
    // that preceded the sunk instruction at its original location.
    // (This lets the caller find a Ld_A that this call inserted.)
    IR::Instr * instrPrev = this->m_prev;
    this->Unlink();
    instrTarget->InsertBefore(this);
    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Instr::UnlinkSrc1
///
///     Unlinks the src1 for 'this' instruction.
///
///----------------------------------------------------------------------------

Opnd *
Instr::UnlinkSrc1()
{LOGMEIN("IR.cpp] 2284\n");
    Opnd * oldSrc = this->m_src1;
    oldSrc->UnUse();
    this->m_src1 = nullptr;

    return oldSrc;
}

///----------------------------------------------------------------------------
///
/// Instr::FreeSrc1
///
///     Unlinks and free the src1 for 'this' instruction.
///
///----------------------------------------------------------------------------

void
Instr::FreeSrc1()
{LOGMEIN("IR.cpp] 2302\n");
    Opnd * unlinkedSrc;
    unlinkedSrc = this->UnlinkSrc1();
    unlinkedSrc->Free(this->m_func);
}

///----------------------------------------------------------------------------
///
/// Instr::ReplaceSrc1
///
///     Unlink src1 from this instr, free it, and replace it with newSrc.
///     The new src is returned.
///
///----------------------------------------------------------------------------

Opnd *
Instr::ReplaceSrc1(Opnd * newSrc)
{LOGMEIN("IR.cpp] 2319\n");
    this->FreeSrc1();
    return this->SetSrc1(newSrc);
}

///----------------------------------------------------------------------------
///
/// Instr::HoistSrc1
///
///     Replace current src with new symbol, and assign new symbol using the
///     given opcode from the previous src.
///
///----------------------------------------------------------------------------

Instr *
Instr::HoistSrc1(Js::OpCode assignOpcode, RegNum regNum, StackSym *newSym)
{LOGMEIN("IR.cpp] 2335\n");
    Opnd *oldSrc, *newSrc;
    Instr * newInstr;
    IRType type;

    oldSrc = this->UnlinkSrc1();
    type = oldSrc->GetType();

    const bool creatingNewSym = !newSym;
    if(creatingNewSym)
    {LOGMEIN("IR.cpp] 2345\n");
        newSym = StackSym::New(type, m_func);
    }
    newSrc = this->SetSrc1(RegOpnd::New(newSym, regNum, type, m_func));
    newSrc->SetValueType(oldSrc->GetValueType());

    newInstr = Instr::New(assignOpcode, newSrc, oldSrc, m_func);
    this->InsertBefore(newInstr);

    if(creatingNewSym)
    {LOGMEIN("IR.cpp] 2355\n");
        if (oldSrc->IsRegOpnd())
        {LOGMEIN("IR.cpp] 2357\n");
            newSym->CopySymAttrs(oldSrc->AsRegOpnd()->m_sym);
        }
        else if (oldSrc->IsImmediateOpnd())
        {LOGMEIN("IR.cpp] 2361\n");
            newSym->SetIsConst();
        }
    }

    return newInstr;
}

///----------------------------------------------------------------------------
///
/// Instr::UnlinkSrc2
///
///     Unlinks the src2 for 'this' instruction.
///
///----------------------------------------------------------------------------

Opnd *
Instr::UnlinkSrc2()
{LOGMEIN("IR.cpp] 2379\n");
    Opnd * oldSrc = this->m_src2;
    oldSrc->UnUse();
    this->m_src2 = nullptr;

    return oldSrc;
}

///----------------------------------------------------------------------------
///
/// Instr::FreeSrc2
///
///     Unlinks and free the src2 for 'this' instruction.
///
///----------------------------------------------------------------------------

void
Instr::FreeSrc2()
{LOGMEIN("IR.cpp] 2397\n");
    Opnd * unlinkedSrc;
    unlinkedSrc = this->UnlinkSrc2();
    unlinkedSrc->Free(this->m_func);
}

///----------------------------------------------------------------------------
///
/// Instr::ReplaceSrc2
///
///     Unlink src2 from this instr, free it, and replace it with newSrc.
///     The new src is returned.
///
///----------------------------------------------------------------------------

Opnd *
Instr::ReplaceSrc2(Opnd * newSrc)
{LOGMEIN("IR.cpp] 2414\n");
    this->FreeSrc2();
    return this->SetSrc2(newSrc);
}

///----------------------------------------------------------------------------
///
/// Instr::HoistSrc2
///
///     Replace current src with new symbol, and assign new symbol using the
///     given opcode from the previous src.
///
///----------------------------------------------------------------------------

Instr *
Instr::HoistSrc2(Js::OpCode assignOpcode, RegNum regNum, StackSym *newSym)
{LOGMEIN("IR.cpp] 2430\n");
    Opnd *oldSrc, *newSrc;
    Instr * newInstr;
    IRType type;

    oldSrc = this->UnlinkSrc2();
    type = oldSrc->GetType();

    const bool creatingNewSym = !newSym;
    if(creatingNewSym)
    {LOGMEIN("IR.cpp] 2440\n");
        newSym = StackSym::New(type, m_func);
    }
    newSrc = this->SetSrc2(RegOpnd::New(newSym, regNum, type, m_func));
    newSrc->SetValueType(oldSrc->GetValueType());

    newInstr = Instr::New(assignOpcode, newSrc, oldSrc, m_func);
    this->InsertBefore(newInstr);

    if(creatingNewSym)
    {LOGMEIN("IR.cpp] 2450\n");
        if (oldSrc->IsRegOpnd())
        {LOGMEIN("IR.cpp] 2452\n");
            newSym->CopySymAttrs(oldSrc->AsRegOpnd()->m_sym);
        }
        else if (oldSrc->IsIntConstOpnd())
        {LOGMEIN("IR.cpp] 2456\n");
            newSym->SetIsIntConst(oldSrc->AsIntConstOpnd()->GetValue());
        }
    }

    return newInstr;
}

///----------------------------------------------------------------------------
///
/// Instr::HoistIndirOffset
///
///     Replace the offset of the given indir with a new symbol, which becomes the indir index.
///     Assign the new symbol by creating an assignment from the constant offset.
///
///----------------------------------------------------------------------------

Instr *
Instr::HoistIndirOffset(IR::IndirOpnd *indirOpnd, RegNum regNum)
{LOGMEIN("IR.cpp] 2475\n");
    int32 offset = indirOpnd->GetOffset();
    if (indirOpnd->GetIndexOpnd())
    {LOGMEIN("IR.cpp] 2478\n");
        return HoistIndirOffsetAsAdd(indirOpnd, indirOpnd->GetBaseOpnd(), offset, regNum);
    }
    IntConstOpnd *offsetOpnd = IntConstOpnd::New(offset, TyInt32, this->m_func);
    RegOpnd *indexOpnd = RegOpnd::New(StackSym::New(TyMachReg, this->m_func), regNum, TyMachReg, this->m_func);

#if defined(DBG) && defined(_M_ARM)
    if (regNum == SCRATCH_REG)
    {LOGMEIN("IR.cpp] 2486\n");
        AssertMsg(indirOpnd->GetBaseOpnd()->GetReg()!= SCRATCH_REG, "Why both are SCRATCH_REG");
        if (this->GetSrc1() && this->GetSrc1()->IsRegOpnd())
        {LOGMEIN("IR.cpp] 2489\n");
            Assert(this->GetSrc1()->AsRegOpnd()->GetReg() != SCRATCH_REG);
        }
        if (this->GetSrc2() && this->GetSrc2()->IsRegOpnd())
        {LOGMEIN("IR.cpp] 2493\n");
            Assert(this->GetSrc2()->AsRegOpnd()->GetReg() != SCRATCH_REG);
        }
        if (this->GetDst() && this->GetDst()->IsRegOpnd())
        {LOGMEIN("IR.cpp] 2497\n");
            Assert(this->GetDst()->AsRegOpnd()->GetReg() != SCRATCH_REG);
        }
    }
#endif
    // Clear the offset and add a new reg as the index.
    indirOpnd->SetOffset(0);
    indirOpnd->SetIndexOpnd(indexOpnd);

    Instr *instrAssign = LowererMD::CreateAssign(indexOpnd, offsetOpnd, this);
    indexOpnd->m_sym->SetIsIntConst(offset);
    return instrAssign;
}

IndirOpnd *
Instr::HoistMemRefAddress(MemRefOpnd *const memRefOpnd, const Js::OpCode loadOpCode)
{LOGMEIN("IR.cpp] 2513\n");
    Assert(memRefOpnd);
#if defined(_M_IX86) || defined(_M_X64)
    Assert(!LowererMDArch::IsLegalMemLoc(memRefOpnd));
#endif
    intptr_t address = memRefOpnd->GetMemLoc();
    IR::AddrOpndKind kind = memRefOpnd->GetAddrKind();
    Func *const func = m_func;
    IR::AddrOpnd * addrOpnd = IR::AddrOpnd::New(address, kind, this->m_func, true);
    IR::IndirOpnd * indirOpnd = func->GetTopFunc()->GetConstantAddressIndirOpnd(address, addrOpnd, kind, memRefOpnd->GetType(), loadOpCode);

    if (indirOpnd == nullptr)
    {LOGMEIN("IR.cpp] 2525\n");
        IR::RegOpnd * addressRegOpnd = IR::RegOpnd::New(TyMachPtr, func);
        IR::Instr *const newInstr =
            IR::Instr::New(
            loadOpCode,
            addressRegOpnd,
            IR::AddrOpnd::New(address, kind, func, true),
            func);
        InsertBefore(newInstr);

        indirOpnd = IR::IndirOpnd::New(addressRegOpnd, 0, memRefOpnd->GetType(), func, true);
#if DBG_DUMP
        // TODO: michhol oop jit, make intptr
        indirOpnd->SetAddrKind(kind, (void*)address);
#endif
    }
    return DeepReplace(memRefOpnd, indirOpnd)->AsIndirOpnd();
}


Opnd *
Instr::Replace(Opnd *oldOpnd, Opnd *newOpnd)
{LOGMEIN("IR.cpp] 2547\n");
    if (oldOpnd == this->GetDst())
    {LOGMEIN("IR.cpp] 2549\n");
        return this->ReplaceDst(newOpnd);
    }
    else
    {
        return this->ReplaceSrc(oldOpnd, newOpnd);
    }
}

Opnd *Instr::DeepReplace(Opnd *const oldOpnd, Opnd *const newOpnd)
{LOGMEIN("IR.cpp] 2559\n");
    Assert(oldOpnd);
    Assert(newOpnd);

    IR::Opnd *opnd = GetDst();
    if(opnd && oldOpnd != opnd && oldOpnd->IsEqual(opnd))
    {LOGMEIN("IR.cpp] 2565\n");
        ReplaceDst(newOpnd);
    }
    opnd = GetSrc1();
    if(opnd && oldOpnd != opnd && oldOpnd->IsEqual(opnd))
    {LOGMEIN("IR.cpp] 2570\n");
        ReplaceSrc1(newOpnd);
    }
    opnd = GetSrc2();
    if(opnd && oldOpnd != opnd && oldOpnd->IsEqual(opnd))
    {LOGMEIN("IR.cpp] 2575\n");
        ReplaceSrc2(newOpnd);
    }

    // Do this last because Replace will delete oldOpnd
    return Replace(oldOpnd, newOpnd);
}

Instr *
Instr::HoistIndirOffsetAsAdd(IR::IndirOpnd *orgOpnd, IR::Opnd *baseOpnd, int offset, RegNum regNum)
{LOGMEIN("IR.cpp] 2585\n");
        IR::RegOpnd *newBaseOpnd = IR::RegOpnd::New(StackSym::New(TyMachPtr, this->m_func), regNum, TyMachPtr, this->m_func);

        IR::IntConstOpnd *src2 = IR::IntConstOpnd::New(offset, TyInt32, this->m_func);
        IR::Instr * instrAdd = IR::Instr::New(Js::OpCode::ADD, newBaseOpnd, baseOpnd, src2, this->m_func);

        this->InsertBefore(instrAdd);

        orgOpnd->ReplaceBaseOpnd(newBaseOpnd);
        orgOpnd->SetOffset(0);

        return instrAdd;
}

Instr *
Instr::HoistIndirIndexOpndAsAdd(IR::IndirOpnd *orgOpnd, IR::Opnd *baseOpnd, IR::Opnd *indexOpnd, RegNum regNum)
{LOGMEIN("IR.cpp] 2601\n");
        IR::RegOpnd *newBaseOpnd = IR::RegOpnd::New(StackSym::New(TyMachPtr, this->m_func), regNum, TyMachPtr, this->m_func);

        IR::Instr * instrAdd = IR::Instr::New(Js::OpCode::ADD, newBaseOpnd, baseOpnd, indexOpnd, this->m_func);

        this->InsertBefore(instrAdd);

        orgOpnd->ReplaceBaseOpnd(newBaseOpnd);
        orgOpnd->SetIndexOpnd(nullptr);

        return instrAdd;
}

Instr *
Instr::HoistSymOffsetAsAdd(IR::SymOpnd *orgOpnd, IR::Opnd *baseOpnd, int offset, RegNum regNum)
{LOGMEIN("IR.cpp] 2616\n");
        IR::IndirOpnd *newIndirOpnd = IR::IndirOpnd::New(baseOpnd->AsRegOpnd(), 0, TyMachPtr, this->m_func);
        this->Replace(orgOpnd, newIndirOpnd); // Replace SymOpnd with IndirOpnd
        return this->HoistIndirOffsetAsAdd(newIndirOpnd, baseOpnd, offset, regNum);
}

///----------------------------------------------------------------------------
///
/// Instr::HoistSymOffset
///
///     Replace the given sym with an indir using the given base and offset.
///     (This is used, for instance, to hoist a sym offset that is too large to encode.)
///
///----------------------------------------------------------------------------

Instr *
Instr::HoistSymOffset(SymOpnd *symOpnd, RegNum baseReg, uint32 offset, RegNum regNum)
{LOGMEIN("IR.cpp] 2633\n");
    IR::RegOpnd *baseOpnd = IR::RegOpnd::New(nullptr, baseReg, TyMachPtr, this->m_func);
    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(baseOpnd, offset, symOpnd->GetType(), this->m_func);
    if (symOpnd == this->GetDst())
    {LOGMEIN("IR.cpp] 2637\n");
        this->ReplaceDst(indirOpnd);
    }
    else
    {
        this->ReplaceSrc(symOpnd, indirOpnd);
    }

    return this->HoistIndirOffset(indirOpnd, regNum);
}

Opnd *
Instr::UnlinkSrc(Opnd *src)
{LOGMEIN("IR.cpp] 2650\n");
    if (src == this->GetSrc1())
    {LOGMEIN("IR.cpp] 2652\n");
        return this->UnlinkSrc1();
    }
    else
    {
        AssertMsg(src == this->GetSrc2(), "Src not found");

        return this->UnlinkSrc2();
    }
}

///----------------------------------------------------------------------------
///
/// Instr::ReplaceSrc
///
///     Unlink oldSrc from this instr, free it, and replace it with newSrc.
///     The new src is returned.
///
///----------------------------------------------------------------------------

Opnd *
Instr::ReplaceSrc(Opnd *oldSrc, Opnd * newSrc)
{LOGMEIN("IR.cpp] 2674\n");
    if (oldSrc == this->GetSrc1())
    {LOGMEIN("IR.cpp] 2676\n");
        return this->ReplaceSrc1(newSrc);
    }
    else
    {
        AssertMsg(oldSrc == this->GetSrc2(), "OldSrc not found");

        return this->ReplaceSrc2(newSrc);
    }
}

///----------------------------------------------------------------------------
///
/// Instr::IsRealInstr
///
///     Does this instr generate code?
///
///----------------------------------------------------------------------------

bool
Instr::IsRealInstr() const
{LOGMEIN("IR.cpp] 2697\n");
    switch (m_opcode)
    {LOGMEIN("IR.cpp] 2699\n");
    case Js::OpCode::Label:
    case Js::OpCode::StatementBoundary:
    case Js::OpCode::NoImplicitCallUses:
    case Js::OpCode::NoIntOverflowBoundary:
        return false;

    default:
        return true;
    }
}

///----------------------------------------------------------------------------
///
/// Instr::GetNextRealInstr
///
///----------------------------------------------------------------------------
IR::Instr *
Instr::GetNextRealInstr() const
{LOGMEIN("IR.cpp] 2718\n");
    IR::Instr *instr = this->m_next;

    while (instr != nullptr && !instr->IsRealInstr())
    {LOGMEIN("IR.cpp] 2722\n");
        AssertMsg(instr->m_next || instr->IsPragmaInstr(), "GetNextRealInstr() failed...");
        instr = instr->m_next;
    }
    return instr;
}

///----------------------------------------------------------------------------
///
/// Instr::GetNextRealInstrOrLabel
///
///----------------------------------------------------------------------------
IR::Instr *
Instr::GetNextRealInstrOrLabel() const
{LOGMEIN("IR.cpp] 2736\n");
    IR::Instr *instr = this->m_next;

    while (instr != nullptr && !instr->IsLabelInstr() && !instr->IsRealInstr())
    {LOGMEIN("IR.cpp] 2740\n");
        instr = instr->m_next;
        AssertMsg(instr, "GetNextRealInstrOrLabel() failed...");
    }
    return instr;
}

IR::Instr *
Instr::GetNextBranchOrLabel() const
{LOGMEIN("IR.cpp] 2749\n");
    IR::Instr *instr = this->m_next;

    while (instr != nullptr && !instr->IsLabelInstr() && !instr->IsBranchInstr())
    {LOGMEIN("IR.cpp] 2753\n");
        instr = instr->m_next;
    }
    return instr;
}

///----------------------------------------------------------------------------
///
/// Instr::GetPrevRealInstr
///
///----------------------------------------------------------------------------
IR::Instr *
Instr::GetPrevRealInstr() const
{LOGMEIN("IR.cpp] 2766\n");
    IR::Instr *instr = this->m_prev;

    while (!instr->IsRealInstr())
    {LOGMEIN("IR.cpp] 2770\n");
        instr = instr->m_prev;
        AssertMsg(instr, "GetPrevRealInstr() failed...");
    }
    return instr;
}

///----------------------------------------------------------------------------
///
/// Instr::GetPrevRealInstrOrLabel
///
///----------------------------------------------------------------------------
IR::Instr *
Instr::GetPrevRealInstrOrLabel() const
{LOGMEIN("IR.cpp] 2784\n");
    IR::Instr *instr = this->m_prev;

    while (!instr->IsLabelInstr() && !instr->IsRealInstr())
    {LOGMEIN("IR.cpp] 2788\n");
        instr = instr->m_prev;
        AssertMsg(instr, "GetPrevRealInstrOrLabel() failed...");
    }
    return instr;
}

///----------------------------------------------------------------------------
///
/// Instr::GetInsertBeforeByteCodeUsesInstr
/// Finds the instruction before which new instructions can be inserted, by skipping ByteCodeUses instructions associated with
/// this instruction.
///
///----------------------------------------------------------------------------
IR::Instr *Instr::GetInsertBeforeByteCodeUsesInstr()
{LOGMEIN("IR.cpp] 2803\n");
    const uint32 byteCodeOffset = GetByteCodeOffset();
    IR::Instr *insertBeforeInstr = this;
    IR::Instr *prevInstr = insertBeforeInstr->m_prev;
    while(prevInstr && prevInstr->IsByteCodeUsesInstr() && prevInstr->GetByteCodeOffset() == byteCodeOffset)
    {LOGMEIN("IR.cpp] 2808\n");
        insertBeforeInstr = prevInstr;
        prevInstr = prevInstr->m_prev;
    }
    return insertBeforeInstr;
}

///----------------------------------------------------------------------------
///
/// Instr::GetOrCreateContinueLabel
///
///----------------------------------------------------------------------------
IR::LabelInstr *
Instr::GetOrCreateContinueLabel(const bool isHelper)
{LOGMEIN("IR.cpp] 2822\n");
    if(m_next && m_next->IsLabelInstr() && m_next->AsLabelInstr()->isOpHelper == isHelper)
    {LOGMEIN("IR.cpp] 2824\n");
        return m_next->AsLabelInstr();
    }

    IR::LabelInstr *const label = IR::LabelInstr::New(Js::OpCode::Label, m_func, isHelper);
    InsertAfter(label);
    return label;
}

///----------------------------------------------------------------------------
///
/// Instr::FindRegUse
///
///     Search a reg use of the given sym.  Return the RegOpnd that uses it.
///
///----------------------------------------------------------------------------

IR::RegOpnd *
Instr::FindRegUse(StackSym *sym)
{LOGMEIN("IR.cpp] 2843\n");
    IR::Opnd *src1 = this->GetSrc1();

    // Check src1
    if (src1)
    {LOGMEIN("IR.cpp] 2848\n");
        if (src1->IsRegOpnd())
        {LOGMEIN("IR.cpp] 2850\n");
            RegOpnd *regOpnd = src1->AsRegOpnd();

            if (regOpnd->m_sym == sym)
            {LOGMEIN("IR.cpp] 2854\n");
                return regOpnd;
            }
        }
        else if (src1->IsIndirOpnd())
        {LOGMEIN("IR.cpp] 2859\n");
            IR::IndirOpnd *indirOpnd = src1->AsIndirOpnd();
            if (indirOpnd->GetBaseOpnd()->m_sym == sym)
            {LOGMEIN("IR.cpp] 2862\n");
                return indirOpnd->GetBaseOpnd();
            }
            else if (indirOpnd->GetIndexOpnd() && indirOpnd->GetIndexOpnd()->m_sym == sym)
            {LOGMEIN("IR.cpp] 2866\n");
                return indirOpnd->GetIndexOpnd();
            }
        }
        IR::Opnd *src2 = this->GetSrc2();

        // Check src2
        if (src2)
        {LOGMEIN("IR.cpp] 2874\n");
            if (src2->IsRegOpnd())
            {LOGMEIN("IR.cpp] 2876\n");
                RegOpnd *regOpnd = src2->AsRegOpnd();

                if (regOpnd->m_sym == sym)
                {LOGMEIN("IR.cpp] 2880\n");
                    return regOpnd;
                }
            }
            else if (src2->IsIndirOpnd())
            {LOGMEIN("IR.cpp] 2885\n");
                IR::IndirOpnd *indirOpnd = src2->AsIndirOpnd();
                if (indirOpnd->GetBaseOpnd()->m_sym == sym)
                {LOGMEIN("IR.cpp] 2888\n");
                    return indirOpnd->GetBaseOpnd();
                }
                else if (indirOpnd->GetIndexOpnd() && indirOpnd->GetIndexOpnd()->m_sym == sym)
                {LOGMEIN("IR.cpp] 2892\n");
                    return indirOpnd->GetIndexOpnd();
                }
            }
        }
    }

    // Check uses in dst
    IR::Opnd *dst = this->GetDst();

    if (dst != nullptr && dst->IsIndirOpnd())
    {LOGMEIN("IR.cpp] 2903\n");
        IR::IndirOpnd *indirOpnd = dst->AsIndirOpnd();
        if (indirOpnd->GetBaseOpnd()->m_sym == sym)
        {LOGMEIN("IR.cpp] 2906\n");
            return indirOpnd->GetBaseOpnd();
        }
        else if (indirOpnd->GetIndexOpnd() && indirOpnd->GetIndexOpnd()->m_sym == sym)
        {LOGMEIN("IR.cpp] 2910\n");
            return indirOpnd->GetIndexOpnd();
        }
    }

    return nullptr;
}

IR::RegOpnd *
Instr::FindRegUseInRange(StackSym *sym, IR::Instr *instrBegin, IR::Instr *instrEnd)
{
    FOREACH_INSTR_IN_RANGE(instr, instrBegin, instrEnd)
    {LOGMEIN("IR.cpp] 2922\n");
        Assert(instr);
        IR::RegOpnd *opnd = instr->FindRegUse(sym);
        if (opnd)
        {LOGMEIN("IR.cpp] 2926\n");
            return opnd;
        }
    }
    NEXT_INSTR_IN_RANGE;

    return nullptr;
}

///----------------------------------------------------------------------------
///
/// Instr::FindRegDef
///
///     Search a reg def of the given sym.  Return the RegOpnd that defines it.
///
///----------------------------------------------------------------------------

IR::RegOpnd *
Instr::FindRegDef(StackSym *sym)
{LOGMEIN("IR.cpp] 2945\n");
    IR::Opnd *dst = this->GetDst();

    if (dst)
    {LOGMEIN("IR.cpp] 2949\n");
        if (dst->IsRegOpnd())
        {LOGMEIN("IR.cpp] 2951\n");
            RegOpnd *regOpnd = dst->AsRegOpnd();

            if (regOpnd->m_sym == sym)
            {LOGMEIN("IR.cpp] 2955\n");
                return regOpnd;
            }
        }
    }

    return nullptr;
}

Instr*
Instr::FindSingleDefInstr(Js::OpCode opCode, Opnd* src)
{LOGMEIN("IR.cpp] 2966\n");
    RegOpnd* src1 = src->IsRegOpnd() ? src->AsRegOpnd() : nullptr;

    return  src1 &&
        src1->m_sym->IsSingleDef() &&
        src1->m_sym->GetInstrDef()->m_opcode == opCode ?
        src1->m_sym->GetInstrDef() :
        nullptr;
}

void
Instr::TransferDstAttributesTo(Instr * instr)
{LOGMEIN("IR.cpp] 2978\n");
    instr->dstIsTempNumber = this->dstIsTempNumber;
    instr->dstIsTempNumberTransferred = this->dstIsTempNumberTransferred;
    instr->dstIsTempObject = this->dstIsTempObject;
}

void
Instr::TransferTo(Instr * instr)
{LOGMEIN("IR.cpp] 2986\n");
    Assert(instr->m_dst == nullptr);
    Assert(instr->m_src1 == nullptr);
    Assert(instr->m_src2 == nullptr);
    this->TransferDstAttributesTo(instr);
    instr->usesStackArgumentsObject = this->usesStackArgumentsObject;
    instr->isCloned = this->isCloned;
    instr->ignoreNegativeZero = this->ignoreNegativeZero;
    instr->ignoreIntOverflow = this->ignoreIntOverflow;
    instr->ignoreIntOverflowInRange = this->ignoreIntOverflowInRange;
    instr->ignoreOverflowBitCount = this->ignoreOverflowBitCount;
    instr->loadedArrayHeadSegment = this->loadedArrayHeadSegment;
    instr->loadedArrayHeadSegmentLength = this->loadedArrayHeadSegmentLength;
    instr->extractedUpperBoundCheckWithoutHoisting = this->extractedUpperBoundCheckWithoutHoisting;
    instr->m_number = this->m_number;
    instr->m_src1 = this->m_src1;
    instr->m_src2 = this->m_src2;
    instr->dstIsAlwaysConvertedToInt32 = this->dstIsAlwaysConvertedToInt32;
    instr->dstIsAlwaysConvertedToNumber = this->dstIsAlwaysConvertedToNumber;
    instr->dataWidth = this->dataWidth;
    IR::Opnd * dst = this->m_dst;

    if (dst)
    {LOGMEIN("IR.cpp] 3009\n");
        instr->m_dst = dst;
        this->m_dst = nullptr;
        if (dst->IsRegOpnd())
        {LOGMEIN("IR.cpp] 3013\n");
            Sym * sym = dst->AsRegOpnd()->m_sym;
            if (sym->IsStackSym() && sym->AsStackSym()->m_isSingleDef)
            {LOGMEIN("IR.cpp] 3016\n");
                Assert(sym->AsStackSym()->m_instrDef == this);
                StackSym * stackSym = sym->AsStackSym();
                stackSym->m_instrDef = instr;
            }
        }
    }

    this->m_src1 = nullptr;
    this->m_src2 = nullptr;
}

IR::Instr *
Instr::ConvertToBailOutInstr(IR::Instr * bailOutTarget, IR::BailOutKind kind, uint32 bailOutOffset)
{LOGMEIN("IR.cpp] 3030\n");
    Func * func = bailOutTarget->m_func;
    BailOutInfo * bailOutInfo = JitAnew(func->m_alloc, BailOutInfo, bailOutOffset == Js::Constants::NoByteCodeOffset ? bailOutTarget->GetByteCodeOffset() : bailOutOffset , func);
#if ENABLE_DEBUG_CONFIG_OPTIONS
    bailOutInfo->bailOutOpcode = this->m_opcode;
#endif
    return this->ConvertToBailOutInstr(bailOutInfo, kind);
}

// Notes:
// - useAuxBailout = true specifies that this bailout further will be invisible to globopt, etc, and we'll use auxBailoutKind instead of BailoutKind.
//   Currently this is used for BailOutIgnoreException for debugger.
//
//   Here's typical workflow for scenario useAuxBailout = true.
//   - IRBuilder::Build calls this with kind == BailOutIgnoreException
//   - In here we save the kind to auxBailOut and save bail out info but set hasBailOutInfo to false.
//   - During globopt optimizations presence of this bail out is not detected and instrs can add/remove bailouts as they need.
//     - If they call to convert this instr to bail out instr, we set bailOutKind to what they want and replace bailOutInfo.
//       ** This assumes that for aux bail out bailoutInfo does not really matter (if its pre/post op, etc) **
//       - This is the case for ignore exception.
//       - This will cause to share aux bail out with regular bail out.
//   - In globopt right after OptInstr we check if there is aux bail out which wasn't shared with regular bail out,
//     and if it's not, we convert it back to regular bail out.
IR::Instr *
Instr::ConvertToBailOutInstr(BailOutInfo * bailOutInfo, IR::BailOutKind kind, bool useAuxBailOut /* = false */)
{LOGMEIN("IR.cpp] 3055\n");
    Assert(!this->HasBailOutInfo());

    AssertMsg(!useAuxBailOut || !this->HasAuxBailOut(), "Already aux bail out!");
    Assert(!this->HasAuxBailOut() || this->GetAuxBailOutKind() != IR::BailOutInvalid);

    IR::Instr * bailOutInstr = nullptr;
    if (this->HasAuxBailOut())
    {LOGMEIN("IR.cpp] 3063\n");
        // This instr has already been converted to bailout instr. Only possible with aux bail out.
        // Typical scenario is when globopt calls to convert to e.g. BailOutOnImplicitCalls for the instr which
        // was already converted to bail out instr with HasBailOutInfo() == false and HasAuxBailOutInfo() == true,
        // so that aux bail out is hidden in between IRBuilder and lowerer.

        AssertMsg((this->GetAuxBailOutKind() & ~(IR::BailOutIgnoreException | IR::BailOutForceByFlag)) == 0, "Only IR::BailOutIgnoreException|ForceByFlag supported here.");
        // What we rely on here is:
        // - bailout doesn't have any args.
        // - bailout doesn't use offset as we get it from DebuggingFlags at time of bailout.

        // Use prev debugger bailout kind as decoration, while keeping new kind as main.
        this->SetBailOutKind_NoAssert(kind);

        // Clear old (aux) info and set to the new bailOutInfo.
        this->ReplaceBailOutInfo(bailOutInfo);
        bailOutInfo->bailOutInstr = this;
        this->hasBailOutInfo = true;

        bailOutInstr = this;
    }
    else
    {
        switch (this->m_kind)
        {LOGMEIN("IR.cpp] 3087\n");
        case InstrKindInstr:
            bailOutInstr = IR::BailOutInstr::New(this->m_opcode, kind, bailOutInfo, bailOutInfo->bailOutFunc);
            break;
        case InstrKindProfiled:
            bailOutInstr = IR::ProfiledBailOutInstr::New(this->m_opcode, kind, bailOutInfo, bailOutInfo->bailOutFunc);
            bailOutInstr->AsProfiledInstr()->u = this->AsProfiledInstr()->u;
            break;
        case InstrKindBranch:
        {LOGMEIN("IR.cpp] 3096\n");
            IR::BranchInstr * branchInstr = this->AsBranchInstr();
            Assert(!branchInstr->IsMultiBranch());
            IR::BranchBailOutInstr * branchBailOutInstr = IR::BranchBailOutInstr::New(this->m_opcode, kind, bailOutInfo, bailOutInfo->bailOutFunc);
            branchBailOutInstr->SetTarget(branchInstr->GetTarget());
            branchBailOutInstr->SetByteCodeReg(branchInstr->GetByteCodeReg());
            bailOutInstr = branchBailOutInstr;
            break;
        }
        default:
            AnalysisAssert(false);
        };

        this->m_next->m_prev = bailOutInstr;
        this->m_prev->m_next = bailOutInstr;
        bailOutInstr->m_next = this->m_next;
        bailOutInstr->m_prev = this->m_prev;

        this->TransferTo(bailOutInstr);

        this->Free();
    }

    if (useAuxBailOut)
    {LOGMEIN("IR.cpp] 3120\n");
        // Move bail out kind from bailOutKind to auxBailOutKind and hide bailOutInfo as if this is not a bail out instr.
        bailOutInstr->SetAuxBailOutKind(kind);
        bailOutInstr->SetBailOutKind_NoAssert(IR::BailOutInvalid);
        bailOutInstr->hasBailOutInfo = false;
        bailOutInstr->hasAuxBailOut = true;
    }

    return bailOutInstr;
}

// Convert aux bailout to regular bail out.
// Called by globopt after all optimizations are done, in case we still have aux bail out on the instr.
void Instr::PromoteAuxBailOut()
{LOGMEIN("IR.cpp] 3134\n");
    Assert(!this->HasBailOutInfo());
    Assert(this->GetAuxBailOutKind() != IR::BailOutInvalid);

    this->SetBailOutKind_NoAssert(this->GetAuxBailOutKind());
    this->SetAuxBailOutKind(IR::BailOutInvalid);

    this->hasBailOutInfo = true;
    this->hasAuxBailOut = false;
}

// Reset all tracks of aux bailout but don't rest the bail out info.
// Used after we extract aux bail out in lowerer.
void Instr::ResetAuxBailOut()
{LOGMEIN("IR.cpp] 3148\n");
    this->SetAuxBailOutKind(IR::BailOutInvalid);
    this->hasAuxBailOut = false;
}

void
Instr::ClearBailOutInfo()
{LOGMEIN("IR.cpp] 3155\n");
    if (this->HasBailOutInfo() || this->HasAuxBailOut())
    {LOGMEIN("IR.cpp] 3157\n");
        BailOutInfo * bailOutInfo = this->GetBailOutInfo();
        Assert(bailOutInfo);

        if (bailOutInfo->bailOutInstr == this)
        {LOGMEIN("IR.cpp] 3162\n");
            JitArenaAllocator * alloc = this->m_func->m_alloc;
            bailOutInfo->Clear(alloc);
            JitAdelete(alloc, bailOutInfo);
        }

        this->hasBailOutInfo = false;
        this->hasAuxBailOut = false;
    }
}

bool Instr::HasAnyLoadHeapArgsOpCode()
{LOGMEIN("IR.cpp] 3174\n");
    switch (m_opcode)
    {LOGMEIN("IR.cpp] 3176\n");
        case Js::OpCode::LdHeapArguments:
        case Js::OpCode::LdHeapArgsCached:
        case Js::OpCode::LdLetHeapArguments:
        case Js::OpCode::LdLetHeapArgsCached:
            return true;
    }
    return false;
}

bool Instr::CanHaveArgOutChain() const
{LOGMEIN("IR.cpp] 3187\n");
    return
        this->m_opcode == Js::OpCode::CallI ||
        this->m_opcode == Js::OpCode::CallIFixed ||
        this->m_opcode == Js::OpCode::NewScObject ||
        this->m_opcode == Js::OpCode::NewScObjectSpread ||
        this->m_opcode == Js::OpCode::NewScObjArray ||
        this->m_opcode == Js::OpCode::NewScObjArraySpread;
}

bool Instr::HasEmptyArgOutChain(IR::Instr** startCallInstrOut)
{LOGMEIN("IR.cpp] 3198\n");
    Assert(CanHaveArgOutChain());

    if (GetSrc2()->IsRegOpnd())
    {LOGMEIN("IR.cpp] 3202\n");
        IR::RegOpnd * argLinkOpnd = GetSrc2()->AsRegOpnd();
        StackSym *argLinkSym = argLinkOpnd->m_sym->AsStackSym();
        AssertMsg(!argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");
        IR::Instr* startCallInstr = argLinkSym->m_instrDef;
        AssertMsg(startCallInstr->m_opcode == Js::OpCode::StartCall, "Problem with arg chain.");
        if (startCallInstrOut != nullptr)
        {LOGMEIN("IR.cpp] 3209\n");
            *startCallInstrOut = startCallInstr;
        }
        return true;
    }

    return false;
}

bool Instr::HasFixedFunctionAddressTarget() const
{LOGMEIN("IR.cpp] 3219\n");
    Assert(
        this->m_opcode == Js::OpCode::CallI ||
        this->m_opcode == Js::OpCode::CallIFixed ||
        this->m_opcode == Js::OpCode::NewScObject ||
        this->m_opcode == Js::OpCode::NewScObjectSpread ||
        this->m_opcode == Js::OpCode::NewScObjArray ||
        this->m_opcode == Js::OpCode::NewScObjArraySpread ||
        this->m_opcode == Js::OpCode::NewScObjectNoCtor);
    return
        this->GetSrc1() != nullptr &&
        this->GetSrc1()->IsAddrOpnd() &&
        this->GetSrc1()->AsAddrOpnd()->GetAddrOpndKind() == IR::AddrOpndKind::AddrOpndKindDynamicVar &&
        this->GetSrc1()->AsAddrOpnd()->m_isFunction;
}


void Instr::MoveArgs(bool generateByteCodeCapture)
{LOGMEIN("IR.cpp] 3237\n");
    Assert(this->m_opcode == Js::OpCode::InlineeStart || this->m_opcode == Js::OpCode::CallDirect ||
        this->m_opcode == Js::OpCode::CallI || this->m_opcode == Js::OpCode::CallIFixed);
    IR::Instr *argInsertInstr = this;
    this->IterateArgInstrs([&](IR::Instr* argInstr)
    {
        if (generateByteCodeCapture)
        {LOGMEIN("IR.cpp] 3244\n");
            argInstr->GenerateBytecodeArgOutCapture();
        }
        argInstr->Move(argInsertInstr);
        argInsertInstr = argInstr;
        return false;
    });
}

void Instr::Move(IR::Instr* insertInstr)
{LOGMEIN("IR.cpp] 3254\n");
    this->Unlink();
    this->ClearByteCodeOffset();
    this->SetByteCodeOffset(insertInstr);
    insertInstr->InsertBefore(this);
}

IR::Instr* Instr::GetBytecodeArgOutCapture()
{LOGMEIN("IR.cpp] 3262\n");
    Assert(this->m_opcode == Js::OpCode::ArgOut_A_Inline ||
        this->m_opcode == Js::OpCode::ArgOut_A ||
        this->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn);
    Assert(this->m_dst->GetStackSym()->m_isArgCaptured);
    IR::Instr* instr = this->GetSrc1()->GetStackSym()->m_instrDef;
    Assert(instr->m_opcode == Js::OpCode::BytecodeArgOutCapture);
    return instr;
}

bool Instr::HasByteCodeArgOutCapture()
{LOGMEIN("IR.cpp] 3273\n");
    Assert(this->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs ||
        this->m_opcode == Js::OpCode::ArgOut_A_Inline ||
        this->m_opcode == Js::OpCode::ArgOut_A ||
        this->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn ||
        this->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs);
    if (this->m_dst->GetStackSym()->m_isArgCaptured)
    {LOGMEIN("IR.cpp] 3280\n");
        Assert(GetBytecodeArgOutCapture() != nullptr);
        return true;
    }
    return false;
}


void Instr::GenerateBytecodeArgOutCapture()
{LOGMEIN("IR.cpp] 3289\n");
    if (!HasByteCodeArgOutCapture())
    {LOGMEIN("IR.cpp] 3291\n");
        this->m_dst->GetStackSym()->m_isArgCaptured = true;
        StackSym* tmpSym = StackSym::NewArgSlotRegSym(this->GetDst()->GetStackSym()->GetArgSlotNum(), this->m_func, this->GetDst()->GetType());
        IR::Instr* instr = this->HoistSrc1(Js::OpCode::BytecodeArgOutCapture, RegNOREG, tmpSym);
        instr->SetByteCodeOffset(this);
    }
}

void Instr::GenerateArgOutSnapshot()
{LOGMEIN("IR.cpp] 3300\n");
    StackSym* tmpSym = StackSym::NewArgSlotRegSym(this->GetDst()->GetStackSym()->GetArgSlotNum(), this->m_func);
    IR::Instr* instr = this->HoistSrc1(Js::OpCode::Ld_A, RegNOREG, tmpSym);
    instr->SetByteCodeOffset(this);
}

IR::Instr* Instr::GetArgOutSnapshot()
{LOGMEIN("IR.cpp] 3307\n");
    Assert(this->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs);
    IR::Instr* instr = this->GetSrc1()->GetStackSym()->m_instrDef;
    Assert(instr->m_opcode == Js::OpCode::Ld_A);
    return instr;
}

bool Instr::HasAnyImplicitCalls() const
{LOGMEIN("IR.cpp] 3315\n");
    // there can be no implicit calls in asm.js
    if (m_func->GetJITFunctionBody()->IsAsmJsMode())
    {LOGMEIN("IR.cpp] 3318\n");
        return false;
    }
    if (OpCodeAttr::HasImplicitCall(this->m_opcode))
    {LOGMEIN("IR.cpp] 3322\n");
        return true;
    }
    if (OpCodeAttr::OpndHasImplicitCall(this->m_opcode))
    {LOGMEIN("IR.cpp] 3326\n");
        if (this->m_dst &&
            ((this->m_dst->IsSymOpnd() && this->m_dst->AsSymOpnd()->m_sym->IsPropertySym()) ||
             this->m_dst->IsIndirOpnd()))
        {LOGMEIN("IR.cpp] 3330\n");
            return true;
        }

        IR::Opnd *src1 = this->GetSrc1();
        if (src1)
        {LOGMEIN("IR.cpp] 3336\n");
            if ((src1->IsSymOpnd() && src1->AsSymOpnd()->m_sym->IsPropertySym()) || src1->IsIndirOpnd())
            {LOGMEIN("IR.cpp] 3338\n");
                return true;
            }

            if (!src1->GetValueType().IsPrimitive())
            {LOGMEIN("IR.cpp] 3343\n");
                return true;
            }

            IR::Opnd *src2 = this->GetSrc2();
            if (src2)
            {LOGMEIN("IR.cpp] 3349\n");
                if ((src2->IsSymOpnd() && src2->AsSymOpnd()->m_sym->IsPropertySym()) || src2->IsIndirOpnd())
                {LOGMEIN("IR.cpp] 3351\n");
                    return true;
                }

                if (!src2->GetValueType().IsPrimitive())
                {LOGMEIN("IR.cpp] 3356\n");
                    return true;
                }
            }
        }
    }

    return false;
}

bool Instr::HasAnySideEffects() const
{LOGMEIN("IR.cpp] 3367\n");
    return (hasSideEffects ||
            OpCodeAttr::HasSideEffects(this->m_opcode) ||
            this->HasAnyImplicitCalls());
}

bool Instr::AreAllOpndInt64() const
{LOGMEIN("IR.cpp] 3374\n");
    bool isDstInt64 = !m_dst || IRType_IsInt64(m_dst->GetType());
    bool isSrc1Int64 = !m_src1 || IRType_IsInt64(m_src1->GetType());
    bool isSrc2Int64 = !m_src2 || IRType_IsInt64(m_src2->GetType());
    return isDstInt64 && isSrc1Int64 && isSrc2Int64;
}

JITTimeFixedField* Instr::GetFixedFunction() const
{LOGMEIN("IR.cpp] 3382\n");
    Assert(HasFixedFunctionAddressTarget());
    JITTimeFixedField* function = (JITTimeFixedField*)this->m_src1->AsAddrOpnd()->m_metadata;
    return function;
}

IR::Instr* Instr::GetNextArg()
{LOGMEIN("IR.cpp] 3389\n");
    Assert(this->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs ||
        this->m_opcode == Js::OpCode::ArgOut_A_Inline ||
        this->m_opcode == Js::OpCode::ArgOut_A ||
        this->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn ||
        this->m_opcode == Js::OpCode::InlineeStart);
    IR::Instr* argInstr = this;
    while (true)
    {LOGMEIN("IR.cpp] 3397\n");
        StackSym* linkSym;
        if (argInstr->GetSrc2()->IsRegOpnd())
        {LOGMEIN("IR.cpp] 3400\n");
            linkSym = argInstr->GetSrc2()->AsRegOpnd()->m_sym->AsStackSym();
        }
        else
        {
            linkSym = argInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym();
            Assert(linkSym->IsArgSlotSym());
        }
        Assert(linkSym->IsSingleDef());
        argInstr = linkSym->m_instrDef;
        if (argInstr->m_opcode == Js::OpCode::ArgOut_A_InlineSpecialized)
        {LOGMEIN("IR.cpp] 3411\n");
            continue;
        }
        if (argInstr->m_opcode == Js::OpCode::StartCall)
        {LOGMEIN("IR.cpp] 3415\n");
            break;
        }
        return argInstr;
    }
    return nullptr;
}

uint Instr::GetArgOutCount(bool getInterpreterArgOutCount)
{LOGMEIN("IR.cpp] 3424\n");
    // There are cases of inlining like .apply and .call target inlining, where we muck around with the ArgOut sequence,
    // and make it different from the one the interpreter sees (and expects, on a bailout).
    // In such cases, we set the interpreter version of the number of ArgOuts as the src2 of StartCall,
    // and any code that queries the argout count for bailout purposes should look at the src2 (if available) of these instructions.

    // If the src2 is not set, that means that the interpreter and the JIT versions of the argout count are the same.

    Js::OpCode opcode = this->m_opcode;
    Assert(opcode == Js::OpCode::StartCall ||
           opcode == Js::OpCode::InlineeEnd || opcode == Js::OpCode::InlineBuiltInEnd|| opcode == Js::OpCode::InlineNonTrackingBuiltInEnd ||
           opcode == Js::OpCode::EndCallForPolymorphicInlinee || opcode == Js::OpCode::LoweredStartCall);
    if (!getInterpreterArgOutCount)
    {LOGMEIN("IR.cpp] 3437\n");
        return this->GetSrc1()->AsIntConstOpnd()->AsUint32();
    }

    Assert(opcode == Js::OpCode::StartCall);
    IntConstType argOutCount = !this->GetSrc2() ? this->GetSrc1()->AsIntConstOpnd()->GetValue() : this->GetSrc2()->AsIntConstOpnd()->GetValue();
    Assert(argOutCount >= 0 && argOutCount < UINT32_MAX);
    return (uint)argOutCount;
}

PropertySymOpnd *Instr::GetPropertySymOpnd() const
{LOGMEIN("IR.cpp] 3448\n");
    if (m_src1 && m_src1->IsSymOpnd() && m_src1->AsSymOpnd()->IsPropertySymOpnd())
    {LOGMEIN("IR.cpp] 3450\n");
        return m_src1->AsPropertySymOpnd();
    }
    if (m_dst && m_dst->IsSymOpnd() && m_dst->AsSymOpnd()->IsPropertySymOpnd())
    {LOGMEIN("IR.cpp] 3454\n");
        return m_dst->AsPropertySymOpnd();
    }
    return nullptr;
}

bool Instr::CallsAccessor(IR::PropertySymOpnd* methodOpnd)
{LOGMEIN("IR.cpp] 3461\n");
    if (methodOpnd)
    {LOGMEIN("IR.cpp] 3463\n");
        Assert(methodOpnd->HasObjTypeSpecFldInfo());
        return methodOpnd->UsesAccessor();
    }

    return CallsGetter() || CallsSetter();
}

bool Instr::CallsSetter(IR::PropertySymOpnd* methodOpnd)
{LOGMEIN("IR.cpp] 3472\n");
    return
        this->IsProfiledInstr() &&
        (this->m_dst && this->m_dst->IsSymOpnd() && this->m_dst->AsSymOpnd()->IsPropertySymOpnd()) &&
        ((this->AsProfiledInstr()->u.FldInfo().flags & Js::FldInfo_FromAccessor) != 0);
}

bool Instr::CallsGetter(IR::PropertySymOpnd* methodOpnd)
{LOGMEIN("IR.cpp] 3480\n");
    return
        this->IsProfiledInstr() &&
        (this->m_src1 && this->m_src1->IsSymOpnd() && this->m_src1->AsSymOpnd()->IsPropertySymOpnd()) &&
        ((this->AsProfiledInstr()->u.FldInfo().flags & Js::FldInfo_FromAccessor) != 0);
}

IR::Instr* IR::Instr::NewConstantLoad(IR::RegOpnd* dstOpnd, intptr_t varConst, ValueType type, Func* func, Js::Var varLocal/* = nullptr*/)
{LOGMEIN("IR.cpp] 3488\n");
    IR::Opnd *srcOpnd = nullptr;
    IR::Instr *instr;

    if (Js::TaggedInt::Is(varConst))
    {LOGMEIN("IR.cpp] 3493\n");
        IntConstType value = Js::TaggedInt::ToInt32((Js::Var)varConst);
        instr = IR::Instr::New(Js::OpCode::LdC_A_I4, dstOpnd, IR::IntConstOpnd::New(value, TyInt32, func), func);
        if (dstOpnd->m_sym->IsSingleDef())
        {LOGMEIN("IR.cpp] 3497\n");
            dstOpnd->m_sym->SetIsIntConst(value);
        }
    }
    else
    {
        if (varConst == func->GetThreadContextInfo()->GetNullFrameDisplayAddr())
        {LOGMEIN("IR.cpp] 3504\n");
            instr = IR::Instr::New(
                Js::OpCode::Ld_A,
                dstOpnd,
                IR::AddrOpnd::New(
                    func->GetThreadContextInfo()->GetNullFrameDisplayAddr(),
                    IR::AddrOpndKindDynamicMisc,
                    func),
                func);
        }
        else if (varConst == func->GetThreadContextInfo()->GetStrictNullFrameDisplayAddr())
        {LOGMEIN("IR.cpp] 3515\n");
            instr = IR::Instr::New(
                Js::OpCode::Ld_A,
                dstOpnd,
                IR::AddrOpnd::New(
                    func->GetThreadContextInfo()->GetStrictNullFrameDisplayAddr(),
                    IR::AddrOpndKindDynamicMisc,
                    func),
                func);
        }
        else
        {

            ValueType valueType;
            if(type.IsString())
            {LOGMEIN("IR.cpp] 3530\n");
                srcOpnd = IR::AddrOpnd::New(varConst, IR::AddrOpndKindDynamicVar, func, true, varLocal);
                instr = IR::Instr::New(Js::OpCode::LdStr, dstOpnd, srcOpnd, func);
                Assert(dstOpnd->m_sym->m_isSingleDef);
                if (dstOpnd->m_sym->IsSingleDef())
                {LOGMEIN("IR.cpp] 3535\n");
                    dstOpnd->m_sym->m_isStrConst = true;
                    dstOpnd->m_sym->m_isConst = true;
                }
                dstOpnd->SetValueType(ValueType::String);
                srcOpnd->SetValueType(ValueType::String);
            }
            else if(type.IsNumber())
            {LOGMEIN("IR.cpp] 3543\n");
                // TODO (michhol): OOP JIT. we may need to unbox before sending over const table

                if (!func->IsOOPJIT())
                {LOGMEIN("IR.cpp] 3547\n");
                    srcOpnd = IR::FloatConstOpnd::New((Js::Var)varConst, TyFloat64, func);
                }
                else
                {
                    srcOpnd = IR::FloatConstOpnd::New((Js::Var)varConst, TyFloat64, func
#if !FLOATVAR
                        ,varLocal
#endif
                    );

                }

                instr = IR::Instr::New(Js::OpCode::LdC_A_R8, dstOpnd, srcOpnd, func);
                if (dstOpnd->m_sym->IsSingleDef())
                {LOGMEIN("IR.cpp] 3562\n");
                    dstOpnd->m_sym->SetIsFloatConst();

#if FLOATVAR
                    dstOpnd->m_sym->m_isNotInt = FALSE;
#else
                    // Don't set m_isNotInt to true if the float constant value is an int32 or uint32. Uint32s may sometimes be
                    // treated as int32s for the purposes of int specialization.
                    dstOpnd->m_sym->m_isNotInt = !Js::JavascriptNumber::IsInt32OrUInt32(((IR::FloatConstOpnd*)srcOpnd)->m_value);


#endif
                }
            }
            else
            {
                if (type.IsUndefined() || type.IsNull() || type.IsBoolean())
                {LOGMEIN("IR.cpp] 3579\n");
                    valueType = type;
                }
                else
                {
                    valueType = ValueType::GetObject(ObjectType::Object);
                }
                srcOpnd = IR::AddrOpnd::New(varConst, IR::AddrOpndKindDynamicVar, func, true, varLocal);
                instr = IR::Instr::New(Js::OpCode::Ld_A, dstOpnd, srcOpnd, func);
                if (dstOpnd->m_sym->IsSingleDef())
                {LOGMEIN("IR.cpp] 3589\n");
                    dstOpnd->m_sym->m_isConst = true;
                }
                dstOpnd->SetValueType(valueType);
                srcOpnd->SetValueType(valueType);
            }
        }
    }
    return instr;
}

bool Instr::UsesAllFields()
{LOGMEIN("IR.cpp] 3601\n");
    return OpCodeAttr::UseAllFields(this->m_opcode) || this->CallsAccessor();
}

BranchInstr *
Instr::ChangeCmCCToBranchInstr(LabelInstr *targetInstr)
{LOGMEIN("IR.cpp] 3607\n");
    Js::OpCode newOpcode;
    switch (this->m_opcode)
    {LOGMEIN("IR.cpp] 3610\n");
    case Js::OpCode::CmEq_A:
        newOpcode = Js::OpCode::BrEq_A;
        break;
    case Js::OpCode::CmGe_A:
        newOpcode = Js::OpCode::BrGe_A;
        break;
    case Js::OpCode::CmGt_A:
        newOpcode = Js::OpCode::BrGt_A;
        break;
    case Js::OpCode::CmLt_A:
        newOpcode = Js::OpCode::BrLt_A;
        break;
    case Js::OpCode::CmLe_A:
        newOpcode = Js::OpCode::BrLe_A;
        break;
    case Js::OpCode::CmUnGe_A:
        newOpcode = Js::OpCode::BrUnGe_A;
        break;
    case Js::OpCode::CmUnGt_A:
        newOpcode = Js::OpCode::BrUnGt_A;
        break;
    case Js::OpCode::CmUnLt_A:
        newOpcode = Js::OpCode::BrUnLt_A;
        break;
    case Js::OpCode::CmUnLe_A:
        newOpcode = Js::OpCode::BrUnLe_A;
        break;
    case Js::OpCode::CmNeq_A:
        newOpcode = Js::OpCode::BrNeq_A;
        break;
    case Js::OpCode::CmSrEq_A:
        newOpcode = Js::OpCode::BrSrEq_A;
        break;
    case Js::OpCode::CmSrNeq_A:
        newOpcode = Js::OpCode::BrSrNeq_A;
        break;
    case Js::OpCode::CmEq_I4:
        newOpcode = Js::OpCode::BrEq_I4;
        break;
    case Js::OpCode::CmGe_I4:
        newOpcode = Js::OpCode::BrGe_I4;
        break;
    case Js::OpCode::CmGt_I4:
        newOpcode = Js::OpCode::BrGt_I4;
        break;
    case Js::OpCode::CmLt_I4:
        newOpcode = Js::OpCode::BrLt_I4;
        break;
    case Js::OpCode::CmLe_I4:
        newOpcode = Js::OpCode::BrLe_I4;
        break;
    case Js::OpCode::CmUnGe_I4:
        newOpcode = Js::OpCode::BrUnGe_I4;
        break;
    case Js::OpCode::CmUnGt_I4:
        newOpcode = Js::OpCode::BrUnGt_I4;
        break;
    case Js::OpCode::CmUnLt_I4:
        newOpcode = Js::OpCode::BrUnLt_I4;
        break;
    case Js::OpCode::CmUnLe_I4:
        newOpcode = Js::OpCode::BrUnLe_I4;
        break;
    case Js::OpCode::CmNeq_I4:
        newOpcode = Js::OpCode::BrNeq_I4;
        break;
    default:
        Assert(UNREACHED);
        __assume(UNREACHED);
    }

    BranchInstr *instrBr = BranchInstr::New(newOpcode, targetInstr, this->m_func);
    this->InsertBefore(instrBr);
    instrBr->SetByteCodeOffset(this);
    instrBr->SetSrc1(this->UnlinkSrc1());
    instrBr->SetSrc2(this->UnlinkSrc2());

    this->Remove();

    return instrBr;
}

bool Instr::IsCmCC_A()
{LOGMEIN("IR.cpp] 3694\n");
    return (this->m_opcode >= Js::OpCode::CmEq_A && this->m_opcode <= Js::OpCode::CmSrNeq_A) && this->GetSrc1()->IsVar();
}

bool Instr::IsCmCC_R8()
{LOGMEIN("IR.cpp] 3699\n");
    return (this->m_opcode >= Js::OpCode::CmEq_A && this->m_opcode <= Js::OpCode::CmSrNeq_A) && this->GetSrc1()->IsFloat64();
}

bool Instr::IsCmCC_I4()
{LOGMEIN("IR.cpp] 3704\n");
    return (this->m_opcode >= Js::OpCode::CmEq_I4 && this->m_opcode <= Js::OpCode::CmUnGe_I4);
}

bool Instr::BinaryCalculator(IntConstType src1Const, IntConstType src2Const, IntConstType *pResult)
{LOGMEIN("IR.cpp] 3709\n");
    IntConstType value = 0;

    switch (this->m_opcode)
    {LOGMEIN("IR.cpp] 3713\n");
    case Js::OpCode::Add_A:
        if (IntConstMath::Add(src1Const, src2Const, &value))
        {LOGMEIN("IR.cpp] 3716\n");
            return false;
        }
        break;

    case Js::OpCode::Sub_A:
        if (IntConstMath::Sub(src1Const, src2Const, &value))
        {LOGMEIN("IR.cpp] 3723\n");
            return false;
        }
        break;

    case Js::OpCode::Mul_A:
        if (IntConstMath::Mul(src1Const, src2Const, &value))
        {LOGMEIN("IR.cpp] 3730\n");
            return false;
        }
        if (value == 0)
        {LOGMEIN("IR.cpp] 3734\n");
            // might be -0
            // Bail for now...
            return false;
        }
        break;

    case Js::OpCode::Div_A:
        if (src2Const == 0)
        {LOGMEIN("IR.cpp] 3743\n");
            // Could fold to INF/-INF
            // instr->HoistSrc1(Js::OpCode::Ld_A);
            return false;
        }
        if (src1Const == 0 && src2Const < 0)
        {LOGMEIN("IR.cpp] 3749\n");
            // folds to -0. Bail for now...
            return false;
        }
        if (IntConstMath::Div(src1Const, src2Const, &value))
        {LOGMEIN("IR.cpp] 3754\n");
            return false;
        }
        if (src1Const % src2Const != 0)
        {LOGMEIN("IR.cpp] 3758\n");
            // Bail for now...
            return false;
        }
        break;

    case Js::OpCode::Rem_A:

        if (src2Const == 0)
        {LOGMEIN("IR.cpp] 3767\n");
            // Bail for now...
            return false;
        }
        if (IntConstMath::Mod(src1Const, src2Const, &value))
        {LOGMEIN("IR.cpp] 3772\n");
            return false;
        }
        if (value == 0)
        {LOGMEIN("IR.cpp] 3776\n");
            // might be -0
            // Bail for now...
            return false;
        }
        break;

    case Js::OpCode::Shl_A:
        // We don't care about overflow here
        IntConstMath::Shl(src1Const, src2Const & 0x1F, &value);
        break;

    case Js::OpCode::Shr_A:
        // We don't care about overflow here, and there shouldn't be any
        IntConstMath::Shr(src1Const, src2Const & 0x1F, &value);
        break;

    case Js::OpCode::ShrU_A:
        // We don't care about overflow here, and there shouldn't be any
        IntConstMath::ShrU(src1Const, src2Const & 0x1F, &value);
        if (value < 0)
        {LOGMEIN("IR.cpp] 3797\n");
            // ShrU produces a UInt32.  If it doesn't fit in an Int32, bail as we don't
            // track signs of int values.
            return false;
        }
        break;

    case Js::OpCode::And_A:
        // We don't care about overflow here, and there shouldn't be any
        IntConstMath::And(src1Const, src2Const, &value);
        break;

    case Js::OpCode::Or_A:
        // We don't care about overflow here, and there shouldn't be any
        IntConstMath::Or(src1Const, src2Const, &value);
        break;

    case Js::OpCode::Xor_A:
        // We don't care about overflow here, and there shouldn't be any
        IntConstMath::Xor(src1Const, src2Const, &value);
        break;

    case Js::OpCode::InlineMathMin:
        value = src1Const < src2Const ? src1Const : src2Const;
        break;

    case Js::OpCode::InlineMathMax:
        value = src1Const > src2Const ? src1Const : src2Const;
        break;

    default:
        return false;
    }

    *pResult = value;

    return true;
}

bool Instr::UnaryCalculator(IntConstType src1Const, IntConstType *pResult)
{LOGMEIN("IR.cpp] 3837\n");
    IntConstType value = 0;

    switch (this->m_opcode)
    {LOGMEIN("IR.cpp] 3841\n");
    case Js::OpCode::Neg_A:
        if (src1Const == 0)
        {LOGMEIN("IR.cpp] 3844\n");
            // Could fold to -0.0
            return false;
        }

        if (IntConstMath::Neg(src1Const, &value))
        {LOGMEIN("IR.cpp] 3850\n");
            return false;
        }
        break;

    case Js::OpCode::Not_A:
        IntConstMath::Not(src1Const, &value);
        break;

    case Js::OpCode::Ld_A:
        if (this->HasBailOutInfo())
        {LOGMEIN("IR.cpp] 3861\n");
            Assert(this->GetBailOutKind() == IR::BailOutExpectingInteger);
            this->ClearBailOutInfo();
        }
        value = src1Const;
        break;

    case Js::OpCode::Conv_Num:
    case Js::OpCode::Ld_I4:
        value = src1Const;
        break;

    case Js::OpCode::Incr_A:
        if (IntConstMath::Inc(src1Const, &value))
        {LOGMEIN("IR.cpp] 3875\n");
            return false;
        }
        break;

    case Js::OpCode::Decr_A:
        if (IntConstMath::Dec(src1Const, &value))
        {LOGMEIN("IR.cpp] 3882\n");
            return false;
        }
        break;


    case Js::OpCode::InlineMathAbs:
        if (src1Const == IntConstMin)
        {LOGMEIN("IR.cpp] 3890\n");
            return false;
        }
        else
        {
            value = src1Const < 0 ? -src1Const : src1Const;
        }
        break;

    case Js::OpCode::InlineMathClz:
        DWORD clz;
        DWORD src1Const32;
        src1Const32 = (DWORD)src1Const;
        if (_BitScanReverse(&clz, src1Const32))
        {LOGMEIN("IR.cpp] 3904\n");
            value = 31 - clz;
        }
        else
        {
            value = 32;
        }
        this->ClearBailOutInfo();
        break;

    case Js::OpCode::InlineMathFloor:
        value = src1Const;
        this->ClearBailOutInfo();
        break;

    case Js::OpCode::InlineMathCeil:
        value = src1Const;
        this->ClearBailOutInfo();
        break;

    case Js::OpCode::InlineMathRound:
        value = src1Const;
        this->ClearBailOutInfo();
        break;
    case Js::OpCode::ToVar:
        if (Js::TaggedInt::IsOverflow(src1Const))
        {LOGMEIN("IR.cpp] 3930\n");
            return false;
        }
        else
        {
            value = src1Const;
            this->ClearBailOutInfo();
            break;
        }
    default:
        return false;
    }

    *pResult = value;
    return true;
}

#if ENABLE_DEBUG_CONFIG_OPTIONS
///----------------------------------------------------------------------------
///
/// Instr::DumpTestTrace
///
///     Dump this instr in TestTrace.
///
///----------------------------------------------------------------------------

void
Instr::DumpTestTrace()
{LOGMEIN("IR.cpp] 3958\n");
    Output::Print(_u("opcode: %s "), Js::OpCodeUtil::GetOpCodeName(m_opcode));
    SymOpnd * symOpnd;

    if (this->m_opcode == Js::OpCode::NewScFunc || this->m_opcode == Js::OpCode::NewScGenFunc)
    {LOGMEIN("IR.cpp] 3963\n");
        Output::Print(_u("\n"));
        return;
    }
    Opnd * src1 = this->GetSrc1();
    if (!src1)
    {LOGMEIN("IR.cpp] 3969\n");
        Output::Print(_u("\n"));
        return;
    }
    if (src1->GetKind() != OpndKindSym)
    {LOGMEIN("IR.cpp] 3974\n");
        Output::Print(_u("\n"));
        return;
    }

    symOpnd = src1->AsSymOpnd();
    if (symOpnd->m_sym->IsPropertySym())
    {LOGMEIN("IR.cpp] 3981\n");
        PropertySym *propertySym = symOpnd->m_sym->AsPropertySym();

        switch (propertySym->m_fieldKind)
        {LOGMEIN("IR.cpp] 3985\n");
        case PropertyKindData:
            if (!JITManager::GetJITManager()->IsOOPJITEnabled())
            {LOGMEIN("IR.cpp] 3988\n");
                Js::PropertyRecord const* fieldName = propertySym->GetFunc()->GetInProcThreadContext()->GetPropertyRecord(propertySym->m_propertyId);
                Output::Print(_u("field: %s "), fieldName->GetBuffer());
                break;
            }
            // else fall through
        case PropertyKindSlots:
            Output::Print(_u("field: [%d] "), propertySym->m_propertyId);
            break;
        case PropertyKindLocalSlots:
            Output::Print(_u("field: l[%d] "), propertySym->m_propertyId);
            break;
        default:
            break;
        }
        Output::Print(_u("\n"));
    }
}

///----------------------------------------------------------------------------
///
/// Instr::DumpFieldCopyPropTestTrace
///
///     Dump fieldcopyprop when testtrace is enabled.
///
///----------------------------------------------------------------------------

void
Instr::DumpFieldCopyPropTestTrace()
{LOGMEIN("IR.cpp] 4017\n");
    switch (m_opcode)
    {LOGMEIN("IR.cpp] 4019\n");
    case Js::OpCode::LdSlot:
    case Js::OpCode::LdSlotArr:
    case Js::OpCode::LdFld:
    case Js::OpCode::LdFldForTypeOf:
    case Js::OpCode::LdRootFld:
    case Js::OpCode::LdRootFldForTypeOf:
    case Js::OpCode::LdMethodFld:
    case Js::OpCode::LdRootMethodFld:
    case Js::OpCode::LdMethodFromFlags:
    case Js::OpCode::ScopedLdMethodFld:
    case Js::OpCode::TypeofElem:

        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(_u("TestTrace fieldcopyprop: function %s (%s) "),
            this->m_func->GetJITFunctionBody()->GetDisplayName(),
            this->m_func->GetDebugNumberSet(debugStringBuffer));
        if (this->IsInlined())
        {LOGMEIN("IR.cpp] 4037\n");
            Output::Print(_u("inlined caller function %s (%s) "),
                this->m_func->GetTopFunc()->GetJITFunctionBody()->GetDisplayName(),
                this->m_func->GetTopFunc()->GetDebugNumberSet(debugStringBuffer));
        }
        this->DumpTestTrace();
    default:
        break;
    }
}
#endif

#if ENABLE_DEBUG_CONFIG_OPTIONS

const char *
Instr::GetBailOutKindName() const
{LOGMEIN("IR.cpp] 4053\n");
    IR::BailOutKind kind = (IR::BailOutKind)0;
    if (this->HasBailOutInfo())
    {LOGMEIN("IR.cpp] 4056\n");
        kind |= this->GetBailOutKind();
    }
    if (this->HasAuxBailOut())
    {LOGMEIN("IR.cpp] 4060\n");
        kind |= this->GetAuxBailOutKind();
    }

    return ::GetBailOutKindName(kind);
}

#endif

//
// Debug dumpers
//

#if DBG_DUMP

void
Instr::DumpByteCodeOffset()
{LOGMEIN("IR.cpp] 4077\n");
    if (m_func->HasByteCodeOffset())
    {LOGMEIN("IR.cpp] 4079\n");
        Output::SkipToColumn(78);
        Output::Print(_u("#"));
        if (this->m_number != Js::Constants::NoByteCodeOffset)
        {LOGMEIN("IR.cpp] 4083\n");
            Output::Print(_u("%04x"), this->m_number);
            Output::Print(this->IsCloned()? _u("*") : _u(" "));
        }
    }
    if (!this->m_func->IsTopFunc())
    {LOGMEIN("IR.cpp] 4089\n");
        Output::SkipToColumn(78);
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(_u(" Func #%s"), this->m_func->GetDebugNumberSet(debugStringBuffer));
    }
#ifdef BAILOUT_INJECTION
    if (this->bailOutByteCodeLocation != (uint)-1)
    {LOGMEIN("IR.cpp] 4096\n");
        Output::SkipToColumn(85);
        Output::Print(_u("@%4d"), this->bailOutByteCodeLocation);
    }
#endif
    if (this->m_opcode == Js::OpCode::InlineeStart)
    {LOGMEIN("IR.cpp] 4102\n");
        Output::Print(_u(" %s"), this->m_func->GetJITFunctionBody()->GetDisplayName());
    }
}

void
Instr::DumpGlobOptInstrString()
{LOGMEIN("IR.cpp] 4109\n");
    if(this->globOptInstrString)
    {LOGMEIN("IR.cpp] 4111\n");
        Output::Print(_u("\n\n GLOBOPT INSTR: %s\n\n"), this->globOptInstrString);
    }
}

///----------------------------------------------------------------------------
///
/// Instr::Dump
///
///     Dump this instr.
///
///----------------------------------------------------------------------------

void
Instr::Dump(IRDumpFlags flags)
{LOGMEIN("IR.cpp] 4126\n");
    bool const AsmDumpMode = flags & IRDumpFlags_AsmDumpMode;
    bool const SimpleForm = !!(flags & IRDumpFlags_SimpleForm);
    bool const SkipByteCodeOffset = !!(flags & IRDumpFlags_SkipByteCodeOffset);

    const auto PrintOpCodeName = [&]() {
        Output::SkipToColumn(23);
        Output::Print(_u("%s "), Js::OpCodeUtil::GetOpCodeName(m_opcode));
        Output::SkipToColumn(38);
    };

    // forward decl before goto statement
    Opnd * dst = nullptr;

    if(m_opcode == Js::OpCode::BoundCheck || m_opcode == Js::OpCode::UnsignedBoundCheck)
    {LOGMEIN("IR.cpp] 4141\n");
        PrintOpCodeName();

        // src1 <= src2 + dst

        Assert(GetSrc1());
        if(GetSrc1()->IsIntConstOpnd())
        {LOGMEIN("IR.cpp] 4148\n");
            Output::Print(_u("%d"), GetSrc1()->AsIntConstOpnd()->GetValue());
        }
        else
        {
            GetSrc1()->Dump(flags, m_func);
        }

        bool useLessThanOrEqual = true;
        bool usePlus = true;
        bool dumpSrc2 = false;
        int32 offset = GetDst() ? GetDst()->AsIntConstOpnd()->AsInt32() : 0;
        if(GetSrc2())
        {LOGMEIN("IR.cpp] 4161\n");
            if(GetSrc2()->IsIntConstOpnd())
            {LOGMEIN("IR.cpp] 4163\n");
            #if DBG
                int32 temp;
                Assert(!Int32Math::Add(offset, GetSrc2()->AsIntConstOpnd()->AsInt32(), &temp));
            #endif
                offset += GetSrc2()->AsIntConstOpnd()->AsInt32();
            }
            else
            {
                dumpSrc2 = true;
                if(offset == -1)
                {LOGMEIN("IR.cpp] 4174\n");
                    useLessThanOrEqual = false; // < instead of <=
                    offset = 0;
                }
                else if(offset < 0 && offset != IntConstMin)
                {LOGMEIN("IR.cpp] 4179\n");
                    usePlus = false;
                    offset = -offset;
                }
            }
        }

        Output::Print(_u(" %S "), useLessThanOrEqual ? "<=" : "<");
        if(dumpSrc2)
        {
            GetSrc2()->Dump(flags, m_func);
        }
        if(offset != 0)
        {LOGMEIN("IR.cpp] 4192\n");
            if(dumpSrc2)
            {LOGMEIN("IR.cpp] 4194\n");
                Output::Print(_u(" %C "), usePlus ? '+' : '-');
            }
            Output::Print(_u("%d"), offset);
        }

        goto PrintByteCodeOffsetEtc;
    }

    Output::SkipToColumn(4);

    dst = this->GetDst();

    if (dst)
    {LOGMEIN("IR.cpp] 4208\n");
        dst->Dump(flags, this->m_func);

        bool const dumpMarkTemp = PHASE_DUMP(Js::MarkTempPhase, m_func)
            || PHASE_TRACE(Js::MarkTempPhase, m_func);
        bool const dumpMarkTempNumber = dumpMarkTemp || PHASE_DUMP(Js::MarkTempNumberPhase, m_func)
            || PHASE_TRACE(Js::MarkTempNumberPhase, m_func);
        bool const dumpMarkTempObject = dumpMarkTemp || PHASE_DUMP(Js::MarkTempObjectPhase, m_func)
            || PHASE_TRACE(Js::MarkTempObjectPhase, m_func);

        if ((dumpMarkTempNumber && (this->dstIsTempNumberTransferred || this->dstIsTempNumber))
            || (dumpMarkTempObject && this->dstIsTempObject))
        {LOGMEIN("IR.cpp] 4220\n");
            Output::Print(_u("["));

            if (dumpMarkTempNumber)
            {LOGMEIN("IR.cpp] 4224\n");
                if (Js::Configuration::Global.flags.Verbose || OpCodeAttr::TempNumberProducing(this->m_opcode))
                {LOGMEIN("IR.cpp] 4226\n");
                    if (this->dstIsTempNumberTransferred)
                    {LOGMEIN("IR.cpp] 4228\n");
                        Assert(this->dstIsTempNumber);
                        Output::Print(_u("x"));
                    }
                    else if (this->dstIsTempNumber)
                    {LOGMEIN("IR.cpp] 4233\n");
                        Output::Print(_u("#"));
                    }
                }
            }
            if (dumpMarkTempObject)
            {LOGMEIN("IR.cpp] 4239\n");
                if (Js::Configuration::Global.flags.Verbose || OpCodeAttr::TempObjectProducing(this->m_opcode))
                {LOGMEIN("IR.cpp] 4241\n");
                    if (this->dstIsTempObject)
                    {LOGMEIN("IR.cpp] 4243\n");
                        Output::Print(_u("o"));
                    }
                }
            }

            Output::Print(_u("tmp]"));
        }
        if(PHASE_DUMP(Js::TrackNegativeZeroPhase, m_func->GetTopFunc()) && !ShouldCheckForNegativeZero())
        {LOGMEIN("IR.cpp] 4252\n");
            Output::Print(_u("[-0]"));
        }
        if (PHASE_DUMP(Js::TypedArrayVirtualPhase, m_func->GetTopFunc()) && (!IsDstNotAlwaysConvertedToInt32() || !IsDstNotAlwaysConvertedToNumber()))
        {LOGMEIN("IR.cpp] 4256\n");
            if (!IsDstNotAlwaysConvertedToInt32())
                Output::Print(_u("[->i]"));
            else
                Output::Print(_u("[->n]"));

        }
        if(PHASE_DUMP(Js::TrackIntOverflowPhase, m_func->GetTopFunc()))
        {LOGMEIN("IR.cpp] 4264\n");
            // ignoring 32-bit overflow ?
            if(!ShouldCheckFor32BitOverflow())
            {LOGMEIN("IR.cpp] 4267\n");
                // ignoring 32-bits or more ?
                if(ShouldCheckForNon32BitOverflow())
                    Output::Print(_u("[OF %d]"), ignoreOverflowBitCount);
                else
                    Output::Print(_u("[OF]"));
            }
        }

        Output::SkipToColumn(20);
        Output::Print(_u("="));
    }

    PrintOpCodeName();

    if (this->IsBranchInstr())
    {LOGMEIN("IR.cpp] 4283\n");
        BranchInstr * branchInstr = this->AsBranchInstr();
        LabelInstr * targetInstr = branchInstr->GetTarget();
        bool labelPrinted = true;
        if (targetInstr == NULL)
        {LOGMEIN("IR.cpp] 4288\n");
            // Checking the 'm_isMultiBranch' field here directly as well to bypass asserting when tracing IR builder
            if(branchInstr->m_isMultiBranch && branchInstr->IsMultiBranch())
            {LOGMEIN("IR.cpp] 4291\n");
                IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();

                // If this MultiBranchInstr has been lowered to a machine instruction, which means
                // its opcode is not Js::OpCode::MultiBr, there is no need to print the labels.
                if (this->m_opcode == Js::OpCode::MultiBr)
                {LOGMEIN("IR.cpp] 4297\n");
                    multiBranchInstr->MapMultiBrLabels([](IR::LabelInstr * labelInstr) -> void
                    {
                        Output::Print(_u("$L%d "), labelInstr->m_id);
                    });
                }
                else
                {
                    labelPrinted = false;
                }
            }
            else
            {
                Output::Print(_u("??"));
            }
        }
        else
        {
            Output::Print(_u("$L%d"), targetInstr->m_id);
        }
        if (this->GetSrc1() && labelPrinted)
        {LOGMEIN("IR.cpp] 4318\n");
            Output::Print(_u(", "));
        }
    }
    else if (this->IsPragmaInstr() && this->m_opcode == Js::OpCode::StatementBoundary)
    {LOGMEIN("IR.cpp] 4323\n");
        Output::Print(_u("#%d"), this->AsPragmaInstr()->m_statementIndex);
    }

    // scope
    {LOGMEIN("IR.cpp] 4328\n");
        Opnd * src1 = this->GetSrc1();
        if (this->m_opcode == Js::OpCode::NewScFunc || this->m_opcode == Js::OpCode::NewScGenFunc)
        {LOGMEIN("IR.cpp] 4331\n");
            Assert(src1->IsIntConstOpnd());
            Js::ParseableFunctionInfo * function = nullptr;
            if (!m_func->IsOOPJIT())
            {LOGMEIN("IR.cpp] 4335\n");
                function = ((Js::ParseableFunctionInfo *)m_func->GetJITFunctionBody()->GetAddr())->GetNestedFunctionForExecution((uint)src1->AsIntConstOpnd()->GetValue())->GetParseableFunctionInfo();
            }
            Output::Print(_u("func:%s()"), function ? function->GetDisplayName() : _u("???"));
            Output::Print(_u(", env:"));
            this->GetSrc2()->AsRegOpnd()->m_sym->Dump(flags);
        }
        else if (src1)
        {LOGMEIN("IR.cpp] 4343\n");
            src1->Dump(flags, this->m_func);
            Opnd * src2 = this->GetSrc2();
            if (src2)
            {LOGMEIN("IR.cpp] 4347\n");
                Output::Print(_u(", "));
                src2->Dump(flags, this->m_func);
            }
        }
    }

    if (this->IsByteCodeUsesInstr())
    {LOGMEIN("IR.cpp] 4355\n");
        if (this->AsByteCodeUsesInstr()->GetByteCodeUpwardExposedUsed())
        {LOGMEIN("IR.cpp] 4357\n");
            bool first = true;
            FOREACH_BITSET_IN_SPARSEBV(id, this->AsByteCodeUsesInstr()->GetByteCodeUpwardExposedUsed())
            {LOGMEIN("IR.cpp] 4360\n");
                Output::Print(first? _u("s%d") : _u(", s%d"), id);
                first = false;
            }
            NEXT_BITSET_IN_SPARSEBV;
        }
        if (this->AsByteCodeUsesInstr()->propertySymUse)
        {LOGMEIN("IR.cpp] 4367\n");
            Output::Print(_u("  PropSym: %d"), this->AsByteCodeUsesInstr()->propertySymUse->m_id);
        }
    }

PrintByteCodeOffsetEtc:
    if (!AsmDumpMode && !SkipByteCodeOffset)
    {LOGMEIN("IR.cpp] 4374\n");
        this->DumpByteCodeOffset();
    }

    if (!SimpleForm)
    {LOGMEIN("IR.cpp] 4379\n");
        if (this->HasBailOutInfo() || this->HasAuxBailOut())
        {LOGMEIN("IR.cpp] 4381\n");
            BailOutInfo * bailOutInfo = this->GetBailOutInfo();
            Output::SkipToColumn(85);
            if (!AsmDumpMode)
            {LOGMEIN("IR.cpp] 4385\n");
                Output::Print(_u("Bailout: #%04x"), bailOutInfo->bailOutOffset);
            }
            if (!bailOutInfo->bailOutFunc->IsTopFunc())
            {LOGMEIN("IR.cpp] 4389\n");
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(_u(" Func %s"), bailOutInfo->bailOutFunc->GetDebugNumberSet(debugStringBuffer));
            }
            Output::Print(_u(" (%S)"), this->GetBailOutKindName());
        }
    }
    if ((flags & IRDumpFlags_SkipEndLine) == 0)
    {LOGMEIN("IR.cpp] 4397\n");
        Output::Print(_u("\n"));
    }
}

///----------------------------------------------------------------------------
///
/// LabelInstr::Dump
///
///     Dump this label.
///
///----------------------------------------------------------------------------

void
LabelInstr::Dump(IRDumpFlags flags)
{LOGMEIN("IR.cpp] 4412\n");
    if (this->m_block != NULL)
    {LOGMEIN("IR.cpp] 4414\n");
        this->m_block->DumpHeader();
    }
    Output::Print(_u("$L%d:"), this->m_id);
    if (this->isOpHelper)
    {LOGMEIN("IR.cpp] 4419\n");
        Output::Print(_u(" [helper]"));
    }
    if (this->m_isLoopTop)
    {LOGMEIN("IR.cpp] 4423\n");
        Output::Print(_u(" >>>>>>>>>>>>>  LOOP TOP  >>>>>>>>>>>>>"));
    }
    if (this->IsProfiledLabelInstr())
    {LOGMEIN("IR.cpp] 4427\n");
        Output::SkipToColumn(50);
        switch (this->AsProfiledLabelInstr()->loopImplicitCallFlags)
        {LOGMEIN("IR.cpp] 4430\n");
        case Js::ImplicitCall_HasNoInfo:
            Output::Print(_u("Implicit call: ???"));
            break;
        case Js::ImplicitCall_None:
            Output::Print(_u("Implicit call: no"));
            break;
        default:
            Output::Print(_u("Implicit call: yes"));
            break;
        }
    }
    if ((flags & (IRDumpFlags_AsmDumpMode | IRDumpFlags_SkipByteCodeOffset)) == 0)
    {LOGMEIN("IR.cpp] 4443\n");
        this->DumpByteCodeOffset();
    }
    Output::Print(_u("\n"));
}


void
PragmaInstr::Dump(IRDumpFlags flags)
{LOGMEIN("IR.cpp] 4452\n");
    if (Js::Configuration::Global.flags.PrintSrcInDump && this->m_opcode == Js::OpCode::StatementBoundary)
    {LOGMEIN("IR.cpp] 4454\n");
        Js::FunctionBody * functionBody = nullptr;
        if (!m_func->IsOOPJIT())
        {LOGMEIN("IR.cpp] 4457\n");
            functionBody = ((Js::FunctionBody*)m_func->GetJITFunctionBody()->GetAddr());
        }
        if (functionBody)
        {LOGMEIN("IR.cpp] 4461\n");
            functionBody->PrintStatementSourceLine(this->m_statementIndex);
        }
    }
    __super::Dump(flags);
}

///----------------------------------------------------------------------------
///
/// Instr::Dump
///
///     Dump a window of instructions around this instr.
///
///----------------------------------------------------------------------------

void
Instr::Dump(int window)
{LOGMEIN("IR.cpp] 4478\n");
    Instr * instr;
    int i;

    Output::Print(_u("-------------------------------------------------------------------------------"));

    if (this == NULL)
    {LOGMEIN("IR.cpp] 4485\n");
        return;
    }

    for (i = 0, instr = this; (instr->m_prev != NULL && i < window/2); instr = instr->m_prev, ++i)
    {LOGMEIN("IR.cpp] 4490\n");} // Nothing


    for (i = 0; (instr != nullptr && i < window); instr = instr->m_next, ++i)
    {LOGMEIN("IR.cpp] 4494\n");
        if (instr == this)
        {LOGMEIN("IR.cpp] 4496\n");
            Output::Print(_u("=>"));
        }
        instr->Dump();
    }
}

void
Instr::Dump()
{LOGMEIN("IR.cpp] 4505\n");
    this->Dump(IRDumpFlags_None);
}

void
Instr::DumpSimple()
{LOGMEIN("IR.cpp] 4511\n");
    this->Dump(IRDumpFlags_SimpleForm);
}

char16 *
Instr::DumpString()
{LOGMEIN("IR.cpp] 4517\n");
    Output::CaptureStart();
    this->Dump();
    return Output::CaptureEnd();
}

void
Instr::DumpRange(Instr *instrEnd)
{LOGMEIN("IR.cpp] 4525\n");
    Output::Print(_u("-------------------------------------------------------------------------------\n"));

    FOREACH_INSTR_IN_RANGE(instr, this, instrEnd)
    {LOGMEIN("IR.cpp] 4529\n");
        instr->Dump();
    }
    NEXT_INSTR_IN_RANGE;

    Output::Print(_u("-------------------------------------------------------------------------------\n"));
}

#endif

} // namespace IR
