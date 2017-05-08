//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
struct FreeObject
{
public:
    FreeObject * GetNext() const
    {TRACE_IT(23237);
        AssertMsg((taggedNext & TaggedBit) == TaggedBit, "Free list corrupted");
        return (FreeObject *)(taggedNext & ~TaggedBit);
    }

    void SetNext(FreeObject * next)
    {TRACE_IT(23238);
        Assert(((INT_PTR)next & TaggedBit) == 0);
        taggedNext = ((INT_PTR)next) | TaggedBit;
    }
    void ZeroNext() {TRACE_IT(23239); taggedNext = 0; }
#ifdef RECYCLER_MEMORY_VERIFY
#pragma warning(suppress:4310)
    void DebugFillNext() {TRACE_IT(23240); taggedNext = (INT_PTR)0xCACACACACACACACA; }
#endif
private:
    INT_PTR taggedNext;
    static INT_PTR const TaggedBit = 0x1;
};
}
