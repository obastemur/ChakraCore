//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class IntOverflowDoesNotMatterRange
{
private:
    IR::Instr *firstInstr;
    IR::Instr *const lastInstr;

    // These syms are required to have int values before the first instruction (before we start ignoring int overflows in this
    // on instructions in this range). Any bailouts necessary to force them to ints need to be inserted before the range.
    BVSparse<JitArenaAllocator> symsRequiredToBeInt;
    BVSparse<JitArenaAllocator> symsRequiredToBeLossyInt; // these are a subset of the above bit-vector

    IntOverflowDoesNotMatterRange *const next;

public:
    IntOverflowDoesNotMatterRange(
        JitArenaAllocator *const allocator,
        IR::Instr *const firstInstr,
        IR::Instr *const lastInstr,
        IntOverflowDoesNotMatterRange *const next)
        : firstInstr(firstInstr),
        lastInstr(lastInstr),
        symsRequiredToBeInt(allocator),
        symsRequiredToBeLossyInt(allocator),
        next(next)
    {LOGMEIN("IntOverflowDoesNotMatterRange.h] 30\n");
        Assert(firstInstr);
        Assert(lastInstr);
        Assert(lastInstr->m_opcode == Js::OpCode::NoIntOverflowBoundary);
    }

    static IntOverflowDoesNotMatterRange *New(
        JitArenaAllocator *const allocator,
        IR::Instr *const firstInstr,
        IR::Instr *const lastInstr,
        IntOverflowDoesNotMatterRange *const next)
    {LOGMEIN("IntOverflowDoesNotMatterRange.h] 41\n");
        return JitAnew(allocator, IntOverflowDoesNotMatterRange, allocator, firstInstr, lastInstr, next);
    }

    void Delete(JitArenaAllocator *const allocator)
    {
        JitAdelete(allocator, this);
    }

public:
    IR::Instr *FirstInstr() const
    {LOGMEIN("IntOverflowDoesNotMatterRange.h] 52\n");
        return firstInstr;
    }

    void SetFirstInstr(IR::Instr *const firstInstr)
    {LOGMEIN("IntOverflowDoesNotMatterRange.h] 57\n");
        Assert(firstInstr);
        this->firstInstr = firstInstr;
    }

    IR::Instr *LastInstr() const
    {LOGMEIN("IntOverflowDoesNotMatterRange.h] 63\n");
        return lastInstr;
    }

    BVSparse<JitArenaAllocator> *SymsRequiredToBeInt()
    {LOGMEIN("IntOverflowDoesNotMatterRange.h] 68\n");
        return &symsRequiredToBeInt;
    }

    BVSparse<JitArenaAllocator> *SymsRequiredToBeLossyInt()
    {LOGMEIN("IntOverflowDoesNotMatterRange.h] 73\n");
        return &symsRequiredToBeLossyInt;
    }

    IntOverflowDoesNotMatterRange *Next() const
    {LOGMEIN("IntOverflowDoesNotMatterRange.h] 78\n");
        return next;
    }

    PREVENT_COPY(IntOverflowDoesNotMatterRange)
};
