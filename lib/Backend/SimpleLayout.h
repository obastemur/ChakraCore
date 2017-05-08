//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class SimpleLayout
{
public:
    SimpleLayout(Func * func) : func(func), currentStatement(NULL) {TRACE_IT(15456);}
    void Layout();

private:
    IR::Instr * MoveHelperBlock(IR::Instr * lastOpHelperLabel, uint32 lastOpHelperStatementIndex, Func* lastOpHelperFunc, IR::LabelInstr * nextLabel,
                              IR::Instr * instrAfter);

private:
    Func * func;
    IR::PragmaInstr * currentStatement;
};
