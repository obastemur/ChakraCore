//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"


#if ENABLE_DEBUG_CONFIG_OPTIONS
#define BAILOUT_VERBOSE_TRACE(functionBody, ...) \
    if (Js::Configuration::Global.flags.Verbose && Js::Configuration::Global.flags.Trace.IsEnabled(Js::BailOutPhase,functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId())) \
    {TRACE_IT(9312); \
        Output::Print(__VA_ARGS__); \
    }

#define BAILOUT_FLUSH(functionBody) \
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::BailOutPhase, functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId()) || \
    Js::Configuration::Global.flags.Trace.IsEnabled(Js::BailOutPhase, functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId())) \
    {TRACE_IT(9313); \
        Output::Flush(); \
    }
#else
#define BAILOUT_VERBOSE_TRACE(functionBody, bailOutKind, ...)
#define BAILOUT_FLUSH(functionBody)
#endif


unsigned int NativeOffsetInlineeFrameRecordOffset::InvalidRecordOffset = (unsigned int)(-1);

void BailoutConstantValue::InitVarConstValue(Js::Var value)
{TRACE_IT(9314);
    this->type = TyVar;
    this->u.varConst.value = value;
}

Js::Var BailoutConstantValue::ToVar(Func* func) const
{TRACE_IT(9315);
    Assert(this->type == TyVar || this->type == TyFloat64 || IRType_IsSignedInt(this->type));
    Js::Var varValue;
    if (this->type == TyVar)
    {TRACE_IT(9316);
        varValue = this->u.varConst.value;
    }
    else if (this->type == TyFloat64)
    {TRACE_IT(9317);
        varValue = func->AllocateNumber((double)this->u.floatConst.value);
    }
    else if (IRType_IsSignedInt(this->type) && TySize[this->type] <= 4 && !Js::TaggedInt::IsOverflow((int32)this->u.intConst.value))
    {TRACE_IT(9318);
        varValue = Js::TaggedInt::ToVarUnchecked((int32)this->u.intConst.value);
    }
    else
    {TRACE_IT(9319);
        varValue = func->AllocateNumber((double)this->u.intConst.value);
    }
    return varValue;

}

bool BailoutConstantValue::IsEqual(const BailoutConstantValue & bailoutConstValue)
{TRACE_IT(9320);
    if (this->type == bailoutConstValue.type)
    {TRACE_IT(9321);
        if (this->type == TyInt32)
        {TRACE_IT(9322);
            return this->u.intConst.value == bailoutConstValue.u.intConst.value;
        }
        else if (this->type == TyVar)
        {TRACE_IT(9323);
            return this->u.varConst.value == bailoutConstValue.u.varConst.value;
        }
        else
        {TRACE_IT(9324);
            return this->u.floatConst.value == bailoutConstValue.u.floatConst.value;
        }
    }
    return false;
}


void InlineeFrameInfo::AllocateRecord(Func* func, intptr_t functionBodyAddr)
{TRACE_IT(9325);
    uint constantCount = 0;

    // If there are no helper calls there is a chance that frame record is not required after all;
    arguments->Map([&](uint index, InlineFrameInfoValue& value){
        if (value.IsConst())
        {TRACE_IT(9326);
            constantCount++;
        }
    });

    if (function.IsConst())
    {TRACE_IT(9327);
        constantCount++;
    }

    // For InlineeEnd's that have been cloned we can result in multiple calls
    // to allocate the record - do not allocate a new record - instead update the existing one.
    // In particular, if the first InlineeEnd resulted in no calls and spills, subsequent might still spill - so it's a good idea to
    // update the record
    if (!this->record)
    {TRACE_IT(9328);
        this->record = InlineeFrameRecord::New(func->GetNativeCodeDataAllocator(), (uint)arguments->Count(), constantCount, functionBodyAddr, this);
    }

    uint i = 0;
    uint constantIndex = 0;
    arguments->Map([&](uint index, InlineFrameInfoValue& value){
        Assert(value.type != InlineeFrameInfoValueType_None);
        if (value.type == InlineeFrameInfoValueType_Sym)
        {TRACE_IT(9329);
            int offset;
#ifdef MD_GROW_LOCALS_AREA_UP
            offset = -((int)value.sym->m_offset + BailOutInfo::StackSymBias);
#else
            // Stack offset are negative, includes the PUSH EBP and return address
            offset = value.sym->m_offset - (2 * MachPtr);
#endif
            Assert(offset < 0);
            this->record->argOffsets[i] = offset;
            if (value.sym->IsFloat64())
            {TRACE_IT(9330);
                this->record->floatArgs.Set(i);
            }
            else if (value.sym->IsInt32())
            {TRACE_IT(9331);
                this->record->losslessInt32Args.Set(i);
            }
        }
        else
        {TRACE_IT(9332);
            // Constants
            Assert(constantIndex < constantCount);
            this->record->constants[constantIndex] = value.constValue.ToVar(func);
            this->record->argOffsets[i] = constantIndex;
            constantIndex++;
        }
        i++;
    });

    if (function.type == InlineeFrameInfoValueType_Sym)
    {TRACE_IT(9333);
        int offset;

#ifdef MD_GROW_LOCALS_AREA_UP
        offset = -((int)function.sym->m_offset + BailOutInfo::StackSymBias);
#else
        // Stack offset are negative, includes the PUSH EBP and return address
        offset = function.sym->m_offset - (2 * MachPtr);
#endif
        this->record->functionOffset = offset;
    }
    else
    {TRACE_IT(9334);
        Assert(constantIndex < constantCount);
        this->record->constants[constantIndex] = function.constValue.ToVar(func);
        this->record->functionOffset = constantIndex;
    }
}

void InlineeFrameRecord::PopulateParent(Func* func)
{TRACE_IT(9335);
    Assert(this->parent == nullptr);
    Assert(!func->IsTopFunc());
    if (func->GetParentFunc()->m_hasInlineArgsOpt)
    {TRACE_IT(9336);
        this->parent = func->GetParentFunc()->frameInfo->record;
        Assert(this->parent != nullptr);
    }
}

void InlineeFrameRecord::Finalize(Func* inlinee, uint32 currentOffset)
{TRACE_IT(9337);
    this->PopulateParent(inlinee);
    this->inlineeStartOffset = currentOffset;
    this->inlineDepth = inlinee->inlineDepth;

#ifdef MD_GROW_LOCALS_AREA_UP
    Func* topFunc = inlinee->GetTopFunc();
    int32 inlineeArgStackSize = topFunc->GetInlineeArgumentStackSize();
    int localsSize = topFunc->m_localStackHeight + topFunc->m_ArgumentsOffset;

    this->MapOffsets([=](int& offset)
    {
        int realOffset = -(offset + BailOutInfo::StackSymBias);
        if (realOffset < 0)
        {TRACE_IT(9338);
            // Not stack offset
            return;
        }
        // The locals size contains the inlined-arg-area size, so remove the inlined-arg-area size from the
        // adjustment for normal locals whose offsets are relative to the start of the locals area.

        realOffset -= (localsSize - inlineeArgStackSize);
        offset = realOffset;
    });
#endif

    Assert(this->inlineDepth != 0);
}

void InlineeFrameRecord::Restore(Js::FunctionBody* functionBody, InlinedFrameLayout *inlinedFrame, Js::JavascriptCallStackLayout * layout) const
{TRACE_IT(9339);
    Assert(this->inlineDepth != 0);
    Assert(inlineeStartOffset != 0);

    BAILOUT_VERBOSE_TRACE(functionBody, _u("Restore function object: "));
    Js::Var varFunction =  this->Restore(this->functionOffset, /*isFloat64*/ false, /*isInt32*/ false, layout, functionBody);
    Assert(Js::ScriptFunction::Is(varFunction));

    Js::ScriptFunction* function = Js::ScriptFunction::FromVar(varFunction);
    BAILOUT_VERBOSE_TRACE(functionBody, _u("Inlinee: %s [%d.%d] \n"), function->GetFunctionBody()->GetDisplayName(), function->GetFunctionBody()->GetSourceContextId(), function->GetFunctionBody()->GetLocalFunctionId());

    inlinedFrame->function = function;
    inlinedFrame->callInfo.InlineeStartOffset = inlineeStartOffset;
    inlinedFrame->callInfo.Count = this->argCount;
    inlinedFrame->MapArgs([=](uint i, Js::Var* varRef) {
        bool isFloat64 = floatArgs.Test(i) != 0;
        bool isInt32 = losslessInt32Args.Test(i) != 0;
        BAILOUT_VERBOSE_TRACE(functionBody, _u("Restore argument %d: "), i);

        Js::Var var = this->Restore(this->argOffsets[i], isFloat64, isInt32, layout, functionBody);
#if DBG
        if (!Js::TaggedNumber::Is(var))
        {TRACE_IT(9340);
            Js::RecyclableObject *const recyclableObject = Js::RecyclableObject::FromVar(var);
            Assert(!ThreadContext::IsOnStack(recyclableObject));
        }
#endif
        *varRef = var;
    });
    inlinedFrame->arguments = nullptr;
    BAILOUT_FLUSH(functionBody);
}

void InlineeFrameRecord::RestoreFrames(Js::FunctionBody* functionBody, InlinedFrameLayout* outerMostFrame, Js::JavascriptCallStackLayout* callstack)
{TRACE_IT(9341);
    InlineeFrameRecord* innerMostRecord = this;
    class AutoReverse
    {
    public:
        InlineeFrameRecord* record;
        AutoReverse(InlineeFrameRecord* record)
        {TRACE_IT(9342);
            this->record = record->Reverse();
        }

        ~AutoReverse()
        {TRACE_IT(9343);
            record->Reverse();
        }
    } autoReverse(innerMostRecord);

    InlineeFrameRecord* currentRecord = autoReverse.record;
    InlinedFrameLayout* currentFrame = outerMostFrame;

    int inlineDepth = 1;

    // Find an inlined frame that needs to be restored.
    while (currentFrame->callInfo.Count != 0)
    {TRACE_IT(9344);
        inlineDepth++;
        currentFrame = currentFrame->Next();
    }

    // Align the inline depth of the record with the frame that needs to be restored
    while (currentRecord && currentRecord->inlineDepth != inlineDepth)
    {TRACE_IT(9345);
        currentRecord = currentRecord->parent;
    }

    while (currentRecord)
    {TRACE_IT(9346);
        currentRecord->Restore(functionBody, currentFrame, callstack);
        currentRecord = currentRecord->parent;
        currentFrame = currentFrame->Next();
    }

    // Terminate the inlined stack
    currentFrame->callInfo.Count = 0;
}

Js::Var InlineeFrameRecord::Restore(int offset, bool isFloat64, bool isInt32, Js::JavascriptCallStackLayout * layout, Js::FunctionBody* functionBody) const
{TRACE_IT(9347);
    Js::Var value;
    bool boxStackInstance = true;
    double dblValue;
    if (offset >= 0)
    {TRACE_IT(9348);
        Assert(static_cast<uint>(offset) < constantCount);
        value = this->constants[offset];
        boxStackInstance = false;
    }
    else
    {
        BAILOUT_VERBOSE_TRACE(functionBody, _u("Stack offset %10d"), offset);
        if (isFloat64)
        {TRACE_IT(9349);
            dblValue = layout->GetDoubleAtOffset(offset);
            value = Js::JavascriptNumber::New(dblValue, functionBody->GetScriptContext());
            BAILOUT_VERBOSE_TRACE(functionBody, _u(", value: %f (ToVar: 0x%p)"), dblValue, value);
        }
        else if (isInt32)
        {TRACE_IT(9350);
            value = (Js::Var)layout->GetInt32AtOffset(offset);
        }
        else
        {TRACE_IT(9351);
            value = layout->GetOffset(offset);
        }
    }

    if (isInt32)
    {TRACE_IT(9352);
        int32 int32Value = ::Math::PointerCastToIntegralTruncate<int32>(value);
        value = Js::JavascriptNumber::ToVar(int32Value, functionBody->GetScriptContext());
        BAILOUT_VERBOSE_TRACE(functionBody, _u(", value: %10d (ToVar: 0x%p)"), int32Value, value);
    }
    else
    {
        BAILOUT_VERBOSE_TRACE(functionBody, _u(", value: 0x%p"), value);
        if (boxStackInstance)
        {TRACE_IT(9353);
            Js::Var oldValue = value;
            value = Js::JavascriptOperators::BoxStackInstance(oldValue, functionBody->GetScriptContext(), /* allowStackFunction */ true);

#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (oldValue != value)
            {
                BAILOUT_VERBOSE_TRACE(functionBody, _u(" (Boxed: 0x%p)"), value);
            }
#endif
        }
    }
    BAILOUT_VERBOSE_TRACE(functionBody, _u("\n"));
    return value;
}

InlineeFrameRecord* InlineeFrameRecord::Reverse()
{TRACE_IT(9354);
    InlineeFrameRecord * prev = nullptr;
    InlineeFrameRecord * current = this;
    while (current)
    {TRACE_IT(9355);
        InlineeFrameRecord * next = current->parent;
        current->parent = prev;
        prev = current;
        current = next;
    }
    return prev;
}

#if DBG_DUMP

void InlineeFrameRecord::Dump() const
{TRACE_IT(9356);
    Output::Print(_u("%s [#%u.%u] args:"), this->functionBody->GetExternalDisplayName(), this->functionBody->GetSourceContextId(), this->functionBody->GetLocalFunctionId());
    for (uint i = 0; i < argCount; i++)
    {TRACE_IT(9357);
        DumpOffset(argOffsets[i]);
        if (floatArgs.Test(i))
        {TRACE_IT(9358);
            Output::Print(_u("f "));
        }
        else if (losslessInt32Args.Test(i))
        {TRACE_IT(9359);
            Output::Print(_u("i "));
        }
        Output::Print(_u(", "));
    }
    this->frameInfo->Dump();

    Output::Print(_u("func: "));
    DumpOffset(functionOffset);

    if (this->parent)
    {TRACE_IT(9360);
        parent->Dump();
    }
}

void InlineeFrameRecord::DumpOffset(int offset) const
{TRACE_IT(9361);
    if (offset >= 0)
    {TRACE_IT(9362);
        Output::Print(_u("%p "), constants[offset]);
    }
    else
    {TRACE_IT(9363);
        Output::Print(_u("<%d> "), offset);
    }
}

void InlineeFrameInfo::Dump() const
{TRACE_IT(9364);
    Output::Print(_u("func: "));
    if (this->function.type == InlineeFrameInfoValueType_Const)
    {TRACE_IT(9365);
        Output::Print(_u("%p(Var) "), this->function.constValue);
    }
    else if (this->function.type == InlineeFrameInfoValueType_Sym)
    {TRACE_IT(9366);
        this->function.sym->Dump();
        Output::Print(_u(" "));
    }

    Output::Print(_u("args: "));
    arguments->Map([=](uint i, InlineFrameInfoValue& value)
    {
        if (value.type == InlineeFrameInfoValueType_Const)
        {TRACE_IT(9367);
            Output::Print(_u("%p(Var) "), value.constValue);
        }
        else if (value.type == InlineeFrameInfoValueType_Sym)
        {TRACE_IT(9368);
            value.sym->Dump();
            Output::Print(_u(" "));
        }
        Output::Print(_u(", "));
    });
}
#endif
