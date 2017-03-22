//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

#pragma region IntBounds

IntBounds::IntBounds(
    const IntConstantBounds &constantBounds,
    const bool wasConstantUpperBoundEstablishedExplicitly,
    JitArenaAllocator *const allocator)
    :
    constantLowerBound(constantBounds.LowerBound()),
    constantUpperBound(constantBounds.UpperBound()),
    wasConstantUpperBoundEstablishedExplicitly(wasConstantUpperBoundEstablishedExplicitly),
    relativeLowerBounds(allocator),
    relativeUpperBounds(allocator)
{LOGMEIN("IntBounds.cpp] 18\n");
}

IntBounds *IntBounds::New(
    const IntConstantBounds &constantBounds,
    const bool wasConstantUpperBoundEstablishedExplicitly,
    JitArenaAllocator *const allocator)
{LOGMEIN("IntBounds.cpp] 25\n");
    Assert(allocator);
    return JitAnew(allocator, IntBounds, constantBounds, wasConstantUpperBoundEstablishedExplicitly, allocator);
}

IntBounds *IntBounds::Clone() const
{LOGMEIN("IntBounds.cpp] 31\n");
    JitArenaAllocator *const allocator = relativeLowerBounds.GetAllocator();
    return JitAnew(allocator, IntBounds, *this);
}

void IntBounds::Delete() const
{LOGMEIN("IntBounds.cpp] 37\n");
    JitArenaAllocator *const allocator = relativeLowerBounds.GetAllocator();
    JitAdelete(allocator, const_cast<IntBounds *>(this));
}

void IntBounds::Verify() const
{LOGMEIN("IntBounds.cpp] 43\n");
    Assert(this);
    Assert(constantLowerBound <= constantUpperBound);
    Assert(HasBounds());
}

bool IntBounds::HasBounds() const
{LOGMEIN("IntBounds.cpp] 50\n");
    return constantLowerBound != IntConstMin || constantUpperBound != IntConstMax || RequiresIntBoundedValueInfo();
}

bool IntBounds::RequiresIntBoundedValueInfo(const ValueType valueType) const
{LOGMEIN("IntBounds.cpp] 55\n");
    Assert(valueType.IsLikelyInt());
    return !valueType.IsInt() || RequiresIntBoundedValueInfo();
}

bool IntBounds::RequiresIntBoundedValueInfo() const
{LOGMEIN("IntBounds.cpp] 61\n");
    return WasConstantUpperBoundEstablishedExplicitly() || relativeLowerBounds.Count() != 0 || relativeUpperBounds.Count() != 0;
}

int IntBounds::ConstantLowerBound() const
{LOGMEIN("IntBounds.cpp] 66\n");
    return constantLowerBound;
}

int IntBounds::ConstantUpperBound() const
{LOGMEIN("IntBounds.cpp] 71\n");
    return constantUpperBound;
}

IntConstantBounds IntBounds::ConstantBounds() const
{LOGMEIN("IntBounds.cpp] 76\n");
    return IntConstantBounds(ConstantLowerBound(), ConstantUpperBound());
}

bool IntBounds::WasConstantUpperBoundEstablishedExplicitly() const
{LOGMEIN("IntBounds.cpp] 81\n");
    return wasConstantUpperBoundEstablishedExplicitly;
}

const RelativeIntBoundSet &IntBounds::RelativeLowerBounds() const
{LOGMEIN("IntBounds.cpp] 86\n");
    return relativeLowerBounds;
}

const RelativeIntBoundSet &IntBounds::RelativeUpperBounds() const
{LOGMEIN("IntBounds.cpp] 91\n");
    return relativeUpperBounds;
}

void IntBounds::SetLowerBound(const int constantBound)
{LOGMEIN("IntBounds.cpp] 96\n");
    if(constantLowerBound < constantBound && constantBound <= constantUpperBound)
        constantLowerBound = constantBound;
}

void IntBounds::SetLowerBound(const int constantBoundBase, const int offset)
{
    SetBound<true>(constantBoundBase, offset, false);
}

void IntBounds::SetUpperBound(const int constantBound, const bool wasEstablishedExplicitly)
{LOGMEIN("IntBounds.cpp] 107\n");
    if(constantLowerBound <= constantBound && constantBound < constantUpperBound)
        constantUpperBound = constantBound;
    if(wasEstablishedExplicitly)
        wasConstantUpperBoundEstablishedExplicitly = true;
}

void IntBounds::SetUpperBound(const int constantBoundBase, const int offset, const bool wasEstablishedExplicitly)
{
    SetBound<false>(constantBoundBase, offset, wasEstablishedExplicitly);
}

template<bool Lower>
void IntBounds::SetBound(const int constantBoundBase, const int offset, const bool wasEstablishedExplicitly)
{LOGMEIN("IntBounds.cpp] 121\n");
    int constantBound;
    if(offset == 0)
        constantBound = constantBoundBase;
    else if(Int32Math::Add(constantBoundBase, offset, &constantBound))
        return;

    if(Lower)
        SetLowerBound(constantBound);
    else
        SetUpperBound(constantBound, wasEstablishedExplicitly);
}

void IntBounds::SetLowerBound(
    const ValueNumber myValueNumber,
    const Value *const baseValue,
    const bool wasEstablishedExplicitly)
{
    SetLowerBound(myValueNumber, baseValue, 0, wasEstablishedExplicitly);
}

void IntBounds::SetLowerBound(
    const ValueNumber myValueNumber,
    const Value *const baseValue,
    const int offset,
    const bool wasEstablishedExplicitly)
{
    SetBound<true>(myValueNumber, baseValue, offset, wasEstablishedExplicitly);
}

void IntBounds::SetUpperBound(
    const ValueNumber myValueNumber,
    const Value *const baseValue,
    const bool wasEstablishedExplicitly)
{
    SetUpperBound(myValueNumber, baseValue, 0, wasEstablishedExplicitly);
}

void IntBounds::SetUpperBound(
    const ValueNumber myValueNumber,
    const Value *const baseValue,
    const int offset,
    const bool wasEstablishedExplicitly)
{
    SetBound<false>(myValueNumber, baseValue, offset, wasEstablishedExplicitly);
}

template<bool Lower>
void IntBounds::SetBound(
    const ValueNumber myValueNumber,
    const Value *const baseValue,
    const int offset,
    const bool wasEstablishedExplicitly)
{LOGMEIN("IntBounds.cpp] 174\n");
    Assert(baseValue);
    Assert(baseValue->GetValueNumber() != myValueNumber);

    // Aggressively merge the constant lower or upper bound of the base value, adjusted by the offset
    ValueInfo *const baseValueInfo = baseValue->GetValueInfo();
    int constantBoundBase;
    const bool success =
        Lower
            ? baseValueInfo->TryGetIntConstantLowerBound(&constantBoundBase, true)
            : baseValueInfo->TryGetIntConstantUpperBound(&constantBoundBase, true);
    Assert(success);
    const bool isBoundConstant = baseValueInfo->HasIntConstantValue();
    SetBound<Lower>(constantBoundBase, offset, wasEstablishedExplicitly && isBoundConstant);

    if(isBoundConstant)
        return;

    // If the base value has relative bounds, pull in the lower or upper bounds adjusted by the offset
    RelativeIntBoundSet &boundSet = Lower ? relativeLowerBounds : relativeUpperBounds;
    const RelativeIntBoundSet &oppositeBoundSet = Lower ? relativeUpperBounds : relativeLowerBounds;
    if(baseValueInfo->IsIntBounded())
    {LOGMEIN("IntBounds.cpp] 196\n");
        const IntBounds *const baseValueBounds = baseValueInfo->AsIntBounded()->Bounds();
        const RelativeIntBoundSet &baseValueBoundSet =
            Lower ? baseValueBounds->relativeLowerBounds : baseValueBounds->relativeUpperBounds;
        for(auto it = baseValueBoundSet.GetIterator(); it.IsValid(); it.MoveNext())
        {LOGMEIN("IntBounds.cpp] 201\n");
            ValueRelativeOffset bound(it.CurrentValue());
            if(bound.BaseValueNumber() == myValueNumber || !bound.Add(offset))
                continue;
            const ValueRelativeOffset *existingOppositeBound;
            if(oppositeBoundSet.TryGetReference(bound.BaseValueNumber(), &existingOppositeBound) &&
                (Lower ? bound.Offset() > existingOppositeBound->Offset() : bound.Offset() < existingOppositeBound->Offset()))
            {LOGMEIN("IntBounds.cpp] 208\n");
                // This bound contradicts the existing opposite bound on the same base value number:
                //     - Setting a lower bound (base + offset) when (base + offset2) is an upper bound and (offset > offset2)
                //     - Setting an upper bound (base + offset) when (base + offset2) is a lower bound and (offset < offset2)
                continue;
            }
            ValueRelativeOffset *existingBound;
            if(boundSet.TryGetReference(bound.BaseValueNumber(), &existingBound))
                existingBound->Merge<Lower, true>(bound);
            else
                boundSet.Add(bound);
        }
    }

    // Set the base value as a relative bound
    const ValueRelativeOffset bound(baseValue, offset, wasEstablishedExplicitly);
    const ValueRelativeOffset *existingOppositeBound;
    if(oppositeBoundSet.TryGetReference(bound.BaseValueNumber(), &existingOppositeBound) &&
        (Lower ? offset > existingOppositeBound->Offset() : offset < existingOppositeBound->Offset()))
    {LOGMEIN("IntBounds.cpp] 227\n");
        // This bound contradicts the existing opposite bound on the same base value number:
        //     - Setting a lower bound (base + offset) when (base + offset2) is an upper bound and (offset > offset2)
        //     - Setting an upper bound (base + offset) when (base + offset2) is a lower bound and (offset < offset2)
        return;
    }
    ValueRelativeOffset *existingBound;
    if(boundSet.TryGetReference(bound.BaseValueNumber(), &existingBound))
        existingBound->Merge<Lower, true>(bound);
    else
        boundSet.Add(bound);
}

bool IntBounds::SetIsNot(const int constantValue, const bool isExplicit)
{LOGMEIN("IntBounds.cpp] 241\n");
    if(constantLowerBound == constantUpperBound)
        return false;

    Assert(constantLowerBound < constantUpperBound);
    if(constantValue == constantLowerBound)
    {LOGMEIN("IntBounds.cpp] 247\n");
        ++constantLowerBound;
        return true;
    }
    if(constantValue == constantUpperBound)
    {LOGMEIN("IntBounds.cpp] 252\n");
        --constantUpperBound;
        if(isExplicit)
            wasConstantUpperBoundEstablishedExplicitly = true;
        return true;
    }
    return false;
}

bool IntBounds::SetIsNot(const Value *const value, const bool isExplicit)
{LOGMEIN("IntBounds.cpp] 262\n");
    Assert(value);

    ValueInfo *const valueInfo = value->GetValueInfo();
    Assert(valueInfo->IsLikelyInt());

    int constantValue;
    bool changed;
    if(valueInfo->TryGetIntConstantValue(&constantValue, true))
    {LOGMEIN("IntBounds.cpp] 271\n");
        changed = SetIsNot(constantValue, isExplicit);
        if(valueInfo->IsInt())
            return changed;
    }
    else
        changed = false;

    const ValueNumber valueNumber = value->GetValueNumber();
    ValueRelativeOffset *existingLowerBound = nullptr;
    const bool existingLowerBoundOffsetIsZero =
        relativeLowerBounds.TryGetReference(valueNumber, &existingLowerBound) && existingLowerBound->Offset() == 0;
    ValueRelativeOffset *existingUpperBound = nullptr;
    const bool existingUpperBoundOffsetIsZero =
        relativeUpperBounds.TryGetReference(valueNumber, &existingUpperBound) && existingUpperBound->Offset() == 0;
    if(existingLowerBoundOffsetIsZero == existingUpperBoundOffsetIsZero)
        return changed; // neither bound can be changed, or changing a bound would contradict the opposite bound
    if(existingLowerBoundOffsetIsZero)
        existingLowerBound->SetOffset(1);
    else
    {LOGMEIN("IntBounds.cpp] 291\n");
        existingUpperBound->SetOffset(-1);
        if(isExplicit)
            existingUpperBound->SetWasEstablishedExplicitly();
    }
    return true;
}

bool IntBounds::IsGreaterThanOrEqualTo(const int constantValue, const int constantBoundBase, const int offset)
{LOGMEIN("IntBounds.cpp] 300\n");
    if(offset == 0)
        return constantValue >= constantBoundBase;
    if(offset == 1)
        return constantValue > constantBoundBase;

    // use unsigned to avoid signed int overflow
    const int constantBound = (unsigned)constantBoundBase + (unsigned)offset;
    return
        offset >= 0
            ? constantBound >= constantBoundBase && constantValue >= constantBound
            : constantBound >= constantBoundBase || constantValue >= constantBound;
}

bool IntBounds::IsLessThanOrEqualTo(const int constantValue, const int constantBoundBase, const int offset)
{LOGMEIN("IntBounds.cpp] 315\n");
    if(offset == 0)
        return constantValue <= constantBoundBase;
    if(offset == -1)
        return constantValue < constantBoundBase;

    // use unsigned to avoid signed int overflow
    const int constantBound = (unsigned)constantBoundBase + (unsigned)offset;
    return
        offset >= 0
            ? constantBound < constantBoundBase || constantValue <= constantBound
            : constantBound < constantBoundBase && constantValue <= constantBound;
}

bool IntBounds::IsGreaterThanOrEqualTo(const int constantBoundBase, const int offset) const
{LOGMEIN("IntBounds.cpp] 330\n");
    return IsGreaterThanOrEqualTo(constantLowerBound, constantBoundBase, offset);
}

bool IntBounds::IsLessThanOrEqualTo(const int constantBoundBase, const int offset) const
{LOGMEIN("IntBounds.cpp] 335\n");
    return IsLessThanOrEqualTo(constantUpperBound, constantBoundBase, offset);
}

bool IntBounds::IsGreaterThanOrEqualTo(const Value *const value, const int offset) const
{LOGMEIN("IntBounds.cpp] 340\n");
    Assert(value);

    ValueInfo *const valueInfo = value->GetValueInfo();
    int constantBoundBase;
    const bool success = valueInfo->TryGetIntConstantUpperBound(&constantBoundBase, true);
    Assert(success);
    if(IsGreaterThanOrEqualTo(constantBoundBase, offset))
        return true;

    if(valueInfo->HasIntConstantValue())
        return false;

    const ValueRelativeOffset *bound;
    return relativeLowerBounds.TryGetReference(value->GetValueNumber(), &bound) && bound->Offset() >= offset;
}

bool IntBounds::IsLessThanOrEqualTo(const Value *const value, const int offset) const
{LOGMEIN("IntBounds.cpp] 358\n");
    Assert(value);

    ValueInfo *const valueInfo = value->GetValueInfo();
    int constantBoundBase;
    const bool success = valueInfo->TryGetIntConstantLowerBound(&constantBoundBase, true);
    Assert(success);
    if(IsLessThanOrEqualTo(constantBoundBase, offset))
        return true;

    if(valueInfo->HasIntConstantValue())
        return false;

    const ValueRelativeOffset *bound;
    return relativeUpperBounds.TryGetReference(value->GetValueNumber(), &bound) && bound->Offset() <= offset;
}

const IntBounds *IntBounds::Add(
    const Value *const baseValue,
    const int n,
    const bool baseValueInfoIsPrecise,
    const IntConstantBounds &newConstantBounds,
    JitArenaAllocator *const allocator)
{LOGMEIN("IntBounds.cpp] 381\n");
    Assert(baseValue);
    Assert(baseValue->GetValueInfo()->IsLikelyInt());
    Assert(!baseValue->GetValueInfo()->HasIntConstantValue());

    // Determine whether the new constant upper bound was established explicitly
    bool wasConstantUpperBoundEstablishedExplicitly = false;
    ValueInfo *const baseValueInfo = baseValue->GetValueInfo();
    const IntBounds *const baseBounds = baseValueInfo->IsIntBounded() ? baseValueInfo->AsIntBounded()->Bounds() : nullptr;
    if(baseBounds && baseBounds->WasConstantUpperBoundEstablishedExplicitly())
    {LOGMEIN("IntBounds.cpp] 391\n");
        int baseAdjustedConstantUpperBound;
        if(!Int32Math::Add(baseBounds->ConstantUpperBound(), n, &baseAdjustedConstantUpperBound) &&
            baseAdjustedConstantUpperBound == newConstantBounds.UpperBound())
        {LOGMEIN("IntBounds.cpp] 395\n");
            wasConstantUpperBoundEstablishedExplicitly = true;
        }
    }

    // Create the new bounds. Don't try to determine the constant bounds by offsetting the current constant bounds, as loop
    // prepasses are conservative. Use the constant bounds that are passed in.
    IntBounds *const newBounds = New(newConstantBounds, wasConstantUpperBoundEstablishedExplicitly, allocator);

    // Adjust the relative bounds by the constant. The base value info would not be precise for instance, when we're in a loop
    // and the value was created before the loop or in a previous loop iteration.
    if(n < 0 && !baseValueInfoIsPrecise)
    {LOGMEIN("IntBounds.cpp] 407\n");
        // Don't know the number of similar decreases in value that will take place
        Assert(newBounds->constantLowerBound == IntConstMin);
    }
    else if(baseBounds)
    {LOGMEIN("IntBounds.cpp] 412\n");
        Assert(!baseBounds->relativeLowerBounds.ContainsKey(baseValue->GetValueNumber()));
        newBounds->relativeLowerBounds.Copy(&baseBounds->relativeLowerBounds);
        if(n != 0)
        {LOGMEIN("IntBounds.cpp] 416\n");
            for(auto it = newBounds->relativeLowerBounds.GetIteratorWithRemovalSupport(); it.IsValid(); it.MoveNext())
            {LOGMEIN("IntBounds.cpp] 418\n");
                ValueRelativeOffset &bound = it.CurrentValueReference();
                if(!bound.Add(n))
                    it.RemoveCurrent();
            }
        }
    }
    if(n > 0 && !baseValueInfoIsPrecise)
    {LOGMEIN("IntBounds.cpp] 426\n");
        // Don't know the number of similar increases in value that will take place, so clear the relative upper bounds
        Assert(newBounds->constantUpperBound == IntConstMax);
    }
    else if(baseBounds)
    {LOGMEIN("IntBounds.cpp] 431\n");
        Assert(!baseBounds->relativeUpperBounds.ContainsKey(baseValue->GetValueNumber()));
        newBounds->relativeUpperBounds.Copy(&baseBounds->relativeUpperBounds);
        if(n != 0)
        {LOGMEIN("IntBounds.cpp] 435\n");
            for(auto it = newBounds->relativeUpperBounds.GetIteratorWithRemovalSupport(); it.IsValid(); it.MoveNext())
            {LOGMEIN("IntBounds.cpp] 437\n");
                ValueRelativeOffset &bound = it.CurrentValueReference();
                if(!bound.Add(n))
                    it.RemoveCurrent();
            }
        }
    }

    // The result is equal to (baseValue +/- n), so set that as a relative lower and upper bound
    if(baseValueInfo->HasIntConstantValue())
        return newBounds;
    const ValueRelativeOffset bound(baseValue, n, true);
    if(n >= 0 || baseValueInfoIsPrecise)
        newBounds->relativeLowerBounds.Add(bound);
    if(n <= 0 || baseValueInfoIsPrecise)
        newBounds->relativeUpperBounds.Add(bound);
    return newBounds;
}

bool IntBounds::AddCannotOverflowBasedOnRelativeBounds(const int n) const
{LOGMEIN("IntBounds.cpp] 457\n");
    Assert(n != 0);

    if(n >= 0)
    {LOGMEIN("IntBounds.cpp] 461\n");
        const int maxBoundOffset = -n;
        for(auto it = relativeUpperBounds.GetIterator(); it.IsValid(); it.MoveNext())
        {LOGMEIN("IntBounds.cpp] 464\n");
            // If (a < b), then (a + 1) cannot overflow
            const ValueRelativeOffset &bound = it.CurrentValue();
            if(bound.Offset() <= maxBoundOffset)
                return true;
        }
        return false;
    }

    return n != IntConstMin && SubCannotOverflowBasedOnRelativeBounds(-n);
}

bool IntBounds::SubCannotOverflowBasedOnRelativeBounds(const int n) const
{LOGMEIN("IntBounds.cpp] 477\n");
    Assert(n != 0);

    if(n >= 0)
    {LOGMEIN("IntBounds.cpp] 481\n");
        const int minBoundOffset = n;
        for(auto it = relativeLowerBounds.GetIterator(); it.IsValid(); it.MoveNext())
        {LOGMEIN("IntBounds.cpp] 484\n");
            // If (a > b), then (a - 1) cannot overflow
            const ValueRelativeOffset &bound = it.CurrentValue();
            if(bound.Offset() >= minBoundOffset)
                return true;
        }
        return false;
    }

    return n != IntConstMin && AddCannotOverflowBasedOnRelativeBounds(-n);
}

const IntBounds *IntBounds::Merge(
    const Value *const bounds0Value,
    const IntBounds *const bounds0,
    const Value *const bounds1Value,
    const IntConstantBounds &constantBounds1)
{LOGMEIN("IntBounds.cpp] 501\n");
    Assert(bounds0Value);
    bounds0->Verify();
    Assert(bounds1Value);

    const IntConstantBounds constantBounds(
        min(bounds0->ConstantLowerBound(), constantBounds1.LowerBound()),
        max(bounds0->ConstantUpperBound(), constantBounds1.UpperBound()));

    const ValueNumber bounds1ValueNumber = bounds1Value->GetValueNumber();
    const ValueRelativeOffset *commonLowerBound;
    if(!bounds0->relativeLowerBounds.TryGetReference(bounds1ValueNumber, &commonLowerBound))
        commonLowerBound = nullptr;
    const ValueRelativeOffset *commonUpperBound;
    if(!bounds0->relativeUpperBounds.TryGetReference(bounds1ValueNumber, &commonUpperBound))
        commonUpperBound = nullptr;

    if(constantBounds.LowerBound() == IntConstMin &&
        constantBounds.UpperBound() == IntConstMax &&
        !commonLowerBound &&
        !commonUpperBound)
    {LOGMEIN("IntBounds.cpp] 522\n");
        return nullptr;
    }

    IntBounds *const mergedBounds = New(constantBounds, false, bounds0->relativeLowerBounds.GetAllocator());

    // A value is implicitly bounded by itself, so preserve and merge bounds where one value is bounded by the other
    if(bounds0Value->GetValueNumber() == bounds1ValueNumber)
        return mergedBounds;
    if(commonLowerBound)
    {LOGMEIN("IntBounds.cpp] 532\n");
        ValueRelativeOffset mergedLowerBound(*commonLowerBound);
        if(constantBounds1.IsConstant())
            mergedLowerBound.MergeConstantValue<true, false>(constantBounds1.LowerBound());
        else
            mergedLowerBound.Merge<true, false>(ValueRelativeOffset(bounds1Value, true));
        mergedBounds->relativeLowerBounds.Add(mergedLowerBound);
    }
    if(commonUpperBound)
    {LOGMEIN("IntBounds.cpp] 541\n");
        ValueRelativeOffset mergedUpperBound(*commonUpperBound);
        if(constantBounds1.IsConstant())
            mergedUpperBound.MergeConstantValue<false, false>(constantBounds1.LowerBound());
        else
            mergedUpperBound.Merge<false, false>(ValueRelativeOffset(bounds1Value, true));
        mergedBounds->relativeUpperBounds.Add(mergedUpperBound);
    }

    mergedBounds->Verify();
    return mergedBounds;
}

const IntBounds *IntBounds::Merge(
    const Value *const bounds0Value,
    const IntBounds *const bounds0,
    const Value *const bounds1Value,
    const IntBounds *const bounds1)
{LOGMEIN("IntBounds.cpp] 559\n");
    Assert(bounds0Value);
    bounds0->Verify();
    Assert(bounds1Value);
    bounds1->Verify();

    if(bounds0 == bounds1)
        return bounds0;

    JitArenaAllocator *const allocator = bounds0->relativeLowerBounds.GetAllocator();
    IntBounds *const mergedBounds =
        New(IntConstantBounds(
                min(bounds0->constantLowerBound, bounds1->constantLowerBound),
                max(bounds0->constantUpperBound, bounds1->constantUpperBound)),
            bounds0->WasConstantUpperBoundEstablishedExplicitly() && bounds1->WasConstantUpperBoundEstablishedExplicitly(),
            allocator);
    MergeBoundSets<true>(bounds0Value, bounds0, bounds1Value, bounds1, mergedBounds);
    MergeBoundSets<false>(bounds0Value, bounds0, bounds1Value, bounds1, mergedBounds);
    if(mergedBounds->HasBounds())
    {LOGMEIN("IntBounds.cpp] 578\n");
        mergedBounds->Verify();
        return mergedBounds;
    }
    mergedBounds->Delete();
    return nullptr;
}

template<bool Lower>
void IntBounds::MergeBoundSets(
    const Value *const bounds0Value,
    const IntBounds *const bounds0,
    const Value *const bounds1Value,
    const IntBounds *const bounds1,
    IntBounds *const mergedBounds)
{LOGMEIN("IntBounds.cpp] 593\n");
    Assert(bounds0Value);
    bounds0->Verify();
    Assert(bounds1Value);
    bounds1->Verify();
    Assert(bounds0 != bounds1);
    Assert(mergedBounds);
    Assert(mergedBounds != bounds0);
    Assert(mergedBounds != bounds1);

    RelativeIntBoundSet *mergedBoundSet;
    const RelativeIntBoundSet *boundSet0, *boundSet1;
    if(Lower)
    {LOGMEIN("IntBounds.cpp] 606\n");
        mergedBoundSet = &mergedBounds->relativeLowerBounds;
        boundSet0 = &bounds0->relativeLowerBounds;
        boundSet1 = &bounds1->relativeLowerBounds;
    }
    else
    {
        mergedBoundSet = &mergedBounds->relativeUpperBounds;
        boundSet0 = &bounds0->relativeUpperBounds;
        boundSet1 = &bounds1->relativeUpperBounds;
    }

    // Iterate over the smaller set and look up in the larger set for compatible bounds that can be merged
    const RelativeIntBoundSet *iterateOver, *lookUpIn;
    if(boundSet0->Count() <= boundSet1->Count())
    {LOGMEIN("IntBounds.cpp] 621\n");
        iterateOver = boundSet0;
        lookUpIn = boundSet1;
    }
    else
    {
        iterateOver = boundSet1;
        lookUpIn = boundSet0;
    }
    for(auto it = iterateOver->GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("IntBounds.cpp] 631\n");
        const ValueRelativeOffset &iterateOver_bound(it.CurrentValue());
        const ValueNumber baseValueNumber = iterateOver_bound.BaseValueNumber();
        const ValueRelativeOffset *lookUpIn_bound;
        if(!lookUpIn->TryGetReference(baseValueNumber, &lookUpIn_bound))
            continue;
        ValueRelativeOffset mergedBound(iterateOver_bound);
        mergedBound.Merge<Lower, false>(*lookUpIn_bound);
        mergedBoundSet->Add(mergedBound);
    }

    // A value is implicitly bounded by itself, so preserve and merge bounds where one value is bounded by the other
    const ValueNumber bounds0ValueNumber = bounds0Value->GetValueNumber(), bounds1ValueNumber = bounds1Value->GetValueNumber();
    if(bounds0ValueNumber == bounds1ValueNumber)
        return;
    const ValueRelativeOffset *bound;
    if(boundSet0->TryGetReference(bounds1ValueNumber, &bound))
    {LOGMEIN("IntBounds.cpp] 648\n");
        ValueRelativeOffset mergedBound(bounds1Value, true);
        mergedBound.Merge<Lower, false>(*bound);
        mergedBoundSet->Add(mergedBound);
    }
    if(boundSet1->TryGetReference(bounds0ValueNumber, &bound))
    {LOGMEIN("IntBounds.cpp] 654\n");
        ValueRelativeOffset mergedBound(bounds0Value, true);
        mergedBound.Merge<Lower, false>(*bound);
        mergedBoundSet->Add(mergedBound);
    }
}

#pragma endregion

#pragma region IntBoundCheckCompatibilityId and IntBoundCheck

IntBoundCheckCompatibilityId::IntBoundCheckCompatibilityId(
    const ValueNumber leftValueNumber,
    const ValueNumber rightValueNumber)
    : leftValueNumber(leftValueNumber), rightValueNumber(rightValueNumber)
{LOGMEIN("IntBounds.cpp] 669\n");
}

bool IntBoundCheckCompatibilityId::operator ==(const IntBoundCheckCompatibilityId &other) const
{LOGMEIN("IntBounds.cpp] 673\n");
    return leftValueNumber == other.leftValueNumber && rightValueNumber == other.rightValueNumber;
}

IntBoundCheckCompatibilityId::operator hash_t() const
{LOGMEIN("IntBounds.cpp] 678\n");
    return static_cast<hash_t>(static_cast<hash_t>(leftValueNumber) + static_cast<hash_t>(rightValueNumber));
}

IntBoundCheck::IntBoundCheck()
#if DBG
    : leftValueNumber(InvalidValueNumber)
#endif
{
    Assert(!IsValid());
}

IntBoundCheck::IntBoundCheck(
    const ValueNumber leftValueNumber,
    const ValueNumber rightValueNumber,
    IR::Instr *const instr,
    BasicBlock *const block)
    : leftValueNumber(leftValueNumber), rightValueNumber(rightValueNumber), instr(instr), block(block)
{LOGMEIN("IntBounds.cpp] 696\n");
    Assert(IsValid());
}

#if DBG

bool IntBoundCheck::IsValid() const
{LOGMEIN("IntBounds.cpp] 703\n");
    return leftValueNumber != InvalidValueNumber;
}

#endif

ValueNumber IntBoundCheck::LeftValueNumber() const
{LOGMEIN("IntBounds.cpp] 710\n");
    Assert(IsValid());
    return leftValueNumber;
}

ValueNumber IntBoundCheck::RightValueNumber() const
{LOGMEIN("IntBounds.cpp] 716\n");
    Assert(IsValid());
    return rightValueNumber;
}

IR::Instr *IntBoundCheck::Instr() const
{LOGMEIN("IntBounds.cpp] 722\n");
    Assert(IsValid());
    return instr;
}

BasicBlock *IntBoundCheck::Block() const
{LOGMEIN("IntBounds.cpp] 728\n");
    Assert(IsValid());
    return block;
}

IntBoundCheckCompatibilityId IntBoundCheck::CompatibilityId() const
{LOGMEIN("IntBounds.cpp] 734\n");
    return IntBoundCheckCompatibilityId(leftValueNumber, rightValueNumber);
}

bool IntBoundCheck::SetBoundOffset(const int offset, const bool isLoopCountBasedBound) const
{LOGMEIN("IntBounds.cpp] 739\n");
    Assert(IsValid());

    // Determine the previous offset from the instruction (src1 <= src2 + dst)
    IR::IntConstOpnd *dstOpnd = nullptr;
    IntConstType previousOffset = 0;
    if (instr->GetDst())
    {LOGMEIN("IntBounds.cpp] 746\n");
        dstOpnd = instr->GetDst()->AsIntConstOpnd();
        previousOffset = dstOpnd->GetValue();
    }

    IR::IntConstOpnd *src1Opnd = nullptr;
    if (instr->GetSrc1()->IsIntConstOpnd())
    {LOGMEIN("IntBounds.cpp] 753\n");
        src1Opnd = instr->GetSrc1()->AsIntConstOpnd();
        if (IntConstMath::Sub(previousOffset, src1Opnd->GetValue(), &previousOffset))
            return false;
    }

    IR::IntConstOpnd *src2Opnd = (instr->GetSrc2()->IsIntConstOpnd() ? instr->GetSrc2()->AsIntConstOpnd() : nullptr);
    if(src2Opnd && IntConstMath::Add(previousOffset, src2Opnd->GetValue(), &previousOffset))
        return false;

    // Given a bounds check (a <= b + offset), the offset may only be decreased such that it does not invalidate the invariant
    // previously established by the check. If the offset needs to be increased, that requirement is already satisfied by the
    // previous check.
    if(offset >= previousOffset)
        return true;

    IntConstType offsetDecrease;
    if(IntConstMath::Sub(previousOffset, offset, &offsetDecrease))
        return false;

    Assert(offsetDecrease > 0);
    if(src1Opnd)
    {LOGMEIN("IntBounds.cpp] 775\n");
        // Prefer to increase src1, as this is an upper bound check and src1 corresponds to the index
        IntConstType newSrc1Value;
        if(IntConstMath::Add(src1Opnd->GetValue(), offsetDecrease, &newSrc1Value))
            return false;
        src1Opnd->SetValue(newSrc1Value);
    }
    else if(dstOpnd)
    {LOGMEIN("IntBounds.cpp] 783\n");
        IntConstType newDstValue;
        if(IntConstMath::Sub(dstOpnd->GetValue(), offsetDecrease, &newDstValue))
            return false;
        if (newDstValue == 0)
            instr->FreeDst();
        else
            dstOpnd->SetValue(newDstValue);
    }
    else
        instr->SetDst(IR::IntConstOpnd::New(-offsetDecrease, TyMachReg, instr->m_func, true));

    switch(instr->GetBailOutKind())
    {LOGMEIN("IntBounds.cpp] 796\n");
        case IR::BailOutOnFailedHoistedBoundCheck:
            if(isLoopCountBasedBound)
                instr->SetBailOutKind(IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck);
            break;

        case IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck:
            break;

        default:
            instr->SetBailOutKind(
                isLoopCountBasedBound
                    ? IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck
                    : IR::BailOutOnFailedHoistedBoundCheck);
            break;
    }
    return true;
}

#pragma endregion
