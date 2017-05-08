//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace IR {

inline IRKind
Instr::GetKind() const
{TRACE_IT(7399);
    return this->m_kind;
}

///----------------------------------------------------------------------------
///
/// Instr::IsEntryInstr
///
///----------------------------------------------------------------------------

inline bool
Instr::IsEntryInstr() const
{TRACE_IT(7400);
    return this->GetKind() == InstrKindEntry;
}

///----------------------------------------------------------------------------
///
/// Instr::AsEntryInstr
///
///     Return this as an EntryInstr *
///
///----------------------------------------------------------------------------

inline EntryInstr *
Instr::AsEntryInstr()
{TRACE_IT(7401);
    AssertMsg(this->IsEntryInstr(), "Bad call to AsEntryInstr()");

    return reinterpret_cast<EntryInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::IsExitInstr
///
///----------------------------------------------------------------------------

inline bool
Instr::IsExitInstr() const
{TRACE_IT(7402);
    return this->GetKind() == InstrKindExit;
}

///----------------------------------------------------------------------------
///
/// Instr::AsExitInstr
///
///     Return this as an ExitInstr *
///
///----------------------------------------------------------------------------

inline ExitInstr *
Instr::AsExitInstr()
{TRACE_IT(7403);
    AssertMsg(this->IsExitInstr(), "Bad call to AsExitInstr()");

    return reinterpret_cast<ExitInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::IsBranchInstr
///
///----------------------------------------------------------------------------

inline bool
Instr::IsBranchInstr() const
{TRACE_IT(7404);
    return this->GetKind() == InstrKindBranch;
}

///----------------------------------------------------------------------------
///
/// Instr::AsBranchInstr
///
///     Return this as a BranchInstr *
///
///----------------------------------------------------------------------------

inline BranchInstr *
Instr::AsBranchInstr()
{TRACE_IT(7405);
    AssertMsg(this->IsBranchInstr(), "Bad call to AsBranchInstr()");

    return reinterpret_cast<BranchInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::IsLabelInstr
///
///----------------------------------------------------------------------------

__forceinline bool
Instr::IsLabelInstr() const
{TRACE_IT(7406);
    return this->GetKind() == InstrKindLabel || this->IsProfiledLabelInstr();
}

///----------------------------------------------------------------------------
///
/// Instr::AsLabelInstr
///
///     Return this as a LabelInstr *
///
///----------------------------------------------------------------------------

inline LabelInstr *
Instr::AsLabelInstr()
{TRACE_IT(7407);
    AssertMsg(this->IsLabelInstr(), "Bad call to AsLabelInstr()");

    return reinterpret_cast<LabelInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::AsMultiBrInstr
///
///     Return this as a MultiBrInstr *
///
///----------------------------------------------------------------------------
inline MultiBranchInstr *
BranchInstr::AsMultiBrInstr()
{TRACE_IT(7408);
    AssertMsg(this->IsMultiBranch(), "Bad call to AsMultiBrInstr()");

    return static_cast<MultiBranchInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::IsPragmaInstr
///
///----------------------------------------------------------------------------

inline bool
Instr::IsPragmaInstr() const
{TRACE_IT(7409);
    return this->GetKind() == InstrKindPragma;
}

inline PragmaInstr *
Instr::AsPragmaInstr()
{TRACE_IT(7410);
    AssertMsg(this->IsPragmaInstr(), "Bad call to AsPragmaInstr()");

    return reinterpret_cast<PragmaInstr *>(this);
}

inline bool
Instr::IsJitProfilingInstr() const
{TRACE_IT(7411);
    return this->GetKind() == InstrKindJitProfiling;
}

inline JitProfilingInstr *
Instr::AsJitProfilingInstr()
{TRACE_IT(7412);
    AssertMsg(this->IsJitProfilingInstr(), "Bad call to AsProfiledInstr()");

    return reinterpret_cast<JitProfilingInstr *>(this);
}

inline bool
Instr::IsProfiledInstr() const
{TRACE_IT(7413);
    return this->GetKind() == InstrKindProfiled;
}

inline ProfiledInstr *
Instr::AsProfiledInstr()
{TRACE_IT(7414);
    AssertMsg(this->IsProfiledInstr(), "Bad call to AsProfiledInstr()");

    return reinterpret_cast<ProfiledInstr *>(this);
}

inline bool
Instr::IsProfiledLabelInstr() const
{TRACE_IT(7415);
    return this->GetKind() == InstrKindProfiledLabel;
}

inline ProfiledLabelInstr *
Instr::AsProfiledLabelInstr()
{TRACE_IT(7416);
    AssertMsg(this->IsProfiledLabelInstr(), "Bad call to AsProfiledLabelInstr()");

    return reinterpret_cast<ProfiledLabelInstr *>(this);
}

inline bool
Instr::IsByteCodeUsesInstr() const
{TRACE_IT(7417);
    return GetKind() == IR::InstrKindByteCodeUses;
}

inline ByteCodeUsesInstr *
Instr::AsByteCodeUsesInstr()
{TRACE_IT(7418);
    AssertMsg(this->IsByteCodeUsesInstr(), "Bad call to AsByteCodeUsesInstr()");
    return (ByteCodeUsesInstr *)this;
}

///----------------------------------------------------------------------------
///
/// Instr::IsLowered
///
///     Is this instr lowered to machine dependent opcode?
///
///----------------------------------------------------------------------------

inline bool
Instr::IsLowered() const
{TRACE_IT(7419);
    return m_opcode > Js::OpCode::MDStart;
}

///----------------------------------------------------------------------------
///
/// Instr::StartsBasicBlock
///
///     Does this instruction mark the beginning of a basic block?
///
///----------------------------------------------------------------------------

inline bool
Instr::StartsBasicBlock() const
{TRACE_IT(7420);
    return this->IsLabelInstr() || this->IsEntryInstr();
}

///----------------------------------------------------------------------------
///
/// Instr::EndsBasicBlock
///
///     Does this instruction mark the end of a basic block?
///
///----------------------------------------------------------------------------

inline bool
Instr::EndsBasicBlock() const
{TRACE_IT(7421);
    return
        this->IsBranchInstr() ||
        this->IsExitInstr() ||
        this->m_opcode == Js::OpCode::Ret ||
        this->m_opcode == Js::OpCode::Throw ||
        this->m_opcode == Js::OpCode::RuntimeTypeError ||
        this->m_opcode == Js::OpCode::RuntimeReferenceError;
}

///----------------------------------------------------------------------------
///
/// Instr::HasFallThrough
///
///     Can execution fall through to the next instruction?
///
///----------------------------------------------------------------------------

inline bool
Instr::HasFallThrough() const
{TRACE_IT(7422);
    return (!(this->IsBranchInstr() && const_cast<Instr*>(this)->AsBranchInstr()->IsUnconditional())
            && OpCodeAttr::HasFallThrough(this->m_opcode));

}


inline bool
Instr::IsNewScObjectInstr() const
{TRACE_IT(7423);
    return this->m_opcode == Js::OpCode::NewScObject || this->m_opcode == Js::OpCode::NewScObjectNoCtor;
}

inline bool
Instr::IsInvalidInstr() const
{TRACE_IT(7424);
    return this == (Instr*)InvalidInstrLayout;
}

inline Instr*
Instr::GetInvalidInstr()
{TRACE_IT(7425);
    return (Instr*)InvalidInstrLayout;
}

///----------------------------------------------------------------------------
///
/// Instr::GetDst
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::GetDst() const
{TRACE_IT(7426);
    return this->m_dst;
}

///----------------------------------------------------------------------------
///
/// Instr::GetSrc1
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::GetSrc1() const
{TRACE_IT(7427);
    return this->m_src1;
}

///----------------------------------------------------------------------------
///
/// Instr::SetSrc1
///
///     Makes a copy if opnd is in use...
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::SetSrc1(Opnd * newSrc)
{TRACE_IT(7428);
    AssertMsg(this->m_src1 == NULL, "Trying to overwrite existing src.");

    newSrc = newSrc->Use(m_func);
    this->m_src1 = newSrc;

    return newSrc;
}

///----------------------------------------------------------------------------
///
/// Instr::GetSrc2
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::GetSrc2() const
{TRACE_IT(7429);
    return this->m_src2;
}

///----------------------------------------------------------------------------
///
/// Instr::SetSrc2
///
///     Makes a copy if opnd is in use...
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::SetSrc2(Opnd * newSrc)
{TRACE_IT(7430);
    AssertMsg(this->m_src2 == NULL, "Trying to overwrite existing src.");

    newSrc = newSrc->Use(m_func);
    this->m_src2 = newSrc;

    return newSrc;
}

// IsInlined - true if the function that contains the instr has been inlined
inline bool
Instr::IsInlined() const
{TRACE_IT(7431);
    return this->m_func->IsInlined();
}


///----------------------------------------------------------------------------
///
/// Instr::ForEachCallDirectArgOutInstrBackward
///
///     Walks the ArgsOut of CallDirect backwards
///
///----------------------------------------------------------------------------
template <typename Fn>
inline bool
Instr::ForEachCallDirectArgOutInstrBackward(Fn fn, uint argsOpndLength) const
{TRACE_IT(7432);
    Assert(this->m_opcode == Js::OpCode::CallDirect); // Right now we call this method only for partial inlining of split, match and exec. Can be modified for other uses also.

    // CallDirect src2
    IR::Opnd * linkOpnd = this->GetSrc2();

    // ArgOut_A_InlineSpecialized
    IR::Instr * tmpInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_instrDef;
    Assert(tmpInstr->m_opcode == Js::OpCode::ArgOut_A_InlineSpecialized);

    // ArgOut_A_InlineSpecialized src2; link to actual argouts.
    linkOpnd = tmpInstr->GetSrc2();
    uint32 argCount = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();

    if (argCount != argsOpndLength)
    {TRACE_IT(7433);
        return false;
    }

    while (linkOpnd->IsSymOpnd() && argCount > 0)
    {TRACE_IT(7434);
        IR::SymOpnd *src2 = linkOpnd->AsSymOpnd();
        StackSym *sym = src2->m_sym->AsStackSym();
        Assert(sym->m_isSingleDef);
        IR::Instr *argInstr = sym->m_instrDef;
        Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A);
        if (fn(argInstr, argCount - 1))
        {TRACE_IT(7435);
            return true;
        }
        argCount--;

        linkOpnd = argInstr->GetSrc2();
    }
    return false;
}

///----------------------------------------------------------------------------
///
/// BranchInstr::SetTarget
///
///----------------------------------------------------------------------------

inline void
BranchInstr::SetTarget(LabelInstr * labelInstr)
{TRACE_IT(7436);
    Assert(!this->m_isMultiBranch);
    if (this->m_branchTarget)
    {TRACE_IT(7437);
        this->m_branchTarget->RemoveLabelRef(this);
    }
    if (labelInstr)
    {TRACE_IT(7438);
        labelInstr->AddLabelRef(this);
    }
    this->m_branchTarget = labelInstr;
}

inline void
BranchInstr::ClearTarget()
{TRACE_IT(7439);
    if (this->IsMultiBranch())
    {TRACE_IT(7440);
        this->AsMultiBrInstr()->ClearTarget();
    }
    else
    {TRACE_IT(7441);
        this->SetTarget(nullptr);
    }
}

///----------------------------------------------------------------------------
///
/// BranchInstr::GetTarget
///
///----------------------------------------------------------------------------

inline LabelInstr *
BranchInstr::GetTarget() const
{TRACE_IT(7442);
    return this->m_branchTarget;
}

///----------------------------------------------------------------------------
///
/// BranchInstr::IsConditional
///
///----------------------------------------------------------------------------

inline bool
BranchInstr::IsConditional() const
{TRACE_IT(7443);
    return !this->IsUnconditional();
}

inline bool
BranchInstr::IsMultiBranch() const
{TRACE_IT(7444);
    if (m_branchTarget)
    {TRACE_IT(7445);
        Assert(!m_isMultiBranch);
        return false;
    }
    else
    {TRACE_IT(7446);
        Assert(m_isMultiBranch);
        return true;    // it's a multi branch instr
    }

}

///----------------------------------------------------------------------------
///
/// BranchInstr::IsUnconditional
///
///----------------------------------------------------------------------------

inline bool
BranchInstr::IsUnconditional() const
{TRACE_IT(7447);
    if (this->IsLowered())
    {TRACE_IT(7448);
        return LowererMD::IsUnconditionalBranch(this);
    }
    else
    {TRACE_IT(7449);
        return (this->m_opcode == Js::OpCode::Br || this->m_opcode == Js::OpCode::MultiBr);
    }
}

///----------------------------------------------------------------------------
///
/// MultiBranchInstr::AddtoDictionary
///     - Adds the string to the list with the targetoffset
///       In order for the dictionary to have the right value, MapBranchTargetAddress
///       needs to be called to populate the dictionary and then it'll be patched up
///       to the right values
///
///----------------------------------------------------------------------------
inline void
MultiBranchInstr::AddtoDictionary(uint32 offset, TBranchKey key, void* remoteVar)
{TRACE_IT(7450);
    Assert(this->m_kind == StrDictionary);
    Assert(key);
    auto dict = this->GetBranchDictionary();
    dict->AddEntry(offset, key, remoteVar);
}

inline void
MultiBranchInstr::AddtoJumpTable(uint32 offset, uint32 jmpIndex)
{TRACE_IT(7451);
    Assert(this->m_kind == IntJumpTable || this->m_kind == SingleCharStrJumpTable);
    Assert(jmpIndex != -1);
    auto table = this->GetBranchJumpTable();
    table->jmpTable[jmpIndex] = (void*)offset;
}

inline void
MultiBranchInstr::FixMultiBrDefaultTarget(uint32 targetOffset)
{TRACE_IT(7452);
    this->GetBranchJumpTable()->defaultTarget = (void *)targetOffset;
}

inline void
MultiBranchInstr::CreateBranchTargetsAndSetDefaultTarget(int size, Kind kind, uint defaultTargetOffset)
{TRACE_IT(7453);
    AssertMsg(size != 0, "The dictionary/jumpTable size should not be zero");

    NativeCodeData::Allocator * allocator = this->m_func->GetNativeCodeDataAllocator();
    m_kind = kind;
    switch (kind)
    {
    case IntJumpTable:
    case SingleCharStrJumpTable:
        {TRACE_IT(7454);
            JitArenaAllocator * jitAllocator = this->m_func->GetTopFunc()->m_alloc;
            BranchJumpTableWrapper * branchTargets = BranchJumpTableWrapper::New(jitAllocator, size);
            branchTargets->defaultTarget = (void *)defaultTargetOffset;
            this->m_branchTargets = branchTargets;
            break;
        }
    case StrDictionary:
        {TRACE_IT(7455);
            BranchDictionaryWrapper * branchTargets = BranchDictionaryWrapper::New(allocator, size, m_func->IsOOPJIT() ? m_func->m_alloc : nullptr);
            branchTargets->defaultTarget = (void *)defaultTargetOffset;
            this->m_branchTargets = branchTargets;
            break;
        }
    default:
        Assert(false);
    };
}

inline MultiBranchInstr::BranchDictionaryWrapper *
MultiBranchInstr::GetBranchDictionary()
{TRACE_IT(7456);
    return reinterpret_cast<MultiBranchInstr::BranchDictionaryWrapper *>(m_branchTargets);
}

inline MultiBranchInstr::BranchJumpTable *
MultiBranchInstr::GetBranchJumpTable()
{TRACE_IT(7457);
    return reinterpret_cast<MultiBranchInstr::BranchJumpTable *>(m_branchTargets);
}

inline void
MultiBranchInstr::ChangeLabelRef(LabelInstr * oldTarget, LabelInstr * newTarget)
{TRACE_IT(7458);
    if (oldTarget)
    {TRACE_IT(7459);
        oldTarget->RemoveLabelRef(this);
    }
    if (newTarget)
    {TRACE_IT(7460);
        newTarget->AddLabelRef(this);
    }
}

///----------------------------------------------------------------------------
///
/// LabelInstr::SetPC
///
///----------------------------------------------------------------------------

inline void
LabelInstr::SetPC(BYTE * pc)
{TRACE_IT(7461);
    this->m_pc.pc = pc;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::GetPC
///
///----------------------------------------------------------------------------

inline BYTE *
LabelInstr::GetPC(void) const
{TRACE_IT(7462);
    return this->m_pc.pc;
}
///----------------------------------------------------------------------------
///
/// LabelInstr::ResetOffset
///
///----------------------------------------------------------------------------

inline void
LabelInstr::ResetOffset(uint32 offset)
{TRACE_IT(7463);
    AssertMsg(this->isInlineeEntryInstr, "As of now only InlineeEntryInstr overwrites the offset at encoder stage");
    this->m_pc.offset = offset;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::SetOffset
///
///----------------------------------------------------------------------------

inline void
LabelInstr::SetOffset(uint32 offset)
{TRACE_IT(7464);
    AssertMsg(this->m_pc.offset == 0, "Overwriting existing byte offset");
    this->m_pc.offset = offset;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::GetOffset
///
///----------------------------------------------------------------------------

inline uint32
LabelInstr::GetOffset(void) const
{TRACE_IT(7465);

    return this->m_pc.offset;
}



///----------------------------------------------------------------------------
///
/// LabelInstr::SetBasicBlock
///
///----------------------------------------------------------------------------

inline void
LabelInstr::SetBasicBlock(BasicBlock * block)
{TRACE_IT(7466);
    AssertMsg(this->m_block == nullptr || block == nullptr, "Overwriting existing block pointer");
    this->m_block = block;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::GetBasicBlock
///
///----------------------------------------------------------------------------

inline BasicBlock *
LabelInstr::GetBasicBlock(void) const
{TRACE_IT(7467);
    return this->m_block;
}

inline void
LabelInstr::SetLoop(Loop* loop)
{TRACE_IT(7468);
    Assert(this->m_isLoopTop);
    this->m_loop = loop;
}

inline Loop*
LabelInstr::GetLoop(void) const
{TRACE_IT(7469);
    Assert(this->m_isLoopTop);
    return this->m_loop;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::UnlinkBasicBlock
///
///----------------------------------------------------------------------------

inline void
LabelInstr::UnlinkBasicBlock(void)
{TRACE_IT(7470);
    this->m_block = nullptr;
}

inline BOOL
LabelInstr::IsUnreferenced(void) const
{TRACE_IT(7471);
    return labelRefs.Empty() && !m_hasNonBranchRef;
}

inline void
LabelInstr::SetRegion(Region * region)
{TRACE_IT(7472);
    this->m_region = region;
}

inline Region *
LabelInstr::GetRegion(void) const
{TRACE_IT(7473);
    return this->m_region;
}

} // namespace IR
