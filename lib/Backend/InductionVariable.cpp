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
{LOGMEIN("InductionVariable.cpp] 18\n");
    Assert(sym);
    Assert(!sym->IsTypeSpec());
    Assert(IsValid());
}

#if DBG

bool InductionVariable::IsValid() const
{LOGMEIN("InductionVariable.cpp] 27\n");
    return !!sym;
}

#endif

StackSym *InductionVariable::Sym() const
{LOGMEIN("InductionVariable.cpp] 34\n");
    Assert(IsValid());
    return sym;
}

ValueNumber InductionVariable::SymValueNumber() const
{LOGMEIN("InductionVariable.cpp] 40\n");
    Assert(IsChangeDeterminate());
    return symValueNumber;
}

void InductionVariable::SetSymValueNumber(const ValueNumber symValueNumber)
{LOGMEIN("InductionVariable.cpp] 46\n");
    Assert(IsChangeDeterminate());
    this->symValueNumber = symValueNumber;
}

bool InductionVariable::IsChangeDeterminate() const
{LOGMEIN("InductionVariable.cpp] 52\n");
    Assert(IsValid());
    return isChangeDeterminate;
}

void InductionVariable::SetChangeIsIndeterminate()
{LOGMEIN("InductionVariable.cpp] 58\n");
    Assert(IsValid());
    isChangeDeterminate = false;
}

const IntConstantBounds &InductionVariable::ChangeBounds() const
{LOGMEIN("InductionVariable.cpp] 64\n");
    Assert(IsChangeDeterminate());
    return changeBounds;
}

bool InductionVariable::IsChangeUnidirectional() const
{LOGMEIN("InductionVariable.cpp] 70\n");
    return
        (ChangeBounds().LowerBound() >= 0 && ChangeBounds().UpperBound() != 0) ||
        (ChangeBounds().UpperBound() <= 0 && ChangeBounds().LowerBound() != 0);
}

bool InductionVariable::Add(const int n)
{LOGMEIN("InductionVariable.cpp] 77\n");
    Assert(IsChangeDeterminate());

    if(n == 0)
        return true;

    int newLowerBound;
    if(changeBounds.LowerBound() == IntConstMin)
    {LOGMEIN("InductionVariable.cpp] 85\n");
        if(n >= 0)
        {LOGMEIN("InductionVariable.cpp] 87\n");
            isChangeDeterminate = false;
            return false;
        }
        newLowerBound = IntConstMin;
    }
    else if(changeBounds.LowerBound() == IntConstMax)
    {LOGMEIN("InductionVariable.cpp] 94\n");
        if(n < 0)
        {LOGMEIN("InductionVariable.cpp] 96\n");
            isChangeDeterminate = false;
            return false;
        }
        newLowerBound = IntConstMax;
    }
    else if(Int32Math::Add(changeBounds.LowerBound(), n, &newLowerBound))
        newLowerBound = n < 0 ? IntConstMin : IntConstMax;

    int newUpperBound;
    if(changeBounds.UpperBound() == IntConstMin)
    {LOGMEIN("InductionVariable.cpp] 107\n");
        if(n >= 0)
        {LOGMEIN("InductionVariable.cpp] 109\n");
            isChangeDeterminate = false;
            return false;
        }
        newUpperBound = IntConstMin;
    }
    else if(changeBounds.UpperBound() == IntConstMax)
    {LOGMEIN("InductionVariable.cpp] 116\n");
        if(n < 0)
        {LOGMEIN("InductionVariable.cpp] 118\n");
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
{LOGMEIN("InductionVariable.cpp] 132\n");
    Assert(IsValid());

    if(!isChangeDeterminate)
        return;

    changeBounds =
        IntConstantBounds(
            changeBounds.LowerBound() < 0 ? IntConstMin : changeBounds.LowerBound(),
            changeBounds.UpperBound() > 0 ? IntConstMax : changeBounds.UpperBound());
}

void InductionVariable::Merge(const InductionVariable &other)
{LOGMEIN("InductionVariable.cpp] 145\n");
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
