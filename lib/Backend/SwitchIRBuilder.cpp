//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

///----------------------------------------------------------------------------
///
/// IRBuilderSwitchAdapter
///
///     Implementation for IRBuilderSwitchAdapter, which passes actions generated
///     by a SwitchIRBuilder to an IRBuilder instance
///----------------------------------------------------------------------------

void
IRBuilderSwitchAdapter::AddBranchInstr(IR::BranchInstr * instr, uint32 offset, uint32 targetOffset, bool clearBackEdge)
{TRACE_IT(15457);
    BranchReloc * reloc = m_builder->AddBranchInstr(instr, offset, targetOffset);

    if (clearBackEdge)
    {TRACE_IT(15458);
        reloc->SetNotBackEdge();
    }
}

void
IRBuilderSwitchAdapter::AddInstr(IR::Instr * instr, uint32 offset)
{TRACE_IT(15459);
    m_builder->AddInstr(instr, offset);
}

void
IRBuilderSwitchAdapter::CreateRelocRecord(IR::BranchInstr * branchInstr, uint32 offset, uint32 targetOffset, bool clearBackEdge)
{TRACE_IT(15460);
    BranchReloc * reloc = m_builder->CreateRelocRecord(
        branchInstr,
        offset,
        targetOffset);

    if (clearBackEdge)
    {TRACE_IT(15461);
        reloc->SetNotBackEdge();
    }
}

void
IRBuilderSwitchAdapter::ConvertToBailOut(IR::Instr * instr, IR::BailOutKind kind)
{TRACE_IT(15462);
    instr = instr->ConvertToBailOutInstr(instr, kind);

    Assert(instr->GetByteCodeOffset() < m_builder->m_offsetToInstructionCount);
    m_builder->m_offsetToInstruction[instr->GetByteCodeOffset()] = instr;
}

///----------------------------------------------------------------------------
///
/// IRBuilderAsmJsSwitchAdapter
///
///     Implementation for IRBuilderSwitchAdapter, which passes actions generated
///     by a SwitchIRBuilder to an IRBuilder instance
///----------------------------------------------------------------------------

#ifdef ASMJS_PLAT
void
IRBuilderAsmJsSwitchAdapter::AddBranchInstr(IR::BranchInstr * instr, uint32 offset, uint32 targetOffset, bool clearBackEdge)
{TRACE_IT(15463);
    BranchReloc * reloc = m_builder->AddBranchInstr(instr, offset, targetOffset);

    if (clearBackEdge)
    {TRACE_IT(15464);
        reloc->SetNotBackEdge();
    }
}

void
IRBuilderAsmJsSwitchAdapter::AddInstr(IR::Instr * instr, uint32 offset)
{TRACE_IT(15465);
    m_builder->AddInstr(instr, offset);
}

void
IRBuilderAsmJsSwitchAdapter::CreateRelocRecord(IR::BranchInstr * branchInstr, uint32 offset, uint32 targetOffset, bool clearBackEdge)
{TRACE_IT(15466);
    BranchReloc * reloc = m_builder->CreateRelocRecord(
        branchInstr,
        offset,
        targetOffset);

    if (clearBackEdge)
    {TRACE_IT(15467);
        reloc->SetNotBackEdge();
    }
}

void
IRBuilderAsmJsSwitchAdapter::ConvertToBailOut(IR::Instr * instr, IR::BailOutKind kind)
{TRACE_IT(15468);
    Assert(false);
    // ConvertToBailOut should never get called for AsmJs
    // switches, since we already know ahead of time that the
    // switch expression is Int32
}
#endif

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::Init
///
///     Initializes the function and temporary allocator for the SwitchIRBuilder
///----------------------------------------------------------------------------

void
SwitchIRBuilder::Init(Func * func, JitArenaAllocator * tempAlloc, bool isAsmJs)
{TRACE_IT(15469);
    m_func = func;
    m_tempAlloc = tempAlloc;
    m_isAsmJs = isAsmJs;

    // caseNodes is a list of Case instructions
    m_caseNodes = CaseNodeList::New(tempAlloc);
    m_seenOnlySingleCharStrCaseNodes = true;
    m_intConstSwitchCases = JitAnew(tempAlloc, BVSparse<JitArenaAllocator>, tempAlloc);
    m_strConstSwitchCases = StrSwitchCaseList::New(tempAlloc);

    m_eqOp = isAsmJs ? Js::OpCode::BrEq_I4 : Js::OpCode::BrSrEq_A;
    m_ltOp = isAsmJs ? Js::OpCode::BrLt_I4 : Js::OpCode::BrLt_A;
    m_leOp = isAsmJs ? Js::OpCode::BrLe_I4 : Js::OpCode::BrLe_A;
    m_gtOp = isAsmJs ? Js::OpCode::BrGt_I4 : Js::OpCode::BrGt_A;
    m_geOp = isAsmJs ? Js::OpCode::BrGe_I4 : Js::OpCode::BrGe_A;
    m_subOp = isAsmJs ? Js::OpCode::Sub_I4 : Js::OpCode::Sub_A;
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::BeginSwitch
///
///     Prepares the SwitchIRBuilder for building a new switch statement
///----------------------------------------------------------------------------

void
SwitchIRBuilder::BeginSwitch()
{TRACE_IT(15470);
    m_intConstSwitchCases->ClearAll();
    m_strConstSwitchCases->Clear();

    if (m_isAsmJs)
    {TRACE_IT(15471);
        // never build bailout information for asmjs
        m_switchOptBuildBail = false;
        // asm.js switch is always integer
        m_switchIntDynProfile = true;
        AssertMsg(!m_switchStrDynProfile, "String profiling should not be enabled for an asm.js switch statement");
    }
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::EndSwitch
///
///     Notifies the switch builder the switch being generated has been completed
///----------------------------------------------------------------------------

void
SwitchIRBuilder::EndSwitch(uint32 offset, uint32 targetOffset)
{TRACE_IT(15472);
    FlushCases(targetOffset);
    AssertMsg(m_caseNodes->Count() == 0, "Not all switch case nodes built by end of switch");

    // only generate the final unconditional jump at the end of the switch
    IR::BranchInstr * branch = IR::BranchInstr::New(Js::OpCode::Br, nullptr, m_func);
    m_adapter->AddBranchInstr(branch, offset, targetOffset, true);

    m_profiledSwitchInstr = nullptr;
}


///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::SetProfiledInstruction
///
///     Sets the profiled switch instruction for the switch statement that
///     is being built
///----------------------------------------------------------------------------


void
SwitchIRBuilder::SetProfiledInstruction(IR::Instr * instr, Js::ProfileId profileId)
{TRACE_IT(15473);
    m_profiledSwitchInstr = instr;
    m_switchOptBuildBail = true;

    //don't optimize if the switch expression is not an Integer (obtained via dynamic profiling data of the BeginSwitch opcode)

    bool hasProfile = m_profiledSwitchInstr->IsProfiledInstr() && m_profiledSwitchInstr->m_func->HasProfileInfo();

    if (hasProfile)
    {TRACE_IT(15474);
        const ValueType valueType(m_profiledSwitchInstr->m_func->GetReadOnlyProfileInfo()->GetSwitchProfileInfo(profileId));
        instr->AsProfiledInstr()->u.FldInfo().valueType = valueType;
        m_switchIntDynProfile = valueType.IsLikelyTaggedInt();
        m_switchStrDynProfile = valueType.IsLikelyString();

        if (PHASE_TESTTRACE1(Js::SwitchOptPhase))
        {TRACE_IT(15475);
            char valueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
            valueType.ToString(valueTypeStr);
#if ENABLE_DEBUG_CONFIG_OPTIONS
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
            PHASE_PRINT_TESTTRACE1(Js::SwitchOptPhase, _u("Func %s, Switch %d: Expression Type : %S\n"),
                m_profiledSwitchInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                m_profiledSwitchInstr->AsProfiledInstr()->u.profileId, valueTypeStr);
        }
    }
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::OnCase
///
///     Handles a case instruction, generating the appropriate branches, or
///     storing the case to generate a more optimized branch later
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::OnCase(IR::RegOpnd * src1Opnd, IR::Opnd * src2Opnd, uint32 offset, uint32 targetOffset)
{TRACE_IT(15476);
    IR::BranchInstr * branchInstr;

    Assert(src2Opnd->IsIntConstOpnd() || src2Opnd->IsRegOpnd());
    // Support only int32 const opnd
    Assert(!src2Opnd->IsIntConstOpnd() || src2Opnd->GetType() == TyInt32);
    StackSym* sym = src2Opnd->GetStackSym();
    const bool isIntConst = src2Opnd->IsIntConstOpnd() || (sym && sym->IsIntConst());
    const bool isStrConst = !isIntConst && sym && sym->m_isStrConst;

    if (GlobOpt::IsSwitchOptEnabled(m_func->GetTopFunc()) && 
        isIntConst && 
        m_intConstSwitchCases->TestAndSet(sym ? sym->GetIntConstValue() : src2Opnd->AsIntConstOpnd()->AsInt32()))
    {TRACE_IT(15477);
        // We've already seen a case statement with the same int const value. No need to emit anything for this.
        return;
    }

    if (GlobOpt::IsSwitchOptEnabled(m_func->GetTopFunc()) && isStrConst
        && TestAndAddStringCaseConst(JITJavascriptString::FromVar(sym->GetConstAddress(true))))
    {TRACE_IT(15478);
        // We've already seen a case statement with the same string const value. No need to emit anything for this.
        return;
    }

    branchInstr = IR::BranchInstr::New(m_eqOp, nullptr, src1Opnd, src2Opnd, m_func);
    branchInstr->m_isSwitchBr = true;

    /*
    //  Switch optimization
    //  For Integers - Binary Search or jump table optimization technique is used
    //  For Strings - Dictionary look up technique is used.
    //
    //  For optimizing, the Load instruction corresponding to the switch instruction is profiled in the interpreter.
    //  Based on the dynamic profile data, optimization technique is decided.
    */

    bool deferred = false;

    if (GlobOpt::IsSwitchOptEnabled(m_func->GetTopFunc()))
    {TRACE_IT(15479);
        if (m_switchIntDynProfile && isIntConst)
        {TRACE_IT(15480);
            CaseNode* caseNode = JitAnew(m_tempAlloc, CaseNode, branchInstr, offset, targetOffset, src2Opnd);
            m_caseNodes->Add(caseNode);
            deferred = true;
        }
        else if (m_switchStrDynProfile && isStrConst)
        {TRACE_IT(15481);
            CaseNode* caseNode = JitAnew(m_tempAlloc, CaseNode, branchInstr, offset, targetOffset, src2Opnd);
            m_caseNodes->Add(caseNode);
            m_seenOnlySingleCharStrCaseNodes = m_seenOnlySingleCharStrCaseNodes && caseNode->GetUpperBoundStringConstLocal()->GetLength() == 1;
            deferred = true;
        }
    }

    if (!deferred)
    {TRACE_IT(15482);
        FlushCases(offset);
        m_adapter->AddBranchInstr(branchInstr, offset, targetOffset);
    }
}


///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::FlushCases
///
///     Called when a scenario for which optimized switch cases cannot be
///     generated occurs, and generates optimized branches for all cases that
///     have been stored up to this point
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::FlushCases(uint32 targetOffset)
{TRACE_IT(15483);
    if (m_caseNodes->Empty())
    {TRACE_IT(15484);
        return;
    }

    if (m_switchIntDynProfile)
    {TRACE_IT(15485);
        BuildCaseBrInstr(targetOffset);
    }
    else if (m_switchStrDynProfile)
    {TRACE_IT(15486);
        BuildMultiBrCaseInstrForStrings(targetOffset);
    }
    else
    {TRACE_IT(15487);
        Assert(false);
    }
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::RefineCaseNodes
///
///     Filter IR instructions for case statements that contain no case blocks.
///     Also sets upper bound and lower bound for case instructions that has a
///     consecutive set of cases with just one case block.
///----------------------------------------------------------------------------

void
SwitchIRBuilder::RefineCaseNodes()
{TRACE_IT(15488);
    m_caseNodes->Sort();

    CaseNodeList * tmpCaseNodes = CaseNodeList::New(m_tempAlloc);

    for (int currCaseIndex = 1; currCaseIndex < m_caseNodes->Count(); currCaseIndex++)
    {TRACE_IT(15489);
        CaseNode * prevCaseNode = m_caseNodes->Item(currCaseIndex - 1);
        CaseNode * currCaseNode = m_caseNodes->Item(currCaseIndex);
        uint32 prevCaseTargetOffset = prevCaseNode->GetTargetOffset();
        uint32 currCaseTargetOffset = currCaseNode->GetTargetOffset();
        int prevCaseConstValue = prevCaseNode->GetUpperBoundIntConst();
        int currCaseConstValue = currCaseNode->GetUpperBoundIntConst();

        /*To handle empty case statements with/without repetition*/
        if (prevCaseTargetOffset == currCaseTargetOffset &&
            (prevCaseConstValue + 1 == currCaseConstValue || prevCaseConstValue == currCaseConstValue))
        {TRACE_IT(15490);
            m_caseNodes->Item(currCaseIndex)->SetLowerBound(prevCaseNode->GetLowerBound());
        }
        else
        {TRACE_IT(15491);
            if (tmpCaseNodes->Count() != 0)
            {TRACE_IT(15492);
                int lastTmpCaseConstValue = tmpCaseNodes->Item(tmpCaseNodes->Count() - 1)->GetUpperBoundIntConst();
                /*To handle duplicate non empty case statements*/
                if (lastTmpCaseConstValue != prevCaseConstValue)
                {TRACE_IT(15493);
                    tmpCaseNodes->Add(prevCaseNode);
                }
            }
            else
            {TRACE_IT(15494);
                tmpCaseNodes->Add(prevCaseNode); //Adding for the first time in tmpCaseNodes
            }
        }

    }

    //Adds the last caseNode in the caseNodes list.
    tmpCaseNodes->Add(m_caseNodes->Item(m_caseNodes->Count() - 1));

    m_caseNodes = tmpCaseNodes;
}

///--------------------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildBinaryTraverseInstr
///
///     Build IR instructions for case statements in a binary search traversal fashion.
///     defaultLeafBranch: offset of the next instruction to be branched after
///                        the set of case instructions under investigation
///--------------------------------------------------------------------------------------

void
SwitchIRBuilder::BuildBinaryTraverseInstr(int start, int end, uint32 defaultLeafBranch)
{TRACE_IT(15495);
    int mid;

    if (start > end)
    {TRACE_IT(15496);
        return;
    }

    if (end - start <= CONFIG_FLAG(MaxLinearIntCaseCount) - 1) // -1 for handling zero index as the base
    {
        //if only 3 elements, then do linear search on the elements
        BuildLinearTraverseInstr(start, end, defaultLeafBranch);
        return;
    }

    mid = start + ((end - start + 1) / 2);
    CaseNode* midNode = m_caseNodes->Item(mid);
    CaseNode* startNode = m_caseNodes->Item(start);

    // if the value that we are switching on is greater than the start case value
    // then we branch right to the right half of the binary search
    IR::BranchInstr* caseInstr = startNode->GetCaseInstr();
    IR::BranchInstr* branchInstr = IR::BranchInstr::New(m_geOp, nullptr, caseInstr->GetSrc1(), midNode->GetLowerBound(), m_func);
    branchInstr->m_isSwitchBr = true;
    m_adapter->AddBranchInstr(branchInstr, startNode->GetOffset(), midNode->GetOffset(), true);

    BuildBinaryTraverseInstr(start, mid - 1, defaultLeafBranch);
    BuildBinaryTraverseInstr(mid, end, defaultLeafBranch);
}

///------------------------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildEmptyCasesInstr
///
///     Build IR instructions for Empty consecutive case statements (with only one case block).
///     defaultLeafBranch: offset of the next instruction to be branched after
///                        the set of case instructions under investigation
///
///------------------------------------------------------------------------------------------

void
SwitchIRBuilder::BuildEmptyCasesInstr(CaseNode* caseNode, uint32 fallThrOffset)
{TRACE_IT(15497);
    IR::BranchInstr* branchInstr;
    IR::Opnd* src1Opnd;

    src1Opnd = caseNode->GetCaseInstr()->GetSrc1();

    AssertMsg(caseNode->GetLowerBound() != caseNode->GetUpperBound(), "The upper bound and lower bound should not be the same");

    //Generate <lb instruction
    branchInstr = IR::BranchInstr::New(m_ltOp, nullptr, src1Opnd, caseNode->GetLowerBound(), m_func);
    branchInstr->m_isSwitchBr = true;
    m_adapter->AddBranchInstr(branchInstr, caseNode->GetOffset(), fallThrOffset, true);

    //Generate <=ub instruction
    branchInstr = IR::BranchInstr::New(m_leOp, nullptr, src1Opnd, caseNode->GetUpperBound(), m_func);
    branchInstr->m_isSwitchBr = true;
    m_adapter->AddBranchInstr(branchInstr, caseNode->GetOffset(), caseNode->GetTargetOffset(), true);

    BuildBailOnNotInteger();
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildLinearTraverseInstr
///
///     Build IR instr for case statements less than a threshold.
///     defaultLeafBranch: offset of the next instruction to be branched after
///                        the set of case instructions under investigation
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::BuildLinearTraverseInstr(int start, int end, uint fallThrOffset)
{TRACE_IT(15498);
    Assert(fallThrOffset);
    for (int index = start; index <= end; index++)
    {TRACE_IT(15499);
        CaseNode* currCaseNode = m_caseNodes->Item(index);

        bool dontBuildEmptyCases = false;

        if (currCaseNode->IsUpperBoundIntConst())
        {TRACE_IT(15500);
            int lowerBoundCaseConstValue = currCaseNode->GetLowerBoundIntConst();
            int upperBoundCaseConstValue = currCaseNode->GetUpperBoundIntConst();

            if (lowerBoundCaseConstValue == upperBoundCaseConstValue)
            {TRACE_IT(15501);
                dontBuildEmptyCases = true;
            }
        }
        else if (currCaseNode->IsUpperBoundStrConst())
        {TRACE_IT(15502);
            dontBuildEmptyCases = true;
        }
        else
        {
            AssertMsg(false, "An integer/String CaseNode is required for BuildLinearTraverseInstr");
        }

        if (dontBuildEmptyCases)
        {TRACE_IT(15503);
            // only if the instruction is not part of a cluster of empty consecutive case statements.
            m_adapter->AddBranchInstr(currCaseNode->GetCaseInstr(), currCaseNode->GetOffset(), currCaseNode->GetTargetOffset());
        }
        else
        {
            BuildEmptyCasesInstr(currCaseNode, fallThrOffset);
        }
    }

    // Adds an unconditional branch instruction at the end

    IR::BranchInstr* branchInstr = IR::BranchInstr::New(Js::OpCode::Br, nullptr, m_func);
    branchInstr->m_isSwitchBr = true;
    m_adapter->AddBranchInstr(branchInstr, Js::Constants::NoByteCodeOffset, fallThrOffset, true);
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::ResetCaseNodes
///
///     Resets SwitchIRBuilder to begin building another switch statement.
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::ResetCaseNodes()
{TRACE_IT(15504);
    m_caseNodes->Clear();
    m_seenOnlySingleCharStrCaseNodes = true;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::BuildCaseBrInstr
///     Generates the branch instructions to optimize the switch case execution flow
///     -Sorts, Refines and generates instructions in binary traversal fashion
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::BuildCaseBrInstr(uint32 targetOffset)
{TRACE_IT(15505);
    Assert(m_isAsmJs || m_profiledSwitchInstr);

    int start = 0;
    int end = m_caseNodes->Count() - 1;

    if (m_caseNodes->Count() <= CONFIG_FLAG(MaxLinearIntCaseCount))
    {
        BuildLinearTraverseInstr(start, end, targetOffset);
        ResetCaseNodes();
        return;
    }

    RefineCaseNodes();

    BuildOptimizedIntegerCaseInstrs(targetOffset);

    ResetCaseNodes(); // clear the list for the next new set of integers - or for a new switch case statement

                      //optimization is definitely performed when the number of cases is greater than the threshold
    if (end - start > CONFIG_FLAG(MaxLinearIntCaseCount) - 1) // -1 for handling zero index as the base
    {TRACE_IT(15506);
        BuildBailOnNotInteger();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::BuildOptimizedIntegerCaseInstrs
///     Identify chunks of integers cases(consecutive integers)
///     Apply  jump table or binary traversal based on the density of the case arms
///
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::BuildOptimizedIntegerCaseInstrs(uint32 targetOffset)
{TRACE_IT(15507);
    int startjmpTableIndex = 0;
    int endjmpTableIndex = 0;
    int startBinaryTravIndex = 0;
    int endBinaryTravIndex = 0;

    IR::MultiBranchInstr * multiBranchInstr = nullptr;

    /*
    *   Algorithm to find chunks of consecutive integers in a given set of case arms(sorted)
    *   -Start and end indices for jump table and binary tree are maintained.
    *   -The corresponding start and end indices indicate that they are suitable candidate for their respective category(binaryTree/jumpTable)
    *   -All holes are filled with an offset corresponding to the default fallthrough instruction and each block is filled with an offset corresponding to the start of the next block
    *    A Block here refers either to a jump table or to a binary tree.
    *   -Blocks of BinaryTrav/Jump table are traversed in a linear fashion.
    **/
    for (int currentIndex = 0; currentIndex < m_caseNodes->Count() - 1; currentIndex++)
    {TRACE_IT(15508);
        int nextIndex = currentIndex + 1;
        //Check if there is no missing value between subsequent case arms
        if (m_caseNodes->Item(currentIndex)->GetUpperBoundIntConst() + 1 != m_caseNodes->Item(nextIndex)->GetUpperBoundIntConst())
        {TRACE_IT(15509);
            //value of the case nodes are guaranteed to be 32 bits or less than 32bits at this point(if it is more, the Switch Opt will not kick in)
            Assert(nextIndex == endjmpTableIndex + 1);
            int64 speculatedEndJmpCaseValue = m_caseNodes->Item(nextIndex)->GetUpperBoundIntConst();
            int64 endJmpCaseValue = m_caseNodes->Item(endjmpTableIndex)->GetUpperBoundIntConst();
            int64 startJmpCaseValue = m_caseNodes->Item(startjmpTableIndex)->GetUpperBoundIntConst();

            int64 speculatedJmpTableSize = speculatedEndJmpCaseValue - startJmpCaseValue + 1;
            int64 jmpTableSize = endJmpCaseValue - startJmpCaseValue + 1;

            int numFilledEntries = nextIndex - startjmpTableIndex + 1;

            //Checks if the % of filled entries(unique targets from the case arms) in the jump table is within the threshold
            if (speculatedJmpTableSize != 0 && ((numFilledEntries)* 100 / speculatedJmpTableSize) < (100 - CONFIG_FLAG(SwitchOptHolesThreshold)))
            {TRACE_IT(15510);
                if (jmpTableSize >= CONFIG_FLAG(MinSwitchJumpTableSize))
                {TRACE_IT(15511);
                    uint32 fallThrOffset = m_caseNodes->Item(endjmpTableIndex)->GetOffset();
                    TryBuildBinaryTreeOrMultiBrForSwitchInts(multiBranchInstr, fallThrOffset, startjmpTableIndex, endjmpTableIndex, startBinaryTravIndex, targetOffset);

                    //Reset start/end indices of BinaryTrav to the next index.
                    startBinaryTravIndex = nextIndex;
                    endBinaryTravIndex = nextIndex;
                }

                //Reset start/end indices of the jump table to the next index.
                startjmpTableIndex = nextIndex;
                endjmpTableIndex = nextIndex;
            }
            else
            {TRACE_IT(15512);
                endjmpTableIndex++;
            }
        }
        else
        {TRACE_IT(15513);
            endjmpTableIndex++;
        }
    }

    int64 endJmpCaseValue = m_caseNodes->Item(endjmpTableIndex)->GetUpperBoundIntConst();
    int64 startJmpCaseValue = m_caseNodes->Item(startjmpTableIndex)->GetUpperBoundIntConst();
    int64 jmpTableSize = endJmpCaseValue - startJmpCaseValue + 1;

    if (jmpTableSize < CONFIG_FLAG(MinSwitchJumpTableSize))
    {TRACE_IT(15514);
        endBinaryTravIndex = endjmpTableIndex;
        BuildBinaryTraverseInstr(startBinaryTravIndex, endBinaryTravIndex, targetOffset);
        if (multiBranchInstr)
        {
            FixUpMultiBrJumpTable(multiBranchInstr, multiBranchInstr->GetNextRealInstr()->GetByteCodeOffset());
            multiBranchInstr = nullptr;
        }
    }
    else
    {TRACE_IT(15515);
        uint32 fallthrOffset = m_caseNodes->Item(endjmpTableIndex)->GetOffset();
        TryBuildBinaryTreeOrMultiBrForSwitchInts(multiBranchInstr, fallthrOffset, startjmpTableIndex, endjmpTableIndex, startBinaryTravIndex, targetOffset);
        FixUpMultiBrJumpTable(multiBranchInstr, targetOffset);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::TryBuildBinaryTreeOrMultiBrForSwitchInts
///     Builds a range of integer cases into either a binary tree or jump table.
///
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::TryBuildBinaryTreeOrMultiBrForSwitchInts(IR::MultiBranchInstr * &multiBranchInstr, uint32 fallthrOffset, int startjmpTableIndex, int endjmpTableIndex, int startBinaryTravIndex, uint32 defaultTargetOffset)
{TRACE_IT(15516);
    int endBinaryTravIndex = startjmpTableIndex;

    //Try Building Binary tree, if there are available case arms, as indicated by the boundary offsets
    if (endBinaryTravIndex != startBinaryTravIndex)
    {TRACE_IT(15517);
        endBinaryTravIndex = startjmpTableIndex - 1;
        BuildBinaryTraverseInstr(startBinaryTravIndex, endBinaryTravIndex, fallthrOffset);
        //Fix up the fallthrOffset for the previous multiBrInstr, if one existed
        //Example => Binary tree immediately succeeds a MultiBr Instr
        if (multiBranchInstr)
        {
            FixUpMultiBrJumpTable(multiBranchInstr, multiBranchInstr->GetNextRealInstr()->GetByteCodeOffset());
            multiBranchInstr = nullptr;
        }
    }

    //Fix up the fallthrOffset for the previous multiBrInstr, if one existed
    //Example -> A multiBr can be followed by another multiBr
    if (multiBranchInstr)
    {
        FixUpMultiBrJumpTable(multiBranchInstr, fallthrOffset);
        multiBranchInstr = nullptr;
    }
    multiBranchInstr = BuildMultiBrCaseInstrForInts(startjmpTableIndex, endjmpTableIndex, defaultTargetOffset);

    //We currently assign the offset of the multiBr Instr same as the offset of the last instruction of the case arm selected for building the jump table
    //AssertMsg(m_lastInstr->GetByteCodeOffset() == fallthrOffset, "The fallthrough offset to the multi branch instruction is wrong");
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::FixUpMultiBrJumpTable
///     Creates Reloc Records for the branch instructions that are generated with the MultiBr Instr
///     Also calls FixMultiBrDefaultTarget to fix the target offset in the MultiBr Instr
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::FixUpMultiBrJumpTable(IR::MultiBranchInstr * multiBranchInstr, uint32 targetOffset)
{TRACE_IT(15518);
    multiBranchInstr->FixMultiBrDefaultTarget(targetOffset);

    uint32 offset = multiBranchInstr->GetByteCodeOffset();

    IR::Instr * subInstr = multiBranchInstr->GetPrevRealInstr();
    IR::Instr * upperBoundCheckInstr = subInstr->GetPrevRealInstr();
    IR::Instr * lowerBoundCheckInstr = upperBoundCheckInstr->GetPrevRealInstr();

    AssertMsg(subInstr->m_opcode == m_subOp, "Missing Offset Calculation instruction");
    AssertMsg(upperBoundCheckInstr->IsBranchInstr() && lowerBoundCheckInstr->IsBranchInstr(), "Invalid boundary check instructions");
    AssertMsg(upperBoundCheckInstr->m_opcode == m_gtOp && lowerBoundCheckInstr->m_opcode == m_ltOp, "Invalid boundary check instructions");

    m_adapter->CreateRelocRecord(upperBoundCheckInstr->AsBranchInstr(), offset, targetOffset, true);
    m_adapter->CreateRelocRecord(lowerBoundCheckInstr->AsBranchInstr(), offset, targetOffset, true);
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::BuildBailOnNotInteger
///     Creates the necessary bailout for a switch case that expected an integer expression
///     but was not.
///
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::BuildBailOnNotInteger()
{TRACE_IT(15519);
    if (!m_switchOptBuildBail)
    {TRACE_IT(15520);
        return;
    }

    m_adapter->ConvertToBailOut(m_profiledSwitchInstr, IR::BailOutExpectingInteger);
    m_switchOptBuildBail = false; // falsify this to avoid generating extra BailOuts when optimization is done again on the same switch statement

#if ENABLE_DEBUG_CONFIG_OPTIONS
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    PHASE_PRINT_TESTTRACE1(Js::SwitchOptPhase, _u("Func %s, Switch %d:Optimized for Integers\n"),
        m_profiledSwitchInstr->m_func->GetDebugNumberSet(debugStringBuffer),
        m_profiledSwitchInstr->AsProfiledInstr()->u.profileId);
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::BuildBailOnNotString
///     Creates the necessary bailout for a switch case that expected a string expression
///     but was not.
///
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::BuildBailOnNotString()
{TRACE_IT(15521);
    if (!m_switchOptBuildBail)
    {TRACE_IT(15522);
        return;
    }

    m_adapter->ConvertToBailOut(m_profiledSwitchInstr, IR::BailOutExpectingString);
    m_switchOptBuildBail = false; // falsify this to avoid generating extra BailOuts when optimization is done again on the same switch statement

#if ENABLE_DEBUG_CONFIG_OPTIONS
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    PHASE_PRINT_TESTTRACE1(Js::SwitchOptPhase, _u("Func %s, Switch %d:Optimized for Strings\n"),
        m_profiledSwitchInstr->m_func->GetDebugNumberSet(debugStringBuffer),
        m_profiledSwitchInstr->AsProfiledInstr()->u.profileId);
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::TestAndAddStringCaseConst
///
///     Checks if strConstSwitchCases already has the string constant
///     - if yes, then return true
///     - if no, then add the string to the list 'strConstSwitchCases' and return false
///
///----------------------------------------------------------------------------

bool
SwitchIRBuilder::TestAndAddStringCaseConst(JITJavascriptString * str)
{TRACE_IT(15523);
    Assert(m_strConstSwitchCases);

    if (m_strConstSwitchCases->Contains(str))
    {TRACE_IT(15524);
        return true;
    }
    else
    {TRACE_IT(15525);
        m_strConstSwitchCases->Add(str);
        return false;
    }
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildMultiBrCaseInstrForStrings
///
///     Build Multi Branch IR instr for a set of Case statements(String case arms).
///     - Builds the multibranch target and adds the instruction
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::BuildMultiBrCaseInstrForStrings(uint32 targetOffset)
{TRACE_IT(15526);
    Assert(m_caseNodes && m_caseNodes->Count() && m_profiledSwitchInstr && !m_isAsmJs);

    if (m_caseNodes->Count() < CONFIG_FLAG(MaxLinearStringCaseCount))
    {TRACE_IT(15527);
        int start = 0;
        int end = m_caseNodes->Count() - 1;
        BuildLinearTraverseInstr(start, end, targetOffset);
        ResetCaseNodes();
        return;
    }

    IR::Opnd * srcOpnd = m_caseNodes->Item(0)->GetCaseInstr()->GetSrc1(); // Src1 is same in all the caseNodes
    IR::MultiBranchInstr * multiBranchInstr = IR::MultiBranchInstr::New(Js::OpCode::MultiBr, srcOpnd, m_func);

    uint32 lastCaseOffset = m_caseNodes->Item(m_caseNodes->Count() - 1)->GetOffset();
    uint caseCount = m_caseNodes->Count();

    bool generateDictionary = true;
    char16 minChar = USHORT_MAX;
    char16 maxChar = 0;

    // Either the jump table is within the limit (<= 128) or it is dense (<= 2 * case Count)
    uint const maxJumpTableSize = max<uint>(CONFIG_FLAG(MaxSingleCharStrJumpTableSize), CONFIG_FLAG(MaxSingleCharStrJumpTableRatio) * caseCount);
    if (this->m_seenOnlySingleCharStrCaseNodes)
    {TRACE_IT(15528);
        generateDictionary = false;
        for (uint i = 0; i < caseCount; i++)
        {TRACE_IT(15529);
            JITJavascriptString * str = m_caseNodes->Item(i)->GetUpperBoundStringConstLocal();
            Assert(str->GetLength() == 1);
            char16 currChar = str->GetString()[0];
            minChar = min(minChar, currChar);
            maxChar = max(maxChar, currChar);
            if ((uint)(maxChar - minChar) > maxJumpTableSize)
            {TRACE_IT(15530);
                generateDictionary = true;
                break;
            }
        }
    }


    if (generateDictionary)
    {TRACE_IT(15531);
        multiBranchInstr->CreateBranchTargetsAndSetDefaultTarget(caseCount, IR::MultiBranchInstr::StrDictionary, targetOffset);

        //Adding normal cases to the instruction (except the default case, which we do it later)
        for (uint i = 0; i < caseCount; i++)
        {TRACE_IT(15532);
            JITJavascriptString * str = m_caseNodes->Item(i)->GetUpperBoundStringConstLocal();
            uint32 caseTargetOffset = m_caseNodes->Item(i)->GetTargetOffset();
            multiBranchInstr->AddtoDictionary(caseTargetOffset, str, m_caseNodes->Item(i)->GetUpperBoundStrConst());
        }
    }
    else
    {TRACE_IT(15533);
        // If we are only going to save 16 entries, just start from 0 so we don't have to subtract
        if (minChar < 16)
        {TRACE_IT(15534);
            minChar = 0;
        }
        multiBranchInstr->m_baseCaseValue = minChar;
        multiBranchInstr->m_lastCaseValue = maxChar;
        uint jumpTableSize = maxChar - minChar + 1;
        multiBranchInstr->CreateBranchTargetsAndSetDefaultTarget(jumpTableSize, IR::MultiBranchInstr::SingleCharStrJumpTable, targetOffset);

        for (uint i = 0; i < jumpTableSize; i++)
        {TRACE_IT(15535);
            // Initialize all the entries to the default target first.
            multiBranchInstr->AddtoJumpTable(targetOffset, i);
        }
        //Adding normal cases to the instruction (except the default case, which we do it later)
        for (uint i = 0; i < caseCount; i++)
        {TRACE_IT(15536);
            JITJavascriptString * str = m_caseNodes->Item(i)->GetUpperBoundStringConstLocal();
            Assert(str->GetLength() == 1);
            uint32 caseTargetOffset = m_caseNodes->Item(i)->GetTargetOffset();
            multiBranchInstr->AddtoJumpTable(caseTargetOffset, str->GetString()[0] - minChar);
        }
    }

    multiBranchInstr->m_isSwitchBr = true;

    m_adapter->CreateRelocRecord(multiBranchInstr, lastCaseOffset, targetOffset);
    m_adapter->AddInstr(multiBranchInstr, lastCaseOffset);
    BuildBailOnNotString();

    ResetCaseNodes();
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildMultiBrCaseInstrForInts
///
///     Build Multi Branch IR instr for a set of Case statements(Integer case arms).
///     - Builds the multibranch target and adds the instruction
///     - Add boundary checks for the jump table and calculates the offset in the jump table
///
///----------------------------------------------------------------------------

IR::MultiBranchInstr *
SwitchIRBuilder::BuildMultiBrCaseInstrForInts(uint32 start, uint32 end, uint32 targetOffset)
{TRACE_IT(15537);
    Assert(m_caseNodes && m_caseNodes->Count() && (m_profiledSwitchInstr || m_isAsmJs));

    IR::Opnd * srcOpnd = m_caseNodes->Item(start)->GetCaseInstr()->GetSrc1(); // Src1 is same in all the caseNodes
    IR::MultiBranchInstr * multiBranchInstr = IR::MultiBranchInstr::New(Js::OpCode::MultiBr, srcOpnd, m_func);

    uint32 lastCaseOffset = m_caseNodes->Item(end)->GetOffset();

    int32 baseCaseValue = m_caseNodes->Item(start)->GetLowerBoundIntConst();
    int32 lastCaseValue = m_caseNodes->Item(end)->GetUpperBoundIntConst();

    multiBranchInstr->m_baseCaseValue = baseCaseValue;
    multiBranchInstr->m_lastCaseValue = lastCaseValue;

    uint32 jmpTableSize = lastCaseValue - baseCaseValue + 1;
    multiBranchInstr->CreateBranchTargetsAndSetDefaultTarget(jmpTableSize, IR::MultiBranchInstr::IntJumpTable, targetOffset);

    int caseIndex = end;
    int lowerBoundCaseConstValue = 0;
    int upperBoundCaseConstValue = 0;
    uint32 caseTargetOffset = 0;

    for (int jmpIndex = jmpTableSize - 1; jmpIndex >= 0; jmpIndex--)
    {TRACE_IT(15538);
        if (caseIndex >= 0 && jmpIndex == m_caseNodes->Item(caseIndex)->GetUpperBoundIntConst() - baseCaseValue)
        {TRACE_IT(15539);
            lowerBoundCaseConstValue = m_caseNodes->Item(caseIndex)->GetLowerBoundIntConst();
            upperBoundCaseConstValue = m_caseNodes->Item(caseIndex)->GetUpperBoundIntConst();
            caseTargetOffset = m_caseNodes->Item(caseIndex--)->GetTargetOffset();
            multiBranchInstr->AddtoJumpTable(caseTargetOffset, jmpIndex);
        }
        else
        {TRACE_IT(15540);
            if (jmpIndex >= lowerBoundCaseConstValue - baseCaseValue && jmpIndex <= upperBoundCaseConstValue - baseCaseValue)
            {TRACE_IT(15541);
                multiBranchInstr->AddtoJumpTable(caseTargetOffset, jmpIndex);
            }
            else
            {TRACE_IT(15542);
                multiBranchInstr->AddtoJumpTable(targetOffset, jmpIndex);
            }
        }
    }

    //Insert Boundary checks for the jump table - Reloc records are created later for these instructions (in FixUpMultiBrJumpTable())
    IR::BranchInstr* lowerBoundChk = IR::BranchInstr::New(m_ltOp, nullptr, srcOpnd, m_caseNodes->Item(start)->GetLowerBound(), m_func);
    lowerBoundChk->m_isSwitchBr = true;
    m_adapter->AddInstr(lowerBoundChk, lastCaseOffset);

    IR::BranchInstr* upperBoundChk = IR::BranchInstr::New(m_gtOp, nullptr, srcOpnd, m_caseNodes->Item(end)->GetUpperBound(), m_func);
    upperBoundChk->m_isSwitchBr = true;
    m_adapter->AddInstr(upperBoundChk, lastCaseOffset);

    //Calculate the offset inside the jump table using the switch operand value and the lowest case arm value (in the considered set of consecutive integers)
    IR::IntConstOpnd *baseCaseValueOpnd = IR::IntConstOpnd::New(multiBranchInstr->m_baseCaseValue, TyInt32, m_func);

    IR::RegOpnd * offset = IR::RegOpnd::New(TyVar, m_func);

    IR::Instr * subInstr = IR::Instr::New(m_subOp, offset, multiBranchInstr->GetSrc1(), baseCaseValueOpnd, m_func);

    //We are sure that the SUB operation will not overflow the int range - It will either bailout or will not optimize if it finds a number that is out of the int range.
    subInstr->ignoreIntOverflow = true;

    m_adapter->AddInstr(subInstr, lastCaseOffset);

    //Source of the multi branch instr will now have the calculated offset
    multiBranchInstr->UnlinkSrc1();
    multiBranchInstr->SetSrc1(offset);
    multiBranchInstr->m_isSwitchBr = true;

    m_adapter->AddInstr(multiBranchInstr, lastCaseOffset);
    m_adapter->CreateRelocRecord(multiBranchInstr, lastCaseOffset, targetOffset);

    return multiBranchInstr;
}
