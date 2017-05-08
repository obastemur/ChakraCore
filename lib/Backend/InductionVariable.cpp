//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

const int InductionVariable::ChangeMagnitudeLimitForLoopCountBasedHoisting = 64 << 10;

InductionVariable::InductionVariable()
#if DBG
    : sym(nullptr)
#endif
{
    Assert(!IsValid());
}

InductionVariable::InductionVariable(StackSym *const sym, const ValueNumber symValueNumber, const int change)
    : sym(sym), symValueNumber(symValueNumber), changeBounds(change, change), isChangeDeterminate(true)
{TRACE_IT(8758);
    Assert(sym);
    Assert(!sym->IsTypeSpec());
    Assert(IsValid());
}

#if DBG

bool InductionVariable::IsValid() const
{TRACE_IT(8759);
    return !!sym;
}

#endif

StackSym *InductionVariable::Sym() const
{TRACE_IT(8760);
    Assert(IsValid());
    return sym;
}

ValueNumber InductionVariable::SymValueNumber() const
{TRACE_IT(8761);
    Assert(IsChangeDeterminate());
    return symValueNumber;
}

void InductionVariable::SetSymValueNumber(const ValueNumber symValueNumber)
{TRACE_IT(8762);
    Assert(IsChangeDeterminate());
    this->symValueNumber = symValueNumber;
}

bool InductionVariable::IsChangeDeterminate() const
{TRACE_IT(8763);
    Assert(IsValid());
    return isChangeDeterminate;
}

void InductionVariable::SetChangeIsIndeterminate()
{TRACE_IT(8764);
    Assert(IsValid());
    isChangeDeterminate = false;
}

const IntConstantBounds &InductionVariable::ChangeBounds() const
{TRACE_IT(8765);
    Assert(IsChangeDeterminate());
    return changeBounds;
}

bool InductionVariable::IsChangeUnidirectional() const
{TRACE_IT(8766);
    return
        (ChangeBounds().LowerBound() >= 0 && ChangeBounds().UpperBound() != 0) ||
        (ChangeBounds().UpperBound() <= 0 && ChangeBounds().LowerBound() != 0);
}

bool InductionVariable::Add(const int n)
{TRACE_IT(8767);
    Assert(IsChangeDeterminate());

    if(n == 0)
        return true;

    int newLowerBound;
    if(changeBounds.LowerBound() == IntConstMin)
    {TRACE_IT(8768);
        if(n >= 0)
        {TRACE_IT(8769);
            isChangeDeterminate = false;
            return false;
        }
        newLowerBound = IntConstMin;
    }
    else if(changeBounds.LowerBound() == IntConstMax)
    {TRACE_IT(8770);
        if(n < 0)
        {TRACE_IT(8771);
            isChangeDeterminate = false;
            return false;
        }
        newLowerBound = IntConstMax;
    }
    else if(Int32Math::Add(changeBounds.LowerBound(), n, &newLowerBound))
        newLowerBound = n < 0 ? IntConstMin : IntConstMax;

    int newUpperBound;
    if(changeBounds.UpperBound() == IntConstMin)
    {TRACE_IT(8772);
        if(n >= 0)
        {TRACE_IT(8773);
            isChangeDeterminate = false;
            return false;
        }
        newUpperBound = IntConstMin;
    }
    else if(changeBounds.UpperBound() == IntConstMax)
    {TRACE_IT(8774);
        if(n < 0)
        {TRACE_IT(8775);
            isChangeDeterminate = false;
            return false;
        }
        newUpperBound = IntConstMax;
    }
    else if(Int32Math::Add(changeBounds.UpperBound(), n, &newUpperBound))
        newUpperBound = n < 0 ? IntConstMin : IntConstMax;

    changeBounds = IntConstantBounds(newLowerBound, newUpperBound);
    return true;
}

void InductionVariable::ExpandInnerLoopChange()
{TRACE_IT(8776);
    Assert(IsValid());

    if(!isChangeDeterminate)
        return;

    changeBounds =
        IntConstantBounds(
            changeBounds.LowerBound() < 0 ? IntConstMin : changeBounds.LowerBound(),
            changeBounds.UpperBound() > 0 ? IntConstMax : changeBounds.UpperBound());
}

void InductionVariable::Merge(const InductionVariable &other)
{TRACE_IT(8777);
    Assert(Sym() == other.Sym());
    // The value number may be different, the caller will give the merged info the appropriate value number

    isChangeDeterminate &= other.isChangeDeterminate;
    if(!isChangeDeterminate)
        return;

    changeBounds =
        IntConstantBounds(
            min(changeBounds.LowerBound(), other.changeBounds.LowerBound()),
            max(changeBounds.UpperBound(), other.changeBounds.UpperBound()));
}
