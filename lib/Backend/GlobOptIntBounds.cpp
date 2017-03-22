//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

#if ENABLE_DEBUG_CONFIG_OPTIONS && DBG_DUMP

#define TRACE_PHASE_VERBOSE(phase, indent, ...) \
    if(PHASE_VERBOSE_TRACE(phase, this->func)) \
    {LOGMEIN("GlobOptIntBounds.cpp] 10\n"); \
        for(int i = 0; i < static_cast<int>(indent); ++i) \
        {LOGMEIN("GlobOptIntBounds.cpp] 12\n"); \
            Output::Print(_u("    ")); \
        } \
        Output::Print(__VA_ARGS__); \
        Output::Flush(); \
    }

#else

#define TRACE_PHASE_VERBOSE(phase, indent, ...)

#endif

void GlobOpt::AddSubConstantInfo::Set(
    StackSym *const srcSym,
    Value *const srcValue,
    const bool srcValueIsLikelyConstant,
    const int32 offset)
{LOGMEIN("GlobOptIntBounds.cpp] 30\n");
    Assert(srcSym);
    Assert(!srcSym->IsTypeSpec());
    Assert(srcValue);
    Assert(srcValue->GetValueInfo()->IsLikelyInt());

    this->srcSym = srcSym;
    this->srcValue = srcValue;
    this->srcValueIsLikelyConstant = srcValueIsLikelyConstant;
    this->offset = offset;
}

void GlobOpt::ArrayLowerBoundCheckHoistInfo::SetCompatibleBoundCheck(
    BasicBlock *const compatibleBoundCheckBlock,
    StackSym *const indexSym,
    const int offset,
    const ValueNumber indexValueNumber)
{LOGMEIN("GlobOptIntBounds.cpp] 47\n");
    Assert(!Loop());
    Assert(compatibleBoundCheckBlock);
    Assert(indexSym);
    Assert(!indexSym->IsTypeSpec());
    Assert(indexValueNumber != InvalidValueNumber);

    this->compatibleBoundCheckBlock = compatibleBoundCheckBlock;
    this->indexSym = indexSym;
    this->offset = offset;
    this->indexValueNumber = indexValueNumber;
}

void GlobOpt::ArrayLowerBoundCheckHoistInfo::SetLoop(
    ::Loop *const loop,
    const int indexConstantValue,
    const bool isLoopCountBasedBound)
{LOGMEIN("GlobOptIntBounds.cpp] 64\n");
    Assert(!CompatibleBoundCheckBlock());
    Assert(loop);

    this->loop = loop;
    indexSym = nullptr;
    offset = 0;
    indexValue = nullptr;
    indexConstantBounds = IntConstantBounds(indexConstantValue, indexConstantValue);
    this->isLoopCountBasedBound = isLoopCountBasedBound;
    loopCount = nullptr;
}

void GlobOpt::ArrayLowerBoundCheckHoistInfo::SetLoop(
    ::Loop *const loop,
    StackSym *const indexSym,
    const int offset,
    Value *const indexValue,
    const IntConstantBounds &indexConstantBounds,
    const bool isLoopCountBasedBound)
{LOGMEIN("GlobOptIntBounds.cpp] 84\n");
    Assert(!CompatibleBoundCheckBlock());
    Assert(loop);
    Assert(indexSym);
    Assert(!indexSym->IsTypeSpec());
    Assert(indexValue);

    this->loop = loop;
    this->indexSym = indexSym;
    this->offset = offset;
    this->indexValueNumber = indexValue->GetValueNumber();
    this->indexValue = indexValue;
    this->indexConstantBounds = indexConstantBounds;
    this->isLoopCountBasedBound = isLoopCountBasedBound;
    loopCount = nullptr;
}

void GlobOpt::ArrayLowerBoundCheckHoistInfo::SetLoopCount(::LoopCount *const loopCount, const int maxMagnitudeChange)
{LOGMEIN("GlobOptIntBounds.cpp] 102\n");
    Assert(Loop());
    Assert(loopCount);
    Assert(maxMagnitudeChange != 0);

    this->loopCount = loopCount;
    this->maxMagnitudeChange = maxMagnitudeChange;
}

void GlobOpt::ArrayUpperBoundCheckHoistInfo::SetCompatibleBoundCheck(
    BasicBlock *const compatibleBoundCheckBlock,
    const int indexConstantValue)
{LOGMEIN("GlobOptIntBounds.cpp] 114\n");
    Assert(!Loop());
    Assert(compatibleBoundCheckBlock);

    this->compatibleBoundCheckBlock = compatibleBoundCheckBlock;
    indexSym = nullptr;
    offset = -1; // simulate < instead of <=
    indexConstantBounds = IntConstantBounds(indexConstantValue, indexConstantValue);
}

void GlobOpt::ArrayUpperBoundCheckHoistInfo::SetLoop(
    ::Loop *const loop,
    const int indexConstantValue,
    Value *const headSegmentLengthValue,
    const IntConstantBounds &headSegmentLengthConstantBounds,
    const bool isLoopCountBasedBound)
{LOGMEIN("GlobOptIntBounds.cpp] 130\n");
    Assert(!CompatibleBoundCheckBlock());
    Assert(loop);
    Assert(headSegmentLengthValue);

    SetLoop(loop, indexConstantValue, isLoopCountBasedBound);
    offset = -1; // simulate < instead of <=
    this->headSegmentLengthValue = headSegmentLengthValue;
    this->headSegmentLengthConstantBounds = headSegmentLengthConstantBounds;
}

void GlobOpt::ArrayUpperBoundCheckHoistInfo::SetLoop(
    ::Loop *const loop,
    StackSym *const indexSym,
    const int offset,
    Value *const indexValue,
    const IntConstantBounds &indexConstantBounds,
    Value *const headSegmentLengthValue,
    const IntConstantBounds &headSegmentLengthConstantBounds,
    const bool isLoopCountBasedBound)
{LOGMEIN("GlobOptIntBounds.cpp] 150\n");
    Assert(headSegmentLengthValue);

    SetLoop(loop, indexSym, offset, indexValue, indexConstantBounds, isLoopCountBasedBound);
    this->headSegmentLengthValue = headSegmentLengthValue;
    this->headSegmentLengthConstantBounds = headSegmentLengthConstantBounds;
}

bool ValueInfo::HasIntConstantValue(const bool includeLikelyInt) const
{LOGMEIN("GlobOptIntBounds.cpp] 159\n");
    int32 constantValue;
    return TryGetIntConstantValue(&constantValue, includeLikelyInt);
}

bool ValueInfo::TryGetIntConstantValue(int32 *const intValueRef, const bool includeLikelyInt) const
{LOGMEIN("GlobOptIntBounds.cpp] 165\n");
    Assert(intValueRef);

    if(!(includeLikelyInt ? IsLikelyInt() : IsInt()))
    {LOGMEIN("GlobOptIntBounds.cpp] 169\n");
        return false;
    }

    switch(structureKind)
    {LOGMEIN("GlobOptIntBounds.cpp] 174\n");
        case ValueStructureKind::IntConstant:
            if(!includeLikelyInt || IsInt())
            {LOGMEIN("GlobOptIntBounds.cpp] 177\n");
                *intValueRef = AsIntConstant()->IntValue();
                return true;
            }
            break;

        case ValueStructureKind::IntRange:
            Assert(includeLikelyInt && !IsInt() || !AsIntRange()->IsConstant());
            break;

        case ValueStructureKind::IntBounded:
        {LOGMEIN("GlobOptIntBounds.cpp] 188\n");
            const IntConstantBounds bounds(AsIntBounded()->Bounds()->ConstantBounds());
            if(bounds.IsConstant())
            {LOGMEIN("GlobOptIntBounds.cpp] 191\n");
                *intValueRef = bounds.LowerBound();
                return true;
            }
            break;
        }
    }
    return false;
}

bool ValueInfo::TryGetIntConstantLowerBound(int32 *const intConstantBoundRef, const bool includeLikelyInt) const
{LOGMEIN("GlobOptIntBounds.cpp] 202\n");
    Assert(intConstantBoundRef);

    if(!(includeLikelyInt ? IsLikelyInt() : IsInt()))
    {LOGMEIN("GlobOptIntBounds.cpp] 206\n");
        return false;
    }

    switch(structureKind)
    {LOGMEIN("GlobOptIntBounds.cpp] 211\n");
        case ValueStructureKind::IntConstant:
            if(!includeLikelyInt || IsInt())
            {LOGMEIN("GlobOptIntBounds.cpp] 214\n");
                *intConstantBoundRef = AsIntConstant()->IntValue();
                return true;
            }
            break;

        case ValueStructureKind::IntRange:
            if(!includeLikelyInt || IsInt())
            {LOGMEIN("GlobOptIntBounds.cpp] 222\n");
                *intConstantBoundRef = AsIntRange()->LowerBound();
                return true;
            }
            break;

        case ValueStructureKind::IntBounded:
            *intConstantBoundRef = AsIntBounded()->Bounds()->ConstantLowerBound();
            return true;
    }

    *intConstantBoundRef = IsTaggedInt() ? Js::Constants::Int31MinValue : IntConstMin;
    return true;
}

bool ValueInfo::TryGetIntConstantUpperBound(int32 *const intConstantBoundRef, const bool includeLikelyInt) const
{LOGMEIN("GlobOptIntBounds.cpp] 238\n");
    Assert(intConstantBoundRef);

    if(!(includeLikelyInt ? IsLikelyInt() : IsInt()))
    {LOGMEIN("GlobOptIntBounds.cpp] 242\n");
        return false;
    }

    switch(structureKind)
    {LOGMEIN("GlobOptIntBounds.cpp] 247\n");
        case ValueStructureKind::IntConstant:
            if(!includeLikelyInt || IsInt())
            {LOGMEIN("GlobOptIntBounds.cpp] 250\n");
                *intConstantBoundRef = AsIntConstant()->IntValue();
                return true;
            }
            break;

        case ValueStructureKind::IntRange:
            if(!includeLikelyInt || IsInt())
            {LOGMEIN("GlobOptIntBounds.cpp] 258\n");
                *intConstantBoundRef = AsIntRange()->UpperBound();
                return true;
            }
            break;

        case ValueStructureKind::IntBounded:
            *intConstantBoundRef = AsIntBounded()->Bounds()->ConstantUpperBound();
            return true;
    }

    *intConstantBoundRef = IsTaggedInt() ? Js::Constants::Int31MaxValue : IntConstMax;
    return true;
}

bool ValueInfo::TryGetIntConstantBounds(IntConstantBounds *const intConstantBoundsRef, const bool includeLikelyInt) const
{LOGMEIN("GlobOptIntBounds.cpp] 274\n");
    Assert(intConstantBoundsRef);

    if(!(includeLikelyInt ? IsLikelyInt() : IsInt()))
    {LOGMEIN("GlobOptIntBounds.cpp] 278\n");
        return false;
    }

    switch(structureKind)
    {LOGMEIN("GlobOptIntBounds.cpp] 283\n");
        case ValueStructureKind::IntConstant:
            if(!includeLikelyInt || IsInt())
            {LOGMEIN("GlobOptIntBounds.cpp] 286\n");
                const int32 intValue = AsIntConstant()->IntValue();
                *intConstantBoundsRef = IntConstantBounds(intValue, intValue);
                return true;
            }
            break;

        case ValueStructureKind::IntRange:
            if(!includeLikelyInt || IsInt())
            {LOGMEIN("GlobOptIntBounds.cpp] 295\n");
                *intConstantBoundsRef = *AsIntRange();
                return true;
            }
            break;

        case ValueStructureKind::IntBounded:
            *intConstantBoundsRef = AsIntBounded()->Bounds()->ConstantBounds();
            return true;
    }

    *intConstantBoundsRef =
        IsTaggedInt()
            ? IntConstantBounds(Js::Constants::Int31MinValue, Js::Constants::Int31MaxValue)
            : IntConstantBounds(INT32_MIN, INT32_MAX);
    return true;
}

bool ValueInfo::WasNegativeZeroPreventedByBailout() const
{LOGMEIN("GlobOptIntBounds.cpp] 314\n");
    if(!IsInt())
    {LOGMEIN("GlobOptIntBounds.cpp] 316\n");
        return false;
    }

    switch(structureKind)
    {LOGMEIN("GlobOptIntBounds.cpp] 321\n");
        case ValueStructureKind::IntRange:
            return AsIntRange()->WasNegativeZeroPreventedByBailout();

        case ValueStructureKind::IntBounded:
            return AsIntBounded()->WasNegativeZeroPreventedByBailout();
    }
    return false;
}

bool ValueInfo::IsEqualTo(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOptIntBounds.cpp] 338\n");
    const bool result =
        IsEqualTo_NoConverse(src1Value, min1, max1, src2Value, min2, max2) ||
        IsEqualTo_NoConverse(src2Value, min2, max2, src1Value, min1, max1);
    Assert(!result || !IsNotEqualTo_NoConverse(src1Value, min1, max1, src2Value, min2, max2));
    Assert(!result || !IsNotEqualTo_NoConverse(src2Value, min2, max2, src1Value, min1, max1));
    return result;
}

bool ValueInfo::IsNotEqualTo(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOptIntBounds.cpp] 354\n");
    const bool result =
        IsNotEqualTo_NoConverse(src1Value, min1, max1, src2Value, min2, max2) ||
        IsNotEqualTo_NoConverse(src2Value, min2, max2, src1Value, min1, max1);
    Assert(!result || !IsEqualTo_NoConverse(src1Value, min1, max1, src2Value, min2, max2));
    Assert(!result || !IsEqualTo_NoConverse(src2Value, min2, max2, src1Value, min1, max1));
    return result;
}

bool ValueInfo::IsEqualTo_NoConverse(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOptIntBounds.cpp] 370\n");
    return
        IsGreaterThanOrEqualTo(src1Value, min1, max1, src2Value, min2, max2) &&
        IsLessThanOrEqualTo(src1Value, min1, max1, src2Value, min2, max2);
}

bool ValueInfo::IsNotEqualTo_NoConverse(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOptIntBounds.cpp] 383\n");
    return
        IsGreaterThan(src1Value, min1, max1, src2Value, min2, max2) ||
        IsLessThan(src1Value, min1, max1, src2Value, min2, max2);
}

bool ValueInfo::IsGreaterThanOrEqualTo(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOptIntBounds.cpp] 396\n");
    return IsGreaterThanOrEqualTo(src1Value, min1, max1, src2Value, min2, max2, 0);
}

bool ValueInfo::IsGreaterThan(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOptIntBounds.cpp] 407\n");
    return IsGreaterThanOrEqualTo(src1Value, min1, max1, src2Value, min2, max2, 1);
}

bool ValueInfo::IsLessThanOrEqualTo(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOptIntBounds.cpp] 418\n");
    return IsLessThanOrEqualTo(src1Value, min1, max1, src2Value, min2, max2, 0);
}

bool ValueInfo::IsLessThan(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOptIntBounds.cpp] 429\n");
    return IsLessThanOrEqualTo(src1Value, min1, max1, src2Value, min2, max2, -1);
}

bool ValueInfo::IsGreaterThanOrEqualTo(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2,
    const int src2Offset)
{LOGMEIN("GlobOptIntBounds.cpp] 441\n");
    return
        IsGreaterThanOrEqualTo_NoConverse(src1Value, min1, max1, src2Value, min2, max2, src2Offset) ||
        src2Offset == IntConstMin ||
        IsLessThanOrEqualTo_NoConverse(src2Value, min2, max2, src1Value, min1, max1, -src2Offset);
}

bool ValueInfo::IsLessThanOrEqualTo(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2,
    const int src2Offset)
{LOGMEIN("GlobOptIntBounds.cpp] 456\n");
    return
        IsLessThanOrEqualTo_NoConverse(src1Value, min1, max1, src2Value, min2, max2, src2Offset) ||
        (
            src2Offset != IntConstMin &&
            IsGreaterThanOrEqualTo_NoConverse(src2Value, min2, max2, src1Value, min1, max1, -src2Offset)
        );
}

bool ValueInfo::IsGreaterThanOrEqualTo_NoConverse(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2,
    const int src2Offset)
{LOGMEIN("GlobOptIntBounds.cpp] 473\n");
    Assert(src1Value || min1 == max1);
    Assert(!src1Value || src1Value->GetValueInfo()->IsLikelyInt());
    Assert(src2Value || min2 == max2);
    Assert(!src2Value || src2Value->GetValueInfo()->IsLikelyInt());

    if(src1Value)
    {LOGMEIN("GlobOptIntBounds.cpp] 480\n");
        if(src2Value && src1Value->GetValueNumber() == src2Value->GetValueNumber())
        {LOGMEIN("GlobOptIntBounds.cpp] 482\n");
            return src2Offset <= 0;
        }

        ValueInfo *const src1ValueInfo = src1Value->GetValueInfo();
        if(src1ValueInfo->structureKind == ValueStructureKind::IntBounded)
        {LOGMEIN("GlobOptIntBounds.cpp] 488\n");
            const IntBounds *const bounds = src1ValueInfo->AsIntBounded()->Bounds();
            return
                src2Value
                    ? bounds->IsGreaterThanOrEqualTo(src2Value, src2Offset)
                    : bounds->IsGreaterThanOrEqualTo(min2, src2Offset);
        }
    }
    return IntBounds::IsGreaterThanOrEqualTo(min1, max2, src2Offset);
}

bool ValueInfo::IsLessThanOrEqualTo_NoConverse(
    const Value *const src1Value,
    const int32 min1,
    const int32 max1,
    const Value *const src2Value,
    const int32 min2,
    const int32 max2,
    const int src2Offset)
{LOGMEIN("GlobOptIntBounds.cpp] 507\n");
    Assert(src1Value || min1 == max1);
    Assert(!src1Value || src1Value->GetValueInfo()->IsLikelyInt());
    Assert(src2Value || min2 == max2);
    Assert(!src2Value || src2Value->GetValueInfo()->IsLikelyInt());

    if(src1Value)
    {LOGMEIN("GlobOptIntBounds.cpp] 514\n");
        if(src2Value && src1Value->GetValueNumber() == src2Value->GetValueNumber())
        {LOGMEIN("GlobOptIntBounds.cpp] 516\n");
            return src2Offset >= 0;
        }

        ValueInfo *const src1ValueInfo = src1Value->GetValueInfo();
        if(src1ValueInfo->structureKind == ValueStructureKind::IntBounded)
        {LOGMEIN("GlobOptIntBounds.cpp] 522\n");
            const IntBounds *const bounds = src1ValueInfo->AsIntBounded()->Bounds();
            return
                src2Value
                    ? bounds->IsLessThanOrEqualTo(src2Value, src2Offset)
                    : bounds->IsLessThanOrEqualTo(min2, src2Offset);
        }
    }
    return IntBounds::IsLessThanOrEqualTo(max1, min2, src2Offset);
}

void GlobOpt::UpdateIntBoundsForEqualBranch(
    Value *const src1Value,
    Value *const src2Value,
    const int32 src2ConstantValue)
{LOGMEIN("GlobOptIntBounds.cpp] 537\n");
    Assert(src1Value);

    if(!DoPathDependentValues() || (src2Value && src1Value->GetValueNumber() == src2Value->GetValueNumber()))
    {LOGMEIN("GlobOptIntBounds.cpp] 541\n");
        return;
    }

#if DBG
    if(!IsLoopPrePass() && DoAggressiveIntTypeSpec() && DoConstFold())
    {LOGMEIN("GlobOptIntBounds.cpp] 547\n");
        IntConstantBounds src1ConstantBounds, src2ConstantBounds;
        AssertVerify(src1Value->GetValueInfo()->TryGetIntConstantBounds(&src1ConstantBounds, true));
        if(src2Value)
        {LOGMEIN("GlobOptIntBounds.cpp] 551\n");
            AssertVerify(src2Value->GetValueInfo()->TryGetIntConstantBounds(&src2ConstantBounds, true));
        }
        else
        {
            src2ConstantBounds = IntConstantBounds(src2ConstantValue, src2ConstantValue);
        }

        Assert(
            !ValueInfo::IsEqualTo(
                src1Value,
                src1ConstantBounds.LowerBound(),
                src1ConstantBounds.UpperBound(),
                src2Value,
                src2ConstantBounds.LowerBound(),
                src2ConstantBounds.UpperBound()));
        Assert(
            !ValueInfo::IsNotEqualTo(
                src1Value,
                src1ConstantBounds.LowerBound(),
                src1ConstantBounds.UpperBound(),
                src2Value,
                src2ConstantBounds.LowerBound(),
                src2ConstantBounds.UpperBound()));
    }
#endif

    SetPathDependentInfo(
        true,
        PathDependentInfo(PathDependentRelationship::Equal, src1Value, src2Value, src2ConstantValue));
    SetPathDependentInfo(
        false,
        PathDependentInfo(PathDependentRelationship::NotEqual, src1Value, src2Value, src2ConstantValue));
}

void GlobOpt::UpdateIntBoundsForNotEqualBranch(
    Value *const src1Value,
    Value *const src2Value,
    const int32 src2ConstantValue)
{LOGMEIN("GlobOptIntBounds.cpp] 590\n");
    Assert(src1Value);

    if(!DoPathDependentValues() || (src2Value && src1Value->GetValueNumber() == src2Value->GetValueNumber()))
    {LOGMEIN("GlobOptIntBounds.cpp] 594\n");
        return;
    }

#if DBG
    if(!IsLoopPrePass() && DoAggressiveIntTypeSpec() && DoConstFold())
    {LOGMEIN("GlobOptIntBounds.cpp] 600\n");
        IntConstantBounds src1ConstantBounds, src2ConstantBounds;
        AssertVerify(src1Value->GetValueInfo()->TryGetIntConstantBounds(&src1ConstantBounds, true));
        if(src2Value)
        {LOGMEIN("GlobOptIntBounds.cpp] 604\n");
            AssertVerify(src2Value->GetValueInfo()->TryGetIntConstantBounds(&src2ConstantBounds, true));
        }
        else
        {
            src2ConstantBounds = IntConstantBounds(src2ConstantValue, src2ConstantValue);
        }

        Assert(
            !ValueInfo::IsEqualTo(
                src1Value,
                src1ConstantBounds.LowerBound(),
                src1ConstantBounds.UpperBound(),
                src2Value,
                src2ConstantBounds.LowerBound(),
                src2ConstantBounds.UpperBound()));
        Assert(
            !ValueInfo::IsNotEqualTo(
                src1Value,
                src1ConstantBounds.LowerBound(),
                src1ConstantBounds.UpperBound(),
                src2Value,
                src2ConstantBounds.LowerBound(),
                src2ConstantBounds.UpperBound()));
    }
#endif

    SetPathDependentInfo(
        true, PathDependentInfo(PathDependentRelationship::NotEqual, src1Value, src2Value, src2ConstantValue));
    SetPathDependentInfo(
        false, PathDependentInfo(PathDependentRelationship::Equal, src1Value, src2Value, src2ConstantValue));
}

void GlobOpt::UpdateIntBoundsForGreaterThanOrEqualBranch(Value *const src1Value, Value *const src2Value)
{LOGMEIN("GlobOptIntBounds.cpp] 638\n");
    Assert(src1Value);
    Assert(src2Value);

    if(!DoPathDependentValues() || src1Value->GetValueNumber() == src2Value->GetValueNumber())
    {LOGMEIN("GlobOptIntBounds.cpp] 643\n");
        return;
    }

#if DBG
    if(!IsLoopPrePass() && DoAggressiveIntTypeSpec() && DoConstFold())
    {LOGMEIN("GlobOptIntBounds.cpp] 649\n");
        IntConstantBounds src1ConstantBounds, src2ConstantBounds;
        AssertVerify(src1Value->GetValueInfo()->TryGetIntConstantBounds(&src1ConstantBounds, true));
        AssertVerify(src2Value->GetValueInfo()->TryGetIntConstantBounds(&src2ConstantBounds, true));

        Assert(
            !ValueInfo::IsGreaterThanOrEqualTo(
                src1Value,
                src1ConstantBounds.LowerBound(),
                src1ConstantBounds.UpperBound(),
                src2Value,
                src2ConstantBounds.LowerBound(),
                src2ConstantBounds.UpperBound()));
        Assert(
            !ValueInfo::IsLessThan(
                src1Value,
                src1ConstantBounds.LowerBound(),
                src1ConstantBounds.UpperBound(),
                src2Value,
                src2ConstantBounds.LowerBound(),
                src2ConstantBounds.UpperBound()));
    }
#endif

    SetPathDependentInfo(true, PathDependentInfo(PathDependentRelationship::GreaterThanOrEqual, src1Value, src2Value));
    SetPathDependentInfo(false, PathDependentInfo(PathDependentRelationship::LessThan, src1Value, src2Value));
}

void GlobOpt::UpdateIntBoundsForGreaterThanBranch(Value *const src1Value, Value *const src2Value)
{LOGMEIN("GlobOptIntBounds.cpp] 678\n");
    return UpdateIntBoundsForLessThanBranch(src2Value, src1Value);
}

void GlobOpt::UpdateIntBoundsForLessThanOrEqualBranch(Value *const src1Value, Value *const src2Value)
{LOGMEIN("GlobOptIntBounds.cpp] 683\n");
    return UpdateIntBoundsForGreaterThanOrEqualBranch(src2Value, src1Value);
}

void GlobOpt::UpdateIntBoundsForLessThanBranch(Value *const src1Value, Value *const src2Value)
{LOGMEIN("GlobOptIntBounds.cpp] 688\n");
    Assert(src1Value);
    Assert(src2Value);

    if(!DoPathDependentValues() || src1Value->GetValueNumber() == src2Value->GetValueNumber())
    {LOGMEIN("GlobOptIntBounds.cpp] 693\n");
        return;
    }

#if DBG
    if(!IsLoopPrePass() && DoAggressiveIntTypeSpec() && DoConstFold())
    {LOGMEIN("GlobOptIntBounds.cpp] 699\n");
        IntConstantBounds src1ConstantBounds, src2ConstantBounds;
        AssertVerify(src1Value->GetValueInfo()->TryGetIntConstantBounds(&src1ConstantBounds, true));
        AssertVerify(src2Value->GetValueInfo()->TryGetIntConstantBounds(&src2ConstantBounds, true));

        Assert(
            !ValueInfo::IsGreaterThanOrEqualTo(
                src1Value,
                src1ConstantBounds.LowerBound(),
                src1ConstantBounds.UpperBound(),
                src2Value,
                src2ConstantBounds.LowerBound(),
                src2ConstantBounds.UpperBound()));
        Assert(
            !ValueInfo::IsLessThan(
                src1Value,
                src1ConstantBounds.LowerBound(),
                src1ConstantBounds.UpperBound(),
                src2Value,
                src2ConstantBounds.LowerBound(),
                src2ConstantBounds.UpperBound()));
    }
#endif

    SetPathDependentInfo(true, PathDependentInfo(PathDependentRelationship::LessThan, src1Value, src2Value));
    SetPathDependentInfo(false, PathDependentInfo(PathDependentRelationship::GreaterThanOrEqual, src1Value, src2Value));
}

IntBounds *GlobOpt::GetIntBoundsToUpdate(
    const ValueInfo *const valueInfo,
    const IntConstantBounds &constantBounds,
    const bool isSettingNewBound,
    const bool isBoundConstant,
    const bool isSettingUpperBound,
    const bool isExplicit)
{LOGMEIN("GlobOptIntBounds.cpp] 734\n");
    Assert(valueInfo);
    Assert(valueInfo->IsLikelyInt());

    if(!DoTrackRelativeIntBounds())
    {LOGMEIN("GlobOptIntBounds.cpp] 739\n");
        return nullptr;
    }

    if(valueInfo->IsIntBounded())
    {LOGMEIN("GlobOptIntBounds.cpp] 744\n");
        const IntBounds *const bounds = valueInfo->AsIntBounded()->Bounds();
        if(bounds->RequiresIntBoundedValueInfo(valueInfo->Type()))
        {LOGMEIN("GlobOptIntBounds.cpp] 747\n");
            return bounds->Clone();
        }
    }

    if(valueInfo->IsInt())
    {LOGMEIN("GlobOptIntBounds.cpp] 753\n");
        if(constantBounds.IsConstant())
        {LOGMEIN("GlobOptIntBounds.cpp] 755\n");
            // Don't start tracking relative bounds for int constant values, just retain existing relative bounds. Will use
            // IntConstantValueInfo instead.
            return nullptr;
        }

        if(isBoundConstant)
        {LOGMEIN("GlobOptIntBounds.cpp] 762\n");
            // There are no relative bounds to track
            if(!(isSettingUpperBound && isExplicit))
            {LOGMEIN("GlobOptIntBounds.cpp] 765\n");
                // We are not setting a constant upper bound that is established explicitly, will use IntRangeValueInfo instead
                return nullptr;
            }
        }
        else if(!isSettingNewBound)
        {LOGMEIN("GlobOptIntBounds.cpp] 771\n");
            // New relative bounds are not being set, will use IntRangeValueInfo instead
            return nullptr;
        }
        return IntBounds::New(constantBounds, false, alloc);
    }

    return nullptr;
}

ValueInfo *GlobOpt::UpdateIntBoundsForEqual(
    Value *const value,
    const IntConstantBounds &constantBounds,
    Value *const boundValue,
    const IntConstantBounds &boundConstantBounds,
    const bool isExplicit)
{LOGMEIN("GlobOptIntBounds.cpp] 787\n");
    Assert(value || constantBounds.IsConstant());
    Assert(boundValue || boundConstantBounds.IsConstant());
    if(!value)
    {LOGMEIN("GlobOptIntBounds.cpp] 791\n");
        return nullptr;
    }
    Assert(!boundValue || value->GetValueNumber() != boundValue->GetValueNumber());

    ValueInfo *const valueInfo = value->GetValueInfo();
    IntBounds *const bounds =
        GetIntBoundsToUpdate(valueInfo, constantBounds, true, boundConstantBounds.IsConstant(), true, isExplicit);
    if(bounds)
    {LOGMEIN("GlobOptIntBounds.cpp] 800\n");
        if(boundValue)
        {LOGMEIN("GlobOptIntBounds.cpp] 802\n");
            const ValueNumber valueNumber = value->GetValueNumber();
            bounds->SetLowerBound(valueNumber, boundValue, isExplicit);
            bounds->SetUpperBound(valueNumber, boundValue, isExplicit);
        }
        else
        {
            bounds->SetLowerBound(boundConstantBounds.LowerBound());
            bounds->SetUpperBound(boundConstantBounds.LowerBound(), isExplicit);
        }
        if(bounds->RequiresIntBoundedValueInfo(valueInfo->Type()))
        {LOGMEIN("GlobOptIntBounds.cpp] 813\n");
            return NewIntBoundedValueInfo(valueInfo, bounds);
        }
        bounds->Delete();
    }

    if(!valueInfo->IsInt())
    {LOGMEIN("GlobOptIntBounds.cpp] 820\n");
        return nullptr;
    }

    const int32 newMin = max(constantBounds.LowerBound(), boundConstantBounds.LowerBound());
    const int32 newMax = min(constantBounds.UpperBound(), boundConstantBounds.UpperBound());
    return newMin <= newMax ? NewIntRangeValueInfo(valueInfo, newMin, newMax) : nullptr;
}

ValueInfo *GlobOpt::UpdateIntBoundsForNotEqual(
    Value *const value,
    const IntConstantBounds &constantBounds,
    Value *const boundValue,
    const IntConstantBounds &boundConstantBounds,
    const bool isExplicit)
{LOGMEIN("GlobOptIntBounds.cpp] 835\n");
    Assert(value || constantBounds.IsConstant());
    Assert(boundValue || boundConstantBounds.IsConstant());
    if(!value)
    {LOGMEIN("GlobOptIntBounds.cpp] 839\n");
        return nullptr;
    }
    Assert(!boundValue || value->GetValueNumber() != boundValue->GetValueNumber());

    ValueInfo *const valueInfo = value->GetValueInfo();
    IntBounds *const bounds =
        GetIntBoundsToUpdate(
            valueInfo,
            constantBounds,
            false,
            boundConstantBounds.IsConstant(),
            boundConstantBounds.IsConstant() && boundConstantBounds.LowerBound() == constantBounds.UpperBound(),
            isExplicit);
    if(bounds)
    {LOGMEIN("GlobOptIntBounds.cpp] 854\n");
        if(boundValue
                ? bounds->SetIsNot(boundValue, isExplicit)
                : bounds->SetIsNot(boundConstantBounds.LowerBound(), isExplicit))
        {LOGMEIN("GlobOptIntBounds.cpp] 858\n");
            if(bounds->RequiresIntBoundedValueInfo(valueInfo->Type()))
            {LOGMEIN("GlobOptIntBounds.cpp] 860\n");
                return NewIntBoundedValueInfo(valueInfo, bounds);
            }
        }
        else
        {
            bounds->Delete();
            return nullptr;
        }
        bounds->Delete();
    }

    if(!valueInfo->IsInt() || !boundConstantBounds.IsConstant())
    {LOGMEIN("GlobOptIntBounds.cpp] 873\n");
        return nullptr;
    }
    const int32 constantBound = boundConstantBounds.LowerBound();

    // The value is not equal to a constant, so narrow the range if the constant is equal to the value's lower or upper bound
    int32 newMin = constantBounds.LowerBound(), newMax = constantBounds.UpperBound();
    if(constantBound == newMin)
    {LOGMEIN("GlobOptIntBounds.cpp] 881\n");
        Assert(newMin <= newMax);
        if(newMin == newMax)
        {LOGMEIN("GlobOptIntBounds.cpp] 884\n");
            return nullptr;
        }
        ++newMin;
    }
    else if(constantBound == newMax)
    {LOGMEIN("GlobOptIntBounds.cpp] 890\n");
        Assert(newMin <= newMax);
        if(newMin == newMax)
        {LOGMEIN("GlobOptIntBounds.cpp] 893\n");
            return nullptr;
        }
        --newMax;
    }
    else
    {
        return nullptr;
    }
    return NewIntRangeValueInfo(valueInfo, newMin, newMax);
}

ValueInfo *GlobOpt::UpdateIntBoundsForGreaterThanOrEqual(
    Value *const value,
    const IntConstantBounds &constantBounds,
    Value *const boundValue,
    const IntConstantBounds &boundConstantBounds,
    const bool isExplicit)
{LOGMEIN("GlobOptIntBounds.cpp] 911\n");
    return UpdateIntBoundsForGreaterThanOrEqual(value, constantBounds, boundValue, boundConstantBounds, 0, isExplicit);
}

ValueInfo *GlobOpt::UpdateIntBoundsForGreaterThanOrEqual(
    Value *const value,
    const IntConstantBounds &constantBounds,
    Value *const boundValue,
    const IntConstantBounds &boundConstantBounds,
    const int boundOffset,
    const bool isExplicit)
{LOGMEIN("GlobOptIntBounds.cpp] 922\n");
    Assert(value || constantBounds.IsConstant());
    Assert(boundValue || boundConstantBounds.IsConstant());
    if(!value)
    {LOGMEIN("GlobOptIntBounds.cpp] 926\n");
        return nullptr;
    }
    Assert(!boundValue || value->GetValueNumber() != boundValue->GetValueNumber());

    ValueInfo *const valueInfo = value->GetValueInfo();
    IntBounds *const bounds =
        GetIntBoundsToUpdate(valueInfo, constantBounds, true, boundConstantBounds.IsConstant(), false, isExplicit);
    if(bounds)
    {LOGMEIN("GlobOptIntBounds.cpp] 935\n");
        if(boundValue)
        {LOGMEIN("GlobOptIntBounds.cpp] 937\n");
            bounds->SetLowerBound(value->GetValueNumber(), boundValue, boundOffset, isExplicit);
        }
        else
        {
            bounds->SetLowerBound(boundConstantBounds.LowerBound(), boundOffset);
        }
        if(bounds->RequiresIntBoundedValueInfo(valueInfo->Type()))
        {LOGMEIN("GlobOptIntBounds.cpp] 945\n");
            return NewIntBoundedValueInfo(valueInfo, bounds);
        }
        bounds->Delete();
    }

    if(!valueInfo->IsInt())
    {LOGMEIN("GlobOptIntBounds.cpp] 952\n");
        return nullptr;
    }

    int32 adjustedBoundMin;
    if(boundOffset == 0)
    {LOGMEIN("GlobOptIntBounds.cpp] 958\n");
        adjustedBoundMin = boundConstantBounds.LowerBound();
    }
    else if(boundOffset == 1)
    {LOGMEIN("GlobOptIntBounds.cpp] 962\n");
        if(boundConstantBounds.LowerBound() + 1 <= boundConstantBounds.LowerBound())
        {LOGMEIN("GlobOptIntBounds.cpp] 964\n");
            return nullptr;
        }
        adjustedBoundMin = boundConstantBounds.LowerBound() + 1;
    }
    else if(Int32Math::Add(boundConstantBounds.LowerBound(), boundOffset, &adjustedBoundMin))
    {LOGMEIN("GlobOptIntBounds.cpp] 970\n");
        return nullptr;
    }
    const int32 newMin = max(constantBounds.LowerBound(), adjustedBoundMin);
    return
        newMin <= constantBounds.UpperBound()
            ? NewIntRangeValueInfo(valueInfo, newMin, constantBounds.UpperBound())
            : nullptr;
}

ValueInfo *GlobOpt::UpdateIntBoundsForGreaterThan(
    Value *const value,
    const IntConstantBounds &constantBounds,
    Value *const boundValue,
    const IntConstantBounds &boundConstantBounds,
    const bool isExplicit)
{LOGMEIN("GlobOptIntBounds.cpp] 986\n");
    return UpdateIntBoundsForGreaterThanOrEqual(value, constantBounds, boundValue, boundConstantBounds, 1, isExplicit);
}

ValueInfo *GlobOpt::UpdateIntBoundsForLessThanOrEqual(
    Value *const value,
    const IntConstantBounds &constantBounds,
    Value *const boundValue,
    const IntConstantBounds &boundConstantBounds,
    const bool isExplicit)
{LOGMEIN("GlobOptIntBounds.cpp] 996\n");
    return UpdateIntBoundsForLessThanOrEqual(value, constantBounds, boundValue, boundConstantBounds, 0, isExplicit);
}

ValueInfo *GlobOpt::UpdateIntBoundsForLessThanOrEqual(
    Value *const value,
    const IntConstantBounds &constantBounds,
    Value *const boundValue,
    const IntConstantBounds &boundConstantBounds,
    const int boundOffset,
    const bool isExplicit)
{LOGMEIN("GlobOptIntBounds.cpp] 1007\n");
    Assert(value || constantBounds.IsConstant());
    Assert(boundValue || boundConstantBounds.IsConstant());
    if(!value)
    {LOGMEIN("GlobOptIntBounds.cpp] 1011\n");
        return nullptr;
    }
    Assert(!boundValue || value->GetValueNumber() != boundValue->GetValueNumber());

    ValueInfo *const valueInfo = value->GetValueInfo();
    IntBounds *const bounds =
        GetIntBoundsToUpdate(valueInfo, constantBounds, true, boundConstantBounds.IsConstant(), true, isExplicit);
    if(bounds)
    {LOGMEIN("GlobOptIntBounds.cpp] 1020\n");
        if(boundValue)
        {LOGMEIN("GlobOptIntBounds.cpp] 1022\n");
            bounds->SetUpperBound(value->GetValueNumber(), boundValue, boundOffset, isExplicit);
        }
        else
        {
            bounds->SetUpperBound(boundConstantBounds.LowerBound(), boundOffset, isExplicit);
        }
        if(bounds->RequiresIntBoundedValueInfo(valueInfo->Type()))
        {LOGMEIN("GlobOptIntBounds.cpp] 1030\n");
            return NewIntBoundedValueInfo(valueInfo, bounds);
        }
        bounds->Delete();
    }

    if(!valueInfo->IsInt())
    {LOGMEIN("GlobOptIntBounds.cpp] 1037\n");
        return nullptr;
    }

    int32 adjustedBoundMax;
    if(boundOffset == 0)
    {LOGMEIN("GlobOptIntBounds.cpp] 1043\n");
        adjustedBoundMax = boundConstantBounds.UpperBound();
    }
    else if(boundOffset == -1)
    {LOGMEIN("GlobOptIntBounds.cpp] 1047\n");
        if(boundConstantBounds.UpperBound() - 1 >= boundConstantBounds.UpperBound())
        {LOGMEIN("GlobOptIntBounds.cpp] 1049\n");
            return nullptr;
        }
        adjustedBoundMax = boundConstantBounds.UpperBound() - 1;
    }
    else if(Int32Math::Add(boundConstantBounds.UpperBound(), boundOffset, &adjustedBoundMax))
    {LOGMEIN("GlobOptIntBounds.cpp] 1055\n");
        return nullptr;
    }
    const int32 newMax = min(constantBounds.UpperBound(), adjustedBoundMax);
    return
        newMax >= constantBounds.LowerBound()
            ? NewIntRangeValueInfo(valueInfo, constantBounds.LowerBound(), newMax)
            : nullptr;
}

ValueInfo *GlobOpt::UpdateIntBoundsForLessThan(
    Value *const value,
    const IntConstantBounds &constantBounds,
    Value *const boundValue,
    const IntConstantBounds &boundConstantBounds,
    const bool isExplicit)
{LOGMEIN("GlobOptIntBounds.cpp] 1071\n");
    return UpdateIntBoundsForLessThanOrEqual(value, constantBounds, boundValue, boundConstantBounds, -1, isExplicit);
}

void GlobOpt::TrackIntSpecializedAddSubConstant(
    IR::Instr *const instr,
    const AddSubConstantInfo *const addSubConstantInfo,
    Value *const dstValue,
    const bool updateSourceBounds)
{LOGMEIN("GlobOptIntBounds.cpp] 1080\n");
    Assert(instr);
    Assert(dstValue);

    if(addSubConstantInfo)
    {LOGMEIN("GlobOptIntBounds.cpp] 1085\n");
        Assert(addSubConstantInfo->HasInfo());
        Assert(!ignoredIntOverflowForCurrentInstr);
        do
        {LOGMEIN("GlobOptIntBounds.cpp] 1089\n");
            if(!IsLoopPrePass() || !DoBoundCheckHoist())
            {LOGMEIN("GlobOptIntBounds.cpp] 1091\n");
                break;
            }

            Assert(
                instr->m_opcode == Js::OpCode::Incr_A ||
                instr->m_opcode == Js::OpCode::Decr_A ||
                instr->m_opcode == Js::OpCode::Add_A ||
                instr->m_opcode == Js::OpCode::Sub_A);

            StackSym *sym = instr->GetDst()->AsRegOpnd()->m_sym;
            bool isPostfixIncDecPattern = false;
            if(addSubConstantInfo->SrcSym() != sym)
            {LOGMEIN("GlobOptIntBounds.cpp] 1104\n");
                // Check for the following patterns.
                //
                // This pattern is used for postfix inc/dec operators:
                //     s2 = Conv_Num s1
                //     s1 = Inc s2
                //
                // This pattern is used for prefix inc/dec operators:
                //     s2 = Inc s1
                //     s1 = Ld s2
                IR::Instr *const prevInstr = instr->m_prev;
                Assert(prevInstr);
                if(prevInstr->m_opcode == Js::OpCode::Conv_Num &&
                    prevInstr->GetSrc1()->IsRegOpnd() &&
                    prevInstr->GetSrc1()->AsRegOpnd()->m_sym == sym &&
                    prevInstr->GetDst()->AsRegOpnd()->m_sym == addSubConstantInfo->SrcSym())
                {LOGMEIN("GlobOptIntBounds.cpp] 1120\n");
                    // s2 will get a new value number, since Conv_Num cannot transfer in the prepass. For the purposes of
                    // induction variable tracking however, it doesn't matter, so record this case and use s1's value in the
                    // current block.
                    isPostfixIncDecPattern = true;
                }
                else
                {
                    IR::Instr *const nextInstr = instr->m_next;
                    Assert(nextInstr);
                    if(nextInstr->m_opcode != Js::OpCode::Ld_A ||
                        !nextInstr->GetSrc1()->IsRegOpnd() ||
                        nextInstr->GetSrc1()->AsRegOpnd()->m_sym != sym)
                    {LOGMEIN("GlobOptIntBounds.cpp] 1133\n");
                        break;
                    }
                    sym = addSubConstantInfo->SrcSym();
                    if(nextInstr->GetDst()->AsRegOpnd()->m_sym != sym)
                    {LOGMEIN("GlobOptIntBounds.cpp] 1138\n");
                        break;
                    }

                    // In the prefix inc/dec pattern, the result of Ld currently gets a new value number, which will cause the
                    // induction variable info to become indeterminate. Indicate that the value number should be updated in the
                    // induction variable info.
                    // Consider: Remove this once loop prepass value transfer scheme is fixed
                    updateInductionVariableValueNumber = true;
                }
            }

            // Track induction variable info
            ValueNumber srcValueNumber;
            if(isPostfixIncDecPattern)
            {LOGMEIN("GlobOptIntBounds.cpp] 1153\n");
                Value *const value = FindValue(sym);
                Assert(value);
                srcValueNumber = value->GetValueNumber();
            }
            else
            {
                srcValueNumber = addSubConstantInfo->SrcValue()->GetValueNumber();
            }
            InductionVariableSet *const inductionVariables = blockData.inductionVariables;
            Assert(inductionVariables);
            InductionVariable *inductionVariable;
            if(!inductionVariables->TryGetReference(sym->m_id, &inductionVariable))
            {LOGMEIN("GlobOptIntBounds.cpp] 1166\n");
                // Only track changes in the current loop's prepass. In subsequent prepasses, the info is only being propagated
                // for use by the parent loop, so changes in the current loop have already been tracked.
                if(prePassLoop != currentBlock->loop)
                {LOGMEIN("GlobOptIntBounds.cpp] 1170\n");
                    updateInductionVariableValueNumber = false;
                    break;
                }

                // Ensure that the sym is live in the landing pad, and that its value has not changed in an unknown way yet
                Value *const landingPadValue = FindValue(currentBlock->loop->landingPad->globOptData.symToValueMap, sym);
                if(!landingPadValue || srcValueNumber != landingPadValue->GetValueNumber())
                {LOGMEIN("GlobOptIntBounds.cpp] 1178\n");
                    updateInductionVariableValueNumber = false;
                    break;
                }
                inductionVariables->Add(
                    InductionVariable(sym, dstValue->GetValueNumber(), addSubConstantInfo->Offset()));
                break;
            }

            if(!inductionVariable->IsChangeDeterminate())
            {LOGMEIN("GlobOptIntBounds.cpp] 1188\n");
                updateInductionVariableValueNumber = false;
                break;
            }

            if(srcValueNumber != inductionVariable->SymValueNumber())
            {LOGMEIN("GlobOptIntBounds.cpp] 1194\n");
                // The sym's value has changed since the last time induction variable info was recorded for it. Due to the
                // unknown change, mark the info as indeterminate.
                inductionVariable->SetChangeIsIndeterminate();
                updateInductionVariableValueNumber = false;
                break;
            }

            // Only track changes in the current loop's prepass. In subsequent prepasses, the info is only being propagated for
            // use by the parent loop, so changes in the current loop have already been tracked. Induction variable value
            // numbers are updated as changes occur, but their change bounds are preserved from the first prepass over the loop.
            inductionVariable->SetSymValueNumber(dstValue->GetValueNumber());
            if(prePassLoop != currentBlock->loop)
            {LOGMEIN("GlobOptIntBounds.cpp] 1207\n");
                break;
            }

            if(!inductionVariable->Add(addSubConstantInfo->Offset()))
            {LOGMEIN("GlobOptIntBounds.cpp] 1212\n");
                updateInductionVariableValueNumber = false;
            }
        } while(false);

        if(updateSourceBounds && addSubConstantInfo->Offset() != IntConstMin)
        {LOGMEIN("GlobOptIntBounds.cpp] 1218\n");
            // Track bounds for add or sub with a constant. For instance, consider (b = a + 2). The value of 'b' should track
            // that it is equal to (the value of 'a') + 2. That part has been done above. Similarly, the value of 'a' should
            // also track that it is equal to (the value of 'b') - 2.
            Value *const value = addSubConstantInfo->SrcValue();
            const ValueInfo *const valueInfo = value->GetValueInfo();
            Assert(valueInfo->IsInt());
            IntConstantBounds constantBounds;
            AssertVerify(valueInfo->TryGetIntConstantBounds(&constantBounds));
            IntBounds *const bounds =
                GetIntBoundsToUpdate(
                    valueInfo,
                    constantBounds,
                    true,
                    dstValue->GetValueInfo()->HasIntConstantValue(),
                    true,
                    true);
            if(bounds)
            {LOGMEIN("GlobOptIntBounds.cpp] 1236\n");
                const ValueNumber valueNumber = value->GetValueNumber();
                const int32 dstOffset = -addSubConstantInfo->Offset();
                bounds->SetLowerBound(valueNumber, dstValue, dstOffset, true);
                bounds->SetUpperBound(valueNumber, dstValue, dstOffset, true);
                ChangeValueInfo(nullptr, value, NewIntBoundedValueInfo(valueInfo, bounds));
            }
        }
        return;
    }

    if(!updateInductionVariableValueNumber)
    {LOGMEIN("GlobOptIntBounds.cpp] 1248\n");
        return;
    }

    // See comment above where this is set to true
    // Consider: Remove this once loop prepass value transfer scheme is fixed
    updateInductionVariableValueNumber = false;

    Assert(IsLoopPrePass());
    Assert(instr->m_opcode == Js::OpCode::Ld_A);
    Assert(
        instr->m_prev->m_opcode == Js::OpCode::Incr_A ||
        instr->m_prev->m_opcode == Js::OpCode::Decr_A ||
        instr->m_prev->m_opcode == Js::OpCode::Add_A ||
        instr->m_prev->m_opcode == Js::OpCode::Sub_A);
    Assert(instr->m_prev->GetDst()->AsRegOpnd()->m_sym == instr->GetSrc1()->AsRegOpnd()->m_sym);

    InductionVariable *inductionVariable;
    AssertVerify(blockData.inductionVariables->TryGetReference(instr->GetDst()->AsRegOpnd()->m_sym->m_id, &inductionVariable));
    inductionVariable->SetSymValueNumber(dstValue->GetValueNumber());
}

void GlobOpt::CloneBoundCheckHoistBlockData(
    BasicBlock *const toBlock,
    GlobOptBlockData *const toData,
    BasicBlock *const fromBlock,
    GlobOptBlockData *const fromData)
{LOGMEIN("GlobOptIntBounds.cpp] 1275\n");
    Assert(DoBoundCheckHoist());
    Assert(toBlock);
    Assert(toData);
    Assert(toData == &toBlock->globOptData || toData == &blockData);
    Assert(fromBlock);
    Assert(fromData);
    Assert(fromData == &fromBlock->globOptData);

    Assert(fromData->availableIntBoundChecks);
    toData->availableIntBoundChecks = fromData->availableIntBoundChecks->Clone();

    if(toBlock->isLoopHeader)
    {LOGMEIN("GlobOptIntBounds.cpp] 1288\n");
        Assert(fromBlock == toBlock->loop->landingPad);

        if(prePassLoop == toBlock->loop)
        {LOGMEIN("GlobOptIntBounds.cpp] 1292\n");
            // When the current prepass loop is the current loop, the loop header's induction variable set needs to start off
            // empty to track changes in the current loop
            toData->inductionVariables = JitAnew(alloc, InductionVariableSet, alloc);
            return;
        }

        if(!IsLoopPrePass())
        {LOGMEIN("GlobOptIntBounds.cpp] 1300\n");
            return;
        }

        // After the prepass on this loop, if we're still in a prepass, this must be an inner loop. Merge the landing pad info
        // for use by the parent loop.
        Assert(fromBlock->loop);
        Assert(fromData->inductionVariables);
        toData->inductionVariables = fromData->inductionVariables->Clone();
        return;
    }

    if(!toBlock->loop || !IsLoopPrePass())
    {LOGMEIN("GlobOptIntBounds.cpp] 1313\n");
        return;
    }

    Assert(fromBlock->loop);
    Assert(toBlock->loop->IsDescendentOrSelf(fromBlock->loop));
    Assert(fromData->inductionVariables);
    toData->inductionVariables = fromData->inductionVariables->Clone();
}

void GlobOpt::MergeBoundCheckHoistBlockData(
    BasicBlock *const toBlock,
    GlobOptBlockData *const toData,
    BasicBlock *const fromBlock,
    GlobOptBlockData *const fromData)
{LOGMEIN("GlobOptIntBounds.cpp] 1328\n");
    Assert(DoBoundCheckHoist());
    Assert(toBlock);
    Assert(toData);
    Assert(toData == &toBlock->globOptData || toData == &blockData);
    Assert(fromBlock);
    Assert(fromData);
    Assert(fromData == &fromBlock->globOptData);
    Assert(toData->availableIntBoundChecks);

    for(auto it = toData->availableIntBoundChecks->GetIteratorWithRemovalSupport(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOptIntBounds.cpp] 1339\n");
        const IntBoundCheck &toDataIntBoundCheck = it.CurrentValue();
        const IntBoundCheck *fromDataIntBoundCheck;
        if(!fromData->availableIntBoundChecks->TryGetReference(
                toDataIntBoundCheck.CompatibilityId(),
                &fromDataIntBoundCheck) ||
            fromDataIntBoundCheck->Instr() != toDataIntBoundCheck.Instr())
        {LOGMEIN("GlobOptIntBounds.cpp] 1346\n");
            it.RemoveCurrent();
        }
    }

    InductionVariableSet *mergeInductionVariablesInto;
    if(toBlock->isLoopHeader)
    {LOGMEIN("GlobOptIntBounds.cpp] 1353\n");
        Assert(fromBlock->loop == toBlock->loop); // The flow is such that you cannot have back-edges from an inner loop

        if(IsLoopPrePass())
        {LOGMEIN("GlobOptIntBounds.cpp] 1357\n");
            // Collect info for the parent loop. Any changes to induction variables in this inner loop need to be expanded in
            // the same direction for the parent loop, so merge expanded info from back-edges. Info about induction variables
            // that changed before the loop but not inside the loop, can be kept intact because the landing pad dominates the
            // loop.
            Assert(prePassLoop != toBlock->loop);
            Assert(fromData->inductionVariables);
            Assert(toData->inductionVariables);

            InductionVariableSet *const mergedInductionVariables = toData->inductionVariables;
            for(auto it = fromData->inductionVariables->GetIterator(); it.IsValid(); it.MoveNext())
            {LOGMEIN("GlobOptIntBounds.cpp] 1368\n");
                InductionVariable backEdgeInductionVariable = it.CurrentValue();
                backEdgeInductionVariable.ExpandInnerLoopChange();
                StackSym *const sym = backEdgeInductionVariable.Sym();
                InductionVariable *mergedInductionVariable;
                if(mergedInductionVariables->TryGetReference(sym->m_id, &mergedInductionVariable))
                {LOGMEIN("GlobOptIntBounds.cpp] 1374\n");
                    mergedInductionVariable->Merge(backEdgeInductionVariable);
                    continue;
                }

                // Ensure that the sym is live in the parent loop's landing pad, and that its value has not changed in an
                // unknown way between the parent loop's landing pad and the current loop's landing pad.
                Value *const parentLandingPadValue =
                    FindValue(currentBlock->loop->parent->landingPad->globOptData.symToValueMap, sym);
                if(!parentLandingPadValue)
                {LOGMEIN("GlobOptIntBounds.cpp] 1384\n");
                    continue;
                }
                Value *const landingPadValue = FindValue(currentBlock->loop->landingPad->globOptData.symToValueMap, sym);
                Assert(landingPadValue);
                if(landingPadValue->GetValueNumber() == parentLandingPadValue->GetValueNumber())
                {LOGMEIN("GlobOptIntBounds.cpp] 1390\n");
                    mergedInductionVariables->Add(backEdgeInductionVariable);
                }
            }
            return;
        }

        // Collect info for the current loop. We want to merge only the back-edge info without the landing pad info, such that
        // the loop's induction variable set reflects changes made inside this loop.
        Assert(fromData->inductionVariables);
        InductionVariableSet *&loopInductionVariables = toBlock->loop->inductionVariables;
        if(!loopInductionVariables)
        {LOGMEIN("GlobOptIntBounds.cpp] 1402\n");
            loopInductionVariables = fromData->inductionVariables->Clone();
            return;
        }
        mergeInductionVariablesInto = loopInductionVariables;
    }
    else if(toBlock->loop && IsLoopPrePass())
    {LOGMEIN("GlobOptIntBounds.cpp] 1409\n");
        Assert(fromBlock->loop);
        Assert(toBlock->loop->IsDescendentOrSelf(fromBlock->loop));
        mergeInductionVariablesInto = toData->inductionVariables;
    }
    else
    {
        return;
    }

    const InductionVariableSet *const fromDataInductionVariables = fromData->inductionVariables;
    InductionVariableSet *const mergedInductionVariables = mergeInductionVariablesInto;

    Assert(fromDataInductionVariables);
    Assert(mergedInductionVariables);

    for(auto it = mergedInductionVariables->GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOptIntBounds.cpp] 1426\n");
        InductionVariable &mergedInductionVariable = it.CurrentValueReference();
        if(!mergedInductionVariable.IsChangeDeterminate())
        {LOGMEIN("GlobOptIntBounds.cpp] 1429\n");
            continue;
        }

        StackSym *const sym = mergedInductionVariable.Sym();
        const InductionVariable *fromDataInductionVariable;
        if(fromDataInductionVariables->TryGetReference(sym->m_id, &fromDataInductionVariable))
        {LOGMEIN("GlobOptIntBounds.cpp] 1436\n");
            mergedInductionVariable.Merge(*fromDataInductionVariable);
            continue;
        }

        // Ensure that the sym is live in the landing pad, and that its value has not changed in an unknown way yet on the path
        // where the sym is not already marked as an induction variable.
        Value *const fromDataValue = FindValue(fromData->symToValueMap, sym);
        if(fromDataValue)
        {LOGMEIN("GlobOptIntBounds.cpp] 1445\n");
            Value *const landingPadValue = FindValue(toBlock->loop->landingPad->globOptData.symToValueMap, sym);
            if(landingPadValue && fromDataValue->GetValueNumber() == landingPadValue->GetValueNumber())
            {LOGMEIN("GlobOptIntBounds.cpp] 1448\n");
                mergedInductionVariable.Merge(InductionVariable(sym, ZeroValueNumber, 0));
                continue;
            }
        }
        mergedInductionVariable.SetChangeIsIndeterminate();
    }

    for(auto it = fromDataInductionVariables->GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOptIntBounds.cpp] 1457\n");
        const InductionVariable &fromDataInductionVariable = it.CurrentValue();
        StackSym *const sym = fromDataInductionVariable.Sym();
        if(mergedInductionVariables->ContainsKey(sym->m_id))
        {LOGMEIN("GlobOptIntBounds.cpp] 1461\n");
            continue;
        }

        // Ensure that the sym is live in the landing pad, and that its value has not changed in an unknown way yet on the path
        // where the sym is not already marked as an induction variable.
        bool indeterminate = true;
        Value *const toDataValue = FindValue(toData->symToValueMap, sym);
        if(toDataValue)
        {LOGMEIN("GlobOptIntBounds.cpp] 1470\n");
            Value *const landingPadValue = FindValue(toBlock->loop->landingPad->globOptData.symToValueMap, sym);
            if(landingPadValue && toDataValue->GetValueNumber() == landingPadValue->GetValueNumber())
            {LOGMEIN("GlobOptIntBounds.cpp] 1473\n");
                indeterminate = false;
            }
        }
        InductionVariable mergedInductionVariable(sym, ZeroValueNumber, 0);
        if(indeterminate)
        {LOGMEIN("GlobOptIntBounds.cpp] 1479\n");
            mergedInductionVariable.SetChangeIsIndeterminate();
        }
        else
        {
            mergedInductionVariable.Merge(fromDataInductionVariable);
        }
        mergedInductionVariables->Add(mergedInductionVariable);
    }
}

void GlobOpt::DetectUnknownChangesToInductionVariables(GlobOptBlockData *const blockData)
{LOGMEIN("GlobOptIntBounds.cpp] 1491\n");
    Assert(DoBoundCheckHoist());
    Assert(IsLoopPrePass());
    Assert(blockData);
    Assert(blockData->inductionVariables);

    // Check induction variable value numbers, and mark those that changed in an unknown way as indeterminate. They must remain
    // in the set though, for merging purposes.
    GlobHashTable *const symToValueMap = blockData->symToValueMap;
    for(auto it = blockData->inductionVariables->GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOptIntBounds.cpp] 1501\n");
        InductionVariable &inductionVariable = it.CurrentValueReference();
        if(!inductionVariable.IsChangeDeterminate())
        {LOGMEIN("GlobOptIntBounds.cpp] 1504\n");
            continue;
        }

        Value *const value = FindValue(symToValueMap, inductionVariable.Sym());
        if(!value || value->GetValueNumber() != inductionVariable.SymValueNumber())
        {LOGMEIN("GlobOptIntBounds.cpp] 1510\n");
            inductionVariable.SetChangeIsIndeterminate();
        }
    }
}

void GlobOpt::SetInductionVariableValueNumbers(GlobOptBlockData *const blockData)
{LOGMEIN("GlobOptIntBounds.cpp] 1517\n");
    Assert(DoBoundCheckHoist());
    Assert(IsLoopPrePass());
    Assert(blockData == &this->blockData);
    Assert(blockData->inductionVariables);

    // Now that all values have been merged, update value numbers in the induction variable info.
    GlobHashTable *const symToValueMap = blockData->symToValueMap;
    for(auto it = blockData->inductionVariables->GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOptIntBounds.cpp] 1526\n");
        InductionVariable &inductionVariable = it.CurrentValueReference();
        if(!inductionVariable.IsChangeDeterminate())
        {LOGMEIN("GlobOptIntBounds.cpp] 1529\n");
            continue;
        }

        Value *const value = FindValue(symToValueMap, inductionVariable.Sym());
        if(value)
        {LOGMEIN("GlobOptIntBounds.cpp] 1535\n");
            inductionVariable.SetSymValueNumber(value->GetValueNumber());
        }
        else
        {
            inductionVariable.SetChangeIsIndeterminate();
        }
    }
}

void GlobOpt::FinalizeInductionVariables(Loop *const loop, GlobOptBlockData *const headerData)
{LOGMEIN("GlobOptIntBounds.cpp] 1546\n");
    Assert(DoBoundCheckHoist());
    Assert(!IsLoopPrePass());
    Assert(loop);
    Assert(loop->GetHeadBlock() == currentBlock);
    Assert(loop->inductionVariables);
    Assert(currentBlock->isLoopHeader);
    Assert(headerData == &this->blockData);

    // Clean up induction variables and for each, install a relationship between its values inside and outside the loop.
    GlobHashTable *const symToValueMap = headerData->symToValueMap;
    GlobOptBlockData &landingPadBlockData = loop->landingPad->globOptData;
    GlobHashTable *const landingPadSymToValueMap = landingPadBlockData.symToValueMap;
    for(auto it = loop->inductionVariables->GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOptIntBounds.cpp] 1560\n");
        InductionVariable &inductionVariable = it.CurrentValueReference();
        if(!inductionVariable.IsChangeDeterminate())
        {LOGMEIN("GlobOptIntBounds.cpp] 1563\n");
            continue;
        }
        if(!inductionVariable.IsChangeUnidirectional())
        {LOGMEIN("GlobOptIntBounds.cpp] 1567\n");
            inductionVariable.SetChangeIsIndeterminate();
            continue;
        }

        StackSym *const sym = inductionVariable.Sym();
        if(!IsInt32TypeSpecialized(sym, headerData))
        {LOGMEIN("GlobOptIntBounds.cpp] 1574\n");
            inductionVariable.SetChangeIsIndeterminate();
            continue;
        }
        Assert(IsInt32TypeSpecialized(sym, &landingPadBlockData));

        Value *const value = FindValue(symToValueMap, sym);
        if(!value)
        {LOGMEIN("GlobOptIntBounds.cpp] 1582\n");
            inductionVariable.SetChangeIsIndeterminate();
            continue;
        }
        Value *const landingPadValue = FindValue(landingPadSymToValueMap, sym);
        Assert(landingPadValue);

        IntConstantBounds constantBounds, landingPadConstantBounds;
        AssertVerify(value->GetValueInfo()->TryGetIntConstantBounds(&constantBounds));
        AssertVerify(landingPadValue->GetValueInfo()->TryGetIntConstantBounds(&landingPadConstantBounds, true));

        // For an induction variable i, update the value of i inside the loop to indicate that it is bounded by the value of i
        // just before the loop.
        if(inductionVariable.ChangeBounds().LowerBound() >= 0)
        {LOGMEIN("GlobOptIntBounds.cpp] 1596\n");
            ValueInfo *const newValueInfo =
                UpdateIntBoundsForGreaterThanOrEqual(value, constantBounds, landingPadValue, landingPadConstantBounds, true);
            ChangeValueInfo(nullptr, value, newValueInfo);
            if(inductionVariable.ChangeBounds().UpperBound() == 0)
            {LOGMEIN("GlobOptIntBounds.cpp] 1601\n");
                AssertVerify(newValueInfo->TryGetIntConstantBounds(&constantBounds, true));
            }
        }
        if(inductionVariable.ChangeBounds().UpperBound() <= 0)
        {LOGMEIN("GlobOptIntBounds.cpp] 1606\n");
            ValueInfo *const newValueInfo =
                UpdateIntBoundsForLessThanOrEqual(value, constantBounds, landingPadValue, landingPadConstantBounds, true);
            ChangeValueInfo(nullptr, value, newValueInfo);
        }
    }
}

bool GlobOpt::DetermineSymBoundOffsetOrValueRelativeToLandingPad(
    StackSym *const sym,
    const bool landingPadValueIsLowerBound,
    ValueInfo *const valueInfo,
    const IntBounds *const bounds,
    GlobHashTable *const landingPadSymToValueMap,
    int *const boundOffsetOrValueRef)
{LOGMEIN("GlobOptIntBounds.cpp] 1621\n");
    Assert(sym);
    Assert(!sym->IsTypeSpec());
    Assert(valueInfo);
    Assert(landingPadSymToValueMap);
    Assert(boundOffsetOrValueRef);
    Assert(valueInfo->IsInt());

    int constantValue;
    if(valueInfo->TryGetIntConstantValue(&constantValue))
    {LOGMEIN("GlobOptIntBounds.cpp] 1631\n");
        // The sym's constant value is the constant bound value, so just return that. This is possible in loops such as
        // for(; i === 1; ++i){...}, where 'i' is an induction variable but has a constant value inside the loop, or in blocks
        // inside the loop such as if(i === 1){...}
        *boundOffsetOrValueRef = constantValue;
        return true; // 'true' indicates that *boundOffsetOrValueRef contains the constant bound value
    }

    Value *const landingPadValue = FindValue(landingPadSymToValueMap, sym);
    Assert(landingPadValue);
    Assert(landingPadValue->GetValueInfo()->IsInt());
    int landingPadConstantValue;
    if(landingPadValue->GetValueInfo()->TryGetIntConstantValue(&landingPadConstantValue))
    {LOGMEIN("GlobOptIntBounds.cpp] 1644\n");
        // The sym's bound already takes the landing pad constant value into consideration, unless the landing pad value was
        // updated to have a more aggressive range (and hence, now a constant value) as part of hoisting a bound check or some
        // other hoisting operation. The sym's bound also takes into consideration the change to the sym so far inside the loop,
        // and the landing pad constant value does not, so use the sym's bound by default.

        int constantBound;
        if(bounds)
        {LOGMEIN("GlobOptIntBounds.cpp] 1652\n");
            constantBound = landingPadValueIsLowerBound ? bounds->ConstantLowerBound() : bounds->ConstantUpperBound();
        }
        else
        {
            AssertVerify(
                landingPadValueIsLowerBound
                    ? valueInfo->TryGetIntConstantLowerBound(&constantBound)
                    : valueInfo->TryGetIntConstantUpperBound(&constantBound));
        }

        if(landingPadValueIsLowerBound ? landingPadConstantValue > constantBound : landingPadConstantValue < constantBound)
        {LOGMEIN("GlobOptIntBounds.cpp] 1664\n");
            // The landing pad value became a constant value as part of a hoisting operation. The landing pad constant value is
            // a more aggressive bound, so use that instead, and take into consideration the change to the sym so far inside the
            // loop, using the relative bound to the landing pad value.
            AnalysisAssert(bounds);
            const ValueRelativeOffset *bound;
            AssertVerify(
                (landingPadValueIsLowerBound ? bounds->RelativeLowerBounds() : bounds->RelativeUpperBounds())
                    .TryGetReference(landingPadValue->GetValueNumber(), &bound));
            constantBound = landingPadConstantValue + bound->Offset();
        }

        *boundOffsetOrValueRef = constantBound;
        return true; // 'true' indicates that *boundOffsetOrValueRef contains the constant bound value
    }

    AnalysisAssert(bounds);
    const ValueRelativeOffset *bound;
    AssertVerify(
        (landingPadValueIsLowerBound ? bounds->RelativeLowerBounds() : bounds->RelativeUpperBounds())
            .TryGetReference(landingPadValue->GetValueNumber(), &bound));
    *boundOffsetOrValueRef = bound->Offset();
    // 'false' indicates that *boundOffsetOrValueRef contains the bound offset, which must be added to the sym's value in the
    // landing pad to get the bound value
    return false;
}

void GlobOpt::DetermineDominatingLoopCountableBlock(Loop *const loop, BasicBlock *const headerBlock)
{LOGMEIN("GlobOptIntBounds.cpp] 1692\n");
    Assert(DoLoopCountBasedBoundCheckHoist());
    Assert(!IsLoopPrePass());
    Assert(loop);
    Assert(headerBlock);
    Assert(headerBlock->isLoopHeader);
    Assert(headerBlock->loop == loop);

    // Determine if the loop header has a unique successor that is inside the loop. If so, then all other paths out of the loop
    // header exit the loop, allowing a loop count to be established and used from the unique in-loop successor block.
    Assert(!loop->dominatingLoopCountableBlock);
    FOREACH_SUCCESSOR_BLOCK(successor, headerBlock)
    {LOGMEIN("GlobOptIntBounds.cpp] 1704\n");
        if(successor->loop != loop)
        {LOGMEIN("GlobOptIntBounds.cpp] 1706\n");
            Assert(!successor->loop || successor->loop->IsDescendentOrSelf(loop->parent));
            continue;
        }

        if(loop->dominatingLoopCountableBlock)
        {LOGMEIN("GlobOptIntBounds.cpp] 1712\n");
            // Found a second successor inside the loop
            loop->dominatingLoopCountableBlock = nullptr;
            break;
        }

        loop->dominatingLoopCountableBlock = successor;
    } NEXT_SUCCESSOR_BLOCK;
}

void GlobOpt::DetermineLoopCount(Loop *const loop)
{LOGMEIN("GlobOptIntBounds.cpp] 1723\n");
    Assert(DoLoopCountBasedBoundCheckHoist());
    Assert(loop);

    GlobOptBlockData &landingPadBlockData = loop->landingPad->globOptData;
    GlobHashTable *const landingPadSymToValueMap = landingPadBlockData.symToValueMap;
    const InductionVariableSet *const inductionVariables = loop->inductionVariables;
    Assert(inductionVariables);
    for(auto inductionVariablesIterator = inductionVariables->GetIterator(); inductionVariablesIterator.IsValid(); inductionVariablesIterator.MoveNext())
    {LOGMEIN("GlobOptIntBounds.cpp] 1732\n");
        InductionVariable &inductionVariable = inductionVariablesIterator.CurrentValueReference();
        if(!inductionVariable.IsChangeDeterminate())
        {LOGMEIN("GlobOptIntBounds.cpp] 1735\n");
            continue;
        }

        // Determine the minimum-magnitude change per iteration, and verify that the change is nonzero and finite
        Assert(inductionVariable.IsChangeUnidirectional());
        int minMagnitudeChange = inductionVariable.ChangeBounds().LowerBound();
        if(minMagnitudeChange >= 0)
        {LOGMEIN("GlobOptIntBounds.cpp] 1743\n");
            if(minMagnitudeChange == 0 || minMagnitudeChange == IntConstMax)
            {LOGMEIN("GlobOptIntBounds.cpp] 1745\n");
                continue;
            }
        }
        else
        {
            minMagnitudeChange = inductionVariable.ChangeBounds().UpperBound();
            Assert(minMagnitudeChange <= 0);
            if(minMagnitudeChange == 0 || minMagnitudeChange == IntConstMin)
            {LOGMEIN("GlobOptIntBounds.cpp] 1754\n");
                continue;
            }
        }

        StackSym *const inductionVariableVarSym = inductionVariable.Sym();
        if(!IsInt32TypeSpecialized(inductionVariableVarSym, &blockData))
        {LOGMEIN("GlobOptIntBounds.cpp] 1761\n");
            inductionVariable.SetChangeIsIndeterminate();
            continue;
        }
        Assert(IsInt32TypeSpecialized(inductionVariableVarSym, &landingPadBlockData));

        Value *const inductionVariableValue = FindValue(inductionVariableVarSym);
        if(!inductionVariableValue)
        {LOGMEIN("GlobOptIntBounds.cpp] 1769\n");
            inductionVariable.SetChangeIsIndeterminate();
            continue;
        }

        ValueInfo *const inductionVariableValueInfo = inductionVariableValue->GetValueInfo();
        const IntBounds *const inductionVariableBounds =
            inductionVariableValueInfo->IsIntBounded() ? inductionVariableValueInfo->AsIntBounded()->Bounds() : nullptr;

        // Look for an invariant bound in the direction of change
        StackSym *boundBaseVarSym = nullptr;
        int boundOffset = 0;
        {
            bool foundBound = false;
            if(inductionVariableBounds)
            {LOGMEIN("GlobOptIntBounds.cpp] 1784\n");
                // Look for a relative bound
                for(auto it =
                        (
                            minMagnitudeChange >= 0
                                ? inductionVariableBounds->RelativeUpperBounds()
                                : inductionVariableBounds->RelativeLowerBounds()
                        ).GetIterator();
                    it.IsValid();
                    it.MoveNext())
                {LOGMEIN("GlobOptIntBounds.cpp] 1794\n");
                    const ValueRelativeOffset &bound = it.CurrentValue();

                    StackSym *currentBoundBaseVarSym = bound.BaseSym();

                    if(!currentBoundBaseVarSym || !IsInt32TypeSpecialized(currentBoundBaseVarSym, &landingPadBlockData))
                    {LOGMEIN("GlobOptIntBounds.cpp] 1800\n");
                        continue;
                    }

                    Value *const boundBaseValue = FindValue(currentBoundBaseVarSym);
                    const ValueNumber boundBaseValueNumber = bound.BaseValueNumber();
                    if(!boundBaseValue || boundBaseValue->GetValueNumber() != boundBaseValueNumber)
                    {LOGMEIN("GlobOptIntBounds.cpp] 1807\n");
                        continue;
                    }

                    Value *const landingPadBoundBaseValue = FindValue(landingPadSymToValueMap, currentBoundBaseVarSym);
                    if(!landingPadBoundBaseValue || landingPadBoundBaseValue->GetValueNumber() != boundBaseValueNumber)
                    {LOGMEIN("GlobOptIntBounds.cpp] 1813\n");
                        continue;
                    }

                    if (foundBound)
                    {LOGMEIN("GlobOptIntBounds.cpp] 1818\n");
                        // We used to pick the first usable bound we saw in this list, but the list contains both
                        // the loop counter's bound *and* relative bounds of the primary bound. These secondary bounds
                        // are not guaranteed to be correct, so if the bound we found on a previous iteration is itself
                        // a bound for the current bound, then choose the current bound.
                        if (!boundBaseValue->GetValueInfo()->IsIntBounded())
                        {LOGMEIN("GlobOptIntBounds.cpp] 1824\n");
                            continue;
                        }
                        // currentBoundBaseVarSym has relative bounds of its own. If we find the saved boundBaseVarSym
                        // in currentBoundBaseVarSym's relative bounds list, let currentBoundBaseVarSym be the
                        // chosen bound.
                        const IntBounds *const currentBounds = boundBaseValue->GetValueInfo()->AsIntBounded()->Bounds();
                        bool foundSecondaryBound = false;
                        for (auto it2 =
                                 (
                                     minMagnitudeChange >= 0
                                     ? currentBounds->RelativeUpperBounds()
                                     : currentBounds->RelativeLowerBounds()
                                     ).GetIterator();
                             it2.IsValid();
                             it2.MoveNext())
                        {LOGMEIN("GlobOptIntBounds.cpp] 1840\n");
                            const ValueRelativeOffset &bound2 = it2.CurrentValue();
                            if (bound2.BaseSym() == boundBaseVarSym)
                            {LOGMEIN("GlobOptIntBounds.cpp] 1843\n");
                                // boundBaseVarSym is a secondary bound. Use currentBoundBaseVarSym instead.
                                foundSecondaryBound = true;
                                break;
                            }
                        }
                        if (!foundSecondaryBound)
                        {LOGMEIN("GlobOptIntBounds.cpp] 1850\n");
                            // boundBaseVarSym is not a relative bound of currentBoundBaseVarSym, so continue
                            // to use boundBaseVarSym.
                            continue;
                        }
                    }

                    boundBaseVarSym = bound.BaseSym();
                    boundOffset = bound.Offset();
                    foundBound = true;
                }
            }

            if(!foundBound)
            {LOGMEIN("GlobOptIntBounds.cpp] 1864\n");
                // No useful relative bound found; look for a constant bound. Exclude large constant bounds established implicitly by
                // <, <=, >, and >=. For example, for a loop condition (i < n), if 'n' is not invariant and hence can't be used,
                // 'i' will still have a constant upper bound of (int32 max - 1) that should be excluded as it's too large. Any
                // other constant bounds must have been established explicitly by the loop condition, and are safe to use.
                boundBaseVarSym = nullptr;
                if(minMagnitudeChange >= 0)
                {LOGMEIN("GlobOptIntBounds.cpp] 1871\n");
                    if(inductionVariableBounds)
                    {LOGMEIN("GlobOptIntBounds.cpp] 1873\n");
                        boundOffset = inductionVariableBounds->ConstantUpperBound();
                    }
                    else
                    {
                        AssertVerify(inductionVariableValueInfo->TryGetIntConstantUpperBound(&boundOffset));
                    }
                    if(boundOffset >= IntConstMax - 1)
                    {LOGMEIN("GlobOptIntBounds.cpp] 1881\n");
                        continue;
                    }
                }
                else
                {
                    if(inductionVariableBounds)
                    {LOGMEIN("GlobOptIntBounds.cpp] 1888\n");
                        boundOffset = inductionVariableBounds->ConstantLowerBound();
                    }
                    else
                    {
                        AssertVerify(inductionVariableValueInfo->TryGetIntConstantLowerBound(&boundOffset));
                    }
                    if(boundOffset <= IntConstMin + 1)
                    {LOGMEIN("GlobOptIntBounds.cpp] 1896\n");
                        continue;
                    }
                }
            }
        }

        // Determine if the induction variable already changed in the loop, and by how much
        int inductionVariableOffset;
        StackSym *inductionVariableSymToAdd;
        if(DetermineSymBoundOffsetOrValueRelativeToLandingPad(
                inductionVariableVarSym,
                minMagnitudeChange >= 0,
                inductionVariableValueInfo,
                inductionVariableBounds,
                landingPadSymToValueMap,
                &inductionVariableOffset))
        {LOGMEIN("GlobOptIntBounds.cpp] 1913\n");
            // The bound value is constant
            inductionVariableSymToAdd = nullptr;
        }
        else
        {
            // The bound value is not constant, the offset needs to be added to the induction variable in the landing pad
            inductionVariableSymToAdd = inductionVariableVarSym->GetInt32EquivSym(nullptr);
            Assert(inductionVariableSymToAdd);
        }

        // Int operands are required
        StackSym *boundBaseSym;
        if(boundBaseVarSym)
        {LOGMEIN("GlobOptIntBounds.cpp] 1927\n");
            boundBaseSym = boundBaseVarSym->IsVar() ? boundBaseVarSym->GetInt32EquivSym(nullptr) : boundBaseVarSym;
            Assert(boundBaseSym);
            Assert(boundBaseSym->GetType() == TyInt32 || boundBaseSym->GetType() == TyUint32);
        }
        else
        {
            boundBaseSym = nullptr;
        }

        // The loop count is computed as follows. We're actually computing the loop count minus one, because the value is used
        // to determine the bound of a secondary induction variable in its direction of change, and at that point the secondary
        // induction variable's information already accounts for changes in the first loop iteration.
        //
        // If the induction variable increases in the loop:
        //     loopCountMinusOne = (upperBound - inductionVariable) / abs(minMagnitudeChange)
        // Or more precisely:
        //     loopCountMinusOne =
        //         ((boundBase - inductionVariable) + (boundOffset - inductionVariableOffset)) / abs(minMagnitudeChange)
        //
        // If the induction variable decreases in the loop, the subtract operands are just reversed to yield a nonnegative
        // number, and the rest is similar. The two offsets are also constant and can be folded. So in general:
        //     loopCountMinusOne = (left - right + offset) / abs(minMagnitudeChange)

        // Determine the left and right information
        StackSym *leftSym, *rightSym;
        int leftOffset, rightOffset;
        if(minMagnitudeChange >= 0)
        {LOGMEIN("GlobOptIntBounds.cpp] 1955\n");
            leftSym = boundBaseSym;
            leftOffset = boundOffset;
            rightSym = inductionVariableSymToAdd;
            rightOffset = inductionVariableOffset;
        }
        else
        {
            minMagnitudeChange = -minMagnitudeChange;
            leftSym = inductionVariableSymToAdd;
            leftOffset = inductionVariableOffset;
            rightSym = boundBaseSym;
            rightOffset = boundOffset;
        }

        // Determine the combined offset, and save the info necessary to generate the loop count
        int offset;
        if(Int32Math::Sub(leftOffset, rightOffset, &offset))
        {LOGMEIN("GlobOptIntBounds.cpp] 1973\n");
            continue;
        }
        void *const loopCountBuffer = JitAnewArray(this->func->GetTopFunc()->m_fg->alloc, byte, sizeof(LoopCount));
        if(!rightSym)
        {LOGMEIN("GlobOptIntBounds.cpp] 1978\n");
            if(!leftSym)
            {LOGMEIN("GlobOptIntBounds.cpp] 1980\n");
                loop->loopCount = new(loopCountBuffer) LoopCount(offset / minMagnitudeChange);
                break;
            }
            if(offset == 0 && minMagnitudeChange == 1)
            {LOGMEIN("GlobOptIntBounds.cpp] 1985\n");
                loop->loopCount = new(loopCountBuffer) LoopCount(leftSym);
                break;
            }
        }
        loop->loopCount = new(loopCountBuffer) LoopCount(leftSym, rightSym, offset, minMagnitudeChange);
        break;
    }
}

void GlobOpt::GenerateLoopCount(Loop *const loop, LoopCount *const loopCount)
{LOGMEIN("GlobOptIntBounds.cpp] 1996\n");
    Assert(DoLoopCountBasedBoundCheckHoist());
    Assert(loop);
    Assert(loopCount);
    Assert(loopCount == loop->loopCount);
    Assert(!loopCount->HasBeenGenerated());

    // loopCountMinusOne = (left - right + offset) / minMagnitudeChange

    // Prepare the landing pad for bailouts and instruction insertion
    BailOutInfo *const bailOutInfo = loop->bailOutInfo;
    Assert(bailOutInfo);
    const IR::BailOutKind bailOutKind = IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck;
    IR::Instr *const insertBeforeInstr = bailOutInfo->bailOutInstr;
    Assert(insertBeforeInstr);
    Func *const func = bailOutInfo->bailOutFunc;

    // intermediateValue = left - right
    IR::IntConstOpnd *offset =
        loopCount->Offset() == 0 ? nullptr : IR::IntConstOpnd::New(loopCount->Offset(), TyInt32, func, true);
    StackSym *const rightSym = loopCount->RightSym();
    StackSym *intermediateValueSym;
    if(rightSym)
    {LOGMEIN("GlobOptIntBounds.cpp] 2019\n");
        IR::BailOutInstr *const instr = IR::BailOutInstr::New(Js::OpCode::Sub_I4, bailOutKind, bailOutInfo, func);

        instr->SetSrc2(IR::RegOpnd::New(rightSym, rightSym->GetType(), func));
        instr->GetSrc2()->SetIsJITOptimizedReg(true);

        StackSym *const leftSym = loopCount->LeftSym();
        if(leftSym)
        {LOGMEIN("GlobOptIntBounds.cpp] 2027\n");
            // intermediateValue = left - right
            instr->SetSrc1(IR::RegOpnd::New(leftSym, leftSym->GetType(), func));
            instr->GetSrc1()->SetIsJITOptimizedReg(true);
        }
        else if(offset)
        {LOGMEIN("GlobOptIntBounds.cpp] 2033\n");
            // intermediateValue = offset - right
            instr->SetSrc1(offset);
            offset = nullptr;
        }
        else
        {
            // intermediateValue = -right
            instr->m_opcode = Js::OpCode::Neg_I4;
            instr->SetSrc1(instr->UnlinkSrc2());
        }

        intermediateValueSym = StackSym::New(TyInt32, func);
        instr->SetDst(IR::RegOpnd::New(intermediateValueSym, intermediateValueSym->GetType(), func));
        instr->GetDst()->SetIsJITOptimizedReg(true);

        instr->SetByteCodeOffset(insertBeforeInstr);
        insertBeforeInstr->InsertBefore(instr);
    }
    else
    {
        // intermediateValue = left
        Assert(loopCount->LeftSym());
        intermediateValueSym = loopCount->LeftSym();
    }

    // intermediateValue += offset
    if(offset)
    {LOGMEIN("GlobOptIntBounds.cpp] 2061\n");
        IR::BailOutInstr *const instr = IR::BailOutInstr::New(Js::OpCode::Add_I4, bailOutKind, bailOutInfo, func);

        instr->SetSrc1(IR::RegOpnd::New(intermediateValueSym, intermediateValueSym->GetType(), func));
        instr->GetSrc1()->SetIsJITOptimizedReg(true);

        if(offset->GetValue() < 0 && offset->GetValue() != IntConstMin)
        {LOGMEIN("GlobOptIntBounds.cpp] 2068\n");
            instr->m_opcode = Js::OpCode::Sub_I4;
            offset->SetValue(-offset->GetValue());
        }
        instr->SetSrc2(offset);

        if(intermediateValueSym == loopCount->LeftSym())
        {LOGMEIN("GlobOptIntBounds.cpp] 2075\n");
            intermediateValueSym = StackSym::New(TyInt32, func);
        }
        instr->SetDst(IR::RegOpnd::New(intermediateValueSym, intermediateValueSym->GetType(), func));
        instr->GetDst()->SetIsJITOptimizedReg(true);

        instr->SetByteCodeOffset(insertBeforeInstr);
        insertBeforeInstr->InsertBefore(instr);
    }

    // intermediateValue /= minMagnitudeChange
    const int minMagnitudeChange = loopCount->MinMagnitudeChange();
    if(minMagnitudeChange != 1)
    {LOGMEIN("GlobOptIntBounds.cpp] 2088\n");
        IR::Instr *const instr = IR::Instr::New(Js::OpCode::Div_I4, func);

        instr->SetSrc1(IR::RegOpnd::New(intermediateValueSym, intermediateValueSym->GetType(), func));
        instr->GetSrc1()->SetIsJITOptimizedReg(true);

        Assert(minMagnitudeChange != 0); // bailout is not needed
        instr->SetSrc2(IR::IntConstOpnd::New(minMagnitudeChange, TyInt32, func, true));

        if(intermediateValueSym == loopCount->LeftSym())
        {LOGMEIN("GlobOptIntBounds.cpp] 2098\n");
            intermediateValueSym = StackSym::New(TyInt32, func);
        }
        instr->SetDst(IR::RegOpnd::New(intermediateValueSym, intermediateValueSym->GetType(), func));
        instr->GetDst()->SetIsJITOptimizedReg(true);

        instr->SetByteCodeOffset(insertBeforeInstr);
        insertBeforeInstr->InsertBefore(instr);
    }
    else
    {
        Assert(intermediateValueSym != loopCount->LeftSym());
    }

    // loopCountMinusOne = intermediateValue
    loopCount->SetLoopCountMinusOneSym(intermediateValueSym);
}

void GlobOpt::GenerateLoopCountPlusOne(Loop *const loop, LoopCount *const loopCount)
{LOGMEIN("GlobOptIntBounds.cpp] 2117\n");
    Assert(loop);
    Assert(loopCount);
    Assert(loopCount == loop->loopCount);
    if (loopCount->HasGeneratedLoopCountSym())
    {LOGMEIN("GlobOptIntBounds.cpp] 2122\n");
        return;
    }
    if (!loopCount->HasBeenGenerated())
    {
        GenerateLoopCount(loop, loopCount);
    }
    Assert(loopCount->HasBeenGenerated());
    // If this is null then the loop count is a constant and there is nothing more to do here
    if (loopCount->LoopCountMinusOneSym())
    {LOGMEIN("GlobOptIntBounds.cpp] 2132\n");
        // Prepare the landing pad for bailouts and instruction insertion
        BailOutInfo *const bailOutInfo = loop->bailOutInfo;
        Assert(bailOutInfo);
        IR::Instr *const insertBeforeInstr = bailOutInfo->bailOutInstr;
        Assert(insertBeforeInstr);
        Func *const func = bailOutInfo->bailOutFunc;

        IRType type = loopCount->LoopCountMinusOneSym()->GetType();

        // loop count is off by one, so add one
        IR::RegOpnd *loopCountOpnd = IR::RegOpnd::New(type, func);
        IR::RegOpnd *minusOneOpnd = IR::RegOpnd::New(loopCount->LoopCountMinusOneSym(), type, func);
        minusOneOpnd->SetIsJITOptimizedReg(true);
        insertBeforeInstr->InsertBefore(IR::Instr::New(Js::OpCode::Add_I4,
                                                       loopCountOpnd,
                                                       minusOneOpnd,
                                                       IR::IntConstOpnd::New(1, type, func, true),
                                                       func));
        loopCount->SetLoopCountSym(loopCountOpnd->GetStackSym());
    }
}

void GlobOpt::GenerateSecondaryInductionVariableBound(
    Loop *const loop,
    StackSym *const inductionVariableSym,
    const LoopCount *const loopCount,
    const int maxMagnitudeChange,
    StackSym *const boundSym)
{LOGMEIN("GlobOptIntBounds.cpp] 2161\n");
    Assert(loop);
    Assert(inductionVariableSym);
    Assert(inductionVariableSym->GetType() == TyInt32 || inductionVariableSym->GetType() == TyUint32);
    Assert(loopCount);
    Assert(loopCount == loop->loopCount);
    Assert(loopCount->LoopCountMinusOneSym());
    Assert(maxMagnitudeChange != 0);
    Assert(maxMagnitudeChange >= -InductionVariable::ChangeMagnitudeLimitForLoopCountBasedHoisting);
    Assert(maxMagnitudeChange <= InductionVariable::ChangeMagnitudeLimitForLoopCountBasedHoisting);
    Assert(boundSym);
    Assert(boundSym->IsInt32());

    // bound = inductionVariable + loopCountMinusOne * maxMagnitudeChange

    // Prepare the landing pad for bailouts and instruction insertion
    BailOutInfo *const bailOutInfo = loop->bailOutInfo;
    Assert(bailOutInfo);
    const IR::BailOutKind bailOutKind = IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck;
    IR::Instr *const insertBeforeInstr = bailOutInfo->bailOutInstr;
    Assert(insertBeforeInstr);
    Func *const func = bailOutInfo->bailOutFunc;

    // intermediateValue = loopCount * maxMagnitudeChange
    StackSym *intermediateValueSym;
    if(maxMagnitudeChange == 1 || maxMagnitudeChange == -1)
    {LOGMEIN("GlobOptIntBounds.cpp] 2187\n");
        intermediateValueSym = loopCount->LoopCountMinusOneSym();
    }
    else
    {
        IR::BailOutInstr *const instr = IR::BailOutInstr::New(Js::OpCode::Mul_I4, bailOutKind, bailOutInfo, func);

        instr->SetSrc1(
            IR::RegOpnd::New(loopCount->LoopCountMinusOneSym(), loopCount->LoopCountMinusOneSym()->GetType(), func));
        instr->GetSrc1()->SetIsJITOptimizedReg(true);

        instr->SetSrc2(IR::IntConstOpnd::New(maxMagnitudeChange, TyInt32, func, true));

        intermediateValueSym = boundSym;
        instr->SetDst(IR::RegOpnd::New(intermediateValueSym, intermediateValueSym->GetType(), func));
        instr->GetDst()->SetIsJITOptimizedReg(true);

        instr->SetByteCodeOffset(insertBeforeInstr);
        insertBeforeInstr->InsertBefore(instr);
    }

    // bound = intermediateValue + inductionVariable
    {LOGMEIN("GlobOptIntBounds.cpp] 2209\n");
        IR::BailOutInstr *const instr = IR::BailOutInstr::New(Js::OpCode::Add_I4, bailOutKind, bailOutInfo, func);

        instr->SetSrc1(IR::RegOpnd::New(intermediateValueSym, intermediateValueSym->GetType(), func));
        instr->GetSrc1()->SetIsJITOptimizedReg(true);

        instr->SetSrc2(IR::RegOpnd::New(inductionVariableSym, inductionVariableSym->GetType(), func));
        instr->GetSrc2()->SetIsJITOptimizedReg(true);

        if(maxMagnitudeChange == -1)
        {LOGMEIN("GlobOptIntBounds.cpp] 2219\n");
            // bound = inductionVariable - intermediateValue[loopCount]
            instr->m_opcode = Js::OpCode::Sub_I4;
            instr->SwapOpnds();
        }

        instr->SetDst(IR::RegOpnd::New(boundSym, boundSym->GetType(), func));
        instr->GetDst()->SetIsJITOptimizedReg(true);

        instr->SetByteCodeOffset(insertBeforeInstr);
        insertBeforeInstr->InsertBefore(instr);
    }
}

void GlobOpt::DetermineArrayBoundCheckHoistability(
    bool needLowerBoundCheck,
    bool needUpperBoundCheck,
    ArrayLowerBoundCheckHoistInfo &lowerHoistInfo,
    ArrayUpperBoundCheckHoistInfo &upperHoistInfo,
    const bool isJsArray,
    StackSym *const indexSym,
    Value *const indexValue,
    const IntConstantBounds &indexConstantBounds,
    StackSym *const headSegmentLengthSym,
    Value *const headSegmentLengthValue,
    const IntConstantBounds &headSegmentLengthConstantBounds,
    Loop *const headSegmentLengthInvariantLoop,
    bool &failedToUpdateCompatibleLowerBoundCheck,
    bool &failedToUpdateCompatibleUpperBoundCheck)
{LOGMEIN("GlobOptIntBounds.cpp] 2248\n");
    Assert(DoBoundCheckHoist());
    Assert(needLowerBoundCheck || needUpperBoundCheck);
    Assert(!lowerHoistInfo.HasAnyInfo());
    Assert(!upperHoistInfo.HasAnyInfo());
    Assert(!indexSym == !indexValue);
    Assert(!needUpperBoundCheck || headSegmentLengthSym);
    Assert(!headSegmentLengthSym == !headSegmentLengthValue);
    Assert(!failedToUpdateCompatibleLowerBoundCheck);
    Assert(!failedToUpdateCompatibleUpperBoundCheck);

    Loop *const currentLoop = currentBlock->loop;
    if(!indexValue)
    {LOGMEIN("GlobOptIntBounds.cpp] 2261\n");
        Assert(!needLowerBoundCheck);
        Assert(needUpperBoundCheck);
        Assert(indexConstantBounds.IsConstant());

        // The index is a constant value, so a bound check on it can be hoisted as far as desired. Just find a compatible bound
        // check that is already available, or the loop in which the head segment length is invariant.

        TRACE_PHASE_VERBOSE(
            Js::Phase::BoundCheckHoistPhase,
            2,
            _u("Index is constant, looking for a compatible upper bound check\n"));
        const int indexConstantValue = indexConstantBounds.LowerBound();
        Assert(indexConstantValue != IntConstMax);
        const IntBoundCheck *compatibleBoundCheck;
        if(blockData.availableIntBoundChecks->TryGetReference(
                IntBoundCheckCompatibilityId(ZeroValueNumber, headSegmentLengthValue->GetValueNumber()),
                &compatibleBoundCheck))
        {LOGMEIN("GlobOptIntBounds.cpp] 2279\n");
            // We need:
            //     index < headSegmentLength
            // Normalize the offset such that:
            //     0 <= headSegmentLength + compatibleBoundCheckOffset
            // Where (compatibleBoundCheckOffset = -1 - index), and -1 is to simulate < instead of <=.
            const int compatibleBoundCheckOffset = -1 - indexConstantValue;
            if(compatibleBoundCheck->SetBoundOffset(compatibleBoundCheckOffset))
            {LOGMEIN("GlobOptIntBounds.cpp] 2287\n");
                TRACE_PHASE_VERBOSE(
                    Js::Phase::BoundCheckHoistPhase,
                    3,
                    _u("Found in block %u\n"),
                    compatibleBoundCheck->Block()->GetBlockNum());
                upperHoistInfo.SetCompatibleBoundCheck(compatibleBoundCheck->Block(), indexConstantValue);
                return;
            }
            failedToUpdateCompatibleUpperBoundCheck = true;
        }
        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Not found\n"));

        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 2, _u("Looking for invariant head segment length\n"));
        Loop *invariantLoop;
        Value *landingPadHeadSegmentLengthValue = nullptr;
        if(headSegmentLengthInvariantLoop)
        {LOGMEIN("GlobOptIntBounds.cpp] 2304\n");
            invariantLoop = headSegmentLengthInvariantLoop;
            landingPadHeadSegmentLengthValue =
                FindValue(invariantLoop->landingPad->globOptData.symToValueMap, headSegmentLengthSym);
        }
        else if(currentLoop)
        {LOGMEIN("GlobOptIntBounds.cpp] 2310\n");
            invariantLoop = nullptr;
            for(Loop *loop = currentLoop; loop; loop = loop->parent)
            {LOGMEIN("GlobOptIntBounds.cpp] 2313\n");
                GlobOptBlockData &landingPadBlockData = loop->landingPad->globOptData;
                GlobHashTable *const landingPadSymToValueMap = landingPadBlockData.symToValueMap;

                Value *const value = FindValue(landingPadSymToValueMap, headSegmentLengthSym);
                if(!value)
                {LOGMEIN("GlobOptIntBounds.cpp] 2319\n");
                    break;
                }

                invariantLoop = loop;
                landingPadHeadSegmentLengthValue = value;
            }
            if(!invariantLoop)
            {LOGMEIN("GlobOptIntBounds.cpp] 2327\n");
                TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Not found\n"));
                return;
            }
        }
        else
        {
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Not found, block is not in a loop\n"));
            return;
        }
        TRACE_PHASE_VERBOSE(
            Js::Phase::BoundCheckHoistPhase,
            3,
            _u("Found in loop %u landing pad block %u\n"),
            invariantLoop->GetLoopNumber(),
            invariantLoop->landingPad->GetBlockNum());

        IntConstantBounds landingPadHeadSegmentLengthConstantBounds;
        AssertVerify(
            landingPadHeadSegmentLengthValue
                ->GetValueInfo()
                ->TryGetIntConstantBounds(&landingPadHeadSegmentLengthConstantBounds));

        if(isJsArray)
        {LOGMEIN("GlobOptIntBounds.cpp] 2351\n");
            // index >= headSegmentLength is currently not possible for JS arrays (except when index == int32 max, which is
            // covered above).
            Assert(
                !ValueInfo::IsGreaterThanOrEqualTo(
                    nullptr,
                    indexConstantValue,
                    indexConstantValue,
                    landingPadHeadSegmentLengthValue,
                    landingPadHeadSegmentLengthConstantBounds.LowerBound(),
                    landingPadHeadSegmentLengthConstantBounds.UpperBound()));
        }
        else if(
            ValueInfo::IsGreaterThanOrEqualTo(
                nullptr,
                indexConstantValue,
                indexConstantValue,
                landingPadHeadSegmentLengthValue,
                landingPadHeadSegmentLengthConstantBounds.LowerBound(),
                landingPadHeadSegmentLengthConstantBounds.UpperBound()))
        {LOGMEIN("GlobOptIntBounds.cpp] 2371\n");
            // index >= headSegmentLength in the landing pad, can't use the index sym. This is possible for typed arrays through
            // conditions on array.length in user code.
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 2, _u("Index >= head segment length\n"));
            return;
        }

        upperHoistInfo.SetLoop(
            invariantLoop,
            indexConstantValue,
            landingPadHeadSegmentLengthValue,
            landingPadHeadSegmentLengthConstantBounds);
        return;
    }

    Assert(!indexConstantBounds.IsConstant());

    ValueInfo *const indexValueInfo = indexValue->GetValueInfo();
    const IntBounds *const indexBounds = indexValueInfo->IsIntBounded() ? indexValueInfo->AsIntBounded()->Bounds() : nullptr;
    {
        // See if a compatible bound check is already available
        TRACE_PHASE_VERBOSE(
            Js::Phase::BoundCheckHoistPhase,
            2,
            _u("Looking for compatible bound checks for index bounds\n"));

        bool searchingLower = needLowerBoundCheck;
        bool searchingUpper = needUpperBoundCheck;
        Assert(searchingLower || searchingUpper);

        bool foundLowerBoundCheck = false;
        const IntBoundCheck *lowerBoundCheck = nullptr;
        ValueNumber lowerHoistBlockIndexValueNumber = InvalidValueNumber;
        int lowerBoundOffset = 0;
        if(searchingLower &&
            blockData.availableIntBoundChecks->TryGetReference(
                IntBoundCheckCompatibilityId(ZeroValueNumber, indexValue->GetValueNumber()),
                &lowerBoundCheck))
        {LOGMEIN("GlobOptIntBounds.cpp] 2409\n");
            if(lowerBoundCheck->SetBoundOffset(0))
            {LOGMEIN("GlobOptIntBounds.cpp] 2411\n");
                foundLowerBoundCheck = true;
                lowerHoistBlockIndexValueNumber = indexValue->GetValueNumber();
                lowerBoundOffset = 0;
                searchingLower = false;
            }
            else
            {
                failedToUpdateCompatibleLowerBoundCheck = true;
            }
        }

        bool foundUpperBoundCheck = false;
        const IntBoundCheck *upperBoundCheck = nullptr;
        ValueNumber upperHoistBlockIndexValueNumber = InvalidValueNumber;
        int upperBoundOffset = 0;
        if(searchingUpper &&
            blockData.availableIntBoundChecks->TryGetReference(
                IntBoundCheckCompatibilityId(indexValue->GetValueNumber(), headSegmentLengthValue->GetValueNumber()),
                &upperBoundCheck))
        {LOGMEIN("GlobOptIntBounds.cpp] 2431\n");
            if(upperBoundCheck->SetBoundOffset(-1)) // -1 is to simulate < instead of <=
            {LOGMEIN("GlobOptIntBounds.cpp] 2433\n");
                foundUpperBoundCheck = true;
                upperHoistBlockIndexValueNumber = indexValue->GetValueNumber();
                upperBoundOffset = 0;
                searchingUpper = false;
            }
            else
            {
                failedToUpdateCompatibleUpperBoundCheck = true;
            }
        }

        if(indexBounds)
        {LOGMEIN("GlobOptIntBounds.cpp] 2446\n");
            searchingLower = searchingLower && indexBounds->RelativeLowerBounds().Count() != 0;
            searchingUpper = searchingUpper && indexBounds->RelativeUpperBounds().Count() != 0;
            if(searchingLower || searchingUpper)
            {LOGMEIN("GlobOptIntBounds.cpp] 2450\n");
                for(auto it = blockData.availableIntBoundChecks->GetIterator(); it.IsValid(); it.MoveNext())
                {LOGMEIN("GlobOptIntBounds.cpp] 2452\n");
                    const IntBoundCheck &boundCheck = it.CurrentValue();

                    if(searchingLower && boundCheck.LeftValueNumber() == ZeroValueNumber)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2456\n");
                        lowerHoistBlockIndexValueNumber = boundCheck.RightValueNumber();
                        const ValueRelativeOffset *bound;
                        if(indexBounds->RelativeLowerBounds().TryGetReference(lowerHoistBlockIndexValueNumber, &bound))
                        {LOGMEIN("GlobOptIntBounds.cpp] 2460\n");
                            // We need:
                            //     0 <= boundBase + boundOffset
                            const int offset = bound->Offset();
                            if(boundCheck.SetBoundOffset(offset))
                            {LOGMEIN("GlobOptIntBounds.cpp] 2465\n");
                                foundLowerBoundCheck = true;
                                lowerBoundCheck = &boundCheck;
                                lowerBoundOffset = offset;

                                searchingLower = false;
                                if(!searchingUpper)
                                {LOGMEIN("GlobOptIntBounds.cpp] 2472\n");
                                    break;
                                }
                            }
                            else
                            {
                                failedToUpdateCompatibleLowerBoundCheck = true;
                            }
                        }
                    }

                    if(searchingUpper && boundCheck.RightValueNumber() == headSegmentLengthValue->GetValueNumber())
                    {LOGMEIN("GlobOptIntBounds.cpp] 2484\n");
                        upperHoistBlockIndexValueNumber = boundCheck.LeftValueNumber();
                        const ValueRelativeOffset *bound;
                        if(indexBounds->RelativeUpperBounds().TryGetReference(upperHoistBlockIndexValueNumber, &bound))
                        {LOGMEIN("GlobOptIntBounds.cpp] 2488\n");
                            // We need:
                            //     boundBase + boundOffset < headSegmentLength
                            // Normalize the offset such that:
                            //     boundBase <= headSegmentLength + compatibleBoundCheckOffset
                            // Where (compatibleBoundCheckOffset = -1 - boundOffset), and -1 is to simulate < instead of <=.
                            const int offset = -1 - bound->Offset();
                            if(boundCheck.SetBoundOffset(offset))
                            {LOGMEIN("GlobOptIntBounds.cpp] 2496\n");
                                foundUpperBoundCheck = true;
                                upperBoundCheck = &boundCheck;
                                upperBoundOffset = bound->Offset();

                                searchingUpper = false;
                                if(!searchingLower)
                                {LOGMEIN("GlobOptIntBounds.cpp] 2503\n");
                                    break;
                                }
                            }
                            else
                            {
                                failedToUpdateCompatibleUpperBoundCheck = true;
                            }
                        }
                    }
                }
            }
        }

        if(foundLowerBoundCheck)
        {LOGMEIN("GlobOptIntBounds.cpp] 2518\n");
            // A bound check takes the form src1 <= src2 + dst
            Assert(lowerBoundCheck->Instr()->GetSrc2());
            Assert(
                lowerBoundCheck->Instr()->GetSrc2()->AsRegOpnd()->m_sym->GetType() == TyInt32 ||
                lowerBoundCheck->Instr()->GetSrc2()->AsRegOpnd()->m_sym->GetType() == TyUint32);
            StackSym *boundCheckIndexSym = lowerBoundCheck->Instr()->GetSrc2()->AsRegOpnd()->m_sym;
            if(boundCheckIndexSym->IsTypeSpec())
            {LOGMEIN("GlobOptIntBounds.cpp] 2526\n");
                boundCheckIndexSym = boundCheckIndexSym->GetVarEquivSym(nullptr);
                Assert(boundCheckIndexSym);
            }

            TRACE_PHASE_VERBOSE(
                Js::Phase::BoundCheckHoistPhase,
                3,
                _u("Found lower bound (s%u + %d) in block %u\n"),
                boundCheckIndexSym->m_id,
                lowerBoundOffset,
                lowerBoundCheck->Block()->GetBlockNum());
            lowerHoistInfo.SetCompatibleBoundCheck(
                lowerBoundCheck->Block(),
                boundCheckIndexSym,
                lowerBoundOffset,
                lowerHoistBlockIndexValueNumber);

            Assert(!searchingLower);
            needLowerBoundCheck = false;
            if(!needUpperBoundCheck)
            {LOGMEIN("GlobOptIntBounds.cpp] 2547\n");
                return;
            }
        }

        if(foundUpperBoundCheck)
        {LOGMEIN("GlobOptIntBounds.cpp] 2553\n");
            // A bound check takes the form src1 <= src2 + dst
            Assert(upperBoundCheck->Instr()->GetSrc1());
            Assert(
                upperBoundCheck->Instr()->GetSrc1()->AsRegOpnd()->m_sym->GetType() == TyInt32 ||
                upperBoundCheck->Instr()->GetSrc1()->AsRegOpnd()->m_sym->GetType() == TyUint32);
            StackSym *boundCheckIndexSym = upperBoundCheck->Instr()->GetSrc1()->AsRegOpnd()->m_sym;
            if(boundCheckIndexSym->IsTypeSpec())
            {LOGMEIN("GlobOptIntBounds.cpp] 2561\n");
                boundCheckIndexSym = boundCheckIndexSym->GetVarEquivSym(nullptr);
                Assert(boundCheckIndexSym);
            }

            TRACE_PHASE_VERBOSE(
                Js::Phase::BoundCheckHoistPhase,
                3,
                _u("Found upper bound (s%u + %d) in block %u\n"),
                boundCheckIndexSym->m_id,
                upperBoundOffset,
                upperBoundCheck->Block()->GetBlockNum());
            upperHoistInfo.SetCompatibleBoundCheck(
                upperBoundCheck->Block(),
                boundCheckIndexSym,
                -1 - upperBoundOffset,
                upperHoistBlockIndexValueNumber);

            Assert(!searchingUpper);
            needUpperBoundCheck = false;
            if(!needLowerBoundCheck)
            {LOGMEIN("GlobOptIntBounds.cpp] 2582\n");
                return;
            }
        }

        Assert(needLowerBoundCheck || needUpperBoundCheck);
        Assert(!needLowerBoundCheck || !lowerHoistInfo.CompatibleBoundCheckBlock());
        Assert(!needUpperBoundCheck || !upperHoistInfo.CompatibleBoundCheckBlock());
    }

    if(!currentLoop)
    {LOGMEIN("GlobOptIntBounds.cpp] 2593\n");
        return;
    }

    // Check if the index sym is invariant in the loop, or if the index value in the landing pad is a lower/upper bound of the
    // index value in the current block
    TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 2, _u("Looking for invariant index or index bounded by itself\n"));
    bool searchingLower = needLowerBoundCheck, searchingUpper = needUpperBoundCheck;
    for(Loop *loop = currentLoop; loop; loop = loop->parent)
    {LOGMEIN("GlobOptIntBounds.cpp] 2602\n");
        GlobOptBlockData &landingPadBlockData = loop->landingPad->globOptData;
        GlobHashTable *const landingPadSymToValueMap = landingPadBlockData.symToValueMap;
        TRACE_PHASE_VERBOSE(
            Js::Phase::BoundCheckHoistPhase,
            3,
            _u("Trying loop %u landing pad block %u\n"),
            loop->GetLoopNumber(),
            loop->landingPad->GetBlockNum());

        Value *const landingPadIndexValue = FindValue(landingPadSymToValueMap, indexSym);
        if(!landingPadIndexValue)
        {LOGMEIN("GlobOptIntBounds.cpp] 2614\n");
            break;
        }

        IntConstantBounds landingPadIndexConstantBounds;
        const bool landingPadIndexValueIsLikelyInt =
            landingPadIndexValue->GetValueInfo()->TryGetIntConstantBounds(&landingPadIndexConstantBounds, true);
        int lowerOffset = 0, upperOffset = 0;
        if(indexValue->GetValueNumber() == landingPadIndexValue->GetValueNumber())
        {LOGMEIN("GlobOptIntBounds.cpp] 2623\n");
            Assert(landingPadIndexValueIsLikelyInt);
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Index is invariant\n"));
        }
        else
        {
            if(!landingPadIndexValueIsLikelyInt)
            {LOGMEIN("GlobOptIntBounds.cpp] 2630\n");
                break;
            }

            if(searchingLower)
            {LOGMEIN("GlobOptIntBounds.cpp] 2635\n");
                if(lowerHoistInfo.Loop() && indexValue->GetValueNumber() == lowerHoistInfo.IndexValueNumber())
                {LOGMEIN("GlobOptIntBounds.cpp] 2637\n");
                    // Prefer using the invariant sym
                    needLowerBoundCheck = searchingLower = false;
                    if(!needUpperBoundCheck)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2641\n");
                        return;
                    }
                    if(!searchingUpper)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2645\n");
                        break;
                    }
                }
                else
                {
                    bool foundBound = false;
                    if(indexBounds)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2653\n");
                        const ValueRelativeOffset *bound;
                        if(indexBounds->RelativeLowerBounds().TryGetReference(landingPadIndexValue->GetValueNumber(), &bound))
                        {LOGMEIN("GlobOptIntBounds.cpp] 2656\n");
                            foundBound = true;
                            lowerOffset = bound->Offset();
                            TRACE_PHASE_VERBOSE(
                                Js::Phase::BoundCheckHoistPhase,
                                4,
                                _u("Found lower bound (index + %d)\n"),
                                lowerOffset);
                        }
                    }
                    if(!foundBound)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2667\n");
                        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Lower bound was not found\n"));
                        searchingLower = false;
                        if(!searchingUpper)
                        {LOGMEIN("GlobOptIntBounds.cpp] 2671\n");
                            break;
                        }
                    }
                }
            }

            if(searchingUpper)
            {LOGMEIN("GlobOptIntBounds.cpp] 2679\n");
                if(upperHoistInfo.Loop() && indexValue->GetValueNumber() == upperHoistInfo.IndexValueNumber())
                {LOGMEIN("GlobOptIntBounds.cpp] 2681\n");
                    // Prefer using the invariant sym
                    needUpperBoundCheck = searchingUpper = false;
                    if(!needLowerBoundCheck)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2685\n");
                        return;
                    }
                    if(!searchingLower)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2689\n");
                        break;
                    }
                }
                else
                {
                    bool foundBound = false;
                    if(indexBounds)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2697\n");
                        const ValueRelativeOffset *bound;
                        if(indexBounds->RelativeUpperBounds().TryGetReference(landingPadIndexValue->GetValueNumber(), &bound))
                        {LOGMEIN("GlobOptIntBounds.cpp] 2700\n");
                            foundBound = true;
                            upperOffset = bound->Offset();
                            TRACE_PHASE_VERBOSE(
                                Js::Phase::BoundCheckHoistPhase,
                                4,
                                _u("Found upper bound (index + %d)\n"),
                                upperOffset);
                        }
                    }
                    if(!foundBound)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2711\n");
                        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Upper bound was not found\n"));
                        searchingUpper = false;
                        if(!searchingLower)
                        {LOGMEIN("GlobOptIntBounds.cpp] 2715\n");
                            break;
                        }
                    }
                }
            }
        }

        if(searchingLower)
        {LOGMEIN("GlobOptIntBounds.cpp] 2724\n");
            if(ValueInfo::IsLessThan(
                    landingPadIndexValue,
                    landingPadIndexConstantBounds.LowerBound(),
                    landingPadIndexConstantBounds.UpperBound(),
                    nullptr,
                    0,
                    0))
            {LOGMEIN("GlobOptIntBounds.cpp] 2732\n");
                // index < 0 in the landing pad; can't use the index sym
                TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 5, _u("Index < 0\n"));
                searchingLower = false;
                if(!searchingUpper)
                {LOGMEIN("GlobOptIntBounds.cpp] 2737\n");
                    break;
                }
            }
            else
            {
                lowerHoistInfo.SetLoop(
                    loop,
                    indexSym,
                    lowerOffset,
                    landingPadIndexValue,
                    landingPadIndexConstantBounds);
            }
        }

        if(!searchingUpper)
        {LOGMEIN("GlobOptIntBounds.cpp] 2753\n");
            continue;
        }

        // Check if the head segment length sym is available in the landing pad
        Value *const landingPadHeadSegmentLengthValue = FindValue(landingPadSymToValueMap, headSegmentLengthSym);
        if(!landingPadHeadSegmentLengthValue)
        {LOGMEIN("GlobOptIntBounds.cpp] 2760\n");
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 5, _u("Head segment length is not invariant\n"));
            searchingUpper = false;
            if(!searchingLower)
            {LOGMEIN("GlobOptIntBounds.cpp] 2764\n");
                break;
            }
            continue;
        }
        IntConstantBounds landingPadHeadSegmentLengthConstantBounds;
        AssertVerify(
            landingPadHeadSegmentLengthValue
                ->GetValueInfo()
                ->TryGetIntConstantBounds(&landingPadHeadSegmentLengthConstantBounds));

        if(ValueInfo::IsGreaterThanOrEqualTo(
                landingPadIndexValue,
                landingPadIndexConstantBounds.LowerBound(),
                landingPadIndexConstantBounds.UpperBound(),
                landingPadHeadSegmentLengthValue,
                landingPadHeadSegmentLengthConstantBounds.LowerBound(),
                landingPadHeadSegmentLengthConstantBounds.UpperBound()))
        {LOGMEIN("GlobOptIntBounds.cpp] 2782\n");
            // index >= headSegmentLength in the landing pad; can't use the index sym
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 5, _u("Index >= head segment length\n"));
            searchingUpper = false;
            if(!searchingLower)
            {LOGMEIN("GlobOptIntBounds.cpp] 2787\n");
                break;
            }
            continue;
        }

        // We need:
        //     boundBase + boundOffset < headSegmentLength
        // Normalize the offset such that:
        //     boundBase <= headSegmentLength + offset
        // Where (offset = -1 - boundOffset), and -1 is to simulate < instead of <=.
        upperOffset = -1 - upperOffset;

        upperHoistInfo.SetLoop(
            loop,
            indexSym,
            upperOffset,
            landingPadIndexValue,
            landingPadIndexConstantBounds,
            landingPadHeadSegmentLengthValue,
            landingPadHeadSegmentLengthConstantBounds);
    }

    if(needLowerBoundCheck && lowerHoistInfo.Loop())
    {LOGMEIN("GlobOptIntBounds.cpp] 2811\n");
        needLowerBoundCheck = false;
        if(!needUpperBoundCheck)
        {LOGMEIN("GlobOptIntBounds.cpp] 2814\n");
            return;
        }
    }
    if(needUpperBoundCheck && upperHoistInfo.Loop())
    {LOGMEIN("GlobOptIntBounds.cpp] 2819\n");
        needUpperBoundCheck = false;
        if(!needLowerBoundCheck)
        {LOGMEIN("GlobOptIntBounds.cpp] 2822\n");
            return;
        }
    }

    // Find an invariant lower/upper bound of the index that can be used for hoisting the bound checks
    TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 2, _u("Looking for invariant index bounds\n"));
    searchingLower = needLowerBoundCheck;
    searchingUpper = needUpperBoundCheck;
    for(Loop *loop = currentLoop; loop; loop = loop->parent)
    {LOGMEIN("GlobOptIntBounds.cpp] 2832\n");
        GlobOptBlockData &landingPadBlockData = loop->landingPad->globOptData;
        GlobHashTable *const landingPadSymToValueMap = landingPadBlockData.symToValueMap;
        TRACE_PHASE_VERBOSE(
            Js::Phase::BoundCheckHoistPhase,
            3,
            _u("Trying loop %u landing pad block %u\n"),
            loop->GetLoopNumber(),
            loop->landingPad->GetBlockNum());

        Value *landingPadHeadSegmentLengthValue = nullptr;
        IntConstantBounds landingPadHeadSegmentLengthConstantBounds;
        if(searchingUpper)
        {LOGMEIN("GlobOptIntBounds.cpp] 2845\n");
            // Check if the head segment length sym is available in the landing pad
            landingPadHeadSegmentLengthValue = FindValue(landingPadSymToValueMap, headSegmentLengthSym);
            if(landingPadHeadSegmentLengthValue)
            {LOGMEIN("GlobOptIntBounds.cpp] 2849\n");
                AssertVerify(
                    landingPadHeadSegmentLengthValue
                        ->GetValueInfo()
                        ->TryGetIntConstantBounds(&landingPadHeadSegmentLengthConstantBounds));
            }
            else
            {
                TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Head segment length is not invariant\n"));
                searchingUpper = false;
                if(!searchingLower)
                {LOGMEIN("GlobOptIntBounds.cpp] 2860\n");
                    break;
                }
            }
        }

        // Look for a relative bound
        if(indexBounds)
        {LOGMEIN("GlobOptIntBounds.cpp] 2868\n");
            for(int j = 0; j < 2; ++j)
            {LOGMEIN("GlobOptIntBounds.cpp] 2870\n");
                const bool searchingRelativeLowerBounds = j == 0;
                if(!(searchingRelativeLowerBounds ? searchingLower : searchingUpper))
                {LOGMEIN("GlobOptIntBounds.cpp] 2873\n");
                    continue;
                }

                for(auto it =
                        (
                            searchingRelativeLowerBounds
                                ? indexBounds->RelativeLowerBounds()
                                : indexBounds->RelativeUpperBounds()
                        ).GetIterator();
                    it.IsValid();
                    it.MoveNext())
                {LOGMEIN("GlobOptIntBounds.cpp] 2885\n");
                    const ValueRelativeOffset &indexBound = it.CurrentValue();

                    StackSym *const indexBoundBaseSym = indexBound.BaseSym();
                    if(!indexBoundBaseSym)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2890\n");
                        continue;
                    }
                    TRACE_PHASE_VERBOSE(
                        Js::Phase::BoundCheckHoistPhase,
                        4,
                        _u("Found %S bound (s%u + %d)\n"),
                        searchingRelativeLowerBounds ? "lower" : "upper",
                        indexBoundBaseSym->m_id,
                        indexBound.Offset());

                    if(!indexBound.WasEstablishedExplicitly())
                    {LOGMEIN("GlobOptIntBounds.cpp] 2902\n");
                        // Don't use a bound that was not established explicitly, as it may be too aggressive. For instance, an
                        // index sym used in an array will obtain an upper bound of the array's head segment length - 1. That
                        // bound is not established explicitly because the bound assertion is not enforced by the source code.
                        // Rather, it is assumed and enforced by the JIT using bailout check. Incrementing the index and using
                        // it in a different array may otherwise cause it to use the first array's head segment length as the
                        // upper bound on which to do the bound check against the second array, and that bound check would
                        // always fail when the arrays are the same size.
                        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 5, _u("Bound was established implicitly\n"));
                        continue;
                    }

                    Value *const landingPadIndexBoundBaseValue = FindValue(landingPadSymToValueMap, indexBoundBaseSym);
                    if(!landingPadIndexBoundBaseValue ||
                        landingPadIndexBoundBaseValue->GetValueNumber() != indexBound.BaseValueNumber())
                    {LOGMEIN("GlobOptIntBounds.cpp] 2917\n");
                        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 5, _u("Bound is not invariant\n"));
                        continue;
                    }

                    IntConstantBounds landingPadIndexBoundBaseConstantBounds;
                    AssertVerify(
                        landingPadIndexBoundBaseValue
                            ->GetValueInfo()
                            ->TryGetIntConstantBounds(&landingPadIndexBoundBaseConstantBounds, true));

                    int offset = indexBound.Offset();
                    if(searchingRelativeLowerBounds)
                    {LOGMEIN("GlobOptIntBounds.cpp] 2930\n");
                        if(offset == IntConstMin ||
                            ValueInfo::IsLessThan(
                                landingPadIndexBoundBaseValue,
                                landingPadIndexBoundBaseConstantBounds.LowerBound(),
                                landingPadIndexBoundBaseConstantBounds.UpperBound(),
                                nullptr,
                                -offset,
                                -offset))
                        {LOGMEIN("GlobOptIntBounds.cpp] 2939\n");
                            // indexBoundBase + indexBoundOffset < 0; can't use this bound
                            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 5, _u("Bound < 0\n"));
                            continue;
                        }

                        lowerHoistInfo.SetLoop(
                            loop,
                            indexBoundBaseSym,
                            offset,
                            landingPadIndexBoundBaseValue,
                            landingPadIndexBoundBaseConstantBounds);
                        break;
                    }

                    if(ValueInfo::IsLessThanOrEqualTo(
                            landingPadHeadSegmentLengthValue,
                            landingPadHeadSegmentLengthConstantBounds.LowerBound(),
                            landingPadHeadSegmentLengthConstantBounds.UpperBound(),
                            landingPadIndexBoundBaseValue,
                            landingPadIndexBoundBaseConstantBounds.LowerBound(),
                            landingPadIndexBoundBaseConstantBounds.UpperBound(),
                            offset))
                    {LOGMEIN("GlobOptIntBounds.cpp] 2962\n");
                        // indexBoundBase + indexBoundOffset >= headSegmentLength; can't use this bound
                        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 5, _u("Bound >= head segment length\n"));
                        continue;
                    }

                    // We need:
                    //     boundBase + boundOffset < headSegmentLength
                    // Normalize the offset such that:
                    //     boundBase <= headSegmentLength + offset
                    // Where (offset = -1 - boundOffset), and -1 is to simulate < instead of <=.
                    offset = -1 - offset;

                    upperHoistInfo.SetLoop(
                        loop,
                        indexBoundBaseSym,
                        offset,
                        landingPadIndexBoundBaseValue,
                        landingPadIndexBoundBaseConstantBounds,
                        landingPadHeadSegmentLengthValue,
                        landingPadHeadSegmentLengthConstantBounds);
                    break;
                }
            }
        }

        if(searchingLower && lowerHoistInfo.Loop() != loop)
        {LOGMEIN("GlobOptIntBounds.cpp] 2989\n");
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Lower bound was not found\n"));
            searchingLower = false;
            if(!searchingUpper)
            {LOGMEIN("GlobOptIntBounds.cpp] 2993\n");
                break;
            }
        }

        if(searchingUpper && upperHoistInfo.Loop() != loop)
        {LOGMEIN("GlobOptIntBounds.cpp] 2999\n");
            // No useful relative bound found; look for a constant bound if the index is an induction variable. Exclude constant
            // bounds of non-induction variables because those bounds may have been established through means other than a loop
            // exit condition, such as math or bitwise operations. Exclude constant bounds established implicitly by <,
            // <=, >, and >=. For example, for a loop condition (i < n - 1), if 'n' is not invariant and hence can't be used,
            // 'i' will still have a constant upper bound of (int32 max - 2) that should be excluded.
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Relative upper bound was not found\n"));
            const InductionVariable *indexInductionVariable;
            if(!upperHoistInfo.Loop() &&
                currentLoop->inductionVariables &&
                currentLoop->inductionVariables->TryGetReference(indexSym->m_id, &indexInductionVariable) &&
                indexInductionVariable->IsChangeDeterminate())
            {LOGMEIN("GlobOptIntBounds.cpp] 3011\n");
                if(!(indexBounds && indexBounds->WasConstantUpperBoundEstablishedExplicitly()))
                {LOGMEIN("GlobOptIntBounds.cpp] 3013\n");
                    TRACE_PHASE_VERBOSE(
                        Js::Phase::BoundCheckHoistPhase,
                        4,
                        _u("Constant upper bound was established implicitly\n"));
                }
                else
                {
                    // See if a compatible bound check is already available
                    const int indexConstantBound = indexBounds->ConstantUpperBound();
                    TRACE_PHASE_VERBOSE(
                        Js::Phase::BoundCheckHoistPhase,
                        4,
                        _u("Found constant upper bound %d, looking for a compatible bound check\n"),
                        indexConstantBound);
                    const IntBoundCheck *boundCheck;
                    if(blockData.availableIntBoundChecks->TryGetReference(
                            IntBoundCheckCompatibilityId(ZeroValueNumber, headSegmentLengthValue->GetValueNumber()),
                            &boundCheck))
                    {LOGMEIN("GlobOptIntBounds.cpp] 3032\n");
                        // We need:
                        //     indexConstantBound < headSegmentLength
                        // Normalize the offset such that:
                        //     0 <= headSegmentLength + compatibleBoundCheckOffset
                        // Where (compatibleBoundCheckOffset = -1 - indexConstantBound), and -1 is to simulate < instead of <=.
                        const int compatibleBoundCheckOffset = -1 - indexConstantBound;
                        if(boundCheck->SetBoundOffset(compatibleBoundCheckOffset))
                        {LOGMEIN("GlobOptIntBounds.cpp] 3040\n");
                            TRACE_PHASE_VERBOSE(
                                Js::Phase::BoundCheckHoistPhase,
                                5,
                                _u("Found in block %u\n"),
                                boundCheck->Block()->GetBlockNum());
                            upperHoistInfo.SetCompatibleBoundCheck(boundCheck->Block(), indexConstantBound);

                            needUpperBoundCheck = searchingUpper = false;
                            if(!needLowerBoundCheck)
                            {LOGMEIN("GlobOptIntBounds.cpp] 3050\n");
                                return;
                            }
                            if(!searchingLower)
                            {LOGMEIN("GlobOptIntBounds.cpp] 3054\n");
                                break;
                            }
                        }
                        else
                        {
                            failedToUpdateCompatibleUpperBoundCheck = true;
                        }
                    }

                    if(searchingUpper)
                    {LOGMEIN("GlobOptIntBounds.cpp] 3065\n");
                        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 5, _u("Not found\n"));
                        upperHoistInfo.SetLoop(
                            loop,
                            indexConstantBound,
                            landingPadHeadSegmentLengthValue,
                            landingPadHeadSegmentLengthConstantBounds);
                    }
                }
            }
            else if(!upperHoistInfo.Loop())
            {LOGMEIN("GlobOptIntBounds.cpp] 3076\n");
                TRACE_PHASE_VERBOSE(
                    Js::Phase::BoundCheckHoistPhase,
                    4,
                    _u("Index is not an induction variable, not using constant upper bound\n"));
            }

            if(searchingUpper && upperHoistInfo.Loop() != loop)
            {LOGMEIN("GlobOptIntBounds.cpp] 3084\n");
                TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Upper bound was not found\n"));
                searchingUpper = false;
                if(!searchingLower)
                {LOGMEIN("GlobOptIntBounds.cpp] 3088\n");
                    break;
                }
            }
        }
    }

    if(needLowerBoundCheck && lowerHoistInfo.Loop())
    {LOGMEIN("GlobOptIntBounds.cpp] 3096\n");
        needLowerBoundCheck = false;
        if(!needUpperBoundCheck)
        {LOGMEIN("GlobOptIntBounds.cpp] 3099\n");
            return;
        }
    }
    if(needUpperBoundCheck && upperHoistInfo.Loop())
    {LOGMEIN("GlobOptIntBounds.cpp] 3104\n");
        needUpperBoundCheck = false;
        if(!needLowerBoundCheck)
        {LOGMEIN("GlobOptIntBounds.cpp] 3107\n");
            return;
        }
    }

    // Try to use the loop count to calculate a missing lower/upper bound that in turn can be used for hoisting a bound check

    TRACE_PHASE_VERBOSE(
        Js::Phase::BoundCheckHoistPhase,
        2,
        _u("Looking for loop count based bound for loop %u landing pad block %u\n"),
        currentLoop->GetLoopNumber(),
        currentLoop->landingPad->GetBlockNum());

    LoopCount *const loopCount = currentLoop->loopCount;
    if(!loopCount)
    {LOGMEIN("GlobOptIntBounds.cpp] 3123\n");
        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Loop was not counted\n"));
        return;
    }

    const InductionVariable *indexInductionVariable;
    if(!currentLoop->inductionVariables ||
        !currentLoop->inductionVariables->TryGetReference(indexSym->m_id, &indexInductionVariable) ||
        !indexInductionVariable->IsChangeDeterminate())
    {LOGMEIN("GlobOptIntBounds.cpp] 3132\n");
        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Index is not an induction variable\n"));
        return;
    }

    // Determine the maximum-magnitude change per iteration, and verify that the change is reasonably finite
    Assert(indexInductionVariable->IsChangeUnidirectional());
    GlobOptBlockData &landingPadBlockData = currentLoop->landingPad->globOptData;
    GlobHashTable *const landingPadSymToValueMap = currentLoop->landingPad->globOptData.symToValueMap;
    int maxMagnitudeChange = indexInductionVariable->ChangeBounds().UpperBound();
    Value *landingPadHeadSegmentLengthValue;
    IntConstantBounds landingPadHeadSegmentLengthConstantBounds;
    if(maxMagnitudeChange > 0)
    {LOGMEIN("GlobOptIntBounds.cpp] 3145\n");
        TRACE_PHASE_VERBOSE(
            Js::Phase::BoundCheckHoistPhase,
            3,
            _u("Index's maximum-magnitude change per iteration is %d\n"),
            maxMagnitudeChange);
        if(!needUpperBoundCheck || maxMagnitudeChange > InductionVariable::ChangeMagnitudeLimitForLoopCountBasedHoisting)
        {LOGMEIN("GlobOptIntBounds.cpp] 3152\n");
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Change magnitude is too large\n"));
            return;
        }

        // Check whether the head segment length is available in the landing pad
        landingPadHeadSegmentLengthValue = FindValue(landingPadSymToValueMap, headSegmentLengthSym);
        Assert(!headSegmentLengthInvariantLoop || landingPadHeadSegmentLengthValue);
        if(!landingPadHeadSegmentLengthValue)
        {LOGMEIN("GlobOptIntBounds.cpp] 3161\n");
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Head segment length is not invariant\n"));
            return;
        }
        AssertVerify(
            landingPadHeadSegmentLengthValue
                ->GetValueInfo()
                ->TryGetIntConstantBounds(&landingPadHeadSegmentLengthConstantBounds));
    }
    else
    {
        maxMagnitudeChange = indexInductionVariable->ChangeBounds().LowerBound();
        Assert(maxMagnitudeChange < 0);
        TRACE_PHASE_VERBOSE(
            Js::Phase::BoundCheckHoistPhase,
            3,
            _u("Index's maximum-magnitude change per iteration is %d\n"),
            maxMagnitudeChange);
        if(!needLowerBoundCheck || maxMagnitudeChange < -InductionVariable::ChangeMagnitudeLimitForLoopCountBasedHoisting)
        {LOGMEIN("GlobOptIntBounds.cpp] 3180\n");
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Change magnitude is too large\n"));
            return;
        }

        landingPadHeadSegmentLengthValue = nullptr;
    }

    // Determine if the index already changed in the loop, and by how much
    int indexOffset;
    StackSym *indexSymToAdd;
    if(DetermineSymBoundOffsetOrValueRelativeToLandingPad(
            indexSym,
            maxMagnitudeChange >= 0,
            indexValueInfo,
            indexBounds,
            currentLoop->landingPad->globOptData.symToValueMap,
            &indexOffset))
    {LOGMEIN("GlobOptIntBounds.cpp] 3198\n");
        // The bound value is constant
        indexSymToAdd = nullptr;
    }
    else
    {
        // The bound value is not constant, the offset needs to be added to the index sym in the landing pad
        indexSymToAdd = indexSym;
    }
    TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Index's offset from landing pad is %d\n"), indexOffset);

    // The secondary induction variable bound is computed as follows:
    //     bound = index + indexOffset + loopCountMinusOne * maxMagnitudeChange
    //
    // If the loop count is constant, (inductionVariableOffset + loopCount * maxMagnitudeChange) can be folded into an offset:
    //     bound = index + offset
    int offset;
    StackSym *indexLoopCountBasedBoundBaseSym;
    Value *indexLoopCountBasedBoundBaseValue;
    IntConstantBounds indexLoopCountBasedBoundBaseConstantBounds;
    bool generateLoopCountBasedIndexBound;
    if(!loopCount->HasBeenGenerated() || loopCount->LoopCountMinusOneSym())
    {LOGMEIN("GlobOptIntBounds.cpp] 3220\n");
        if(loopCount->HasBeenGenerated())
        {LOGMEIN("GlobOptIntBounds.cpp] 3222\n");
            TRACE_PHASE_VERBOSE(
                Js::Phase::BoundCheckHoistPhase,
                3,
                _u("Loop count is assigned to s%u\n"),
                loopCount->LoopCountMinusOneSym()->m_id);
        }
        else
        {
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Loop count has not been generated yet\n"));
        }

        offset = indexOffset;

        // Check if there is already a loop count based bound sym for the index. If not, generate it.
        do
        {LOGMEIN("GlobOptIntBounds.cpp] 3238\n");
            const SymID indexSymId = indexSym->m_id;
            SymIdToStackSymMap *&loopCountBasedBoundBaseSyms = currentLoop->loopCountBasedBoundBaseSyms;
            if(!loopCountBasedBoundBaseSyms)
            {LOGMEIN("GlobOptIntBounds.cpp] 3242\n");
                loopCountBasedBoundBaseSyms = JitAnew(alloc, SymIdToStackSymMap, alloc);
            }
            else if(loopCountBasedBoundBaseSyms->TryGetValue(indexSymId, &indexLoopCountBasedBoundBaseSym))
            {LOGMEIN("GlobOptIntBounds.cpp] 3246\n");
                TRACE_PHASE_VERBOSE(
                    Js::Phase::BoundCheckHoistPhase,
                    3,
                    _u("Loop count based bound is assigned to s%u\n"),
                    indexLoopCountBasedBoundBaseSym->m_id);
                indexLoopCountBasedBoundBaseValue = FindValue(landingPadSymToValueMap, indexLoopCountBasedBoundBaseSym);
                Assert(indexLoopCountBasedBoundBaseValue);
                AssertVerify(
                    indexLoopCountBasedBoundBaseValue
                        ->GetValueInfo()
                        ->TryGetIntConstantBounds(&indexLoopCountBasedBoundBaseConstantBounds));
                generateLoopCountBasedIndexBound = false;
                break;
            }

            indexLoopCountBasedBoundBaseSym = StackSym::New(TyInt32, func);
            TRACE_PHASE_VERBOSE(
                Js::Phase::BoundCheckHoistPhase,
                3,
                _u("Assigning s%u to the loop count based bound\n"),
                indexLoopCountBasedBoundBaseSym->m_id);
            loopCountBasedBoundBaseSyms->Add(indexSymId, indexLoopCountBasedBoundBaseSym);
            indexLoopCountBasedBoundBaseValue = NewValue(ValueInfo::New(alloc, ValueType::GetInt(true)));
            SetValue(&landingPadBlockData, indexLoopCountBasedBoundBaseValue, indexLoopCountBasedBoundBaseSym);
            indexLoopCountBasedBoundBaseConstantBounds = IntConstantBounds(IntConstMin, IntConstMax);
            generateLoopCountBasedIndexBound = true;
        } while(false);
    }
    else
    {
        // The loop count is constant, fold (indexOffset + loopCountMinusOne * maxMagnitudeChange)
        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Loop count is constant, folding\n"));
        if(Int32Math::Mul(loopCount->LoopCountMinusOneConstantValue(), maxMagnitudeChange, &offset) ||
            Int32Math::Add(offset, indexOffset, &offset))
        {LOGMEIN("GlobOptIntBounds.cpp] 3281\n");
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Folding failed\n"));
            return;
        }

        if(!indexSymToAdd)
        {LOGMEIN("GlobOptIntBounds.cpp] 3287\n");
            // The loop count based bound is constant
            const int loopCountBasedConstantBound = offset;
            TRACE_PHASE_VERBOSE(
                Js::Phase::BoundCheckHoistPhase,
                3,
                _u("Loop count based bound is constant: %d\n"),
                loopCountBasedConstantBound);

            if(maxMagnitudeChange < 0)
            {LOGMEIN("GlobOptIntBounds.cpp] 3297\n");
                if(loopCountBasedConstantBound < 0)
                {LOGMEIN("GlobOptIntBounds.cpp] 3299\n");
                    // Can't use this bound
                    TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Bound < 0\n"));
                    return;
                }

                lowerHoistInfo.SetLoop(currentLoop, loopCountBasedConstantBound, true);
                return;
            }

            // loopCountBasedConstantBound >= headSegmentLength is currently not possible, except when
            // loopCountBasedConstantBound == int32 max
            Assert(
                loopCountBasedConstantBound == IntConstMax ||
                !ValueInfo::IsGreaterThanOrEqualTo(
                    nullptr,
                    loopCountBasedConstantBound,
                    loopCountBasedConstantBound,
                    landingPadHeadSegmentLengthValue,
                    landingPadHeadSegmentLengthConstantBounds.LowerBound(),
                    landingPadHeadSegmentLengthConstantBounds.UpperBound()));

            // See if a compatible bound check is already available
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Looking for a compatible bound check\n"));
            const IntBoundCheck *boundCheck;
            if(blockData.availableIntBoundChecks->TryGetReference(
                    IntBoundCheckCompatibilityId(ZeroValueNumber, headSegmentLengthValue->GetValueNumber()),
                    &boundCheck))
            {LOGMEIN("GlobOptIntBounds.cpp] 3327\n");
                // We need:
                //     loopCountBasedConstantBound < headSegmentLength
                // Normalize the offset such that:
                //     0 <= headSegmentLength + compatibleBoundCheckOffset
                // Where (compatibleBoundCheckOffset = -1 - loopCountBasedConstantBound), and -1 is to simulate < instead of <=.
                const int compatibleBoundCheckOffset = -1 - loopCountBasedConstantBound;
                if(boundCheck->SetBoundOffset(compatibleBoundCheckOffset, true))
                {LOGMEIN("GlobOptIntBounds.cpp] 3335\n");
                    TRACE_PHASE_VERBOSE(
                        Js::Phase::BoundCheckHoistPhase,
                        4,
                        _u("Found in block %u\n"),
                        boundCheck->Block()->GetBlockNum());
                    upperHoistInfo.SetCompatibleBoundCheck(boundCheck->Block(), loopCountBasedConstantBound);
                    return;
                }
                failedToUpdateCompatibleUpperBoundCheck = true;
            }
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Not found\n"));

            upperHoistInfo.SetLoop(
                currentLoop,
                loopCountBasedConstantBound,
                landingPadHeadSegmentLengthValue,
                landingPadHeadSegmentLengthConstantBounds,
                true);
            return;
        }

        // The loop count based bound is not constant; we need to add the offset of the index sym in the landing pad. Instead
        // of adding though, we will treat the index sym as the loop count based bound base sym and adjust the offset that will
        // be used in the bound check itself.
        indexLoopCountBasedBoundBaseSym = indexSymToAdd;
        indexLoopCountBasedBoundBaseValue = FindValue(landingPadSymToValueMap, indexSymToAdd);
        Assert(indexLoopCountBasedBoundBaseValue);
        AssertVerify(
            indexLoopCountBasedBoundBaseValue
                ->GetValueInfo()
                ->TryGetIntConstantBounds(&indexLoopCountBasedBoundBaseConstantBounds));
        generateLoopCountBasedIndexBound = false;
    }

    if(maxMagnitudeChange >= 0)
    {LOGMEIN("GlobOptIntBounds.cpp] 3371\n");
        // We need:
        //     indexLoopCountBasedBoundBase + indexOffset < headSegmentLength
        // Normalize the offset such that:
        //     indexLoopCountBasedBoundBase <= headSegmentLength + offset
        // Where (offset = -1 - indexOffset), and -1 is to simulate < instead of <=.
        offset = -1 - offset;
    }

    if(!generateLoopCountBasedIndexBound)
    {LOGMEIN("GlobOptIntBounds.cpp] 3381\n");
        if(maxMagnitudeChange < 0)
        {LOGMEIN("GlobOptIntBounds.cpp] 3383\n");
            if(offset != IntConstMax &&
                ValueInfo::IsGreaterThanOrEqualTo(
                    nullptr,
                    0,
                    0,
                    indexLoopCountBasedBoundBaseValue,
                    indexLoopCountBasedBoundBaseConstantBounds.LowerBound(),
                    indexLoopCountBasedBoundBaseConstantBounds.UpperBound(),
                    offset + 1)) // + 1 to simulate > instead of >=
            {LOGMEIN("GlobOptIntBounds.cpp] 3393\n");
                // loopCountBasedBound < 0, can't use this bound
                TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Bound < 0\n"));
                return;
            }
        }
        else if(
            ValueInfo::IsGreaterThanOrEqualTo(
                indexLoopCountBasedBoundBaseValue,
                indexLoopCountBasedBoundBaseConstantBounds.LowerBound(),
                indexLoopCountBasedBoundBaseConstantBounds.UpperBound(),
                landingPadHeadSegmentLengthValue,
                landingPadHeadSegmentLengthConstantBounds.LowerBound(),
                landingPadHeadSegmentLengthConstantBounds.UpperBound(),
                offset))
        {LOGMEIN("GlobOptIntBounds.cpp] 3408\n");
            // loopCountBasedBound >= headSegmentLength, can't use this bound
            TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Bound >= head segment length\n"));
            return;
        }

        // See if a compatible bound check is already available
        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 3, _u("Looking for a compatible bound check\n"));
        const ValueNumber indexLoopCountBasedBoundBaseValueNumber = indexLoopCountBasedBoundBaseValue->GetValueNumber();
        const IntBoundCheck *boundCheck;
        if(blockData.availableIntBoundChecks->TryGetReference(
                maxMagnitudeChange < 0
                    ?   IntBoundCheckCompatibilityId(
                            ZeroValueNumber,
                            indexLoopCountBasedBoundBaseValueNumber)
                    :   IntBoundCheckCompatibilityId(
                            indexLoopCountBasedBoundBaseValueNumber,
                            headSegmentLengthValue->GetValueNumber()),
                &boundCheck))
        {LOGMEIN("GlobOptIntBounds.cpp] 3427\n");
            if(boundCheck->SetBoundOffset(offset, true))
            {LOGMEIN("GlobOptIntBounds.cpp] 3429\n");
                TRACE_PHASE_VERBOSE(
                    Js::Phase::BoundCheckHoistPhase,
                    4,
                    _u("Found in block %u\n"),
                    boundCheck->Block()->GetBlockNum());
                if(maxMagnitudeChange < 0)
                {LOGMEIN("GlobOptIntBounds.cpp] 3436\n");
                    lowerHoistInfo.SetCompatibleBoundCheck(
                        boundCheck->Block(),
                        indexLoopCountBasedBoundBaseSym,
                        offset,
                        indexLoopCountBasedBoundBaseValueNumber);
                }
                else
                {
                    upperHoistInfo.SetCompatibleBoundCheck(
                        boundCheck->Block(),
                        indexLoopCountBasedBoundBaseSym,
                        offset,
                        indexLoopCountBasedBoundBaseValueNumber);
                }
                return;
            }
            (maxMagnitudeChange < 0 ? failedToUpdateCompatibleLowerBoundCheck : failedToUpdateCompatibleUpperBoundCheck) = true;
        }
        TRACE_PHASE_VERBOSE(Js::Phase::BoundCheckHoistPhase, 4, _u("Not found\n"));
    }

    if(maxMagnitudeChange < 0)
    {LOGMEIN("GlobOptIntBounds.cpp] 3459\n");
        lowerHoistInfo.SetLoop(
            currentLoop,
            indexLoopCountBasedBoundBaseSym,
            offset,
            indexLoopCountBasedBoundBaseValue,
            indexLoopCountBasedBoundBaseConstantBounds,
            true);
        if(generateLoopCountBasedIndexBound)
        {LOGMEIN("GlobOptIntBounds.cpp] 3468\n");
            lowerHoistInfo.SetLoopCount(loopCount, maxMagnitudeChange);
        }
        return;
    }

    upperHoistInfo.SetLoop(
        currentLoop,
        indexLoopCountBasedBoundBaseSym,
        offset,
        indexLoopCountBasedBoundBaseValue,
        indexLoopCountBasedBoundBaseConstantBounds,
        landingPadHeadSegmentLengthValue,
        landingPadHeadSegmentLengthConstantBounds,
        true);
    if(generateLoopCountBasedIndexBound)
    {LOGMEIN("GlobOptIntBounds.cpp] 3484\n");
        upperHoistInfo.SetLoopCount(loopCount, maxMagnitudeChange);
    }
}
