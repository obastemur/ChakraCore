//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

namespace IR
{
void
Instr::Init(Js::OpCode opcode, IRKind kind, Func * func)
{TRACE_IT(6850);
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
{TRACE_IT(6851);
    Assert(m_func->HasByteCodeOffset());
    return m_number;
}

void
Instr::SetByteCodeOffset(uint32 offset)
{TRACE_IT(6852);
    Assert(m_func->HasByteCodeOffset());
    Assert(m_number == Js::Constants::NoByteCodeOffset);
    m_number = offset;
}

void
Instr::SetByteCodeOffset(IR::Instr * instr)
{TRACE_IT(6853);
    SetByteCodeOffset(instr->GetByteCodeOffset());
}

void
Instr::ClearByteCodeOffset()
{TRACE_IT(6854);
    Assert(m_func->HasByteCodeOffset());
    m_number = Js::Constants::NoByteCodeOffset;
}

uint32
Instr::GetNumber() const
{TRACE_IT(6855);
    Assert(m_func->HasInstrNumber());
    return m_number;
}

void
Instr::SetNumber(uint32 number)
{TRACE_IT(6856);
    Assert(m_func->HasInstrNumber());
    m_number = number;
}

bool
Instr::IsPlainInstr() const
{TRACE_IT(6857);
    return this->GetKind() == IR::InstrKindInstr;
}

bool
Instr::DoStackArgsOpt(Func *topFunc) const
{TRACE_IT(6858);
    return this->usesStackArgumentsObject && m_func->IsStackArgsEnabled();
}

bool
Instr::HasTypeCheckBailOut() const
{TRACE_IT(6859);
    return this->HasBailOutInfo() && IR::IsTypeCheckBailOutKind(this->GetBailOutKind());
}

bool
Instr::HasEquivalentTypeCheckBailOut() const
{TRACE_IT(6860);
    return this->HasBailOutInfo() && IR::IsEquivalentTypeCheckBailOutKind(this->GetBailOutKind());
}

void
Instr::ChangeEquivalentToMonoTypeCheckBailOut()
{TRACE_IT(6861);
    Assert(this->HasEquivalentTypeCheckBailOut());

    this->SetBailOutKind(IR::EquivalentToMonoTypeCheckBailOutKind(this->GetBailOutKind()));
}

intptr_t
Instr::TryOptimizeInstrWithFixedDataProperty(IR::Instr **pInstr, GlobOpt * globopt)
{TRACE_IT(6862);
    IR::Instr *&instr = *pInstr;
    Assert(OpCodeAttr::CanLoadFixedFields(instr->m_opcode));
    IR::Opnd * src1 = instr->GetSrc1();
    Assert(src1 && src1->IsSymOpnd() && src1->AsSymOpnd()->IsPropertySymOpnd());

    IR::PropertySymOpnd * propSymOpnd = src1->AsSymOpnd()->AsPropertySymOpnd();
    if (propSymOpnd->HasFixedValue() && !propSymOpnd->IsPoly())
    {TRACE_IT(6863);
        intptr_t fixedValue = propSymOpnd->GetFieldValueAsFixedData();
        Assert(instr->IsProfiledInstr());
        ValueType valType = instr->AsProfiledInstr()->u.FldInfo().valueType;
        if (fixedValue && ((Js::TaggedInt::Is(fixedValue) && (valType.IsUninitialized() || valType.IsLikelyInt())) || PHASE_ON1(Js::FixDataVarPropsPhase)))
        {TRACE_IT(6864);
            // Change Ld[Root]Fld to CheckFixedFld, which doesn't need a dst.
            instr->m_opcode = Js::OpCode::CheckFixedFld;
            IR::RegOpnd* dataValueDstOpnd = instr->UnlinkDst()->AsRegOpnd();
            if (globopt)
            {TRACE_IT(6865);
                globopt->GenerateBailAtOperation(&instr, !propSymOpnd->HasEquivalentTypeSet() ? IR::BailOutFailedFixedFieldTypeCheck : IR::BailOutFailedEquivalentFixedFieldTypeCheck);
            }
            else
            {TRACE_IT(6866);
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
{TRACE_IT(6867);
    Assert(compareInstr);
    if (this->GetKind() == compareInstr->GetKind()
        && this->m_opcode == compareInstr->m_opcode)
    {TRACE_IT(6868);
        IR::Opnd *dst = this->GetDst();
        IR::Opnd *src1 = this->GetSrc1();
        IR::Opnd *src2 = this->GetSrc2();
        IR::Opnd *compareDst = compareInstr->GetDst();
        IR::Opnd *compareSrc1 = compareInstr->GetSrc1();
        IR::Opnd *compareSrc2 = compareInstr->GetSrc2();

        // when both dst and compareDst are null, they are equal, same applies to src1, src2
        if ((dst != compareDst) && (!dst || !compareDst || !dst->IsEqual(compareDst)))
        {TRACE_IT(6869);
            return false;
        }
        if ((src1 != compareSrc1) && (!src1 || !compareSrc1 || !src1->IsEqual(compareSrc1)))
        {TRACE_IT(6870);
            return false;
        }
        if ((src2 != compareSrc2) && (!src2 || !compareSrc2 || !src2->IsEqual(compareSrc2)))
        {TRACE_IT(6871);
            return false;
        }

        return true;
    }
    else
    {TRACE_IT(6872);
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
{TRACE_IT(6873);
    Assert(!instr->IsLinked());

    Instr * prevInstr = this->m_prev;
    instr->m_prev = prevInstr;
    this->m_prev = instr;

    if (prevInstr)
    {TRACE_IT(6874);
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
{TRACE_IT(6875);
    Assert(!instr->IsLinked());

    Instr * nextInstr = this->m_next;
    instr->m_next = nextInstr;
    this->m_next = instr;

    if (nextInstr)
    {TRACE_IT(6876);
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
{TRACE_IT(6877);
    Instr * prevInstr = this->m_prev;

    startInstr->m_prev = prevInstr;
    this->m_prev = endInstr;

    if (prevInstr)
    {TRACE_IT(6878);
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
{TRACE_IT(6879);
    Instr *startInstr = endInstr->m_prev;

    if (startInstr) // more than one instruction to insert
    {TRACE_IT(6880);
        while (startInstr->m_prev)
        {TRACE_IT(6881);
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
{TRACE_IT(6882);
    Instr * nextInstr = this->m_next;

    endInstr->m_next = nextInstr;
    this->m_next = startInstr;

    if (nextInstr)
    {TRACE_IT(6883);
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
{TRACE_IT(6884);
    Instr *startInstr = endInstr->m_prev;

    if (startInstr) //more than one instruction to insert
    {TRACE_IT(6885);
        while (startInstr->m_prev)
        {TRACE_IT(6886);
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
{TRACE_IT(6887);
    AssertMsg(!this->IsLabelInstr() || !this->AsLabelInstr()->m_hasNonBranchRef,
        "Cannot free label with non-branch reference");

    switch (this->GetKind())
    {
    case InstrKindBranch:
        {TRACE_IT(6888);
            IR::BranchInstr *branchInstr = this->AsBranchInstr();
            branchInstr->ClearTarget();
            break;
        }
    }

    IR::Opnd * dstOpnd = this->GetDst();
    if (dstOpnd)
    {TRACE_IT(6889);
        StackSym * stackSym = dstOpnd->GetStackSym();
        if (stackSym)
        {TRACE_IT(6890);
            if (stackSym->m_isSingleDef)
            {TRACE_IT(6891);
                Assert(!stackSym->m_isEncodedConstant);
                if (stackSym->m_instrDef == this)
                {TRACE_IT(6892);
                    Assert(!dstOpnd->isFakeDst);
                    if (stackSym->IsConst())
                    {TRACE_IT(6893);
                        // keep the instruction around so we can get the constant value
                        // from the symbol
                        return;
                    }
                    Assert(this->m_func->GetTopFunc()->allowRemoveBailOutArgInstr || !stackSym->m_isBailOutReferenced);
                    stackSym->m_instrDef = nullptr;
                }
                else
                {TRACE_IT(6894);
                    Assert(dstOpnd->isFakeDst);
                }
            }
            else
            {TRACE_IT(6895);
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
{TRACE_IT(6896);
    m_prev->m_next = m_next;
    if (m_next)
    {TRACE_IT(6897);
        m_next->m_prev = m_prev;
    }
    else
    {TRACE_IT(6898);
        Assert(this == this->m_func->m_tailInstr);
    }

#if DBG_DUMP
    // Transferring the globOptInstrString to the next non-Label Instruction
    if(this->globOptInstrString != nullptr && m_next && m_next->globOptInstrString == nullptr && !m_next->IsLabelInstr())
    {TRACE_IT(6899);
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
{TRACE_IT(6900);
    this->Unlink();
    this->Free();
}

void
Instr::SwapOpnds()
{TRACE_IT(6901);
    IR::Opnd *opndTemp = m_src1;
    m_src1 = m_src2;
    m_src2 = opndTemp;
}

// Copy a vanilla instruction.
Instr *
Instr::Copy()
{TRACE_IT(6902);
    Instr * instrCopy;

    if (this->HasBailOutInfo() || this->HasAuxBailOut())
    {TRACE_IT(6903);
        instrCopy = BailOutInstr::New(this->m_opcode, this->GetBailOutKind(), this->GetBailOutInfo(), this->m_func);
        instrCopy->SetByteCodeOffset(this->GetByteCodeOffset());
        if (this->HasAuxBailOut())
        {TRACE_IT(6904);
            instrCopy->hasAuxBailOut = true;
            instrCopy->SetAuxBailOutKind(this->GetAuxBailOutKind());
        }
    }
    else
    {TRACE_IT(6905);
        switch (this->GetKind())
        {
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
    {TRACE_IT(6906);
        instrCopy->SetDst(opnd->Copy(this->m_func));
    }
    opnd = this->GetSrc1();
    if (opnd)
    {TRACE_IT(6907);
        instrCopy->SetSrc1(opnd->Copy(this->m_func));
        opnd = this->GetSrc2();
        if (opnd)
        {TRACE_IT(6908);
            instrCopy->SetSrc2(opnd->Copy(this->m_func));
        }
    }

    instrCopy->isInlineeEntryInstr = this->isInlineeEntryInstr;

    if (this->m_func->DoMaintainByteCodeOffset())
    {TRACE_IT(6909);
        instrCopy->SetByteCodeOffset(this->GetByteCodeOffset());
    }
    instrCopy->usesStackArgumentsObject = this->usesStackArgumentsObject;
    return instrCopy;
}

LabelInstr *
LabelInstr::CloneLabel(BOOL fCreate)
{TRACE_IT(6910);
    Func * func = this->m_func;
    Cloner * cloner = func->GetCloner();
    IR::LabelInstr * instrLabel = nullptr;

    AssertMsg(cloner, "Use Func::BeginClone to initialize cloner");

    if (cloner->labelMap == nullptr)
    {TRACE_IT(6911);
        if (!fCreate)
        {TRACE_IT(6912);
            return nullptr;
        }
        cloner->labelMap = HashTable<LabelInstr*>::New(cloner->alloc, 7);
    }
    else
    {TRACE_IT(6913);
        IR::LabelInstr ** map = cloner->labelMap->Get(this->m_id);
        if (map)
        {TRACE_IT(6914);
            instrLabel = *map;
        }
    }

    if (instrLabel == nullptr)
    {TRACE_IT(6915);
        if (!fCreate)
        {TRACE_IT(6916);
            return nullptr;
        }
        if (this->IsProfiledLabelInstr())
        {TRACE_IT(6917);
            instrLabel = IR::ProfiledLabelInstr::New(this->m_opcode, func, this->AsProfiledLabelInstr()->loopImplicitCallFlags, this->AsProfiledLabelInstr()->loopFlags);
#if DBG
            instrLabel->AsProfiledLabelInstr()->loopNum = this->AsProfiledLabelInstr()->loopNum;
#endif
        }
        else
        {TRACE_IT(6918);
            instrLabel = IR::LabelInstr::New(this->m_opcode, func, this->isOpHelper);
        }
        instrLabel->m_isLoopTop = this->m_isLoopTop;
        cloner->labelMap->FindOrInsert(instrLabel, this->m_id);
    }

    return instrLabel;
}

ProfiledLabelInstr::ProfiledLabelInstr(JitArenaAllocator * allocator)
    : LabelInstr(allocator)
{TRACE_IT(6919);
}

ProfiledLabelInstr *
ProfiledLabelInstr::New(Js::OpCode opcode, Func *func, Js::ImplicitCallFlags flags, Js::LoopFlags loopFlags)
{TRACE_IT(6920);
    ProfiledLabelInstr * profiledLabelInstr = JitAnew(func->m_alloc, ProfiledLabelInstr, func->m_alloc);
    profiledLabelInstr->Init(opcode, InstrKindProfiledLabel, func, false);
    profiledLabelInstr->loopImplicitCallFlags = flags;
    profiledLabelInstr->loopFlags = loopFlags;
    return profiledLabelInstr;
}

void
BranchInstr::RetargetClonedBranch()
{TRACE_IT(6921);
    IR::LabelInstr * instrLabel = this->m_branchTarget->CloneLabel(false);
    if (instrLabel == nullptr)
    {TRACE_IT(6922);
        // Jumping outside the cloned range. No retarget.
        return;
    }

    this->SetTarget(instrLabel);
}

PragmaInstr *
PragmaInstr::ClonePragma()
{TRACE_IT(6923);
    return this->CopyPragma();
}

PragmaInstr *
PragmaInstr::CopyPragma()
{TRACE_IT(6924);
    IR::PragmaInstr * instrPragma = IR::PragmaInstr::New(this->m_opcode, 0, this->m_func);
    return instrPragma;
}

Instr *
Instr::CloneInstr() const
{TRACE_IT(6925);
    if (this->HasBailOutInfo() || this->HasAuxBailOut())
    {TRACE_IT(6926);
        return ((BailOutInstr *)this)->CloneBailOut();
    }

    IR::Instr *clone = IR::Instr::New(this->m_opcode, this->m_func);
    clone->isInlineeEntryInstr = this->isInlineeEntryInstr;

    return clone;
}

// Clone a vanilla instruction, replacing single-def StackSym's with new syms where appropriate.
Instr *
Instr::Clone()
{TRACE_IT(6927);
    Func * func = this->m_func;
    Cloner *cloner = func->GetCloner();
    IR::Instr * instrClone;
    IR::Opnd * opnd;

    switch (this->GetKind())
    {
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
    {TRACE_IT(6928);
        instrClone->SetDst(opnd->CloneDef(func));
    }
    opnd = this->GetSrc1();
    if (opnd)
    {TRACE_IT(6929);
        instrClone->SetSrc1(opnd->CloneUse(func));
        opnd = this->GetSrc2();
        if (opnd)
        {TRACE_IT(6930);
            instrClone->SetSrc2(opnd->CloneUse(func));
        }
    }
    if (this->m_func->DoMaintainByteCodeOffset())
    {TRACE_IT(6931);
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
{TRACE_IT(6932);
    IR::Instr * instrReturn = instrAfter;

    Func * topFunc = instrStart->m_func->GetTopFunc();
    topFunc->BeginClone(lowerer, alloc);
    topFunc->GetCloner()->clonedInstrGetOrigArgSlotSym = clonedInstrGetOrigArgSlotSym;

    FOREACH_INSTR_IN_RANGE(instr, instrStart, instrLast)
    {TRACE_IT(6933);
        Instr * instrClone = instr->Clone();
        instrAfter->InsertAfter(instrClone);
        instrAfter = instrClone;
        instr->isCloned = true;
        if (fMapTest(instrClone))
        {TRACE_IT(6934);
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
{TRACE_IT(6935);
    if (instrLast->m_next != nullptr)
    {TRACE_IT(6936);
        instrLast->m_next->m_prev = instrStart->m_prev;
    }
    else
    {TRACE_IT(6937);
        instrLast->m_func->m_tailInstr = instrStart->m_prev;
    }

    if (instrStart->m_prev != nullptr)
    {TRACE_IT(6938);
        instrStart->m_prev->m_next = instrLast->m_next;
    }
    else
    {TRACE_IT(6939);
        instrStart->m_func->m_headInstr = instrLast->m_next;
    }

    instrStart->m_prev = instrAfter;
    instrLast->m_next = instrAfter->m_next;
    if (instrAfter->m_next != nullptr)
    {TRACE_IT(6940);
        instrAfter->m_next->m_prev = instrLast;
    }
    else
    {TRACE_IT(6941);
        instrAfter->m_func->m_tailInstr = instrLast;
    }
    instrAfter->m_next = instrStart;
}

JitProfilingInstr *
JitProfilingInstr::New(Js::OpCode opcode, Opnd *dstOpnd, Opnd *src1Opnd, Opnd *src2Opnd, Func * func)
{TRACE_IT(6942);
    JitProfilingInstr * profiledInstr = JitProfilingInstr::New(opcode, dstOpnd, src1Opnd, func);
    profiledInstr->SetSrc2(src2Opnd);

    return profiledInstr;
}

JitProfilingInstr *
JitProfilingInstr::New(Js::OpCode opcode, Opnd *dstOpnd, Opnd *src1Opnd, Func * func)
{TRACE_IT(6943);
    Assert(func->DoSimpleJitDynamicProfile());

    JitProfilingInstr * profiledInstr = JitAnew(func->m_alloc, IR::JitProfilingInstr);
    profiledInstr->Init(opcode, InstrKindJitProfiling, func);

    if (dstOpnd)
    {TRACE_IT(6944);
        profiledInstr->SetDst(dstOpnd);
    }
    if (src1Opnd)
    {TRACE_IT(6945);
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
{TRACE_IT(6946);
    // Adapted from Profiled::CloneProfiledInstr. Note that the dst and srcs are not set.

    Assert(!(this->HasBailOutInfo() || this->HasAuxBailOut())); // Shouldn't have bailout info in a jitprofiling instr

    return this->CopyJitProfiling();
}


JitProfilingInstr*
JitProfilingInstr::CopyJitProfiling() const
{TRACE_IT(6947);
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
{TRACE_IT(6948);
    ProfiledInstr * profiledInstr = ProfiledInstr::New(opcode, dstOpnd, src1Opnd, func);
    profiledInstr->SetSrc2(src2Opnd);

    return profiledInstr;
}

ProfiledInstr *
ProfiledInstr::New(Js::OpCode opcode, Opnd *dstOpnd, Opnd *src1Opnd, Func * func)
{TRACE_IT(6949);
    ProfiledInstr * profiledInstr = JitAnew(func->m_alloc, IR::ProfiledInstr);
    profiledInstr->Init(opcode, InstrKindProfiled, func);

    if (dstOpnd)
    {TRACE_IT(6950);
        profiledInstr->SetDst(dstOpnd);
    }
    if (src1Opnd)
    {TRACE_IT(6951);
        profiledInstr->SetSrc1(src1Opnd);
    }

    profiledInstr->u.ldElemInfo = nullptr;
    return profiledInstr;
}

ProfiledInstr *
ProfiledInstr::CloneProfiledInstr() const
{TRACE_IT(6952);
    IR::ProfiledInstr * profiledInstr;
    if (this->HasBailOutInfo() || this->HasAuxBailOut())
    {TRACE_IT(6953);
        profiledInstr = ((ProfiledBailOutInstr *)this)->CloneBailOut();
        profiledInstr->u = this->u;
    }
    else
    {TRACE_IT(6954);
        profiledInstr = this->CopyProfiledInstr();
    }

    return profiledInstr;
}

ProfiledInstr *
ProfiledInstr::CopyProfiledInstr() const
{TRACE_IT(6955);
    IR::ProfiledInstr * profiledInstr;

    profiledInstr = JitAnew(this->m_func->m_alloc, IR::ProfiledInstr);
    profiledInstr->Init(this->m_opcode, InstrKindProfiled, this->m_func);
    profiledInstr->u = this->u;

    return profiledInstr;
}

ByteCodeUsesInstr *
ByteCodeUsesInstr::New(IR::Instr * originalBytecodeInstr)
{TRACE_IT(6956);
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
{TRACE_IT(6957);
    ByteCodeUsesInstr * byteCodeUses = JitAnew(func->m_alloc, IR::ByteCodeUsesInstr);
    byteCodeUses->Init(Js::OpCode::ByteCodeUses, InstrKindByteCodeUses, func);
    byteCodeUses->byteCodeUpwardExposedUsed = nullptr;
    byteCodeUses->propertySymUse = nullptr;
    byteCodeUses->SetByteCodeOffset(offset);
    return byteCodeUses;
}

const BVSparse<JitArenaAllocator> * ByteCodeUsesInstr::GetByteCodeUpwardExposedUsed() const
{TRACE_IT(6958);
    return this->byteCodeUpwardExposedUsed;
}

// In the case of instances where you would like to add a ByteCodeUses to some sym,
// which doesn't have an operand associated with it (like a block closure sym), use
// this to set it without needing to pass the check for JIT-Optimized registers.
void ByteCodeUsesInstr::SetNonOpndSymbol(uint symId)
{TRACE_IT(6959);
    if (!this->byteCodeUpwardExposedUsed)
    {TRACE_IT(6960);
        this->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
    }
    this->byteCodeUpwardExposedUsed->Set(symId);
}

// In cases where the operand you're working on may be changed between when you get
// access to it and when you determine that you can set it in the ByteCodeUsesInstr
// set method, cache the values and use this caller.
void ByteCodeUsesInstr::SetRemovedOpndSymbol(bool isJITOptimizedReg, uint symId)
{TRACE_IT(6961);
    if (isJITOptimizedReg)
    {
        AssertMsg(false, "Tried to add a jit-optimized register to a ByteCodeUses instruction!");
        // Although we assert on debug builds, we should actually be ok with release builds
        // if we ignore the operand; not ignoring it, however, can cause us to introduce an
        // inconsistency in bytecode register lifetimes.
        return;
    }
    if(!this->byteCodeUpwardExposedUsed)
    {TRACE_IT(6962);
        this->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
    }
    this->byteCodeUpwardExposedUsed->Set(symId);
}

void ByteCodeUsesInstr::Set(IR::Opnd * originalOperand)
{TRACE_IT(6963);
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
    {TRACE_IT(6964);
        this->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
    }
    this->byteCodeUpwardExposedUsed->Set(symId);
}

void ByteCodeUsesInstr::Clear(uint symId)
{TRACE_IT(6965);
    Assert(byteCodeUpwardExposedUsed != nullptr);
    this->byteCodeUpwardExposedUsed->Clear(symId);
}

void ByteCodeUsesInstr::SetBV(BVSparse<JitArenaAllocator>* newbv)
{TRACE_IT(6966);
    Assert(byteCodeUpwardExposedUsed == nullptr && newbv != nullptr);
    byteCodeUpwardExposedUsed = newbv;
}

// If possible, we want to aggregate with subsequent ByteCodeUses Instructions, so
// that we can do some optimizations in other places where we can simplify args in
// a compare, but still need to generate them for bailouts. Without this, we cause
// problems because we end up with an instruction losing atomicity in terms of its
// bytecode use and generation lifetimes.
void ByteCodeUsesInstr::Aggregate()
{TRACE_IT(6967);
    IR::Instr* scanner = this->m_next;
    while (scanner && scanner->m_opcode == Js::OpCode::ByteCodeUses && scanner->GetByteCodeOffset() == this->GetByteCodeOffset() && scanner->GetDst() == nullptr)
    {TRACE_IT(6968);
        IR::ByteCodeUsesInstr* target = scanner->AsByteCodeUsesInstr();
        Assert(this->m_func == target->m_func);
        if (target->byteCodeUpwardExposedUsed)
        {TRACE_IT(6969);
            if (this->byteCodeUpwardExposedUsed)
            {TRACE_IT(6970);
                this->byteCodeUpwardExposedUsed->Or(target->byteCodeUpwardExposedUsed);
                JitAdelete(target->byteCodeUpwardExposedUsed->GetAllocator(), target->byteCodeUpwardExposedUsed);
                target->byteCodeUpwardExposedUsed = nullptr;
            }
            else
            {TRACE_IT(6971);
                this->byteCodeUpwardExposedUsed = target->byteCodeUpwardExposedUsed;
                target->byteCodeUpwardExposedUsed = nullptr;
            }
        }
        scanner = scanner->m_next;
    }
}

BailOutInfo *
Instr::GetBailOutInfo() const
{TRACE_IT(6972);
    Assert(this->HasBailOutInfo() || this->HasAuxBailOut());
    switch (this->m_kind)
    {
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
{TRACE_IT(6973);
    Assert(this->HasBailOutInfo());
    switch (this->m_kind)
    {
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
{TRACE_IT(6974);
    return GetBailOutKind() & ~IR::BailOutKindBits;
}

BailOutKind
Instr::GetAuxBailOutKind() const
{TRACE_IT(6975);
    Assert(this->HasAuxBailOut());
    switch (this->m_kind)
    {
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
{TRACE_IT(6976);
    Assert(this->HasBailOutInfo());
    Assert(bailOutKind != IR::BailOutInvalid);
    this->SetBailOutKind_NoAssert(bailOutKind);
}

// Helper to set bail out kind, doesn't assert.
void Instr::SetBailOutKind_NoAssert(const IR::BailOutKind bailOutKind)
{TRACE_IT(6977);
    Assert(IsValidBailOutKindAndBits(bailOutKind));
    switch (this->m_kind)
    {
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
{TRACE_IT(6978);
    switch (this->m_kind)
    {
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
{TRACE_IT(6979);
    BailOutInfo *bailOutInfo;
    Assert(this->HasBailOutInfo() || this->HasAuxBailOut());

    switch (this->m_kind)
    {
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
    {TRACE_IT(6980);
        bailOutInfo->bailOutInstr = nullptr;
    }
#endif
    this->hasBailOutInfo = false;
    this->hasAuxBailOut = false;

    return bailOutInfo;
}

bool
Instr::ReplaceBailOutInfo(BailOutInfo *newBailOutInfo)
{TRACE_IT(6981);
    BailOutInfo *oldBailOutInfo;
    bool deleteOld = false;

#if DBG
    newBailOutInfo->wasCopied = true;
#endif

    Assert(this->HasBailOutInfo() || this->HasAuxBailOut());
    switch (this->m_kind)
    {
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
    {TRACE_IT(6982);
        JitArenaAllocator * alloc = this->m_func->m_alloc;
        oldBailOutInfo->Clear(alloc);
        JitAdelete(alloc, oldBailOutInfo);
        deleteOld = true;
    }

    return deleteOld;
}

IR::Instr *Instr::ShareBailOut()
{TRACE_IT(6983);
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
{TRACE_IT(6984);
#ifdef _M_IX86
    // The StartCall instruction is being deleted, or is being moved and may later be deleted,
    // so remove its references from bailouts in the given range.
    // This only happens during cloning, which is rare, and only across the range of instructions
    // that evaluate outgoing arguments, which is long only in synthetic cases.

    Assert(this->m_opcode == Js::OpCode::StartCall);

    if (!this->m_func->hasBailout)
    {TRACE_IT(6985);
        return;
    }

    FOREACH_INSTR_IN_RANGE(instr, this->m_next, endInstr)
    {TRACE_IT(6986);
        if (instr->HasBailOutInfo())
        {TRACE_IT(6987);
            BailOutInfo *bailOutInfo = instr->GetBailOutInfo();
            bailOutInfo->UnlinkStartCall(this);
        }
    }
    NEXT_INSTR_IN_RANGE;
#endif
}

Opnd *Instr::FindCallArgumentOpnd(const Js::ArgSlot argSlot, IR::Instr * *const ownerInstrRef)
{TRACE_IT(6988);
    Assert(OpCodeAttr::CallInstr(m_opcode));
    Assert(argSlot != static_cast<Js::ArgSlot>(0));

    IR::Instr *argInstr = this;
    Assert(argInstr->GetSrc2());
    Assert(argInstr->GetSrc2()->IsSymOpnd());
    do
    {TRACE_IT(6989);
        StackSym *const linkSym = argInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym();
        Assert(linkSym->IsSingleDef());
        Assert(linkSym->IsArgSlotSym());

        argInstr = linkSym->m_instrDef;
        Assert(argInstr->GetSrc2());
        if(argInstr->m_opcode == Js::OpCode::ArgOut_A_InlineSpecialized)
        {TRACE_IT(6990);
            // This is a fake ArgOut, skip it
            continue;
        }

        if(linkSym->GetArgSlotNum() == argSlot)
        {TRACE_IT(6991);
            if(ownerInstrRef)
            {TRACE_IT(6992);
                *ownerInstrRef = argInstr;
            }
            return argInstr->GetSrc1();
        }
    } while(argInstr->GetSrc2()->IsSymOpnd());
    return nullptr;
}


bool
Instr::FetchOperands(_Out_writes_(argsOpndLength) IR::Opnd **argsOpnd, uint argsOpndLength)
{TRACE_IT(6993);
    return this->ForEachCallDirectArgOutInstrBackward([&](IR::Instr *argOutInstr, uint argNum)
    {
        argsOpnd[argNum] = argOutInstr->GetSrc1();
        return argNum == 0;
    }, argsOpndLength);
}

bool Instr::ShouldCheckForNegativeZero() const
{TRACE_IT(6994);
    return !ignoreNegativeZero;
}

bool Instr::IsDstNotAlwaysConvertedToInt32() const
{TRACE_IT(6995);
    return !dstIsAlwaysConvertedToInt32;
}

bool Instr::IsDstNotAlwaysConvertedToNumber() const
{TRACE_IT(6996);
    return !dstIsAlwaysConvertedToNumber;
}

bool Instr::ShouldCheckForIntOverflow() const
{TRACE_IT(6997);
    return ShouldCheckFor32BitOverflow() || ShouldCheckForNon32BitOverflow();
}

bool Instr::ShouldCheckFor32BitOverflow() const
{TRACE_IT(6998);
    return !(ignoreIntOverflow || ignoreIntOverflowInRange);
}

bool Instr::ShouldCheckForNon32BitOverflow() const
{TRACE_IT(6999);
    return ignoreOverflowBitCount != 32;
}

template <typename InstrType> struct IRKindMap;

template <> struct IRKindMap<IR::Instr> { static const IRKind InstrKind = InstrKindInstr; };
template <> struct IRKindMap<IR::ProfiledInstr> { static const IRKind InstrKind = InstrKindProfiled; };
template <> struct IRKindMap<IR::BranchInstr> { static const IRKind InstrKind = InstrKindBranch; };

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::New(Js::OpCode opcode, BailOutKind kind, IR::Instr * bailOutTarget, Func * func)
{TRACE_IT(7000);
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
{TRACE_IT(7001);
    BailOutInstrTemplate *instr = BailOutInstrTemplate::New(opcode, kind, bailOutTarget, func);
    instr->SetDst(dst);

    return instr;
}

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::New(Js::OpCode opcode, IR::Opnd *dst, IR::Opnd *src1, BailOutKind kind, IR::Instr * bailOutTarget, Func * func)
{TRACE_IT(7002);
    BailOutInstrTemplate *instr = BailOutInstrTemplate::New(opcode, dst, kind, bailOutTarget, func);
    instr->SetSrc1(src1);

    return instr;
}

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::New(Js::OpCode opcode, IR::Opnd *dst, IR::Opnd *src1, IR::Opnd *src2, BailOutKind kind, IR::Instr * bailOutTarget, Func * func)
{TRACE_IT(7003);
    BailOutInstrTemplate *instr = BailOutInstrTemplate::New(opcode, dst, src1, kind, bailOutTarget, func);
    instr->SetSrc2(src2);

    return instr;
}

template <typename InstrType>
BailOutInstrTemplate<InstrType> *
BailOutInstrTemplate<InstrType>::New(Js::OpCode opcode, BailOutKind kind, BailOutInfo * bailOutInfo, Func * func)
{TRACE_IT(7004);
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
    {TRACE_IT(7005);
        bailOutInfo->bailOutInstr = bailOutInstr;
    }
    else if (bailOutInfo->sharedBailOutKind)
    {TRACE_IT(7006);
        if (bailOutInfo->bailOutInstr->HasBailOutInfo())
        {TRACE_IT(7007);
            bailOutInfo->sharedBailOutKind = bailOutInfo->bailOutInstr->GetBailOutKind() ==  kind;
        }
        else
        {TRACE_IT(7008);
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
{TRACE_IT(7009);
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
{TRACE_IT(7010);
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
{TRACE_IT(7011);
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
{TRACE_IT(7012);
    LabelInstr * labelInstr;

    labelInstr = JitAnew(func->m_alloc, IR::LabelInstr, func->m_alloc);
    labelInstr->Init(opcode, InstrKindLabel, func, isOpHelper);
    return labelInstr;
}

void
LabelInstr::Init(Js::OpCode opcode, IRKind kind, Func *func, bool isOpHelper)
{TRACE_IT(7013);
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
{TRACE_IT(7014);
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
    {TRACE_IT(7015);
        if (branchEntry == branchRef)
        {TRACE_IT(7016);
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
{TRACE_IT(7017);
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
{TRACE_IT(7018);
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
{TRACE_IT(7019);
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
{TRACE_IT(7020);
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
{TRACE_IT(7021);
   MultiBranchInstr * multiBranchInstr;

   multiBranchInstr = MultiBranchInstr::New(opcode, func);

   multiBranchInstr->SetSrc1(srcOpnd);

   return multiBranchInstr;
}

MultiBranchInstr *
MultiBranchInstr::New(Js::OpCode opcode, Func * func)
{TRACE_IT(7022);
   JitArenaAllocator * m_funcAlloc = func->m_alloc;

   MultiBranchInstr * multiBranchInstr;

   multiBranchInstr = JitAnew(m_funcAlloc, IR::MultiBranchInstr);

   multiBranchInstr->Init(opcode, InstrKindBranch, func);

   return multiBranchInstr;
}

bool
BranchInstr::ReplaceTarget(IR::LabelInstr * oldLabelInstr, IR::LabelInstr * newLabelInstr)
{TRACE_IT(7023);
    if (this->IsMultiBranch())
    {TRACE_IT(7024);
        return this->AsMultiBrInstr()->ReplaceTarget(oldLabelInstr, newLabelInstr);
    }
    if (this->GetTarget() == oldLabelInstr)
    {TRACE_IT(7025);
        this->SetTarget(newLabelInstr);
        return true;
    }
    return false;
}

bool
MultiBranchInstr::ReplaceTarget(IR::LabelInstr * oldLabelInstr, IR::LabelInstr * newLabelInstr)
{TRACE_IT(7026);
    Assert(this->IsMultiBranch());
    bool remapped = false;
    this->UpdateMultiBrLabels([=, &remapped](IR::LabelInstr * targetLabel) -> IR::LabelInstr *
    {
        if (targetLabel == oldLabelInstr)
        {TRACE_IT(7027);
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
{TRACE_IT(7028);
    Assert(IsMultiBranch());

    MapMultiBrLabels([&](LabelInstr *const targetLabel)
    {
        ChangeLabelRef(targetLabel, nullptr);
    });
    m_branchTargets = nullptr;
}

BranchInstr *
BranchInstr::CloneBranchInstr() const
{TRACE_IT(7029);
    AssertMsg(!this->IsMultiBranch(),"Cloning Not supported for MultiBranchInstr");
    Func * func = this->m_func;
    // See if the target has already been cloned.
    IR::LabelInstr * instrLabel = this->GetTarget()->CloneLabel(false);
    if (instrLabel == nullptr)
    {TRACE_IT(7030);
        // We didn't find a clone for this label.
        // We'll go back and retarget the cloned branch if the target turns up in the cloned range.
        instrLabel = this->GetTarget();
        func->GetCloner()->fRetargetClonedBranch = TRUE;
    }
    return IR::BranchInstr::New(this->m_opcode, instrLabel, func);
}

void
BranchInstr::Invert()
{TRACE_IT(7031);
    /*
     * If one of the operands to a relational operator is 'undefined', the result
     * is always false. Don't invert such branches as they result in a jump to
     * the wrong target.
     */

    switch (this->m_opcode)
    {
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
{TRACE_IT(7032);
    Assert(func->isPostLower);
    IR::LabelInstr * target = this->GetTarget();
    if (!target->m_isLoopTop)
    {TRACE_IT(7033);
        return false;
    }

    IR::BranchInstr * lastBranchInstr = nullptr;
    uint32 lastBranchNum = 0;
    FOREACH_SLISTCOUNTED_ENTRY(IR::BranchInstr *, ref, &target->labelRefs)
    {TRACE_IT(7034);
        if (ref->GetNumber() > lastBranchNum)
        {TRACE_IT(7035);
            lastBranchInstr = ref;
            lastBranchNum = lastBranchInstr->GetNumber();
        }
    }
    NEXT_SLISTCOUNTED_ENTRY;

    if (this == lastBranchInstr)
    {TRACE_IT(7036);
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
{TRACE_IT(7037);
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
{TRACE_IT(7038);
    // Currently the only pragma instructions are for Source Info
    Assert(this->m_func->GetTopFunc()->DoRecordNativeMap());
    if (!m_func->IsOOPJIT())
    {TRACE_IT(7039);
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
{TRACE_IT(7040);
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
{TRACE_IT(7041);
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
{TRACE_IT(7042);
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
{TRACE_IT(7043);
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
{TRACE_IT(7044);
    AssertMsg(newDst != nullptr, "Calling SetDst with a NULL dst");
    AssertMsg(this->m_dst == nullptr, "Calling SetDst without unlinking/freeing the current dst");
    Assert(!(newDst->IsRegOpnd() && newDst->AsRegOpnd()->IsSymValueFrozen()));

    newDst = newDst->Use(m_func);
    this->m_dst = newDst;

    // If newDst isSingleDef, set instrDef

    StackSym *stackSym;

    if (newDst->IsRegOpnd() && newDst->AsRegOpnd()->m_sym)
    {TRACE_IT(7045);
        stackSym = newDst->AsRegOpnd()->m_sym->AsStackSym();
    }
    else if (newDst->IsSymOpnd() && newDst->AsSymOpnd()->m_sym->IsStackSym())
    {TRACE_IT(7046);
        stackSym = newDst->AsSymOpnd()->m_sym->AsStackSym();
    }
    else
    {TRACE_IT(7047);
        stackSym = nullptr;
    }

    if (stackSym && stackSym->m_isSingleDef)
    {TRACE_IT(7048);
        if (stackSym->m_instrDef)
        {TRACE_IT(7049);
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
        {TRACE_IT(7050);
            stackSym->m_instrDef = this;
        }
    }

    return newDst;
}

Opnd *
Instr::SetFakeDst(Opnd * newDst)
{TRACE_IT(7051);
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
{TRACE_IT(7052);
    Opnd * oldDst = this->m_dst;
    StackSym *stackSym = nullptr;

    // If oldDst isSingleDef, clear instrDef

    if (oldDst->IsRegOpnd())
    {TRACE_IT(7053);
        stackSym = oldDst->AsRegOpnd()->m_sym;
    }
    else if (oldDst->IsSymOpnd())
    {TRACE_IT(7054);
        Sym *sym = oldDst->AsSymOpnd()->m_sym;
        if (sym->IsStackSym())
        {TRACE_IT(7055);
            stackSym = sym->AsStackSym();
        }
    }

#if DBG
    if (oldDst->isFakeDst)
    {TRACE_IT(7056);
        oldDst->isFakeDst = false;
    }
#endif
    if (stackSym && stackSym->m_isSingleDef)
    {TRACE_IT(7057);
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
{TRACE_IT(7058);
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
{TRACE_IT(7059);
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
{TRACE_IT(7060);
    return SinkDst(assignOpcode, StackSym::New(TyVar, m_func), regNum, insertAfterInstr);
}

Instr *
Instr::SinkDst(Js::OpCode assignOpcode, StackSym * stackSym, RegNum regNum, IR::Instr *insertAfterInstr)
{TRACE_IT(7061);
    if(!insertAfterInstr)
    {TRACE_IT(7062);
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
{TRACE_IT(7063);
    // Move this instruction down to the target location, preserving
    // the use(s), if necessary, from redefinition between the original
    // location and the new one.

    if (this->m_next == instrTarget)
    {TRACE_IT(7064);
        return this->m_prev;
    }

    StackSym *sym;
    if (this->m_src1)
    {TRACE_IT(7065);
        sym = this->m_src1->GetStackSym();
        if (sym && !sym->m_isSingleDef)
        {TRACE_IT(7066);
            this->HoistSrc1(Js::OpCode::Ld_A);
        }

        if (this->m_src2)
        {TRACE_IT(7067);
            sym = this->m_src2->GetStackSym();
            if (sym && !sym->m_isSingleDef)
            {TRACE_IT(7068);
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
{TRACE_IT(7069);
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
{TRACE_IT(7070);
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
{TRACE_IT(7071);
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
{TRACE_IT(7072);
    Opnd *oldSrc, *newSrc;
    Instr * newInstr;
    IRType type;

    oldSrc = this->UnlinkSrc1();
    type = oldSrc->GetType();

    const bool creatingNewSym = !newSym;
    if(creatingNewSym)
    {TRACE_IT(7073);
        newSym = StackSym::New(type, m_func);
    }
    newSrc = this->SetSrc1(RegOpnd::New(newSym, regNum, type, m_func));
    newSrc->SetValueType(oldSrc->GetValueType());

    newInstr = Instr::New(assignOpcode, newSrc, oldSrc, m_func);
    this->InsertBefore(newInstr);

    if(creatingNewSym)
    {TRACE_IT(7074);
        if (oldSrc->IsRegOpnd())
        {TRACE_IT(7075);
            newSym->CopySymAttrs(oldSrc->AsRegOpnd()->m_sym);
        }
        else if (oldSrc->IsImmediateOpnd())
        {TRACE_IT(7076);
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
{TRACE_IT(7077);
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
{TRACE_IT(7078);
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
{TRACE_IT(7079);
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
{TRACE_IT(7080);
    Opnd *oldSrc, *newSrc;
    Instr * newInstr;
    IRType type;

    oldSrc = this->UnlinkSrc2();
    type = oldSrc->GetType();

    const bool creatingNewSym = !newSym;
    if(creatingNewSym)
    {TRACE_IT(7081);
        newSym = StackSym::New(type, m_func);
    }
    newSrc = this->SetSrc2(RegOpnd::New(newSym, regNum, type, m_func));
    newSrc->SetValueType(oldSrc->GetValueType());

    newInstr = Instr::New(assignOpcode, newSrc, oldSrc, m_func);
    this->InsertBefore(newInstr);

    if(creatingNewSym)
    {TRACE_IT(7082);
        if (oldSrc->IsRegOpnd())
        {TRACE_IT(7083);
            newSym->CopySymAttrs(oldSrc->AsRegOpnd()->m_sym);
        }
        else if (oldSrc->IsIntConstOpnd())
        {TRACE_IT(7084);
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
{TRACE_IT(7085);
    int32 offset = indirOpnd->GetOffset();
    if (indirOpnd->GetIndexOpnd())
    {TRACE_IT(7086);
        return HoistIndirOffsetAsAdd(indirOpnd, indirOpnd->GetBaseOpnd(), offset, regNum);
    }
    IntConstOpnd *offsetOpnd = IntConstOpnd::New(offset, TyInt32, this->m_func);
    RegOpnd *indexOpnd = RegOpnd::New(StackSym::New(TyMachReg, this->m_func), regNum, TyMachReg, this->m_func);

#if defined(DBG) && defined(_M_ARM)
    if (regNum == SCRATCH_REG)
    {TRACE_IT(7087);
        AssertMsg(indirOpnd->GetBaseOpnd()->GetReg()!= SCRATCH_REG, "Why both are SCRATCH_REG");
        if (this->GetSrc1() && this->GetSrc1()->IsRegOpnd())
        {TRACE_IT(7088);
            Assert(this->GetSrc1()->AsRegOpnd()->GetReg() != SCRATCH_REG);
        }
        if (this->GetSrc2() && this->GetSrc2()->IsRegOpnd())
        {TRACE_IT(7089);
            Assert(this->GetSrc2()->AsRegOpnd()->GetReg() != SCRATCH_REG);
        }
        if (this->GetDst() && this->GetDst()->IsRegOpnd())
        {TRACE_IT(7090);
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
{TRACE_IT(7091);
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
    {TRACE_IT(7092);
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
{TRACE_IT(7093);
    if (oldOpnd == this->GetDst())
    {TRACE_IT(7094);
        return this->ReplaceDst(newOpnd);
    }
    else
    {TRACE_IT(7095);
        return this->ReplaceSrc(oldOpnd, newOpnd);
    }
}

Opnd *Instr::DeepReplace(Opnd *const oldOpnd, Opnd *const newOpnd)
{TRACE_IT(7096);
    Assert(oldOpnd);
    Assert(newOpnd);

    IR::Opnd *opnd = GetDst();
    if(opnd && oldOpnd != opnd && oldOpnd->IsEqual(opnd))
    {TRACE_IT(7097);
        ReplaceDst(newOpnd);
    }
    opnd = GetSrc1();
    if(opnd && oldOpnd != opnd && oldOpnd->IsEqual(opnd))
    {TRACE_IT(7098);
        ReplaceSrc1(newOpnd);
    }
    opnd = GetSrc2();
    if(opnd && oldOpnd != opnd && oldOpnd->IsEqual(opnd))
    {TRACE_IT(7099);
        ReplaceSrc2(newOpnd);
    }

    // Do this last because Replace will delete oldOpnd
    return Replace(oldOpnd, newOpnd);
}

Instr *
Instr::HoistIndirOffsetAsAdd(IR::IndirOpnd *orgOpnd, IR::Opnd *baseOpnd, int offset, RegNum regNum)
{TRACE_IT(7100);
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
{TRACE_IT(7101);
        IR::RegOpnd *newBaseOpnd = IR::RegOpnd::New(StackSym::New(TyMachPtr, this->m_func), regNum, TyMachPtr, this->m_func);

        IR::Instr * instrAdd = IR::Instr::New(Js::OpCode::ADD, newBaseOpnd, baseOpnd, indexOpnd, this->m_func);

        this->InsertBefore(instrAdd);

        orgOpnd->ReplaceBaseOpnd(newBaseOpnd);
        orgOpnd->SetIndexOpnd(nullptr);

        return instrAdd;
}

Instr *
Instr::HoistSymOffsetAsAdd(IR::SymOpnd *orgOpnd, IR::Opnd *baseOpnd, int offset, RegNum regNum)
{TRACE_IT(7102);
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
{TRACE_IT(7103);
    IR::RegOpnd *baseOpnd = IR::RegOpnd::New(nullptr, baseReg, TyMachPtr, this->m_func);
    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(baseOpnd, offset, symOpnd->GetType(), this->m_func);
    if (symOpnd == this->GetDst())
    {TRACE_IT(7104);
        this->ReplaceDst(indirOpnd);
    }
    else
    {TRACE_IT(7105);
        this->ReplaceSrc(symOpnd, indirOpnd);
    }

    return this->HoistIndirOffset(indirOpnd, regNum);
}

Opnd *
Instr::UnlinkSrc(Opnd *src)
{TRACE_IT(7106);
    if (src == this->GetSrc1())
    {TRACE_IT(7107);
        return this->UnlinkSrc1();
    }
    else
    {TRACE_IT(7108);
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
{TRACE_IT(7109);
    if (oldSrc == this->GetSrc1())
    {TRACE_IT(7110);
        return this->ReplaceSrc1(newSrc);
    }
    else
    {TRACE_IT(7111);
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
{TRACE_IT(7112);
    switch (m_opcode)
    {
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
{TRACE_IT(7113);
    IR::Instr *instr = this->m_next;

    while (instr != nullptr && !instr->IsRealInstr())
    {TRACE_IT(7114);
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
{TRACE_IT(7115);
    IR::Instr *instr = this->m_next;

    while (instr != nullptr && !instr->IsLabelInstr() && !instr->IsRealInstr())
    {TRACE_IT(7116);
        instr = instr->m_next;
        AssertMsg(instr, "GetNextRealInstrOrLabel() failed...");
    }
    return instr;
}

IR::Instr *
Instr::GetNextBranchOrLabel() const
{TRACE_IT(7117);
    IR::Instr *instr = this->m_next;

    while (instr != nullptr && !instr->IsLabelInstr() && !instr->IsBranchInstr())
    {TRACE_IT(7118);
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
{TRACE_IT(7119);
    IR::Instr *instr = this->m_prev;

    while (!instr->IsRealInstr())
    {TRACE_IT(7120);
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
{TRACE_IT(7121);
    IR::Instr *instr = this->m_prev;

    while (!instr->IsLabelInstr() && !instr->IsRealInstr())
    {TRACE_IT(7122);
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
{TRACE_IT(7123);
    const uint32 byteCodeOffset = GetByteCodeOffset();
    IR::Instr *insertBeforeInstr = this;
    IR::Instr *prevInstr = insertBeforeInstr->m_prev;
    while(prevInstr && prevInstr->IsByteCodeUsesInstr() && prevInstr->GetByteCodeOffset() == byteCodeOffset)
    {TRACE_IT(7124);
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
{TRACE_IT(7125);
    if(m_next && m_next->IsLabelInstr() && m_next->AsLabelInstr()->isOpHelper == isHelper)
    {TRACE_IT(7126);
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
{TRACE_IT(7127);
    IR::Opnd *src1 = this->GetSrc1();

    // Check src1
    if (src1)
    {TRACE_IT(7128);
        if (src1->IsRegOpnd())
        {TRACE_IT(7129);
            RegOpnd *regOpnd = src1->AsRegOpnd();

            if (regOpnd->m_sym == sym)
            {TRACE_IT(7130);
                return regOpnd;
            }
        }
        else if (src1->IsIndirOpnd())
        {TRACE_IT(7131);
            IR::IndirOpnd *indirOpnd = src1->AsIndirOpnd();
            if (indirOpnd->GetBaseOpnd()->m_sym == sym)
            {TRACE_IT(7132);
                return indirOpnd->GetBaseOpnd();
            }
            else if (indirOpnd->GetIndexOpnd() && indirOpnd->GetIndexOpnd()->m_sym == sym)
            {TRACE_IT(7133);
                return indirOpnd->GetIndexOpnd();
            }
        }
        IR::Opnd *src2 = this->GetSrc2();

        // Check src2
        if (src2)
        {TRACE_IT(7134);
            if (src2->IsRegOpnd())
            {TRACE_IT(7135);
                RegOpnd *regOpnd = src2->AsRegOpnd();

                if (regOpnd->m_sym == sym)
                {TRACE_IT(7136);
                    return regOpnd;
                }
            }
            else if (src2->IsIndirOpnd())
            {TRACE_IT(7137);
                IR::IndirOpnd *indirOpnd = src2->AsIndirOpnd();
                if (indirOpnd->GetBaseOpnd()->m_sym == sym)
                {TRACE_IT(7138);
                    return indirOpnd->GetBaseOpnd();
                }
                else if (indirOpnd->GetIndexOpnd() && indirOpnd->GetIndexOpnd()->m_sym == sym)
                {TRACE_IT(7139);
                    return indirOpnd->GetIndexOpnd();
                }
            }
        }
    }

    // Check uses in dst
    IR::Opnd *dst = this->GetDst();

    if (dst != nullptr && dst->IsIndirOpnd())
    {TRACE_IT(7140);
        IR::IndirOpnd *indirOpnd = dst->AsIndirOpnd();
        if (indirOpnd->GetBaseOpnd()->m_sym == sym)
        {TRACE_IT(7141);
            return indirOpnd->GetBaseOpnd();
        }
        else if (indirOpnd->GetIndexOpnd() && indirOpnd->GetIndexOpnd()->m_sym == sym)
        {TRACE_IT(7142);
            return indirOpnd->GetIndexOpnd();
        }
    }

    return nullptr;
}

IR::RegOpnd *
Instr::FindRegUseInRange(StackSym *sym, IR::Instr *instrBegin, IR::Instr *instrEnd)
{
    FOREACH_INSTR_IN_RANGE(instr, instrBegin, instrEnd)
    {TRACE_IT(7143);
        Assert(instr);
        IR::RegOpnd *opnd = instr->FindRegUse(sym);
        if (opnd)
        {TRACE_IT(7144);
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
{TRACE_IT(7145);
    IR::Opnd *dst = this->GetDst();

    if (dst)
    {TRACE_IT(7146);
        if (dst->IsRegOpnd())
        {TRACE_IT(7147);
            RegOpnd *regOpnd = dst->AsRegOpnd();

            if (regOpnd->m_sym == sym)
            {TRACE_IT(7148);
                return regOpnd;
            }
        }
    }

    return nullptr;
}

Instr*
Instr::FindSingleDefInstr(Js::OpCode opCode, Opnd* src)
{TRACE_IT(7149);
    RegOpnd* src1 = src->IsRegOpnd() ? src->AsRegOpnd() : nullptr;

    return  src1 &&
        src1->m_sym->IsSingleDef() &&
        src1->m_sym->GetInstrDef()->m_opcode == opCode ?
        src1->m_sym->GetInstrDef() :
        nullptr;
}

void
Instr::TransferDstAttributesTo(Instr * instr)
{TRACE_IT(7150);
    instr->dstIsTempNumber = this->dstIsTempNumber;
    instr->dstIsTempNumberTransferred = this->dstIsTempNumberTransferred;
    instr->dstIsTempObject = this->dstIsTempObject;
}

void
Instr::TransferTo(Instr * instr)
{TRACE_IT(7151);
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
    {TRACE_IT(7152);
        instr->m_dst = dst;
        this->m_dst = nullptr;
        if (dst->IsRegOpnd())
        {TRACE_IT(7153);
            Sym * sym = dst->AsRegOpnd()->m_sym;
            if (sym->IsStackSym() && sym->AsStackSym()->m_isSingleDef)
            {TRACE_IT(7154);
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
{TRACE_IT(7155);
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
{TRACE_IT(7156);
    Assert(!this->HasBailOutInfo());

    AssertMsg(!useAuxBailOut || !this->HasAuxBailOut(), "Already aux bail out!");
    Assert(!this->HasAuxBailOut() || this->GetAuxBailOutKind() != IR::BailOutInvalid);

    IR::Instr * bailOutInstr = nullptr;
    if (this->HasAuxBailOut())
    {TRACE_IT(7157);
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
    {TRACE_IT(7158);
        switch (this->m_kind)
        {
        case InstrKindInstr:
            bailOutInstr = IR::BailOutInstr::New(this->m_opcode, kind, bailOutInfo, bailOutInfo->bailOutFunc);
            break;
        case InstrKindProfiled:
            bailOutInstr = IR::ProfiledBailOutInstr::New(this->m_opcode, kind, bailOutInfo, bailOutInfo->bailOutFunc);
            bailOutInstr->AsProfiledInstr()->u = this->AsProfiledInstr()->u;
            break;
        case InstrKindBranch:
        {TRACE_IT(7159);
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
    {TRACE_IT(7160);
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
{TRACE_IT(7161);
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
{TRACE_IT(7162);
    this->SetAuxBailOutKind(IR::BailOutInvalid);
    this->hasAuxBailOut = false;
}

void
Instr::ClearBailOutInfo()
{TRACE_IT(7163);
    if (this->HasBailOutInfo() || this->HasAuxBailOut())
    {TRACE_IT(7164);
        BailOutInfo * bailOutInfo = this->GetBailOutInfo();
        Assert(bailOutInfo);

        if (bailOutInfo->bailOutInstr == this)
        {TRACE_IT(7165);
            JitArenaAllocator * alloc = this->m_func->m_alloc;
            bailOutInfo->Clear(alloc);
            JitAdelete(alloc, bailOutInfo);
        }

        this->hasBailOutInfo = false;
        this->hasAuxBailOut = false;
    }
}

bool Instr::HasAnyLoadHeapArgsOpCode()
{TRACE_IT(7166);
    switch (m_opcode)
    {
        case Js::OpCode::LdHeapArguments:
        case Js::OpCode::LdHeapArgsCached:
        case Js::OpCode::LdLetHeapArguments:
        case Js::OpCode::LdLetHeapArgsCached:
            return true;
    }
    return false;
}

bool Instr::CanHaveArgOutChain() const
{TRACE_IT(7167);
    return
        this->m_opcode == Js::OpCode::CallI ||
        this->m_opcode == Js::OpCode::CallIFixed ||
        this->m_opcode == Js::OpCode::NewScObject ||
        this->m_opcode == Js::OpCode::NewScObjectSpread ||
        this->m_opcode == Js::OpCode::NewScObjArray ||
        this->m_opcode == Js::OpCode::NewScObjArraySpread;
}

bool Instr::HasEmptyArgOutChain(IR::Instr** startCallInstrOut)
{TRACE_IT(7168);
    Assert(CanHaveArgOutChain());

    if (GetSrc2()->IsRegOpnd())
    {TRACE_IT(7169);
        IR::RegOpnd * argLinkOpnd = GetSrc2()->AsRegOpnd();
        StackSym *argLinkSym = argLinkOpnd->m_sym->AsStackSym();
        AssertMsg(!argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");
        IR::Instr* startCallInstr = argLinkSym->m_instrDef;
        AssertMsg(startCallInstr->m_opcode == Js::OpCode::StartCall, "Problem with arg chain.");
        if (startCallInstrOut != nullptr)
        {TRACE_IT(7170);
            *startCallInstrOut = startCallInstr;
        }
        return true;
    }

    return false;
}

bool Instr::HasFixedFunctionAddressTarget() const
{TRACE_IT(7171);
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
{TRACE_IT(7172);
    Assert(this->m_opcode == Js::OpCode::InlineeStart || this->m_opcode == Js::OpCode::CallDirect ||
        this->m_opcode == Js::OpCode::CallI || this->m_opcode == Js::OpCode::CallIFixed);
    IR::Instr *argInsertInstr = this;
    this->IterateArgInstrs([&](IR::Instr* argInstr)
    {
        if (generateByteCodeCapture)
        {TRACE_IT(7173);
            argInstr->GenerateBytecodeArgOutCapture();
        }
        argInstr->Move(argInsertInstr);
        argInsertInstr = argInstr;
        return false;
    });
}

void Instr::Move(IR::Instr* insertInstr)
{TRACE_IT(7174);
    this->Unlink();
    this->ClearByteCodeOffset();
    this->SetByteCodeOffset(insertInstr);
    insertInstr->InsertBefore(this);
}

IR::Instr* Instr::GetBytecodeArgOutCapture()
{TRACE_IT(7175);
    Assert(this->m_opcode == Js::OpCode::ArgOut_A_Inline ||
        this->m_opcode == Js::OpCode::ArgOut_A ||
        this->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn);
    Assert(this->m_dst->GetStackSym()->m_isArgCaptured);
    IR::Instr* instr = this->GetSrc1()->GetStackSym()->m_instrDef;
    Assert(instr->m_opcode == Js::OpCode::BytecodeArgOutCapture);
    return instr;
}

bool Instr::HasByteCodeArgOutCapture()
{TRACE_IT(7176);
    Assert(this->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs ||
        this->m_opcode == Js::OpCode::ArgOut_A_Inline ||
        this->m_opcode == Js::OpCode::ArgOut_A ||
        this->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn ||
        this->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs);
    if (this->m_dst->GetStackSym()->m_isArgCaptured)
    {TRACE_IT(7177);
        Assert(GetBytecodeArgOutCapture() != nullptr);
        return true;
    }
    return false;
}


void Instr::GenerateBytecodeArgOutCapture()
{TRACE_IT(7178);
    if (!HasByteCodeArgOutCapture())
    {TRACE_IT(7179);
        this->m_dst->GetStackSym()->m_isArgCaptured = true;
        StackSym* tmpSym = StackSym::NewArgSlotRegSym(this->GetDst()->GetStackSym()->GetArgSlotNum(), this->m_func, this->GetDst()->GetType());
        IR::Instr* instr = this->HoistSrc1(Js::OpCode::BytecodeArgOutCapture, RegNOREG, tmpSym);
        instr->SetByteCodeOffset(this);
    }
}

void Instr::GenerateArgOutSnapshot()
{TRACE_IT(7180);
    StackSym* tmpSym = StackSym::NewArgSlotRegSym(this->GetDst()->GetStackSym()->GetArgSlotNum(), this->m_func);
    IR::Instr* instr = this->HoistSrc1(Js::OpCode::Ld_A, RegNOREG, tmpSym);
    instr->SetByteCodeOffset(this);
}

IR::Instr* Instr::GetArgOutSnapshot()
{TRACE_IT(7181);
    Assert(this->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs);
    IR::Instr* instr = this->GetSrc1()->GetStackSym()->m_instrDef;
    Assert(instr->m_opcode == Js::OpCode::Ld_A);
    return instr;
}

bool Instr::HasAnyImplicitCalls() const
{TRACE_IT(7182);
    // there can be no implicit calls in asm.js
    if (m_func->GetJITFunctionBody()->IsAsmJsMode())
    {TRACE_IT(7183);
        return false;
    }
    if (OpCodeAttr::HasImplicitCall(this->m_opcode))
    {TRACE_IT(7184);
        return true;
    }
    if (OpCodeAttr::OpndHasImplicitCall(this->m_opcode))
    {TRACE_IT(7185);
        if (this->m_dst &&
            ((this->m_dst->IsSymOpnd() && this->m_dst->AsSymOpnd()->m_sym->IsPropertySym()) ||
             this->m_dst->IsIndirOpnd()))
        {TRACE_IT(7186);
            return true;
        }

        IR::Opnd *src1 = this->GetSrc1();
        if (src1)
        {TRACE_IT(7187);
            if ((src1->IsSymOpnd() && src1->AsSymOpnd()->m_sym->IsPropertySym()) || src1->IsIndirOpnd())
            {TRACE_IT(7188);
                return true;
            }

            if (!src1->GetValueType().IsPrimitive())
            {TRACE_IT(7189);
                return true;
            }

            IR::Opnd *src2 = this->GetSrc2();
            if (src2)
            {TRACE_IT(7190);
                if ((src2->IsSymOpnd() && src2->AsSymOpnd()->m_sym->IsPropertySym()) || src2->IsIndirOpnd())
                {TRACE_IT(7191);
                    return true;
                }

                if (!src2->GetValueType().IsPrimitive())
                {TRACE_IT(7192);
                    return true;
                }
            }
        }
    }

    return false;
}

bool Instr::HasAnySideEffects() const
{TRACE_IT(7193);
    return (hasSideEffects ||
            OpCodeAttr::HasSideEffects(this->m_opcode) ||
            this->HasAnyImplicitCalls());
}

bool Instr::AreAllOpndInt64() const
{TRACE_IT(7194);
    bool isDstInt64 = !m_dst || IRType_IsInt64(m_dst->GetType());
    bool isSrc1Int64 = !m_src1 || IRType_IsInt64(m_src1->GetType());
    bool isSrc2Int64 = !m_src2 || IRType_IsInt64(m_src2->GetType());
    return isDstInt64 && isSrc1Int64 && isSrc2Int64;
}

JITTimeFixedField* Instr::GetFixedFunction() const
{TRACE_IT(7195);
    Assert(HasFixedFunctionAddressTarget());
    JITTimeFixedField* function = (JITTimeFixedField*)this->m_src1->AsAddrOpnd()->m_metadata;
    return function;
}

IR::Instr* Instr::GetNextArg()
{TRACE_IT(7196);
    Assert(this->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs ||
        this->m_opcode == Js::OpCode::ArgOut_A_Inline ||
        this->m_opcode == Js::OpCode::ArgOut_A ||
        this->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn ||
        this->m_opcode == Js::OpCode::InlineeStart);
    IR::Instr* argInstr = this;
    while (true)
    {TRACE_IT(7197);
        StackSym* linkSym;
        if (argInstr->GetSrc2()->IsRegOpnd())
        {TRACE_IT(7198);
            linkSym = argInstr->GetSrc2()->AsRegOpnd()->m_sym->AsStackSym();
        }
        else
        {TRACE_IT(7199);
            linkSym = argInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym();
            Assert(linkSym->IsArgSlotSym());
        }
        Assert(linkSym->IsSingleDef());
        argInstr = linkSym->m_instrDef;
        if (argInstr->m_opcode == Js::OpCode::ArgOut_A_InlineSpecialized)
        {TRACE_IT(7200);
            continue;
        }
        if (argInstr->m_opcode == Js::OpCode::StartCall)
        {TRACE_IT(7201);
            break;
        }
        return argInstr;
    }
    return nullptr;
}

uint Instr::GetArgOutCount(bool getInterpreterArgOutCount)
{TRACE_IT(7202);
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
    {TRACE_IT(7203);
        return this->GetSrc1()->AsIntConstOpnd()->AsUint32();
    }

    Assert(opcode == Js::OpCode::StartCall);
    IntConstType argOutCount = !this->GetSrc2() ? this->GetSrc1()->AsIntConstOpnd()->GetValue() : this->GetSrc2()->AsIntConstOpnd()->GetValue();
    Assert(argOutCount >= 0 && argOutCount < UINT32_MAX);
    return (uint)argOutCount;
}

PropertySymOpnd *Instr::GetPropertySymOpnd() const
{TRACE_IT(7204);
    if (m_src1 && m_src1->IsSymOpnd() && m_src1->AsSymOpnd()->IsPropertySymOpnd())
    {TRACE_IT(7205);
        return m_src1->AsPropertySymOpnd();
    }
    if (m_dst && m_dst->IsSymOpnd() && m_dst->AsSymOpnd()->IsPropertySymOpnd())
    {TRACE_IT(7206);
        return m_dst->AsPropertySymOpnd();
    }
    return nullptr;
}

bool Instr::CallsAccessor(IR::PropertySymOpnd* methodOpnd)
{TRACE_IT(7207);
    if (methodOpnd)
    {TRACE_IT(7208);
        Assert(methodOpnd->HasObjTypeSpecFldInfo());
        return methodOpnd->UsesAccessor();
    }

    return CallsGetter() || CallsSetter();
}

bool Instr::CallsSetter(IR::PropertySymOpnd* methodOpnd)
{TRACE_IT(7209);
    return
        this->IsProfiledInstr() &&
        (this->m_dst && this->m_dst->IsSymOpnd() && this->m_dst->AsSymOpnd()->IsPropertySymOpnd()) &&
        ((this->AsProfiledInstr()->u.FldInfo().flags & Js::FldInfo_FromAccessor) != 0);
}

bool Instr::CallsGetter(IR::PropertySymOpnd* methodOpnd)
{TRACE_IT(7210);
    return
        this->IsProfiledInstr() &&
        (this->m_src1 && this->m_src1->IsSymOpnd() && this->m_src1->AsSymOpnd()->IsPropertySymOpnd()) &&
        ((this->AsProfiledInstr()->u.FldInfo().flags & Js::FldInfo_FromAccessor) != 0);
}

IR::Instr* IR::Instr::NewConstantLoad(IR::RegOpnd* dstOpnd, intptr_t varConst, ValueType type, Func* func, Js::Var varLocal/* = nullptr*/)
{TRACE_IT(7211);
    IR::Opnd *srcOpnd = nullptr;
    IR::Instr *instr;

    if (Js::TaggedInt::Is(varConst))
    {TRACE_IT(7212);
        IntConstType value = Js::TaggedInt::ToInt32((Js::Var)varConst);
        instr = IR::Instr::New(Js::OpCode::LdC_A_I4, dstOpnd, IR::IntConstOpnd::New(value, TyInt32, func), func);
        if (dstOpnd->m_sym->IsSingleDef())
        {TRACE_IT(7213);
            dstOpnd->m_sym->SetIsIntConst(value);
        }
    }
    else
    {TRACE_IT(7214);
        if (varConst == func->GetThreadContextInfo()->GetNullFrameDisplayAddr())
        {TRACE_IT(7215);
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
        {TRACE_IT(7216);
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
        {TRACE_IT(7217);

            ValueType valueType;
            if(type.IsString())
            {TRACE_IT(7218);
                srcOpnd = IR::AddrOpnd::New(varConst, IR::AddrOpndKindDynamicVar, func, true, varLocal);
                instr = IR::Instr::New(Js::OpCode::LdStr, dstOpnd, srcOpnd, func);
                Assert(dstOpnd->m_sym->m_isSingleDef);
                if (dstOpnd->m_sym->IsSingleDef())
                {TRACE_IT(7219);
                    dstOpnd->m_sym->m_isStrConst = true;
                    dstOpnd->m_sym->m_isConst = true;
                }
                dstOpnd->SetValueType(ValueType::String);
                srcOpnd->SetValueType(ValueType::String);
            }
            else if(type.IsNumber())
            {TRACE_IT(7220);
                // TODO (michhol): OOP JIT. we may need to unbox before sending over const table

                if (!func->IsOOPJIT())
                {TRACE_IT(7221);
                    srcOpnd = IR::FloatConstOpnd::New((Js::Var)varConst, TyFloat64, func);
                }
                else
                {TRACE_IT(7222);
                    srcOpnd = IR::FloatConstOpnd::New((Js::Var)varConst, TyFloat64, func
#if !FLOATVAR
                        ,varLocal
#endif
                    );

                }

                instr = IR::Instr::New(Js::OpCode::LdC_A_R8, dstOpnd, srcOpnd, func);
                if (dstOpnd->m_sym->IsSingleDef())
                {TRACE_IT(7223);
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
            {TRACE_IT(7224);
                if (type.IsUndefined() || type.IsNull() || type.IsBoolean())
                {TRACE_IT(7225);
                    valueType = type;
                }
                else
                {TRACE_IT(7226);
                    valueType = ValueType::GetObject(ObjectType::Object);
                }
                srcOpnd = IR::AddrOpnd::New(varConst, IR::AddrOpndKindDynamicVar, func, true, varLocal);
                instr = IR::Instr::New(Js::OpCode::Ld_A, dstOpnd, srcOpnd, func);
                if (dstOpnd->m_sym->IsSingleDef())
                {TRACE_IT(7227);
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
{TRACE_IT(7228);
    return OpCodeAttr::UseAllFields(this->m_opcode) || this->CallsAccessor();
}

BranchInstr *
Instr::ChangeCmCCToBranchInstr(LabelInstr *targetInstr)
{TRACE_IT(7229);
    Js::OpCode newOpcode;
    switch (this->m_opcode)
    {
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
{TRACE_IT(7230);
    return (this->m_opcode >= Js::OpCode::CmEq_A && this->m_opcode <= Js::OpCode::CmSrNeq_A) && this->GetSrc1()->IsVar();
}

bool Instr::IsCmCC_R8()
{TRACE_IT(7231);
    return (this->m_opcode >= Js::OpCode::CmEq_A && this->m_opcode <= Js::OpCode::CmSrNeq_A) && this->GetSrc1()->IsFloat64();
}

bool Instr::IsCmCC_I4()
{TRACE_IT(7232);
    return (this->m_opcode >= Js::OpCode::CmEq_I4 && this->m_opcode <= Js::OpCode::CmUnGe_I4);
}

bool Instr::BinaryCalculator(IntConstType src1Const, IntConstType src2Const, IntConstType *pResult)
{TRACE_IT(7233);
    IntConstType value = 0;

    switch (this->m_opcode)
    {
    case Js::OpCode::Add_A:
        if (IntConstMath::Add(src1Const, src2Const, &value))
        {TRACE_IT(7234);
            return false;
        }
        break;

    case Js::OpCode::Sub_A:
        if (IntConstMath::Sub(src1Const, src2Const, &value))
        {TRACE_IT(7235);
            return false;
        }
        break;

    case Js::OpCode::Mul_A:
        if (IntConstMath::Mul(src1Const, src2Const, &value))
        {TRACE_IT(7236);
            return false;
        }
        if (value == 0)
        {TRACE_IT(7237);
            // might be -0
            // Bail for now...
            return false;
        }
        break;

    case Js::OpCode::Div_A:
        if (src2Const == 0)
        {TRACE_IT(7238);
            // Could fold to INF/-INF
            // instr->HoistSrc1(Js::OpCode::Ld_A);
            return false;
        }
        if (src1Const == 0 && src2Const < 0)
        {TRACE_IT(7239);
            // folds to -0. Bail for now...
            return false;
        }
        if (IntConstMath::Div(src1Const, src2Const, &value))
        {TRACE_IT(7240);
            return false;
        }
        if (src1Const % src2Const != 0)
        {TRACE_IT(7241);
            // Bail for now...
            return false;
        }
        break;

    case Js::OpCode::Rem_A:

        if (src2Const == 0)
        {TRACE_IT(7242);
            // Bail for now...
            return false;
        }
        if (IntConstMath::Mod(src1Const, src2Const, &value))
        {TRACE_IT(7243);
            return false;
        }
        if (value == 0)
        {TRACE_IT(7244);
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
        {TRACE_IT(7245);
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
{TRACE_IT(7246);
    IntConstType value = 0;

    switch (this->m_opcode)
    {
    case Js::OpCode::Neg_A:
        if (src1Const == 0)
        {TRACE_IT(7247);
            // Could fold to -0.0
            return false;
        }

        if (IntConstMath::Neg(src1Const, &value))
        {TRACE_IT(7248);
            return false;
        }
        break;

    case Js::OpCode::Not_A:
        IntConstMath::Not(src1Const, &value);
        break;

    case Js::OpCode::Ld_A:
        if (this->HasBailOutInfo())
        {TRACE_IT(7249);
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
        {TRACE_IT(7250);
            return false;
        }
        break;

    case Js::OpCode::Decr_A:
        if (IntConstMath::Dec(src1Const, &value))
        {TRACE_IT(7251);
            return false;
        }
        break;


    case Js::OpCode::InlineMathAbs:
        if (src1Const == IntConstMin)
        {TRACE_IT(7252);
            return false;
        }
        else
        {TRACE_IT(7253);
            value = src1Const < 0 ? -src1Const : src1Const;
        }
        break;

    case Js::OpCode::InlineMathClz:
        DWORD clz;
        DWORD src1Const32;
        src1Const32 = (DWORD)src1Const;
        if (_BitScanReverse(&clz, src1Const32))
        {TRACE_IT(7254);
            value = 31 - clz;
        }
        else
        {TRACE_IT(7255);
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
        {TRACE_IT(7256);
            return false;
        }
        else
        {TRACE_IT(7257);
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
{TRACE_IT(7258);
    Output::Print(_u("opcode: %s "), Js::OpCodeUtil::GetOpCodeName(m_opcode));
    SymOpnd * symOpnd;

    if (this->m_opcode == Js::OpCode::NewScFunc || this->m_opcode == Js::OpCode::NewScGenFunc)
    {TRACE_IT(7259);
        Output::Print(_u("\n"));
        return;
    }
    Opnd * src1 = this->GetSrc1();
    if (!src1)
    {TRACE_IT(7260);
        Output::Print(_u("\n"));
        return;
    }
    if (src1->GetKind() != OpndKindSym)
    {TRACE_IT(7261);
        Output::Print(_u("\n"));
        return;
    }

    symOpnd = src1->AsSymOpnd();
    if (symOpnd->m_sym->IsPropertySym())
    {TRACE_IT(7262);
        PropertySym *propertySym = symOpnd->m_sym->AsPropertySym();

        switch (propertySym->m_fieldKind)
        {
        case PropertyKindData:
            if (!JITManager::GetJITManager()->IsOOPJITEnabled())
            {TRACE_IT(7263);
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
{TRACE_IT(7264);
    switch (m_opcode)
    {
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
        {TRACE_IT(7265);
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
{TRACE_IT(7266);
    IR::BailOutKind kind = (IR::BailOutKind)0;
    if (this->HasBailOutInfo())
    {TRACE_IT(7267);
        kind |= this->GetBailOutKind();
    }
    if (this->HasAuxBailOut())
    {TRACE_IT(7268);
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
{TRACE_IT(7269);
    if (m_func->HasByteCodeOffset())
    {TRACE_IT(7270);
        Output::SkipToColumn(78);
        Output::Print(_u("#"));
        if (this->m_number != Js::Constants::NoByteCodeOffset)
        {TRACE_IT(7271);
            Output::Print(_u("%04x"), this->m_number);
            Output::Print(this->IsCloned()? _u("*") : _u(" "));
        }
    }
    if (!this->m_func->IsTopFunc())
    {TRACE_IT(7272);
        Output::SkipToColumn(78);
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(_u(" Func #%s"), this->m_func->GetDebugNumberSet(debugStringBuffer));
    }
#ifdef BAILOUT_INJECTION
    if (this->bailOutByteCodeLocation != (uint)-1)
    {TRACE_IT(7273);
        Output::SkipToColumn(85);
        Output::Print(_u("@%4d"), this->bailOutByteCodeLocation);
    }
#endif
    if (this->m_opcode == Js::OpCode::InlineeStart)
    {TRACE_IT(7274);
        Output::Print(_u(" %s"), this->m_func->GetJITFunctionBody()->GetDisplayName());
    }
}

void
Instr::DumpGlobOptInstrString()
{TRACE_IT(7275);
    if(this->globOptInstrString)
    {TRACE_IT(7276);
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
{TRACE_IT(7277);
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
    {TRACE_IT(7278);
        PrintOpCodeName();

        // src1 <= src2 + dst

        Assert(GetSrc1());
        if(GetSrc1()->IsIntConstOpnd())
        {TRACE_IT(7279);
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
        {TRACE_IT(7280);
            if(GetSrc2()->IsIntConstOpnd())
            {TRACE_IT(7281);
            #if DBG
                int32 temp;
                Assert(!Int32Math::Add(offset, GetSrc2()->AsIntConstOpnd()->AsInt32(), &temp));
            #endif
                offset += GetSrc2()->AsIntConstOpnd()->AsInt32();
            }
            else
            {TRACE_IT(7282);
                dumpSrc2 = true;
                if(offset == -1)
                {TRACE_IT(7283);
                    useLessThanOrEqual = false; // < instead of <=
                    offset = 0;
                }
                else if(offset < 0 && offset != IntConstMin)
                {TRACE_IT(7284);
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
        {TRACE_IT(7285);
            if(dumpSrc2)
            {TRACE_IT(7286);
                Output::Print(_u(" %C "), usePlus ? '+' : '-');
            }
            Output::Print(_u("%d"), offset);
        }

        goto PrintByteCodeOffsetEtc;
    }

    Output::SkipToColumn(4);

    dst = this->GetDst();

    if (dst)
    {TRACE_IT(7287);
        dst->Dump(flags, this->m_func);

        bool const dumpMarkTemp = PHASE_DUMP(Js::MarkTempPhase, m_func)
            || PHASE_TRACE(Js::MarkTempPhase, m_func);
        bool const dumpMarkTempNumber = dumpMarkTemp || PHASE_DUMP(Js::MarkTempNumberPhase, m_func)
            || PHASE_TRACE(Js::MarkTempNumberPhase, m_func);
        bool const dumpMarkTempObject = dumpMarkTemp || PHASE_DUMP(Js::MarkTempObjectPhase, m_func)
            || PHASE_TRACE(Js::MarkTempObjectPhase, m_func);

        if ((dumpMarkTempNumber && (this->dstIsTempNumberTransferred || this->dstIsTempNumber))
            || (dumpMarkTempObject && this->dstIsTempObject))
        {TRACE_IT(7288);
            Output::Print(_u("["));

            if (dumpMarkTempNumber)
            {TRACE_IT(7289);
                if (Js::Configuration::Global.flags.Verbose || OpCodeAttr::TempNumberProducing(this->m_opcode))
                {TRACE_IT(7290);
                    if (this->dstIsTempNumberTransferred)
                    {TRACE_IT(7291);
                        Assert(this->dstIsTempNumber);
                        Output::Print(_u("x"));
                    }
                    else if (this->dstIsTempNumber)
                    {TRACE_IT(7292);
                        Output::Print(_u("#"));
                    }
                }
            }
            if (dumpMarkTempObject)
            {TRACE_IT(7293);
                if (Js::Configuration::Global.flags.Verbose || OpCodeAttr::TempObjectProducing(this->m_opcode))
                {TRACE_IT(7294);
                    if (this->dstIsTempObject)
                    {TRACE_IT(7295);
                        Output::Print(_u("o"));
                    }
                }
            }

            Output::Print(_u("tmp]"));
        }
        if(PHASE_DUMP(Js::TrackNegativeZeroPhase, m_func->GetTopFunc()) && !ShouldCheckForNegativeZero())
        {TRACE_IT(7296);
            Output::Print(_u("[-0]"));
        }
        if (PHASE_DUMP(Js::TypedArrayVirtualPhase, m_func->GetTopFunc()) && (!IsDstNotAlwaysConvertedToInt32() || !IsDstNotAlwaysConvertedToNumber()))
        {TRACE_IT(7297);
            if (!IsDstNotAlwaysConvertedToInt32())
                Output::Print(_u("[->i]"));
            else
                Output::Print(_u("[->n]"));

        }
        if(PHASE_DUMP(Js::TrackIntOverflowPhase, m_func->GetTopFunc()))
        {TRACE_IT(7298);
            // ignoring 32-bit overflow ?
            if(!ShouldCheckFor32BitOverflow())
            {TRACE_IT(7299);
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
    {TRACE_IT(7300);
        BranchInstr * branchInstr = this->AsBranchInstr();
        LabelInstr * targetInstr = branchInstr->GetTarget();
        bool labelPrinted = true;
        if (targetInstr == NULL)
        {TRACE_IT(7301);
            // Checking the 'm_isMultiBranch' field here directly as well to bypass asserting when tracing IR builder
            if(branchInstr->m_isMultiBranch && branchInstr->IsMultiBranch())
            {TRACE_IT(7302);
                IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();

                // If this MultiBranchInstr has been lowered to a machine instruction, which means
                // its opcode is not Js::OpCode::MultiBr, there is no need to print the labels.
                if (this->m_opcode == Js::OpCode::MultiBr)
                {TRACE_IT(7303);
                    multiBranchInstr->MapMultiBrLabels([](IR::LabelInstr * labelInstr) -> void
                    {
                        Output::Print(_u("$L%d "), labelInstr->m_id);
                    });
                }
                else
                {TRACE_IT(7304);
                    labelPrinted = false;
                }
            }
            else
            {TRACE_IT(7305);
                Output::Print(_u("??"));
            }
        }
        else
        {TRACE_IT(7306);
            Output::Print(_u("$L%d"), targetInstr->m_id);
        }
        if (this->GetSrc1() && labelPrinted)
        {TRACE_IT(7307);
            Output::Print(_u(", "));
        }
    }
    else if (this->IsPragmaInstr() && this->m_opcode == Js::OpCode::StatementBoundary)
    {TRACE_IT(7308);
        Output::Print(_u("#%d"), this->AsPragmaInstr()->m_statementIndex);
    }

    // scope
    {TRACE_IT(7309);
        Opnd * src1 = this->GetSrc1();
        if (this->m_opcode == Js::OpCode::NewScFunc || this->m_opcode == Js::OpCode::NewScGenFunc)
        {TRACE_IT(7310);
            Assert(src1->IsIntConstOpnd());
            Js::ParseableFunctionInfo * function = nullptr;
            if (!m_func->IsOOPJIT())
            {TRACE_IT(7311);
                function = ((Js::ParseableFunctionInfo *)m_func->GetJITFunctionBody()->GetAddr())->GetNestedFunctionForExecution((uint)src1->AsIntConstOpnd()->GetValue())->GetParseableFunctionInfo();
            }
            Output::Print(_u("func:%s()"), function ? function->GetDisplayName() : _u("???"));
            Output::Print(_u(", env:"));
            this->GetSrc2()->AsRegOpnd()->m_sym->Dump(flags);
        }
        else if (src1)
        {TRACE_IT(7312);
            src1->Dump(flags, this->m_func);
            Opnd * src2 = this->GetSrc2();
            if (src2)
            {TRACE_IT(7313);
                Output::Print(_u(", "));
                src2->Dump(flags, this->m_func);
            }
        }
    }

    if (this->IsByteCodeUsesInstr())
    {TRACE_IT(7314);
        if (this->AsByteCodeUsesInstr()->GetByteCodeUpwardExposedUsed())
        {TRACE_IT(7315);
            bool first = true;
            FOREACH_BITSET_IN_SPARSEBV(id, this->AsByteCodeUsesInstr()->GetByteCodeUpwardExposedUsed())
            {TRACE_IT(7316);
                Output::Print(first? _u("s%d") : _u(", s%d"), id);
                first = false;
            }
            NEXT_BITSET_IN_SPARSEBV;
        }
        if (this->AsByteCodeUsesInstr()->propertySymUse)
        {TRACE_IT(7317);
            Output::Print(_u("  PropSym: %d"), this->AsByteCodeUsesInstr()->propertySymUse->m_id);
        }
    }

PrintByteCodeOffsetEtc:
    if (!AsmDumpMode && !SkipByteCodeOffset)
    {TRACE_IT(7318);
        this->DumpByteCodeOffset();
    }

    if (!SimpleForm)
    {TRACE_IT(7319);
        if (this->HasBailOutInfo() || this->HasAuxBailOut())
        {TRACE_IT(7320);
            BailOutInfo * bailOutInfo = this->GetBailOutInfo();
            Output::SkipToColumn(85);
            if (!AsmDumpMode)
            {TRACE_IT(7321);
                Output::Print(_u("Bailout: #%04x"), bailOutInfo->bailOutOffset);
            }
            if (!bailOutInfo->bailOutFunc->IsTopFunc())
            {TRACE_IT(7322);
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(_u(" Func %s"), bailOutInfo->bailOutFunc->GetDebugNumberSet(debugStringBuffer));
            }
            Output::Print(_u(" (%S)"), this->GetBailOutKindName());
        }
    }
    if ((flags & IRDumpFlags_SkipEndLine) == 0)
    {TRACE_IT(7323);
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
{TRACE_IT(7324);
    if (this->m_block != NULL)
    {TRACE_IT(7325);
        this->m_block->DumpHeader();
    }
    Output::Print(_u("$L%d:"), this->m_id);
    if (this->isOpHelper)
    {TRACE_IT(7326);
        Output::Print(_u(" [helper]"));
    }
    if (this->m_isLoopTop)
    {TRACE_IT(7327);
        Output::Print(_u(" >>>>>>>>>>>>>  LOOP TOP  >>>>>>>>>>>>>"));
    }
    if (this->IsProfiledLabelInstr())
    {TRACE_IT(7328);
        Output::SkipToColumn(50);
        switch (this->AsProfiledLabelInstr()->loopImplicitCallFlags)
        {
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
    {TRACE_IT(7329);
        this->DumpByteCodeOffset();
    }
    Output::Print(_u("\n"));
}


void
PragmaInstr::Dump(IRDumpFlags flags)
{TRACE_IT(7330);
    if (Js::Configuration::Global.flags.PrintSrcInDump && this->m_opcode == Js::OpCode::StatementBoundary)
    {TRACE_IT(7331);
        Js::FunctionBody * functionBody = nullptr;
        if (!m_func->IsOOPJIT())
        {TRACE_IT(7332);
            functionBody = ((Js::FunctionBody*)m_func->GetJITFunctionBody()->GetAddr());
        }
        if (functionBody)
        {TRACE_IT(7333);
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
{TRACE_IT(7334);
    Instr * instr;
    int i;

    Output::Print(_u("-------------------------------------------------------------------------------"));

    if (this == NULL)
    {TRACE_IT(7335);
        return;
    }

    for (i = 0, instr = this; (instr->m_prev != NULL && i < window/2); instr = instr->m_prev, ++i)
    {TRACE_IT(7336);} // Nothing


    for (i = 0; (instr != nullptr && i < window); instr = instr->m_next, ++i)
    {TRACE_IT(7337);
        if (instr == this)
        {TRACE_IT(7338);
            Output::Print(_u("=>"));
        }
        instr->Dump();
    }
}

void
Instr::Dump()
{TRACE_IT(7339);
    this->Dump(IRDumpFlags_None);
}

void
Instr::DumpSimple()
{TRACE_IT(7340);
    this->Dump(IRDumpFlags_SimpleForm);
}

char16 *
Instr::DumpString()
{TRACE_IT(7341);
    Output::CaptureStart();
    this->Dump();
    return Output::CaptureEnd();
}

void
Instr::DumpRange(Instr *instrEnd)
{TRACE_IT(7342);
    Output::Print(_u("-------------------------------------------------------------------------------\n"));

    FOREACH_INSTR_IN_RANGE(instr, this, instrEnd)
    {TRACE_IT(7343);
        instr->Dump();
    }
    NEXT_INSTR_IN_RANGE;

    Output::Print(_u("-------------------------------------------------------------------------------\n"));
}

#endif

} // namespace IR
