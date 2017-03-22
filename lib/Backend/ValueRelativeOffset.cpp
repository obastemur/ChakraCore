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
{LOGMEIN("ValueRelativeOffset.cpp] 20\n");
    Assert(baseValue);
    Assert(IsValid());
}

ValueRelativeOffset::ValueRelativeOffset(const Value *const baseValue, const int offset, const bool wasEstablishedExplicitly)
    : baseValue(baseValue), offset(offset), wasEstablishedExplicitly(wasEstablishedExplicitly)
{LOGMEIN("ValueRelativeOffset.cpp] 27\n");
    Assert(baseValue);
    Assert(IsValid());
}

#if DBG

bool ValueRelativeOffset::IsValid() const
{LOGMEIN("ValueRelativeOffset.cpp] 35\n");
    return !!baseValue;
}

#endif

ValueNumber ValueRelativeOffset::BaseValueNumber() const
{LOGMEIN("ValueRelativeOffset.cpp] 42\n");
    Assert(IsValid());
    return baseValue->GetValueNumber();
}

StackSym *ValueRelativeOffset::BaseSym() const
{LOGMEIN("ValueRelativeOffset.cpp] 48\n");
    Assert(IsValid());

    Sym *const baseSym = baseValue->GetValueInfo()->GetSymStore();
    return baseSym && baseSym->IsStackSym() ? baseSym->AsStackSym() : nullptr;
}

int ValueRelativeOffset::Offset() const
{LOGMEIN("ValueRelativeOffset.cpp] 56\n");
    Assert(IsValid());
    return offset;
}

void ValueRelativeOffset::SetOffset(const int offset)
{LOGMEIN("ValueRelativeOffset.cpp] 62\n");
    Assert(IsValid());
    this->offset = offset;
}

bool ValueRelativeOffset::WasEstablishedExplicitly() const
{LOGMEIN("ValueRelativeOffset.cpp] 68\n");
    Assert(IsValid());
    return wasEstablishedExplicitly;
}

void ValueRelativeOffset::SetWasEstablishedExplicitly()
{LOGMEIN("ValueRelativeOffset.cpp] 74\n");
    Assert(IsValid());
    wasEstablishedExplicitly = true;
}

bool ValueRelativeOffset::Add(const int n)
{LOGMEIN("ValueRelativeOffset.cpp] 80\n");
    Assert(IsValid());
    return !Int32Math::Add(offset, n, &offset);
}

template<bool Lower, bool Aggressive>
void ValueRelativeOffset::Merge(const ValueRelativeOffset &other)
{LOGMEIN("ValueRelativeOffset.cpp] 87\n");
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
{LOGMEIN("ValueRelativeOffset.cpp] 105\n");
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
