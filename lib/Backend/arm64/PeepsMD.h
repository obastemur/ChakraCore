//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Peeps;

class PeepsMD
{
private:
    Func *      func;
    Peeps *     peeps;
public:
    PeepsMD(Func *func) : func(func) {TRACE_IT(18043);}

    void        Init(Peeps *peeps) {TRACE_IT(18044); __debugbreak(); }
    void        ProcessImplicitRegs(IR::Instr *instr) {TRACE_IT(18045); __debugbreak(); }
    void        PeepAssign(IR::Instr *instr) {TRACE_IT(18046); __debugbreak(); }
};


