//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

IR::Instr *
SimpleLayout::MoveHelperBlock(IR::Instr * lastOpHelperLabel, uint32 lastOpHelperStatementIndex, Func* lastOpHelperFunc, IR::LabelInstr * nextLabel,
                              IR::Instr * instrAfter)
{LOGMEIN("SimpleLayout.cpp] 9\n");
    // Add pragma instructions around the moved code to track source mapping

    Func* pragmatInstrFunc = lastOpHelperFunc ? lastOpHelperFunc : this->func;
    if (instrAfter->IsPragmaInstr() && instrAfter->m_opcode == Js::OpCode::StatementBoundary)
    {LOGMEIN("SimpleLayout.cpp] 14\n");
        IR::PragmaInstr* pragmaInstr = instrAfter->AsPragmaInstr();
        pragmaInstr->m_statementIndex = lastOpHelperStatementIndex;
        pragmaInstr->m_func = pragmatInstrFunc;
    }
    else
    {
        IR::PragmaInstr* pragmaInstr = IR::PragmaInstr::New(Js::OpCode::StatementBoundary, lastOpHelperStatementIndex, pragmatInstrFunc);
        instrAfter->InsertAfter(pragmaInstr);
        instrAfter = pragmaInstr;
    }

    // Move all the instruction between lastOpHelperLabel to lastOpHelperInstr
    // to the end of the function

    IR::Instr * lastOpHelperInstr = nextLabel->GetPrevRealInstrOrLabel();
    IR::Instr::MoveRangeAfter(lastOpHelperLabel, lastOpHelperInstr, instrAfter);
    instrAfter = lastOpHelperInstr;

    // Add the jmp back if the lastOpHelperInstr has fall through

    if (instrAfter->HasFallThrough())
    {LOGMEIN("SimpleLayout.cpp] 36\n");
        IR::BranchInstr * branchInstr = IR::BranchInstr::New(
            LowererMD::MDUncondBranchOpcode, nextLabel, this->func);
        instrAfter->InsertAfter(branchInstr);
        instrAfter = branchInstr;
    }

    // Add pragma terminating this source mapping range
    IR::PragmaInstr* pragmaInstr = IR::PragmaInstr::New(Js::OpCode::StatementBoundary, Js::Constants::NoStatementIndex, this->func);
    instrAfter->InsertAfter(pragmaInstr);
    instrAfter = pragmaInstr;

    return instrAfter;
}

void
SimpleLayout::Layout()
{LOGMEIN("SimpleLayout.cpp] 53\n");
    if (PHASE_OFF(Js::LayoutPhase, this->func) || CONFIG_ISENABLED(Js::DebugFlag))
    {LOGMEIN("SimpleLayout.cpp] 55\n");
        return;
    }

    // Do simple layout of helper block.  Push them to after FunctionExit.

    IR::Instr * lastInstr = func->m_tailInstr;
    IR::LabelInstr * lastOpHelperLabel = NULL;
    uint32 lastOpHelperStatementIndex = Js::Constants::NoStatementIndex;
    Func* lastOpHelperFunc = nullptr;
    FOREACH_INSTR_EDITING_IN_RANGE(instr, instrNext, func->m_headInstr, func->m_tailInstr->m_prev)
    {LOGMEIN("SimpleLayout.cpp] 66\n");
        if (instr->IsPragmaInstr() && instr->m_opcode == Js::OpCode::StatementBoundary)
        {LOGMEIN("SimpleLayout.cpp] 68\n");
            currentStatement = instr->AsPragmaInstr();
        }
        else if (instr->IsLabelInstr())
        {LOGMEIN("SimpleLayout.cpp] 72\n");
            IR::LabelInstr * labelInstr = instr->AsLabelInstr();
            if (labelInstr->isOpHelper)
            {LOGMEIN("SimpleLayout.cpp] 75\n");
                if (lastOpHelperLabel == NULL)
                {LOGMEIN("SimpleLayout.cpp] 77\n");
                    lastOpHelperLabel = labelInstr;
                    lastOpHelperStatementIndex = currentStatement ? currentStatement->m_statementIndex : Js::Constants::NoStatementIndex;
                    lastOpHelperFunc = currentStatement ? currentStatement->m_func : nullptr;
                }
            }
            else if (lastOpHelperLabel != NULL)
            {LOGMEIN("SimpleLayout.cpp] 84\n");
                IR::Instr * prevInstr = lastOpHelperLabel->GetPrevRealInstrOrLabel();
                if (prevInstr->IsBranchInstr())
                {LOGMEIN("SimpleLayout.cpp] 87\n");
                    // If the previous instruction is to jump around this helper block
                    // Then we move the helper block to the end of the function to
                    // avoid the jmp in the fast path.

                    //      jxx $label          <== prevInstr
                    // $helper:                 <== lastOpHelperLabel
                    //      ...
                    //      ...                 <== lastOpHelperInstr
                    // $label:                  <== labelInstr

                    IR::BranchInstr * prevBranchInstr = prevInstr->AsBranchInstr();
                    if (prevBranchInstr->GetTarget() == labelInstr)
                    {LOGMEIN("SimpleLayout.cpp] 100\n");
                        lastInstr = this->MoveHelperBlock(lastOpHelperLabel, lastOpHelperStatementIndex, lastOpHelperFunc, labelInstr, lastInstr);


                        if (prevBranchInstr->IsUnconditional())
                        {LOGMEIN("SimpleLayout.cpp] 105\n");
                            // Remove the branch to next after the helper block is moved.
                            prevBranchInstr->Remove();
                        }
                        else
                        {
                            // Reverse the condition
                            LowererMD::InvertBranch(prevBranchInstr);
                            prevBranchInstr->SetTarget(lastOpHelperLabel);
                        }
                    }
                    else if (prevBranchInstr->IsUnconditional())
                    {LOGMEIN("SimpleLayout.cpp] 117\n");
                        IR::Instr * prevPrevInstr = prevInstr->GetPrevRealInstrOrLabel();
                        if (prevPrevInstr->IsBranchInstr()
                            && prevPrevInstr->AsBranchInstr()->IsConditional()
                            && prevPrevInstr->AsBranchInstr()->GetTarget() == labelInstr)
                        {LOGMEIN("SimpleLayout.cpp] 122\n");

                            //      jcc $label          <== prevPrevInstr
                            //      jmp $blah           <== prevInstr
                            // $helper:                 <== lastOpHelperLabel
                            //      ...
                            //      ...                 <== lastOpHelperInstr
                            // $label:                  <== labelInstr

                            // Transform to

                            //      jncc $blah          <== prevPrevInstr
                            // $label:                  <== labelInstr

                            // $helper:                 <== lastOpHelperLabel
                            //      ...
                            //      ...                 <== lastOpHelperInstr
                            //      jmp $label:         <== labelInstr

                            lastInstr = this->MoveHelperBlock(lastOpHelperLabel, lastOpHelperStatementIndex, lastOpHelperFunc, labelInstr, lastInstr);

                            LowererMD::InvertBranch(prevPrevInstr->AsBranchInstr());
                            prevPrevInstr->AsBranchInstr()->SetTarget(prevBranchInstr->GetTarget());
                            prevBranchInstr->Remove();
                        }
                        else
                        {
                            IR::Instr *lastOpHelperInstr = labelInstr->GetPrevRealInstr();
                            if (lastOpHelperInstr->IsBranchInstr())
                            {LOGMEIN("SimpleLayout.cpp] 151\n");
                                IR::BranchInstr *lastOpHelperBranchInstr = lastOpHelperInstr->AsBranchInstr();

                                //      jmp $target         <== prevInstr           //this is unconditional jump
                                // $helper:                 <== lastOpHelperLabel
                                //      ...
                                //      jmp $labeln         <== lastOpHelperInstr   //Conditional/Unconditional jump
                                // $label:                  <== labelInstr

                                lastInstr = this->MoveHelperBlock(lastOpHelperLabel, lastOpHelperStatementIndex, lastOpHelperFunc, labelInstr, lastInstr);
                                //Compensation code if its not unconditional jump
                                if (!lastOpHelperBranchInstr->IsUnconditional())
                                {LOGMEIN("SimpleLayout.cpp] 163\n");
                                    IR::BranchInstr *branchInstr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelInstr, this->func);
                                    lastOpHelperBranchInstr->InsertAfter(branchInstr);
                                }
                            }

                        }
                    }
                }
                lastOpHelperLabel = NULL;
            }
        }
    }
    NEXT_INSTR_EDITING_IN_RANGE;

    func->m_tailInstr = lastInstr;
}
