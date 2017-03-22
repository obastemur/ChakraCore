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
    PeepsMD(Func *func) : func(func) {LOGMEIN("PeepsMD.h] 14\n");}

    void        Init(Peeps *peeps) {LOGMEIN("PeepsMD.h] 16\n"); __debugbreak(); }
    void        ProcessImplicitRegs(IR::Instr *instr) {LOGMEIN("PeepsMD.h] 17\n"); __debugbreak(); }
    void        PeepAssign(IR::Instr *instr) {LOGMEIN("PeepsMD.h] 18\n"); __debugbreak(); }
};


