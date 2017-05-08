//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

IR::Instr *
SimpleLayout::MoveHelperBlock(IR::Instr * lastOpHelperLabel, uint32 lastOpHelperStatementIndex, Func* lastOpHelperFunc, IR::LabelInstr * nextLabel,
                              IR::Instr * instrAfter)
{TRACE_IT(15435);
    // Add pragma instructions around the moved code to track source mapping

    Func* pragmatInstrFunc = lastOpHelperFunc ? lastOpHelperFunc : this->func;
    if (instrAfter->IsPragmaInstr() && instrAfter->m_opcode == Js::OpCode::StatementBoundary)
    {TRACE_IT(15436);
        IR::PragmaInstr* pragmaInstr = instrAfter->AsPragmaInstr();
        pragmaInstr->m_statementIndex = lastOpHelperStatementIndex;
        pragmaInstr->m_func = pragmatInstrFunc;
    }
    else
    {TRACE_IT(15437);
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
    {TRACE_IT(15438);
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
{TRACE_IT(15439);
    if (PHASE_OFF(Js::LayoutPhase, this->func) || CONFIG_ISENABLED(Js::DebugFlag))
    {TRACE_IT(15440);
        return;
    }

    // Do simple layout of helper block.  Push them to after FunctionExit.

    IR::Instr * lastInstr = func->m_tailInstr;
    IR::LabelInstr * lastOpHelperLabel = NULL;
    uint32 lastOpHelperStatementIndex = Js::Constants::NoStatementIndex;
    Func* lastOpHelperFunc = nullptr;
    FOREACH_INSTR_EDITING_IN_RANGE(instr, instrNext, func->m_headInstr, func->m_tailInstr->m_prev)
    {TRACE_IT(15441);
        if (instr->IsPragmaInstr() && instr->m_opcode == Js::OpCode::StatementBoundary)
        {TRACE_IT(15442);
            currentStatement = instr->AsPragmaInstr();
        }
        else if (instr->IsLabelInstr())
        {TRACE_IT(15443);
            IR::LabelInstr * labelInstr = instr->AsLabelInstr();
            if (labelInstr->isOpHelper)
            {TRACE_IT(15444);
                if (lastOpHelperLabel == NULL)
                {TRACE_IT(15445);
                    lastOpHelperLabel = labelInstr;
                    lastOpHelperStatementIndex = currentStatement ? currentStatement->m_statementIndex : Js::Constants::NoStatementIndex;
                    lastOpHelperFunc = currentStatement ? currentStatement->m_func : nullptr;
                }
            }
            else if (lastOpHelperLabel != NULL)
            {TRACE_IT(15446);
                IR::Instr * prevInstr = lastOpHelperLabel->GetPrevRealInstrOrLabel();
                if (prevInstr->IsBranchInstr())
                {TRACE_IT(15447);
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
                    {TRACE_IT(15448);
                        lastInstr = this->MoveHelperBlock(lastOpHelperLabel, lastOpHelperStatementIndex, lastOpHelperFunc, labelInstr, lastInstr);


                        if (prevBranchInstr->IsUnconditional())
                        {TRACE_IT(15449);
                            // Remove the branch to next after the helper block is moved.
                            prevBranchInstr->Remove();
                        }
                        else
                        {TRACE_IT(15450);
                            // Reverse the condition
                            LowererMD::InvertBranch(prevBranchInstr);
                            prevBranchInstr->SetTarget(lastOpHelperLabel);
                        }
                    }
                    else if (prevBranchInstr->IsUnconditional())
                    {TRACE_IT(15451);
                        IR::Instr * prevPrevInstr = prevInstr->GetPrevRealInstrOrLabel();
                        if (prevPrevInstr->IsBranchInstr()
                            && prevPrevInstr->AsBranchInstr()->IsConditional()
                            && prevPrevInstr->AsBranchInstr()->GetTarget() == labelInstr)
                        {TRACE_IT(15452);

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
                        {TRACE_IT(15453);
                            IR::Instr *lastOpHelperInstr = labelInstr->GetPrevRealInstr();
                            if (lastOpHelperInstr->IsBranchInstr())
                            {TRACE_IT(15454);
                                IR::BranchInstr *lastOpHelperBranchInstr = lastOpHelperInstr->AsBranchInstr();

                                //      jmp $target         <== prevInstr           //this is unconditional jump
                                // $helper:                 <== lastOpHelperLabel
                                //      ...
                                //      jmp $labeln         <== lastOpHelperInstr   //Conditional/Unconditional jump
                                // $label:                  <== labelInstr

                                lastInstr = this->MoveHelperBlock(lastOpHelperLabel, lastOpHelperStatementIndex, lastOpHelperFunc, labelInstr, lastInstr);
                                //Compensation code if its not unconditional jump
                                if (!lastOpHelperBranchInstr->IsUnconditional())
                                {TRACE_IT(15455);
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
