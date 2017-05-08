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
    {TRACE_IT(9598);
        Assert(firstInstr);
        Assert(lastInstr);
        Assert(lastInstr->m_opcode == Js::OpCode::NoIntOverflowBoundary);
    }

    static IntOverflowDoesNotMatterRange *New(
        JitArenaAllocator *const allocator,
        IR::Instr *const firstInstr,
        IR::Instr *const lastInstr,
        IntOverflowDoesNotMatterRange *const next)
    {TRACE_IT(9599);
        return JitAnew(allocator, IntOverflowDoesNotMatterRange, allocator, firstInstr, lastInstr, next);
    }

    void Delete(JitArenaAllocator *const allocator)
    {
        JitAdelete(allocator, this);
    }

public:
    IR::Instr *FirstInstr() const
    {TRACE_IT(9600);
        return firstInstr;
    }

    void SetFirstInstr(IR::Instr *const firstInstr)
    {TRACE_IT(9601);
        Assert(firstInstr);
        this->firstInstr = firstInstr;
    }

    IR::Instr *LastInstr() const
    {TRACE_IT(9602);
        return lastInstr;
    }

    BVSparse<JitArenaAllocator> *SymsRequiredToBeInt()
    {TRACE_IT(9603);
        return &symsRequiredToBeInt;
    }

    BVSparse<JitArenaAllocator> *SymsRequiredToBeLossyInt()
    {TRACE_IT(9604);
        return &symsRequiredToBeLossyInt;
    }

    IntOverflowDoesNotMatterRange *Next() const
    {TRACE_IT(9605);
        return next;
    }

    PREVENT_COPY(IntOverflowDoesNotMatterRange)
};
