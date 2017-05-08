//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

const ValueNumber InvalidValueNumber = 0;
const ValueNumber ZeroValueNumber = 1;
const ValueNumber FirstNewValueNumber = ZeroValueNumber + 1;

ValueRelativeOffset::ValueRelativeOffset()
#if DBG
    : baseValue(nullptr)
#endif
{
    Assert(!IsValid());
}

ValueRelativeOffset::ValueRelativeOffset(const Value *const baseValue, const bool wasEstablishedExplicitly)
    : baseValue(baseValue), offset(0), wasEstablishedExplicitly(wasEstablishedExplicitly)
{TRACE_IT(16023);
    Assert(baseValue);
    Assert(IsValid());
}

ValueRelativeOffset::ValueRelativeOffset(const Value *const baseValue, const int offset, const bool wasEstablishedExplicitly)
    : baseValue(baseValue), offset(offset), wasEstablishedExplicitly(wasEstablishedExplicitly)
{TRACE_IT(16024);
    Assert(baseValue);
    Assert(IsValid());
}

#if DBG

bool ValueRelativeOffset::IsValid() const
{TRACE_IT(16025);
    return !!baseValue;
}

#endif

ValueNumber ValueRelativeOffset::BaseValueNumber() const
{TRACE_IT(16026);
    Assert(IsValid());
    return baseValue->GetValueNumber();
}

StackSym *ValueRelativeOffset::BaseSym() const
{TRACE_IT(16027);
    Assert(IsValid());

    Sym *const baseSym = baseValue->GetValueInfo()->GetSymStore();
    return baseSym && baseSym->IsStackSym() ? baseSym->AsStackSym() : nullptr;
}

int ValueRelativeOffset::Offset() const
{TRACE_IT(16028);
    Assert(IsValid());
    return offset;
}

void ValueRelativeOffset::SetOffset(const int offset)
{TRACE_IT(16029);
    Assert(IsValid());
    this->offset = offset;
}

bool ValueRelativeOffset::WasEstablishedExplicitly() const
{TRACE_IT(16030);
    Assert(IsValid());
    return wasEstablishedExplicitly;
}

void ValueRelativeOffset::SetWasEstablishedExplicitly()
{TRACE_IT(16031);
    Assert(IsValid());
    wasEstablishedExplicitly = true;
}

bool ValueRelativeOffset::Add(const int n)
{TRACE_IT(16032);
    Assert(IsValid());
    return !Int32Math::Add(offset, n, &offset);
}

template<bool Lower, bool Aggressive>
void ValueRelativeOffset::Merge(const ValueRelativeOffset &other)
{TRACE_IT(16033);
    Assert(IsValid());
    Assert(other.IsValid());
    Assert(BaseValueNumber() == other.BaseValueNumber());

    if(!BaseSym() && other.BaseSym())
        baseValue = other.baseValue;
    MergeConstantValue<Lower, Aggressive>(other.offset);
    if(other.wasEstablishedExplicitly == Aggressive)
        wasEstablishedExplicitly = Aggressive;
}
template void ValueRelativeOffset::Merge<false, false>(const ValueRelativeOffset &other);
template void ValueRelativeOffset::Merge<false, true>(const ValueRelativeOffset &other);
template void ValueRelativeOffset::Merge<true, false>(const ValueRelativeOffset &other);
template void ValueRelativeOffset::Merge<true, true>(const ValueRelativeOffset &other);

template<bool Lower, bool Aggressive>
void ValueRelativeOffset::MergeConstantValue(const int constantValue)
{TRACE_IT(16034);
    Assert(IsValid());

    // Merge down for a conservative lower bound or aggressive upper bound merge, or merge up for a conservative upper bound or
    // aggressive lower bound merge
    if(Lower ^ Aggressive ? constantValue < offset : constantValue > offset)
        offset = constantValue;
}
template void ValueRelativeOffset::MergeConstantValue<false, false>(const int constantValue);
template void ValueRelativeOffset::MergeConstantValue<false, true>(const int constantValue);
template void ValueRelativeOffset::MergeConstantValue<true, false>(const int constantValue);
template void ValueRelativeOffset::MergeConstantValue<true, true>(const int constantValue);
