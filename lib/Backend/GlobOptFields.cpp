//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"
/*
Field Hoisting
--------------
The backward pass calculates field load values that are reachable from the loop top.
It optimistically assumes that a[] doesn't kill any fields in the hopes that glob opt
will have more information to not kill the field.

During the forward pass the root prepass will assume that the field hoist candidate
is live (set the livefields bitvector). (GlobOpt::PreparePrepassFieldHoisting)
The "hoistable field" bitvector is used to keep track of whether the current instruction
may have the initial field value at the loop top. If so, they are hoistable.
Even when the value is only visible at the loop top from one path, there is benefit in
hoisting the field.

e.g 1. We can hoist the field in this case and get benefit:
    while {
        if  {
            o.x =       <== kills field value
        }
        = o.x           <== only has the loop top field value on the "!if" path
    }
After hoisting the field:
    s1 = o.x            <== hoisted field load
    while {
        if {
            o.x =
            s1 =        <== maintain the hoisted field value
        }
        = s1            <== avoided a field load
    }

When we identify a field load as hoistable, we will add the instruction to a list on the loop.

After the prepass, we will determine the fields that we are going to hoist from the
field candidates. (GlobOpt::PrepareFieldHoisting)

If it is not live - even if we detect a hoistable load - it is not beneficial to hoist
the load as we will have to insert a field load to compensate for the loop back edges.
We will just rely on field copy prop to optimize in that case.

e.g 2. Hoisting this require us to add a field load back at the end of the loop with no benefit.
    while {
        = o.x           <== hoistable field but isn't live on back edge
        b.x =           <== kills o.x as o and b may be aliased
    }

If it is live on back edge then it is possible to hoist the field load.

e.g 3. Although the field is killed, if the value is live on back edge we can still hoist it.
    while {
        = o.x
        = o.x
        b.x =
        = o.x
    }
After hoisting the field, s1 is live for the whole loop
    s1 = o.x
    while {
        = s1            <== eliminated one field load
        = s1            <== copy prop
        b.x =
        s1 = o.x
    }

However, since our register allocator doesn't handle long lifetimes, copy prop may do a better job.
We would only replace one field load - instead of two.
(Currently we hoist in this case)

e.g. 4. Live time of s1 is much shorter, which works better with our current register allocator.
    while {
        s1 = o.x
        = s1            <== copy prop
        b.x =
        s1 = o.x
    }

May want to add heuristics to determine whether to hoist by looking at the number of field
loads we can replace. See unittest\fieldopts\fieldhoist5.js for timing with various -off/-force
of fieldhoist/fieldcopyprop. Currently, field hoist is better or the same as field copy prop
except for kill_singleuse in the test where we can only eliminate one field load compared to copy prop
If we ever improve the register allocator to do better, we might lift this restriction.

In GlobOpt::PrepareFieldHoisting, we go through all the hoistable field loads that are live on the back edge.
We create a preassigned symbol for the hoisted field and add it to the fieldHoistSymMap of the loop.
If the value is live coming into the loop (via field copy prop, we will create the instruction to
assign the value to the preassigned sym. (GlobOpt::HoistFieldLoadValue)
If we don't know the value yet, we will create the load field instead. (GlobOpt::HoistFieldLoad)

As we are processing instructions in the non-prepass, when we see a field load, if it is live already then we have
a value in the preassigned sym and we can just replace the load. (GlobOpt::CopyPropHoistedFields)
If it is not live, then we don't have the value of the field, so keep the field load, but also
assign the loaded value to the preassigned sym. (GlobOpt::ReloadFieldHoistStackSym)

If the instruction is a store of a hoisted field, then create an assignment of the value to the preassigned
symbol to maintain a live field value. (GlobOpt::CopyStoreFieldHoistStackSym)
*/

bool
GlobOpt::DoFieldCopyProp() const
{TRACE_IT(5959);
    BasicBlock *block = this->currentBlock;
    Loop *loop = block->loop;
    if (this->isRecursiveCallOnLandingPad)
    {TRACE_IT(5960);
        // The landing pad at this point only contains load hosted by PRE.
        // These need to be copy-prop'd into the loop.
        // We want to look at the implicit-call info of the loop, not it's parent.

        Assert(block->IsLandingPad());
        loop = block->next->loop;
        Assert(loop);
    }

    return DoFieldCopyProp(loop);
}

bool
GlobOpt::DoFunctionFieldCopyProp() const
{TRACE_IT(5961);
    return DoFieldCopyProp(nullptr);
}

bool
GlobOpt::DoFieldCopyProp(Loop * loop) const
{TRACE_IT(5962);
    if (PHASE_OFF(Js::CopyPropPhase, this->func))
    {TRACE_IT(5963);
        // Can't do field copy prop without copy prop
        return false;
    }

    if (PHASE_FORCE(Js::FieldCopyPropPhase, this->func))
    {TRACE_IT(5964);
        // Force always turns on field copy prop
        return true;
    }

    if (this->DoFieldHoisting(loop))
    {TRACE_IT(5965);
        // Have to do field copy prop when we are doing field hoisting
        return true;
    }

    if (PHASE_OFF(Js::FieldCopyPropPhase, this->func))
    {TRACE_IT(5966);
        return false;
    }

    return this->DoFieldOpts(loop);
}

bool
GlobOpt::DoFieldHoisting(Loop *loop)
{TRACE_IT(5967);
    if (loop == nullptr)
    {TRACE_IT(5968);
        return false;
    }

    Func * func = loop->GetHeadBlock()->GetFirstInstr()->m_func->GetTopFunc();
    if (PHASE_OFF(Js::CopyPropPhase, func))
    {TRACE_IT(5969);
        // Can't do field hoisting without copy prop
        return false;
    }

    if (PHASE_OFF(Js::FieldHoistPhase, func))
    {TRACE_IT(5970);
        return false;
    }

    if (!PHASE_OFF(Js::FieldPREPhase, func))
    {TRACE_IT(5971);
        return false;
    }

    if (PHASE_FORCE(Js::FieldHoistPhase, func))
    {TRACE_IT(5972);
        // Force always turns on field hoisting
        return true;
    }

    return loop->CanDoFieldHoist();
}

bool
GlobOpt::DoFieldHoisting() const
{TRACE_IT(5973);
    return this->DoFieldHoisting(this->currentBlock->loop);
}

bool
GlobOpt::DoObjTypeSpec() const
{TRACE_IT(5974);
    return this->DoObjTypeSpec(this->currentBlock->loop);
}

bool
GlobOpt::DoObjTypeSpec(Loop *loop) const
{TRACE_IT(5975);
    if (!this->func->DoFastPaths())
    {TRACE_IT(5976);
        return false;
    }
    if (PHASE_FORCE(Js::ObjTypeSpecPhase, this->func))
    {TRACE_IT(5977);
        return true;
    }
    if (PHASE_OFF(Js::ObjTypeSpecPhase, this->func))
    {TRACE_IT(5978);
        return false;
    }
    if (this->func->IsLoopBody() && this->func->HasProfileInfo() && this->func->GetReadOnlyProfileInfo()->IsObjTypeSpecDisabledInJitLoopBody())
    {TRACE_IT(5979);
        return false;
    }
    if (this->ImplicitCallFlagsAllowOpts(this->func))
    {TRACE_IT(5980);
        Assert(loop == nullptr || loop->CanDoFieldCopyProp());
        return true;
    }
    return loop != nullptr && loop->CanDoFieldCopyProp();
}

bool
GlobOpt::DoFieldOpts(Loop * loop) const
{TRACE_IT(5981);
    if (this->ImplicitCallFlagsAllowOpts(this->func))
    {TRACE_IT(5982);
        Assert(loop == nullptr || loop->CanDoFieldCopyProp());
        return true;
    }
    return loop != nullptr && loop->CanDoFieldCopyProp();
}

bool GlobOpt::DoFieldPRE() const
{TRACE_IT(5983);
    Loop *loop = this->currentBlock->loop;

    return DoFieldPRE(loop);
}

bool
GlobOpt::DoFieldPRE(Loop *loop) const
{TRACE_IT(5984);
    if (PHASE_OFF(Js::FieldPREPhase, this->func))
    {TRACE_IT(5985);
        return false;
    }

    if (PHASE_FORCE(Js::FieldPREPhase, func))
    {TRACE_IT(5986);
        // Force always turns on field PRE
        return true;
    }

    return DoFieldOpts(loop);
}

bool GlobOpt::HasMemOp(Loop *loop)
{TRACE_IT(5987);
#pragma prefast(suppress: 6285, "logical-or of constants is by design")
    return (
        loop &&
        loop->doMemOp &&
        (
            !PHASE_OFF(Js::MemSetPhase, this->func) ||
            !PHASE_OFF(Js::MemCopyPhase, this->func)
        ) &&
        loop->memOpInfo &&
        loop->memOpInfo->candidates &&
        !loop->memOpInfo->candidates->Empty()
    );
}

bool
GlobOpt::TrackHoistableFields() const
{TRACE_IT(5988);
    return this->IsLoopPrePass() && this->currentBlock->loop == this->prePassLoop;
}

void
GlobOpt::KillLiveFields(StackSym * stackSym, BVSparse<JitArenaAllocator> * bv)
{TRACE_IT(5989);
    if (stackSym->IsTypeSpec())
    {TRACE_IT(5990);
        stackSym = stackSym->GetVarEquivSym(this->func);
    }
    Assert(stackSym);

    // If the sym has no objectSymInfo, it must not represent an object and, hence, has no type sym or
    // property syms to kill.
    if (!stackSym->HasObjectInfo())
    {TRACE_IT(5991);
        return;
    }

    // Note that the m_writeGuardSym is killed here as well, because it is part of the
    // m_propertySymList of the object.
    ObjectSymInfo * objectSymInfo = stackSym->GetObjectInfo();
    PropertySym * propertySym = objectSymInfo->m_propertySymList;
    while (propertySym != nullptr)
    {TRACE_IT(5992);
        Assert(propertySym->m_stackSym == stackSym);
        bv->Clear(propertySym->m_id);
        if (this->IsLoopPrePass())
        {TRACE_IT(5993);
            for (Loop * loop = this->rootLoopPrePass; loop != nullptr; loop = loop->parent)
            {TRACE_IT(5994);
                loop->fieldKilled->Set(propertySym->m_id);
            }
        }
        else if (bv->IsEmpty())
        {TRACE_IT(5995);
            // shortcut
            break;
        }
        propertySym = propertySym->m_nextInStackSymList;
    }

    this->KillObjectType(stackSym, bv);
}

void
GlobOpt::KillLiveFields(PropertySym * propertySym, BVSparse<JitArenaAllocator> * bv)
{TRACE_IT(5996);
    KillLiveFields(propertySym->m_propertyEquivSet, bv);
}

void GlobOpt::KillLiveFields(BVSparse<JitArenaAllocator> *const propertyEquivSet, BVSparse<JitArenaAllocator> *const bv) const
{TRACE_IT(5997);
    Assert(bv);

    if (propertyEquivSet)
    {TRACE_IT(5998);
        bv->Minus(propertyEquivSet);

        if (this->IsLoopPrePass())
        {TRACE_IT(5999);
            for (Loop * loop = this->rootLoopPrePass; loop != nullptr; loop = loop->parent)
            {TRACE_IT(6000);
                loop->fieldKilled->Or(propertyEquivSet);
            }
        }
    }
}

void
GlobOpt::KillLiveElems(IR::IndirOpnd * indirOpnd, BVSparse<JitArenaAllocator> * bv, bool inGlobOpt, Func *func)
{TRACE_IT(6001);
    IR::RegOpnd *indexOpnd = indirOpnd->GetIndexOpnd();

    // obj.x = 10;
    // obj["x"] = ...;   // This needs to kill obj.x...  We need to kill all fields...
    //
    // Also, 'arguments[i] =' needs to kill all slots even if 'i' is an int.
    //
    // NOTE: we only need to kill slots here, not all fields. It may be good to separate these one day.
    //
    // Regarding the check for type specialization:
    // - Type specialization does not always update the value to a definite type.
    // - The loop prepass is conservative on values even when type specialization occurs.
    // - We check the type specialization status for the sym as well. For the purpose of doing kills, we can assume that
    //   if type specialization happened, that fields don't need to be killed. Note that they may be killed in the next
    //   pass based on the value.
    if (func->GetThisOrParentInlinerHasArguments() ||
        (
            indexOpnd &&
            (
                indexOpnd->m_sym->m_isNotInt ||
                (inGlobOpt && !indexOpnd->GetValueType().IsNumber() && !IsTypeSpecialized(indexOpnd->m_sym, &blockData))
            )
        ))
    {TRACE_IT(6002);
        this->KillAllFields(bv); // This also kills all property type values, as the same bit-vector tracks those stack syms
        SetAnyPropertyMayBeWrittenTo();
    }
}

void
GlobOpt::KillAllFields(BVSparse<JitArenaAllocator> * bv)
{TRACE_IT(6003);
    bv->ClearAll();
    if (this->IsLoopPrePass())
    {TRACE_IT(6004);
        for (Loop * loop = this->rootLoopPrePass; loop != nullptr; loop = loop->parent)
        {TRACE_IT(6005);
            loop->allFieldsKilled = true;
        }
    }
}

void
GlobOpt::SetAnyPropertyMayBeWrittenTo()
{TRACE_IT(6006);
    this->func->anyPropertyMayBeWrittenTo = true;
}

void
GlobOpt::AddToPropertiesWrittenTo(Js::PropertyId propertyId)
{TRACE_IT(6007);
    this->func->EnsurePropertiesWrittenTo();
    this->func->propertiesWrittenTo->Item(propertyId);
}

void
GlobOpt::ProcessFieldKills(IR::Instr *instr, BVSparse<JitArenaAllocator> *bv, bool inGlobOpt)
{TRACE_IT(6008);
    if (bv->IsEmpty() && (!this->IsLoopPrePass() || this->rootLoopPrePass->allFieldsKilled))
    {TRACE_IT(6009);
        return;
    }

    if (instr->m_opcode == Js::OpCode::FromVar || instr->m_opcode == Js::OpCode::Conv_Prim)
    {TRACE_IT(6010);
        return;
    }

    IR::Opnd * dstOpnd = instr->GetDst();
    if (dstOpnd)
    {TRACE_IT(6011);
        if (dstOpnd->IsRegOpnd())
        {TRACE_IT(6012);
            Sym * sym = dstOpnd->AsRegOpnd()->m_sym;
            if (sym->IsStackSym())
            {TRACE_IT(6013);
                KillLiveFields(sym->AsStackSym(), bv);
            }
        }
        else if (dstOpnd->IsSymOpnd())
        {TRACE_IT(6014);
            Sym * sym = dstOpnd->AsSymOpnd()->m_sym;
            if (sym->IsStackSym())
            {TRACE_IT(6015);
                KillLiveFields(sym->AsStackSym(), bv);
            }
            else
            {TRACE_IT(6016);
                Assert(sym->IsPropertySym());
                if (instr->m_opcode == Js::OpCode::InitLetFld || instr->m_opcode == Js::OpCode::InitConstFld || instr->m_opcode == Js::OpCode::InitFld)
                {TRACE_IT(6017);
                    // These can grow the aux slot of the activation object.
                    // We need to kill the slot array sym as well.
                    PropertySym * slotArraySym = PropertySym::Find(sym->AsPropertySym()->m_stackSym->m_id,
                        (Js::DynamicObject::GetOffsetOfAuxSlots())/sizeof(Js::Var) /*, PropertyKindSlotArray */, instr->m_func);
                    if (slotArraySym)
                    {TRACE_IT(6018);
                        bv->Clear(slotArraySym->m_id);
                    }
                }
            }
        }
    }

    if (bv->IsEmpty() && (!this->IsLoopPrePass() || this->rootLoopPrePass->allFieldsKilled))
    {TRACE_IT(6019);
        return;
    }

    Sym *sym;
    IR::JnHelperMethod fnHelper;
    switch(instr->m_opcode)
    {
    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
        Assert(dstOpnd != nullptr);
        KillLiveFields(this->lengthEquivBv, bv);
        KillLiveElems(dstOpnd->AsIndirOpnd(), bv, inGlobOpt, instr->m_func);
        break;

    case Js::OpCode::DeleteElemI_A:
    case Js::OpCode::DeleteElemIStrict_A:
        Assert(dstOpnd != nullptr);
        KillLiveElems(instr->GetSrc1()->AsIndirOpnd(), bv, inGlobOpt, instr->m_func);
        break;

    case Js::OpCode::DeleteFld:
    case Js::OpCode::DeleteRootFld:
    case Js::OpCode::DeleteFldStrict:
    case Js::OpCode::DeleteRootFldStrict:
        sym = instr->GetSrc1()->AsSymOpnd()->m_sym;
        KillLiveFields(sym->AsPropertySym(), bv);
        if (inGlobOpt)
        {TRACE_IT(6020);
            AddToPropertiesWrittenTo(sym->AsPropertySym()->m_propertyId);
            this->KillAllObjectTypes(bv);
        }
        break;

    case Js::OpCode::InitSetFld:
    case Js::OpCode::InitGetFld:
    case Js::OpCode::InitClassMemberGet:
    case Js::OpCode::InitClassMemberSet:
        sym = instr->GetDst()->AsSymOpnd()->m_sym;
        KillLiveFields(sym->AsPropertySym(), bv);
        if (inGlobOpt)
        {TRACE_IT(6021);
            AddToPropertiesWrittenTo(sym->AsPropertySym()->m_propertyId);
            this->KillAllObjectTypes(bv);
        }
        break;

    case Js::OpCode::StFld:
    case Js::OpCode::StRootFld:
    case Js::OpCode::StFldStrict:
    case Js::OpCode::StRootFldStrict:
    case Js::OpCode::StSlot:
    case Js::OpCode::StSlotChkUndecl:
        Assert(dstOpnd != nullptr);
        sym = dstOpnd->AsSymOpnd()->m_sym;
        if (inGlobOpt)
        {TRACE_IT(6022);
            AddToPropertiesWrittenTo(sym->AsPropertySym()->m_propertyId);
        }
        if ((inGlobOpt && (sym->AsPropertySym()->m_propertyId == Js::PropertyIds::valueOf || sym->AsPropertySym()->m_propertyId == Js::PropertyIds::toString)) ||
            instr->CallsAccessor())
        {TRACE_IT(6023);
            // If overriding valueof/tostring, we might have expected a previous LdFld to bailout on implicitCalls but didn't.
            // CSE's for example would have expected a bailout. Clear all fields to prevent optimizing across.
            this->KillAllFields(bv);
        }
        else
        {TRACE_IT(6024);
            KillLiveFields(sym->AsPropertySym(), bv);
        }
        break;

    case Js::OpCode::InlineArrayPush:
    case Js::OpCode::InlineArrayPop:
        KillLiveFields(this->lengthEquivBv, bv);
        break;

    case Js::OpCode::InlineeStart:
    case Js::OpCode::InlineeEnd:
        Assert(!instr->UsesAllFields());

        // Kill all live 'arguments' and 'caller' fields, as 'inlineeFunction.arguments' and 'inlineeFunction.caller' 
        // cannot be copy-propped across different instances of the same inlined function.
        KillLiveFields(argumentsEquivBv, bv);
        KillLiveFields(callerEquivBv, bv);
        break;

    case Js::OpCode::CallDirect:
        fnHelper = instr->GetSrc1()->AsHelperCallOpnd()->m_fnHelper;

        // Kill length field for built-ins that can update it.
        if(nullptr != this->lengthEquivBv && (fnHelper == IR::JnHelperMethod::HelperArray_Shift || fnHelper == IR::JnHelperMethod::HelperArray_Splice
            || fnHelper == IR::JnHelperMethod::HelperArray_Unshift))
        {TRACE_IT(6025);
            KillLiveFields(this->lengthEquivBv, bv);
        }

        if ((fnHelper == IR::JnHelperMethod::HelperRegExp_Exec)
           || (fnHelper == IR::JnHelperMethod::HelperString_Match)
           || (fnHelper == IR::JnHelperMethod::HelperString_Replace))
        {TRACE_IT(6026);
            // Consider: We may not need to kill all fields here.
            this->KillAllFields(bv);
        }
        break;

    default:
        if (instr->UsesAllFields())
        {TRACE_IT(6027);
            // This also kills all property type values, as the same bit-vector tracks those stack syms.
            this->KillAllFields(bv);
        }
        break;
    }
}

void
GlobOpt::ProcessFieldKills(IR::Instr * instr)
{TRACE_IT(6028);
    if (!this->DoFieldCopyProp() && !this->DoFieldRefOpts() && !DoCSE())
    {TRACE_IT(6029);
        Assert(this->blockData.liveFields->IsEmpty());
        return;
    }

    ProcessFieldKills(instr, this->blockData.liveFields, true);
    if (this->blockData.hoistableFields)
    {TRACE_IT(6030);
        Assert(this->TrackHoistableFields());

        // Fields that are killed are no longer hoistable.
        this->blockData.hoistableFields->And(this->blockData.liveFields);
    }
}

void
GlobOpt::PreparePrepassFieldHoisting(Loop * loop)
{TRACE_IT(6031);
    BVSparse<JitArenaAllocator> * fieldHoistCandidates = loop->fieldHoistCandidates;

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {TRACE_IT(6032);
        Output::Print(_u("\nFieldHoist: Start Loop: "));
        loop->GetHeadBlock()->DumpHeader();
        Output::Print(_u("FieldHoist: Backward candidates          : "));
        fieldHoistCandidates->Dump();
    }
#endif
#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {TRACE_IT(6033);
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(_u("FieldHoist: START LOOP function %s (%s)\n"), this->func->GetJITFunctionBody()->GetDisplayName(), this->func->GetDebugNumberSet(debugStringBuffer));
    }
#endif

    loop->fieldHoistCandidateTypes = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);

    if (fieldHoistCandidates->IsEmpty())
    {TRACE_IT(6034);
        return;
    }

    BasicBlock * landingPad = loop->landingPad;

    // If it is live, the field doesn't need to be hoisted
    Assert(loop->liveInFieldHoistCandidates == nullptr);
    BVSparse<JitArenaAllocator> * liveInFieldHoistCandidates = fieldHoistCandidates->AndNew(landingPad->globOptData.liveFields);
    loop->liveInFieldHoistCandidates = liveInFieldHoistCandidates;

    if (!liveInFieldHoistCandidates->IsEmpty())
    {TRACE_IT(6035);
        // Assume the live fields don't need to hoist for now
        fieldHoistCandidates->Minus(liveInFieldHoistCandidates);

        // If it was hoisted in an outer loop, and the value is live coming in, we don't need to hoist it again
        Loop * currentLoop = loop->parent;
        while (currentLoop != nullptr && this->DoFieldHoisting(currentLoop))
        {TRACE_IT(6036);
            if (currentLoop->hoistedFields)
            {TRACE_IT(6037);
                liveInFieldHoistCandidates->Minus(currentLoop->hoistedFields);
            }
            currentLoop = currentLoop->parent;
        }

        FOREACH_BITSET_IN_SPARSEBV(index, liveInFieldHoistCandidates)
        {TRACE_IT(6038);
            if (this->FindValueFromHashTable(landingPad->globOptData.symToValueMap, index) == nullptr)
            {TRACE_IT(6039);
                // Create initial values if we don't have one already for live fields
                Value * newValue = this->NewGenericValue(ValueType::Uninitialized);
                Value * oldValue = CopyValue(newValue, newValue->GetValueNumber());
                Sym *sym = this->func->m_symTable->Find(index);
                this->SetValue(&landingPad->globOptData, oldValue, sym);
                this->SetValue(&this->blockData, newValue, sym);
            }
        }
        NEXT_BITSET_IN_SPARSEBV;
    }

    // Assume that the candidates are hoisted on prepass
    landingPad->globOptData.liveFields->Or(fieldHoistCandidates);
    this->blockData.liveFields->Or(fieldHoistCandidates);

    Loop * parentLoop = loop->parent;
    FOREACH_BITSET_IN_SPARSEBV(index, fieldHoistCandidates)
    {TRACE_IT(6040);
        // Create initial values
        Value * newValue = this->NewGenericValue(ValueType::Uninitialized);
        Value * oldValue = CopyValue(newValue, newValue->GetValueNumber());
        Sym *sym = this->func->m_symTable->Find(index);
        this->SetValue(&landingPad->globOptData, oldValue, sym);
        this->SetValue(&this->blockData, newValue, sym);

        StackSym* objectSym = sym->AsPropertySym()->m_stackSym;
        if (objectSym->HasObjectTypeSym())
        {TRACE_IT(6041);
            StackSym* typeSym = objectSym->GetObjectTypeSym();

            // If the type isn't live into the loop, let's keep track of it, so we can add it to
            // live fields on pre-pass, verify if it is invariant through the loop, and if so produce it
            // into the loop on the real pass.
            if (!loop->landingPad->globOptData.liveFields->Test(typeSym->m_id))
            {TRACE_IT(6042);
                Assert(!this->blockData.liveFields->Test(typeSym->m_id));
                loop->fieldHoistCandidateTypes->Set(typeSym->m_id);

                // Set object type live on prepass so we can track if it got killed in the loop. (see FinishOptHoistedPropOps)
                JsTypeValueInfo* typeValueInfo = JsTypeValueInfo::New(this->alloc, nullptr, nullptr);
                typeValueInfo->SetIsShared();
                this->SetSymStoreDirect(typeValueInfo, typeSym);

                ValueNumber typeValueNumber = this->NewValueNumber();
                Value* landingPadTypeValue = NewValue(typeValueNumber, typeValueInfo);
                Value* headerTypeValue = NewValue(typeValueNumber, typeValueInfo);

                SetObjectTypeFromTypeSym(typeSym, landingPadTypeValue, landingPad);
                SetObjectTypeFromTypeSym(typeSym, headerTypeValue, this->currentBlock);
            }
        }

        // If the sym holding the hoisted value is used as an instance pointer in the outer loop,
        // its type may appear to be live in the inner loop. But the instance itself is being killed
        // here, so make sure the type is killed as well.
        if (parentLoop != nullptr)
        {TRACE_IT(6043);
            StackSym * copySym;
            Loop * hoistedLoop = FindFieldHoistStackSym(parentLoop, index, &copySym, nullptr);
            if (hoistedLoop != nullptr)
            {TRACE_IT(6044);
                this->KillObjectType(copySym);
            }
        }
    }
    NEXT_BITSET_IN_SPARSEBV;

    Assert(this->TrackHoistableFields());

    // Initialize the bit vector to keep track of whether the hoisted value will reach a field load
    // to determine whether it should be hoisted.
    if (this->blockData.hoistableFields)
    {TRACE_IT(6045);
        this->blockData.hoistableFields->Copy(fieldHoistCandidates);
    }
    else
    {TRACE_IT(6046);
        this->blockData.hoistableFields = fieldHoistCandidates->CopyNew(this->alloc);
        this->currentBlock->globOptData.hoistableFields = this->blockData.hoistableFields;
    }
    this->blockData.hoistableFields->Or(liveInFieldHoistCandidates);

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {TRACE_IT(6047);
        Output::Print(_u("FieldHoist: Prepass candidates (not live): "));
        fieldHoistCandidates->Dump();
        Output::Print(_u("FieldHoist: Prepass candidates (live)    : "));
        liveInFieldHoistCandidates->Dump();
    }
#endif
}

void
GlobOpt::PrepareFieldHoisting(Loop * loop)
{TRACE_IT(6048);
    Assert(!this->IsLoopPrePass());

    if (loop->parent != nullptr)
    {TRACE_IT(6049);
        loop->hasHoistedFields = loop->parent->hasHoistedFields;
    }

    BVSparse<JitArenaAllocator> * fieldHoistCandidates = loop->fieldHoistCandidates;
    BVSparse<JitArenaAllocator> * liveInFieldHoistCandidates = loop->liveInFieldHoistCandidates;
    if (fieldHoistCandidates->IsEmpty() && (!liveInFieldHoistCandidates || liveInFieldHoistCandidates->IsEmpty()))
    {TRACE_IT(6050);
        if (loop->hasHoistedFields)
        {TRACE_IT(6051);
            loop->hoistedFieldCopySyms = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);

            AnalysisAssert(loop->parent && loop->parent->hasHoistedFields);
            loop->hoistedFieldCopySyms->Copy(loop->parent->hoistedFieldCopySyms);
            loop->regAlloc.liveOnBackEdgeSyms->Or(loop->hoistedFieldCopySyms);
        }

        return;
    }

    BasicBlock * landingPad = loop->landingPad;
    Assert(landingPad->globOptData.hoistableFields == nullptr);
    Assert(this->blockData.hoistableFields == nullptr);

    BVSparse<JitArenaAllocator>* fieldHoistCandidateTypes = loop->fieldHoistCandidateTypes;

    // Remove the live fields that are added during prepass
    landingPad->globOptData.liveFields->Minus(fieldHoistCandidates);
    landingPad->globOptData.liveFields->Minus(fieldHoistCandidateTypes);

    // After prepass, if the field is not loaded on the back edge then we shouldn't hoist it
    fieldHoistCandidates->And(this->blockData.liveFields);
    liveInFieldHoistCandidates->And(this->blockData.liveFields);
    fieldHoistCandidateTypes->And(this->blockData.liveFields);

    // Remove the live fields that were added during prepass
    this->blockData.liveFields->Minus(fieldHoistCandidates);
    this->blockData.liveFields->Minus(fieldHoistCandidateTypes);

    loop->hoistedFields = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
    loop->hoistedFieldCopySyms = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);

    if (loop->parent && loop->parent->hasHoistedFields)
    {TRACE_IT(6052);
        loop->hoistedFieldCopySyms->Copy(loop->parent->hoistedFieldCopySyms);
    }

    Func * loopTopFunc = loop->GetFunc();

    // We built the list in reverse order, i.e., by prepending to it. Reverse it now so
    // the hoisted instr's can be inserted in the correct order.
    loop->prepassFieldHoistInstrCandidates.Reverse();

    // Hoist the field load
    FOREACH_SLISTBASE_ENTRY(IR::Instr *, instr, &loop->prepassFieldHoistInstrCandidates)
    {TRACE_IT(6053);
        // We should have removed all fields that are hoisted in outer loops already.
#if DBG
        AssertCanCopyPropOrCSEFieldLoad(instr);
#endif
        PropertySym * propertySym = instr->GetSrc1()->AsSymOpnd()->m_sym->AsPropertySym();
        SymID symId = propertySym->m_id;

        if (loop->fieldHoistSymMap.ContainsKey(symId))
        {TRACE_IT(6054);
            // The field is already hoisted
#if DBG
            StackSym * hoistedCopySym;
            Assert(loop == FindFieldHoistStackSym(loop, symId, &hoistedCopySym, instr));
#endif
            continue;
        }

        Assert(GlobOpt::IsLive(propertySym->m_stackSym, landingPad));

        if (fieldHoistCandidates->Test(symId))
        {TRACE_IT(6055);
            // Hoist non-live field in
            Value * oldValue = this->FindValueFromHashTable(landingPad->globOptData.symToValueMap, symId);
            Value * newValue = this->FindValueFromHashTable(this->blockData.symToValueMap, symId);
            HoistFieldLoad(propertySym, loop, instr, oldValue, newValue);
            continue;
        }

        if (!liveInFieldHoistCandidates->Test(symId))
        {TRACE_IT(6056);
            // Not live in back edge; don't hoist field
            Assert(!this->blockData.liveFields->Test(symId));
            continue;
        }

        Assert(landingPad->globOptData.liveFields->Test(symId));
        Assert(this->blockData.liveFields->Test(symId));

        // If the value is live in, we shouldn't have a hoisted symbol already
        Assert(!this->IsHoistedPropertySym(symId, loop->parent));
        Value * oldValue = this->FindPropertyValue(landingPad->globOptData.symToValueMap, symId);
        AssertMsg(oldValue != nullptr, "We should have created an initial value for the field");
        ValueInfo *oldValueInfo = oldValue->GetValueInfo();

        Value * newValue = this->FindPropertyValue(this->blockData.symToValueMap, symId);

        // The value of the loop isn't invariant, we need to create a value to hold the field through the loop

        int32 oldIntConstantValue;
        if (oldValueInfo->TryGetIntConstantValue(&oldIntConstantValue))
        {TRACE_IT(6057);
            // Generate the constant load
            IR::IntConstOpnd * intConstOpnd = IR::IntConstOpnd::New(oldIntConstantValue, TyInt32, loopTopFunc);
            this->HoistFieldLoadValue(loop, newValue, symId, Js::OpCode::LdC_A_I4, intConstOpnd);
        }
        else if (oldValueInfo->IsFloatConstant())
        {TRACE_IT(6058);
            // Generate the constant load
            this->HoistFieldLoadValue(loop, newValue, symId,
                Js::OpCode::LdC_A_R8, IR::FloatConstOpnd::New(oldValueInfo->AsFloatConstant()->FloatValue(), TyFloat64, loopTopFunc));
        }
        else
        {TRACE_IT(6059);
            // This should be looking at the landingPad's value
            Sym * copySym = this->GetCopyPropSym(landingPad, nullptr, oldValue);

            if (copySym != nullptr)
            {TRACE_IT(6060);
                if (newValue && oldValue->GetValueNumber() == newValue->GetValueNumber())
                {TRACE_IT(6061);
                    // The value of the field is invariant through the loop.
                    // Copy prop can deal with this so we don't need to do anything.
                    continue;
                }

                StackSym * copyStackSym = copySym->AsStackSym();

                // Transfer from an old copy prop value
                IR::RegOpnd * srcOpnd = IR::RegOpnd::New(copyStackSym, TyVar, loopTopFunc);
                srcOpnd->SetIsJITOptimizedReg(true);
                this->HoistFieldLoadValue(loop, newValue, symId, Js::OpCode::Ld_A, srcOpnd);
            }
            else
            {TRACE_IT(6062);
                // We don't have a copy sym, even though the field value is live, we can't copy prop.
                // Generate the field load instead.
#if DBG
                landingPad->globOptData.liveFields->Clear(symId);
                this->blockData.liveFields->Clear(symId);
                liveInFieldHoistCandidates->Clear(symId);
                fieldHoistCandidates->Set(symId);
#endif
                HoistNewFieldLoad(propertySym, loop, instr, oldValue,  newValue);
            }
        }
    }
    NEXT_SLISTBASE_ENTRY;

    this->FinishOptHoistedPropOps(loop);

    JitAdelete(this->alloc, loop->fieldHoistCandidateTypes);
    fieldHoistCandidateTypes = nullptr;
    loop->fieldHoistCandidateTypes = nullptr;

    loop->regAlloc.liveOnBackEdgeSyms->Or(loop->hoistedFieldCopySyms);

#if DBG || DBG_DUMP
    if (loop->hoistedFields->IsEmpty())
    {TRACE_IT(6063);
        Assert(loop->fieldHoistSymMap.Count() == 0);
        liveInFieldHoistCandidates->ClearAll();
    }
    else
    {TRACE_IT(6064);
        // Update liveInFieldHoistCandidates for assert in FindFieldHoistStackSym
        liveInFieldHoistCandidates->And(loop->hoistedFields);

        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {TRACE_IT(6065);
            Output::Print(_u("FieldHoist: All candidates: "));
            loop->hoistedFields->Dump();
            Output::Print(_u("FieldHoist: Live in candidates: "));
            liveInFieldHoistCandidates->Dump();
        }
    }
#else
    JitAdelete(this->alloc, liveInFieldHoistCandidates);
    loop->liveInFieldHoistCandidates = nullptr;
#endif

    JitAdelete(this->alloc, fieldHoistCandidates);
    loop->fieldHoistCandidates = nullptr;
}

void
GlobOpt::CheckFieldHoistCandidate(IR::Instr * instr, PropertySym * sym)
{TRACE_IT(6066);
    // See if this field load is hoistable.
    // This load probably may have a store or kill before it.
    // We will hoist it in another path. Just copy prop the value from the field store.
    //
    // For example:
    // loop
    // {
    //      if ()
    //      {
    //          o.i =
    //              = o.i   <= not hoistable (but can copy prop)
    //      }
    //      else
    //      {
    //          = o.i       <= hoistable
    //      }
    // }
    if (this->blockData.hoistableFields->TestAndClear(sym->m_id))
    {TRACE_IT(6067);
        Assert(this->blockData.liveFields->Test(sym->m_id));
        // We're adding this instruction as a candidate for hoisting. If it gets hoisted, its jit-time inline
        // cache will be used to generate the type check and bailout at the top of the loop. After we bail out,
        // however, we may not go down the code path on which this instruction resides, and so the inline cache
        // will not turn polymorphic. If we then re-jit, we would hoist the same instruction again, and get
        // stuck in infinite bailout cycle. That's why we use BailOutRecord::polymorphicCacheIndex for hoisted
        // field loads to force the profile info for the right inline cache into polymorphic state.
        this->rootLoopPrePass->prepassFieldHoistInstrCandidates.Prepend(this->alloc, instr);
#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {TRACE_IT(6068);
            Output::Print(_u("FieldHoist: Prepass marked hoist load"));
            Output::SkipToColumn(30);
            Output::Print(_u(" : "));
            instr->Dump();
        }
#endif
    }
}

void
GlobOpt::FinishOptHoistedPropOps(Loop * loop)
{TRACE_IT(6069);
    // Set up hoisted fields for object type specialization.
    Assert(loop);

    // This extra check for parent loop was added as a fix for Windows 8 Bug 480217.  The issue there might have affected
    // the original redundant type elimination, but does not cause problems for object type spec.  With this check some
    // operations which were candidates for object type spec in the backward pass (where we only checked the current loop),
    // could unexpectedly not be candidates, anymore.  This led to problems in the lowerer.
    // (Do this only if we're doing the optimization in the loop's parent, which is where we're inserting
    // the hoisted instruction.)
    //if (loop->parent && !DoFieldRefOpts(loop->parent))
    //{
    //    return;
    //}

    bool doFieldRefOpts = DoFieldRefOpts(loop);
    bool forceFieldHoisting = PHASE_FORCE(Js::FieldHoistPhase, this->func);
    bool doForcedTypeChecksOnly = !doFieldRefOpts && forceFieldHoisting;

    if (!doFieldRefOpts && !forceFieldHoisting)
    {TRACE_IT(6070);
        IR::Instr * instrEnd = loop->endDisableImplicitCall;
        if (instrEnd == nullptr)
        {TRACE_IT(6071);
            return;
        }

        FOREACH_INSTR_EDITING_IN_RANGE(instr, instrNext, loop->landingPad->GetFirstInstr(), instrEnd)
        {TRACE_IT(6072);
            // LdMethodFromFlags must always have a type check and bailout.  If we hoisted it as a result of
            // -force:fieldHoist, we will have to set the bailout here again, even if there are implicit calls
            // in the loop (and DoFieldRefOpts returns false).  See Windows Blue Bugs 608503 and 610237.
            if (instr->m_opcode == Js::OpCode::LdMethodFromFlags)
            {TRACE_IT(6073);
                instr = SetTypeCheckBailOut(instr->GetSrc1(), instr, loop->bailOutInfo);
            }
        }
        NEXT_INSTR_EDITING_IN_RANGE;

        return;
    }

    // Walk the implicit-call-disabled region in the loop header, creating PropertySymOpnd's and
    // tracking liveness of the type/slot-array syms.
    IR::Instr * instrEnd = loop->endDisableImplicitCall;
    if (instrEnd == nullptr)
    {TRACE_IT(6074);
        return;
    }
    Assert(loop->bailOutInfo->bailOutInstr != nullptr);

    // Consider (ObjTypeSpec): Do we really need all this extra tracking of live fields on back edges, so as to
    // remove them from the live fields on the loop header?  We already do this in MergeBlockData called from
    // MergePredBlocksValueMaps, which takes place just before we get here.

    // Build the set of fields that are live on all back edges.
    // Use this to limit the type symbols we make live into the loop. We made the types of the hoisted fields
    // live in the prepass, so if they're not live on a back edge, that means some path through the loop
    // kills them.
    BVSparse<JitArenaAllocator> *bvBackEdge = nullptr;
    FOREACH_PREDECESSOR_BLOCK(predBlock, loop->GetHeadBlock())
    {TRACE_IT(6075);
        if (!loop->IsDescendentOrSelf(predBlock->loop))
        {TRACE_IT(6076);
            // This is the edge that enters the loop - not interesting here.
            continue;
        }
        if (!bvBackEdge)
        {TRACE_IT(6077);
            bvBackEdge = predBlock->globOptData.liveFields;
        }
        else
        {TRACE_IT(6078);
            bvBackEdge = bvBackEdge->AndNew(predBlock->globOptData.liveFields, this->alloc);
        }
    }
    NEXT_PREDECESSOR_BLOCK;

    if (!doForcedTypeChecksOnly)
    {
        FOREACH_INSTR_EDITING_IN_RANGE(instr, instrNext, loop->landingPad->GetFirstInstr(), instrEnd)
        {TRACE_IT(6079);
            IR::Opnd *opnd = instr->GetSrc1();
            if (opnd && opnd->IsSymOpnd() && opnd->AsSymOpnd()->IsPropertySymOpnd())
            {TRACE_IT(6080);
                bool isHoistedTypeValue = false;
                bool isTypeInvariant = false;
                if (opnd->AsPropertySymOpnd()->HasObjectTypeSym())
                {TRACE_IT(6081);
                    StackSym* typeSym = opnd->AsPropertySymOpnd()->GetObjectTypeSym();

                    // We've cleared the live bits for types that are purely hoisted (not live into the loop),
                    // so we can't use FindObjectTypeValue here.
                    Value* landingPadValue = FindValueFromHashTable(loop->landingPad->globOptData.symToValueMap, typeSym->m_id);
                    Value* headerValue = FindValueFromHashTable(loop->GetHeadBlock()->globOptData.symToValueMap, typeSym->m_id);
                    isHoistedTypeValue = landingPadValue != nullptr && loop->fieldHoistCandidateTypes->Test(typeSym->m_id);
                    isTypeInvariant = landingPadValue != nullptr && headerValue != nullptr && landingPadValue->GetValueNumber() == headerValue->GetValueNumber();
                }

                // Prepare the operand for object type specialization by creating a type sym for it, if not yet present
                // and marking it as candidate for specialization.
                PreparePropertySymOpndForTypeCheckSeq(opnd->AsPropertySymOpnd(), instr, loop);

                // Let's update the existing type value, if possible, to retain the value number created in pre-pass.
                bool changesTypeValue = false;
                FinishOptPropOp(instr, opnd->AsPropertySymOpnd(), loop->landingPad, /* updateExistingValue = */ isHoistedTypeValue, nullptr, &changesTypeValue);
                instr = SetTypeCheckBailOut(opnd, instr, loop->bailOutInfo);

                // If we changed the type's value in the landing pad we want to reflect this change in the header block as well,
                // but only if the type is invariant throughout the loop. Note that if the type was live into the loop and
                // live on all back edges, but not invariant, it will already be live in the header, but its value will be blank,
                // because we merge type values conservatively on loop back edges. (see MergeJsTypeValueInfo)

                // Consider (ObjTypeSpec): There are corner cases where we copy prop an object pointer into the newly hoisted instruction,
                // and that object doesn't have a type yet. We then create a type on the fly (see GenerateHoistFieldLoad and
                // CopyPropPropertySymObj), and don't have a value for it in the landing pad. Thus we can't prove that the type is invariant
                // throughout the loop, and so we won't produce a value for it into the loop. This could be addressed by creating
                // a mapping of type syms from before to after object pointer copy prop.
                if (changesTypeValue && isTypeInvariant)
                {TRACE_IT(6082);
                    Assert(opnd->AsPropertySymOpnd()->HasObjectTypeSym());
                    StackSym* typeSym = opnd->AsPropertySymOpnd()->GetObjectTypeSym();

                    // If we changed the type value in the landing pad, we must have set it live there.
                    Value* landingPadValue = FindObjectTypeValue(typeSym->m_id, loop->landingPad);
                    Assert(landingPadValue != nullptr && landingPadValue->GetValueInfo()->IsJsType());

                    // But in the loop header we may have only a value with the live bit still cleared,
                    // so we can't use FindObjectTypeValue here.
                    Value* headerValue = FindValueFromHashTable(loop->GetHeadBlock()->globOptData.symToValueMap, typeSym->m_id);
                    Assert(headerValue != nullptr && headerValue->GetValueInfo()->IsJsType());

                    Assert(!isHoistedTypeValue || landingPadValue->GetValueNumber() == headerValue->GetValueNumber());
                    JsTypeValueInfo* valueInfo = landingPadValue->GetValueInfo()->AsJsType();
                    valueInfo->SetIsShared();
                    headerValue->SetValueInfo(valueInfo);

                    loop->GetHeadBlock()->globOptData.liveFields->Set(typeSym->m_id);
                }

#if DBG
                if (opnd->AsPropertySymOpnd()->HasObjectTypeSym())
                {TRACE_IT(6083);
                    StackSym* typeSym = opnd->AsPropertySymOpnd()->GetObjectTypeSym();
                    Assert(!isHoistedTypeValue || isTypeInvariant || !loop->GetHeadBlock()->globOptData.liveFields->Test(typeSym->m_id));
                }
#endif
            }
        }
        NEXT_INSTR_EDITING_IN_RANGE;
    }
    else
    {
        FOREACH_INSTR_EDITING_IN_RANGE(instr, instrNext, loop->landingPad->GetFirstInstr(), instrEnd)
        {TRACE_IT(6084);
            // LdMethodFromFlags must always have a type check and bailout. If we hoisted it as a result of
            // -force:fieldHoist, we will have to set the bailout here again, even if there are implicit calls
            // in the loop.
            if (instr->m_opcode == Js::OpCode::LdMethodFromFlags)
            {TRACE_IT(6085);
                instr = SetTypeCheckBailOut(instr->GetSrc1(), instr, loop->bailOutInfo);
            }
        }
        NEXT_INSTR_EDITING_IN_RANGE;
    }

    if (bvBackEdge)
    {TRACE_IT(6086);
        // Take the fields not live on some back edge out of the set that's live into the loop.
        this->blockData.liveFields->And(bvBackEdge);
    }
}

void
GlobOpt::HoistFieldLoadValue(Loop * loop, Value * newValue, SymID symId, Js::OpCode opcode, IR::Opnd * srcOpnd)
{TRACE_IT(6087);
    IR::Instr * insertInstr = this->EnsureDisableImplicitCallRegion(loop);

    Assert(!this->IsLoopPrePass());
    Assert(IsPropertySymId(symId));
    Assert(!loop->fieldHoistCandidates->Test(symId));
    Assert(loop->landingPad->globOptData.liveFields->Test(symId));
    Assert(this->blockData.liveFields->Test(symId));

    Func * loopTopFunc = loop->GetFunc();

    // Just transfer the copy prop sym to a new stack sym for the property.
    // Consider: What happens if the outer loop already has a field hoist stack sym for this propertysym?
    StackSym * newStackSym = StackSym::New(TyVar, loopTopFunc);

    // This new stack sym may or may not be single def.
    // Just make it not a single def so that we don't lose the value when it become non-single def.
    newStackSym->m_isSingleDef = false;
    IR::RegOpnd * newOpnd = IR::RegOpnd::New(newStackSym, TyVar, loopTopFunc);
    IR::Instr * newInstr = IR::Instr::New(opcode, newOpnd, srcOpnd, loopTopFunc);

    insertInstr->InsertBefore(newInstr);
    loop->landingPad->globOptData.liveVarSyms->Set(newStackSym->m_id);
    loop->varSymsOnEntry->Set(newStackSym->m_id);

    // Update value in the current block
    if (newValue == nullptr)
    {TRACE_IT(6088);
        // Even though we don't use the symStore to copy prop the hoisted stack sym in the loop
        // we might be able to propagate it out of the loop. Create a value just in case.
        newValue = this->NewGenericValue(ValueType::Uninitialized, newStackSym);

        // This should pass the sym directly.
        Sym *sym = this->func->m_symTable->Find(symId);

        this->SetValue(&this->blockData, newValue, sym);
        Assert(newValue->GetValueInfo()->GetSymStore() == newStackSym);
    }
    else
    {TRACE_IT(6089);
        this->SetValue(&this->blockData, newValue, newStackSym);
        this->SetSymStoreDirect(newValue->GetValueInfo(), newStackSym);
    }


    this->blockData.liveVarSyms->Set(newStackSym->m_id);
    loop->fieldHoistSymMap.Add(symId, newStackSym);
    loop->hoistedFieldCopySyms->Set(newStackSym->m_id);

    loop->hasHoistedFields = true;
    loop->hoistedFields->Set(symId);

    if(newInstr->GetSrc1()->IsRegOpnd())
    {TRACE_IT(6090);
        // Make sure the source sym is available as a var
        const auto srcRegOpnd = newInstr->GetSrc1()->AsRegOpnd();
        if(!loop->landingPad->globOptData.liveVarSyms->Test(srcRegOpnd->m_sym->m_id))
        {TRACE_IT(6091);
            this->ToVar(newInstr, srcRegOpnd, loop->landingPad, nullptr, false);
        }
    }

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {TRACE_IT(6092);
        Output::Print(_u("FieldHoist: Live value load "));
        this->func->m_symTable->Find(symId)->Dump();
        Output::SkipToColumn(30);
        Output::Print(_u(" : "));
        newInstr->Dump();
    }
#endif
}

bool
GlobOpt::IsHoistablePropertySym(SymID symId) const
{TRACE_IT(6093);
    return this->blockData.hoistableFields && this->blockData.hoistableFields->Test(symId);
}

bool
GlobOpt::HasHoistableFields(BasicBlock * basicBlock)
{TRACE_IT(6094);
    return HasHoistableFields(&basicBlock->globOptData);
}

bool
GlobOpt::HasHoistableFields(GlobOptBlockData const * globOptData)
{TRACE_IT(6095);
    return globOptData->hoistableFields && !globOptData->hoistableFields->IsEmpty();
}

Loop *
GlobOpt::FindFieldHoistStackSym(Loop * startLoop, SymID propertySymId, StackSym ** copySym, IR::Instr * instrToHoist) const
{TRACE_IT(6096);
    Assert(IsPropertySymId(propertySymId));

    if (instrToHoist && instrToHoist->m_opcode == Js::OpCode::LdMethodFromFlags)
    {TRACE_IT(6097);
        return nullptr;
    }

    Loop * loop = startLoop;

    while (loop && this->DoFieldHoisting(loop))
    {TRACE_IT(6098);
        if (loop->fieldHoistSymMap.TryGetValue(propertySymId, copySym))
        {TRACE_IT(6099);
            Assert(loop->hasHoistedFields);
            Assert(loop->hoistedFields->Test(propertySymId));

            if (this->IsLoopPrePass())
            {TRACE_IT(6100);
                return loop;
            }

            BasicBlock * landingPad = loop->landingPad;
#if DBG
            BOOL liveInSym = FALSE;
            liveInSym = loop->liveInFieldHoistCandidates->Test(propertySymId);

            Assert(landingPad->globOptData.liveFields->Test(propertySymId));
            Assert(landingPad->globOptData.liveVarSyms->Test((*copySym)->m_id));
#endif

            // This has been hoisted already.
            // Verify the hoisted instruction.
            bool found = false;
            FOREACH_INSTR_BACKWARD_IN_BLOCK(instr, landingPad)
            {TRACE_IT(6101);
                IR::Opnd * dstOpnd = instr->GetDst();
                if (dstOpnd && dstOpnd->IsRegOpnd() && dstOpnd->AsRegOpnd()->m_sym == *copySym)
                {TRACE_IT(6102);
                    found = true;
#if DBG
                    // We used to try to assert that the property sym on the instruction in the landing pad
                    // matched the one on the instruction we're changing now. But we may have done object ptr
                    // copy prop in the landing pad, so the assertion no longer holds.
                    if (liveInSym)
                    {TRACE_IT(6103);
                        Assert((instr->m_opcode == Js::OpCode::Ld_A && instr->GetSrc1()->IsRegOpnd())
                            || (instr->m_opcode == Js::OpCode::LdC_A_I4 && instr->GetSrc1()->IsIntConstOpnd())
                            || instr->m_opcode == Js::OpCode::LdC_A_R8 && instr->GetSrc1()->IsFloatConstOpnd());
                    }
                    else if (instrToHoist)
                    {TRACE_IT(6104);
                        bool instrIsLdFldEquivalent = (instr->m_opcode == Js::OpCode::LdFld || instr->m_opcode == Js::OpCode::LdFldForCallApplyTarget);
                        bool instrToHoistIsLdFldEquivalent = (instrToHoist->m_opcode == Js::OpCode::LdFld || instrToHoist->m_opcode == Js::OpCode::LdFldForCallApplyTarget);
                        Assert(instr->m_opcode == instrToHoist->m_opcode ||
                               instrIsLdFldEquivalent && instrToHoistIsLdFldEquivalent ||
                               instr->m_opcode == Js::OpCode::LdMethodFld ||
                               instr->m_opcode == Js::OpCode::LdRootMethodFld ||
                               instr->m_opcode == Js::OpCode::ScopedLdMethodFld ||
                               instrToHoist->m_opcode == Js::OpCode::LdMethodFld ||
                               instrToHoist->m_opcode == Js::OpCode::LdRootMethodFld ||
                               instrToHoist->m_opcode == Js::OpCode::ScopedLdMethodFld ||
                               (instrIsLdFldEquivalent && instrToHoist->m_opcode == Js::OpCode::LdRootFld) ||
                               (instr->m_opcode == Js::OpCode::LdMethodFld && instrToHoist->m_opcode == Js::OpCode::LdRootMethodFld) ||
                               (instrToHoistIsLdFldEquivalent && instr->m_opcode == Js::OpCode::LdRootFld) ||
                               (instrToHoist->m_opcode == Js::OpCode::LdMethodFld && instr->m_opcode == Js::OpCode::LdRootMethodFld));
                    }
#endif
                    if (instrToHoist
                        && (instrToHoist->m_opcode == Js::OpCode::LdMethodFld ||
                            instrToHoist->m_opcode == Js::OpCode::LdRootMethodFld ||
                            instrToHoist->m_opcode == Js::OpCode::ScopedLdMethodFld)
                        && instr->m_opcode != Js::OpCode::Ld_A
                        && instr->m_opcode != Js::OpCode::LdC_A_I4
                        && instr->m_opcode != Js::OpCode::LdC_A_R8)
                    {TRACE_IT(6105);
                        // We may have property sym referred to by both Ld[Root]Fld and Ld[Root]MethodFld
                        // in the loop. If this happens, make sure the hoisted instruction is Ld[Root]MethodFld
                        // so we get the prototype inline cache fast path we want.
                        // Other differences such as error messages and HostDispatch behavior shouldn't
                        // matter, because we'll bail out in those cases.
                        Assert(instr->GetSrc1()->IsSymOpnd() && instr->GetSrc1()->AsSymOpnd()->m_sym->IsPropertySym());
                        instr->m_opcode = instrToHoist->m_opcode;
                    }
                    else if (instrToHoist &&
                           ((instr->m_opcode == Js::OpCode::LdFld && instrToHoist->m_opcode == Js::OpCode::LdRootFld)
                            || (instr->m_opcode == Js::OpCode::LdMethodFld && instrToHoist->m_opcode == Js::OpCode::LdRootMethodFld)))
                    {TRACE_IT(6106);
                        instr->m_opcode = instrToHoist->m_opcode;
                    }
                    break;
                }
            }
            NEXT_INSTR_BACKWARD_IN_BLOCK;
            Assert(found);

            return loop;
        }
        Assert(!loop->hoistedFields || !loop->hoistedFields->Test(propertySymId));
        loop = loop->parent;
    }
    return nullptr;
}

void
GlobOpt::HoistFieldLoad(PropertySym * sym, Loop * loop, IR::Instr * instr, Value * oldValue, Value * newValue)
{TRACE_IT(6107);
    Loop * parentLoop = loop->parent;
    if (parentLoop != nullptr)
    {TRACE_IT(6108);
        StackSym * copySym;
        Loop * hoistedLoop = FindFieldHoistStackSym(parentLoop, sym->m_id, &copySym, instr);
        if (hoistedLoop != nullptr)
        {TRACE_IT(6109);
            // Use an outer loop pre-assigned stack sym if it is already hoisted there
            Assert(hoistedLoop != loop);
            GenerateHoistFieldLoad(sym, loop, instr, copySym, oldValue, newValue);
            return;
        }
    }

    HoistNewFieldLoad(sym, loop, instr, oldValue, newValue);
}

void
GlobOpt::HoistNewFieldLoad(PropertySym * sym, Loop * loop, IR::Instr * instr, Value * oldValue, Value * newValue)
{TRACE_IT(6110);
    Assert(!this->IsHoistedPropertySym(sym->m_id, loop));

    StackSym * newStackSym = StackSym::New(TyVar, this->func);

    // This new stack sym may or may not be single def.
    // Just make it not a single def so that we don't lose the value when it become non-single def.
    newStackSym->m_isSingleDef = false;

    GenerateHoistFieldLoad(sym, loop, instr, newStackSym, oldValue, newValue);
}

void
GlobOpt::GenerateHoistFieldLoad(PropertySym * sym, Loop * loop, IR::Instr * instr, StackSym * newStackSym, Value * oldValue, Value * newValue)
{TRACE_IT(6111);
    Assert(loop != nullptr);

    SymID symId = sym->m_id;
    BasicBlock * landingPad = loop->landingPad;

#if DBG
    Assert(!this->IsLoopPrePass());
    AssertCanCopyPropOrCSEFieldLoad(instr);
    Assert(instr->GetSrc1()->AsSymOpnd()->m_sym == sym);

    Assert(loop->fieldHoistCandidates->Test(symId));
    Assert(!landingPad->globOptData.liveFields->Test(sym->m_id));
    Assert(!this->blockData.liveFields->Test(sym->m_id));
    Assert(!loop->fieldHoistSymMap.ContainsKey(symId));
#endif

    loop->fieldHoistSymMap.Add(symId, newStackSym);
    loop->hoistedFieldCopySyms->Set(newStackSym->m_id);

    Func * loopTopFunc = loop->GetFunc();

    // Generate the hoisted field load
    IR::RegOpnd * newDst = IR::RegOpnd::New(newStackSym, TyVar, loopTopFunc);
    IR::SymOpnd * newSrc;

    if (instr->GetSrc1() && instr->GetSrc1()->IsSymOpnd() && instr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
    {TRACE_IT(6112);
        IR::PropertySymOpnd * srcPropertySymOpnd = instr->GetSrc1()->AsPropertySymOpnd();
        AssertMsg(!srcPropertySymOpnd->IsTypeAvailable() && !srcPropertySymOpnd->IsTypeChecked() && !srcPropertySymOpnd->IsWriteGuardChecked(),
            "Why are the object type spec bits set before we specialized this instruction?");

        // We only set guarded properties in the dead store pass, so they shouldn't be set here yet. If they were
        // we would need to move them from this operand to the operand which is being copy propagated.
        Assert(srcPropertySymOpnd->GetGuardedPropOps() == nullptr);

        // We're hoisting an instruction from the loop, so we're placing it in a different position in the flow. Make sure only the flow
        // insensitive info is copied.
        IR::PropertySymOpnd * newPropertySymOpnd = srcPropertySymOpnd->CopyWithoutFlowSensitiveInfo(loopTopFunc);
        Assert(newPropertySymOpnd->GetObjTypeSpecFlags() == 0);
        Value *const propertyOwnerValueInLandingPad =
            FindValue(loop->landingPad->globOptData.symToValueMap, srcPropertySymOpnd->GetObjectSym());
        if(propertyOwnerValueInLandingPad)
        {TRACE_IT(6113);
            newPropertySymOpnd->SetPropertyOwnerValueType(propertyOwnerValueInLandingPad->GetValueInfo()->Type());
        }
        newSrc = newPropertySymOpnd;
    }
    else
    {TRACE_IT(6114);
        newSrc = IR::SymOpnd::New(sym, TyVar, func);
    }

    IR::Instr * newInstr = nullptr;
    ValueType profiledFieldType;

    if (instr->IsProfiledInstr())
    {TRACE_IT(6115);
        profiledFieldType = instr->AsProfiledInstr()->u.FldInfo().valueType;
    }

    newInstr = IR::Instr::New(instr->m_opcode, newDst, newSrc, loopTopFunc);

    // Win8 910551: Kill the live field for this hoisted field load
    KillLiveFields(newStackSym, this->blockData.liveFields);

    IR::Instr * insertInstr = this->EnsureDisableImplicitCallRegion(loop);
    insertInstr->InsertBefore(newInstr);

    // Track use/def of arguments object
    this->OptArguments(newInstr);

    landingPad->globOptData.liveFields->Set(symId);
    this->blockData.liveFields->Set(symId);

    // If we are reusing an already hoisted stack sym, while the var version is made live, we need to make sure that specialized
    // versions of it are not live since this is effectively a field reload.
    this->ToVarStackSym(newStackSym, landingPad);
    this->ToVarStackSym(newStackSym, this->currentBlock);
    loop->varSymsOnEntry->Set(newStackSym->m_id);
    loop->int32SymsOnEntry->Clear(newStackSym->m_id);
    loop->lossyInt32SymsOnEntry->Clear(newStackSym->m_id);
    loop->float64SymsOnEntry->Clear(newStackSym->m_id);

    Assert(oldValue != nullptr);

    // Create a value in case we can copy prop out of the loop
    if (newValue == nullptr || newValue->GetValueInfo()->IsUninitialized())
    {TRACE_IT(6116);
        const bool hoistValue = newValue && oldValue->GetValueNumber() == newValue->GetValueNumber();

        if(newValue)
        {TRACE_IT(6117);
            // Assuming the profile data gives more precise value types based on the path it took at runtime, we can improve the
            // original value type.
            newValue->GetValueInfo()->Type() = profiledFieldType;
        }
        else
        {TRACE_IT(6118);
            newValue = NewGenericValue(profiledFieldType, newDst);
        }

        this->SetValue(&this->blockData, newValue, sym);
        if(hoistValue)
        {TRACE_IT(6119);
            // The field value is invariant through the loop. Since we're updating its value to a more precise value, hoist the
            // new value up to the loop landing pad where the field is being hoisted.
            Assert(loop == currentBlock->loop);
            Assert(landingPad == loop->landingPad);
            oldValue = CopyValue(newValue, newValue->GetValueNumber());
            SetValue(&landingPad->globOptData, oldValue, sym);
        }
    }

    newInstr->GetDst()->SetValueType(oldValue->GetValueInfo()->Type());
    newInstr->GetSrc1()->SetValueType(oldValue->GetValueInfo()->Type());
    this->SetValue(&loop->landingPad->globOptData, oldValue, newStackSym);

    this->SetValue(&this->blockData, newValue, newStackSym);
    instr->GetSrc1()->SetValueType(newValue->GetValueInfo()->Type());

    loop->hasHoistedFields = true;
    loop->hoistedFields->Set(sym->m_id);

    // Try to do object pointer copy prop. Do it now because, for instance, we want the ToVar we insert below
    // to define the right sym (Win8 906875).
    // Consider: Restructure field hoisting to call OptBlock on the completed loop landing pad instead of
    // doing these optimizations and bitvector updates piecemeal.
#ifdef DBG
    PropertySym *propertySymUseBefore = nullptr;
    Assert(this->byteCodeUses == nullptr);
    this->byteCodeUsesBeforeOpt->ClearAll();
    GlobOpt::TrackByteCodeSymUsed(instr, this->byteCodeUsesBeforeOpt, &propertySymUseBefore);
#endif
    this->CaptureByteCodeSymUses(newInstr);

    // Consider (ObjTypeSpec): If we copy prop an object sym into the hoisted instruction we lose track of the original
    // object sym's type being invariant through the loop and so we won't produce the new type's value into the loop,
    // and end up with unnecessary type checks in the loop. If the new type isn't live in the landing pad (that is
    // we weren't tracking its liveness and invariance through the loop), but the old type was invariant, let's add
    // the new type to fieldHoistCandidateTypes and produce a value for it in the landing pad and loop header. If the
    // old type was live then its liveness and invariance are already correctly reflected and there is nothing to do.
    this->CopyPropPropertySymObj(newSrc, newInstr);
    if (this->byteCodeUses != nullptr)
    {TRACE_IT(6120);
        sym = newSrc->m_sym->AsPropertySym();
        this->InsertByteCodeUses(newInstr);
    }

    StackSym * propertyBase = sym->m_stackSym;
    if (!landingPad->globOptData.liveVarSyms->Test(propertyBase->m_id))
    {TRACE_IT(6121);
        IR::RegOpnd *newOpnd = IR::RegOpnd::New(propertyBase, TyVar, instr->m_func);
        this->ToVar(newInstr, newOpnd, landingPad, this->FindValue(propertyBase), false);
    }

    if (landingPad->globOptData.canStoreTempObjectSyms && landingPad->globOptData.canStoreTempObjectSyms->Test(propertyBase->m_id))
    {TRACE_IT(6122);
        newSrc->SetCanStoreTemp();
    }

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {TRACE_IT(6123);
        Output::Print(_u("FieldHoist: Hoisted Load "));
        Output::SkipToColumn(30);
        Output::Print(_u(" : "));
        newInstr->Dump();
    }
#endif
#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {TRACE_IT(6124);
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(_u("    FieldHoist: function %s (%s) "), this->func->GetJITFunctionBody()->GetDisplayName(), this->func->GetDebugNumberSet(debugStringBuffer));
        newInstr->DumpTestTrace();
    }
#endif
}

Value *
GlobOpt::CreateFieldSrcValue(PropertySym * sym, PropertySym * originalSym, IR::Opnd ** ppOpnd, IR::Instr * instr)
{TRACE_IT(6125);
#if DBG
    // If the opcode going to kill all field values immediate anyway, we shouldn't be giving it a value
    Assert(!instr->UsesAllFields());

    AssertCanCopyPropOrCSEFieldLoad(instr);

    Assert(instr->GetSrc1() == *ppOpnd);
#endif

    // Only give a value to fields if we are doing field copy prop.
    // Consider: We should always copy prop local slots, but the only use right now is LdSlot from jit loop body.
    // This should have one onus load, and thus no need for copy prop of field itself.  We may want to support
    // copy prop LdSlot if there are other uses of local slots
    if (!this->DoFieldCopyProp())
    {TRACE_IT(6126);
        return nullptr;
    }

    BOOL wasLive = this->blockData.liveFields->TestAndSet(sym->m_id);

    if (this->DoFieldHoisting())
    {TRACE_IT(6127);
        // We don't track copy prop sym for fields on loop prepass, no point in creating an empty unknown value.
        // If we can copy prop through the back edge, we would have hoisted the field load, in which case we will
        // just pick the live in copy prop sym for the field or create a new sym for the stack sym of the hoist field.
        if (this->IsLoopPrePass())
        {TRACE_IT(6128);
            // We don't clear the value when we kill the field.
            // Clear it to make sure we don't use the old value.
            this->blockData.symToValueMap->Clear(sym->m_id);
            return nullptr;
        }
    }
    else if (sym != originalSym)
    {TRACE_IT(6129);
        this->blockData.liveFields->TestAndSet(originalSym->m_id);
    }

    if (!wasLive)
    {TRACE_IT(6130);
        // We don't clear the value when we kill the field.
        // Clear it to make sure we don't use the old value.
        this->blockData.symToValueMap->Clear(sym->m_id);
        this->blockData.symToValueMap->Clear(originalSym->m_id);
    }

    Assert((*ppOpnd)->AsSymOpnd()->m_sym == sym || this->IsLoopPrePass());
    if (wasLive)
    {TRACE_IT(6131);
        // We should have dealt with field hoist already
        Assert(!IsHoistedPropertySym(sym) || instr->m_opcode == Js::OpCode::CheckFixedFld);

        // We don't use the sym store to do copy prop on hoisted fields, but create a value
        // in case it can be copy prop out of the loop.
    }
    else
    {TRACE_IT(6132);
        // If it wasn't live, it should not be hoistable
        Assert(!this->IsHoistablePropertySym(sym->m_id));
    }

    return this->NewGenericValue(ValueType::Uninitialized, *ppOpnd);
}

bool
GlobOpt::FieldHoistOptSrc(IR::Opnd *opnd, IR::Instr *instr, PropertySym * propertySym)
{TRACE_IT(6133);
    if (!DoFieldHoisting())
    {TRACE_IT(6134);
        return false;
    }
    if (!GlobOpt::TransferSrcValue(instr) || instr->m_opcode == Js::OpCode::LdMethodFromFlags)
    {TRACE_IT(6135);
        // Instructions like typeof don't transfer value of the field, we can't hoist those right now.
        return false;
    }
    if (TrackHoistableFields() && HasHoistableFields(&this->blockData))
    {TRACE_IT(6136);
        Assert(this->DoFieldHoisting());
        CheckFieldHoistCandidate(instr, propertySym);

        // This may have been a hoistable field with respect to the current loop. If so, that means:
        // - It is assumed that it will be live on the back-edge and hence currently live for the purposes of determining
        //   whether to hoist the field.
        // - It is not already hoisted outside a parent loop or not live coming into this loop.
        // - It is not already marked for hoisting in this loop.
        //
        // If this is a hoistable field, and if the field is ultimately chosen to be hoisted outside this loop, the field will
        // be reloaded in this loop's landing pad. However, since the field may already have been hoisted outside a parent
        // loop with a specialized stack sym still live and a value still available (since these are killed lazily), neither of
        // which are valid anymore due to the reload, we still need to kill the specialized stack syms and the field value. On
        // the other hand, if this was not a hoistable field, we need to treat it as a field load anyway. So, since this is the
        // first use of the field in this loop, fall through to reload the field.
    }
    else if (!this->IsLoopPrePass())
    {
        if (CopyPropHoistedFields(propertySym, &opnd, instr))
        {TRACE_IT(6137);
            return true;
        }
    }

    this->ReloadFieldHoistStackSym(instr, propertySym);
    return false;
}

void
GlobOpt::FieldHoistOptDst(IR::Instr * instr, PropertySym * propertySym, Value * src1Val)
{TRACE_IT(6138);
    if(DoFieldHoisting())
    {TRACE_IT(6139);
        switch (instr->m_opcode)
        {
        case Js::OpCode::StSlot:
        case Js::OpCode::StSlotChkUndecl:
        case Js::OpCode::StFld:
        case Js::OpCode::StRootFld:
        case Js::OpCode::StFldStrict:
        case Js::OpCode::StRootFldStrict:
            CopyStoreFieldHoistStackSym(instr, propertySym, src1Val);
            break;
        }
    }
}

bool
GlobOpt::CopyPropHoistedFields(PropertySym * sym, IR::Opnd ** ppOpnd, IR::Instr * instr)
{TRACE_IT(6140);
    Assert(GlobOpt::TransferSrcValue(instr));
    if (!this->blockData.liveFields->Test(sym->m_id))
    {TRACE_IT(6141);
        // Not live
        return false;
    }

    StackSym * hoistedCopySym;
    Loop * loop = FindFieldHoistStackSym(this->currentBlock->loop, sym->m_id, &hoistedCopySym, instr);
    Assert(loop != nullptr || !this->IsHoistablePropertySym(sym->m_id));

    if (loop)
    {TRACE_IT(6142);
        // The field was live before, so we have the hoisted stack sym live value, just copy prop it
        *ppOpnd = CopyPropReplaceOpnd(instr, *ppOpnd, hoistedCopySym);

#if DBG
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {TRACE_IT(6143);
            Output::Print(_u("FieldHoist: Copy prop "));
            sym->Dump();
            Output::SkipToColumn(30);
            Output::Print(_u(" : "));
            instr->Dump();
        }
#endif
        return true;
    }
    return false;
}

void
GlobOpt::ReloadFieldHoistStackSym(IR::Instr * instr, PropertySym * propertySym)
{TRACE_IT(6144);
    Assert(GlobOpt::TransferSrcValue(instr));
    StackSym * fieldHoistSym;
    Loop * loop = this->FindFieldHoistStackSym(this->currentBlock->loop, propertySym->m_id, &fieldHoistSym, instr);

    if (loop == nullptr)
    {TRACE_IT(6145);
        return;
    }

    // When a field is killed, ideally the specialized versions of the corresponding hoisted stack syms should also be killed,
    // since the field needs to be reloaded the next time it's used (which may be earlier in the loop). However, killing the
    // specialized stack syms when the field is killed requires discovering and walking all fields that are killed and their
    // hoisted stack syms, which requires more computation (since many fields can be killed at once).
    //
    // Alternatively, we can kill the specialized stack syms for a field when the field is reloaded, which is what's happening
    // here. Since this happens per field and lazily, it requires less work. It works because killing the specialized stack
    // syms only matters when the field is reloaded.
    //
    // Furthermore, to handle the case where a field is not live on entry into the loop (field is killed in the loop and not
    // reloaded in the same loop afterwards), the specialized stack syms for that field must also be killed on entry into the
    // loop. Instead of checking all hoisted field stack syms on entry into a loop after the prepass merge, and killing them if
    // their corresponding field is not live, this is also done in a lazy fashion as above, only when a field is reloaded. If a
    // field is reloaded in a loop before it's killed, and not reloaded again after the kill, the field won't be live on entry,
    // and hence the specialized stack syms should also not be live on entry. This is true for all parent loops up to the
    // nearest parent loop out of which the field is hoisted.

    ToVarStackSym(fieldHoistSym, currentBlock);
    if(!this->IsLoopPrePass())
    {TRACE_IT(6146);
        for(Loop *currentLoop = currentBlock->loop;
            currentLoop != loop->parent && !currentLoop->liveFieldsOnEntry->Test(propertySym->m_id);
            currentLoop = currentLoop->parent)
        {TRACE_IT(6147);
            currentLoop->int32SymsOnEntry->Clear(fieldHoistSym->m_id);
            currentLoop->lossyInt32SymsOnEntry->Clear(fieldHoistSym->m_id);
            currentLoop->float64SymsOnEntry->Clear(fieldHoistSym->m_id);
        }
    }

    // Win8 943662: Kill the live field for this hoisted field load
    this->KillLiveFields(fieldHoistSym, this->blockData.liveFields);

    if (this->IsLoopPrePass())
    {TRACE_IT(6148);
        // In the prepass we are conservative and always assume that the fields are going to be reloaded
        // because we don't loop until value is unchanged and we are unable to detect dependencies.

        // Clear the value of the field to kill the value of the field even if it still live now.
        this->blockData.liveFields->Clear(propertySym->m_id);

        // If we have to reload, we don't know the value, kill the old value for the fieldHoistSym.
        this->blockData.symToValueMap->Clear(fieldHoistSym->m_id);

        // No IR transformations in the prepass.
        return;
    }

    // If we are reloading, the field should be dead. CreateFieldSrc will create a value for the field.
    Assert(!this->blockData.liveFields->Test(propertySym->m_id));

    // Copy the dst to the field hoist sym.
    IR::Instr * copyInstr = IR::Instr::New(Js::OpCode::Ld_A, IR::RegOpnd::New(fieldHoistSym, TyVar, instr->m_func), instr->GetDst(), instr->m_func);
    instr->InsertAfter(copyInstr);

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {TRACE_IT(6149);
        Output::Print(_u("FieldHoist: Reload field sym "));
        Output::SkipToColumn(30);
        Output::Print(_u(" : "));
        instr->Dump();
    }
#endif
}

void
GlobOpt::CopyStoreFieldHoistStackSym(IR::Instr * storeFldInstr, PropertySym * sym, Value * src1Val)
{TRACE_IT(6150);
    // In the real (not prepass) pass, do the actual IR rewrites.
    // In the prepass, only track the impact that the rewrites will have. (See Win8 521029)

    Assert(storeFldInstr->m_opcode == Js::OpCode::StSlot
        || storeFldInstr->m_opcode == Js::OpCode::StSlotChkUndecl
        || storeFldInstr->m_opcode == Js::OpCode::StFld
        || storeFldInstr->m_opcode == Js::OpCode::StRootFld
        || storeFldInstr->m_opcode == Js::OpCode::StFldStrict
        || storeFldInstr->m_opcode == Js::OpCode::StRootFldStrict);
    Assert(storeFldInstr->GetDst()->GetType() == TyVar);

    // We may use StSlot for all sort of things other then assigning TyVars
    Assert(storeFldInstr->GetSrc1()->GetType() == TyVar || storeFldInstr->m_opcode == Js::OpCode::StSlot || storeFldInstr->m_opcode == Js::OpCode::StSlotChkUndecl);
    Assert(storeFldInstr->GetSrc2() == nullptr);

    StackSym * copySym;
    Loop * loop = this->FindFieldHoistStackSym(this->currentBlock->loop, sym->m_id, &copySym);
    if (loop == nullptr)
    {TRACE_IT(6151);
        return;
    }
    IR::Opnd * srcOpnd = storeFldInstr->GetSrc1();
    Func * storeFldFunc = storeFldInstr->m_func;
    IR::Instr * newInstr;
    if (!this->IsLoopPrePass())
    {TRACE_IT(6152);
        this->CaptureByteCodeSymUses(storeFldInstr);

        IR::RegOpnd * dstOpnd = IR::RegOpnd::New(copySym, TyVar, storeFldFunc);
        dstOpnd->SetIsJITOptimizedReg(true);
        storeFldInstr->UnlinkSrc1();
        newInstr = IR::Instr::New(Js::OpCode::Ld_A, dstOpnd, srcOpnd, storeFldFunc);
        storeFldInstr->SetSrc1(dstOpnd);
        storeFldInstr->InsertBefore(newInstr);
    }
    this->ToVarStackSym(copySym, this->currentBlock); // The field-hoisted stack sym is now unspecialized

    Value * dstVal = this->CopyValue(src1Val);
    TrackCopiedValueForKills(dstVal);
    this->SetSymStoreDirect(dstVal->GetValueInfo(), copySym);
    this->SetValue(&this->blockData, dstVal, copySym);

    // Copy the type specialized sym as well, in case we have a use for them
    bool neededCopySymDef = false;
    if(srcOpnd->IsRegOpnd())
    {TRACE_IT(6153);
        StackSym *const srcSym = srcOpnd->AsRegOpnd()->m_sym;
        if (this->blockData.liveInt32Syms->Test(srcSym->m_id))
        {TRACE_IT(6154);
            this->blockData.liveInt32Syms->Set(copySym->m_id);
            if(this->blockData.liveLossyInt32Syms->Test(srcSym->m_id))
            {TRACE_IT(6155);
                this->blockData.liveLossyInt32Syms->Set(copySym->m_id);
            }
            if (!this->IsLoopPrePass())
            {TRACE_IT(6156);
                StackSym * int32CopySym = copySym->GetInt32EquivSym(storeFldFunc);
                IR::RegOpnd * int32CopyOpnd = IR::RegOpnd::New(int32CopySym, TyInt32, storeFldFunc);
                IR::RegOpnd * int32SrcOpnd = IR::RegOpnd::New(srcSym->GetInt32EquivSym(nullptr),
                    TyInt32, storeFldFunc);
                newInstr = IR::Instr::New(Js::OpCode::Ld_I4, int32CopyOpnd, int32SrcOpnd, storeFldFunc);
                int32SrcOpnd->SetIsJITOptimizedReg(true);
                storeFldInstr->InsertBefore(newInstr);
            }
            neededCopySymDef = true;
        }
        if (this->blockData.liveFloat64Syms->Test(srcSym->m_id))
        {TRACE_IT(6157);
            this->blockData.liveFloat64Syms->Set(copySym->m_id);
            if (!this->IsLoopPrePass())
            {TRACE_IT(6158);
                StackSym * float64CopySym = copySym->GetFloat64EquivSym(storeFldFunc);
                IR::RegOpnd * float64CopyOpnd = IR::RegOpnd::New(float64CopySym, TyFloat64, storeFldFunc);
                IR::RegOpnd * float64SrcOpnd = IR::RegOpnd::New(srcSym->GetFloat64EquivSym(nullptr),
                    TyFloat64, storeFldFunc);
                newInstr = IR::Instr::New(Js::OpCode::Ld_A, float64CopyOpnd, float64SrcOpnd, storeFldFunc);
                float64SrcOpnd->SetIsJITOptimizedReg(true);
                storeFldInstr->InsertBefore(newInstr);
            }
            neededCopySymDef = true;
        }
    }
    else if(srcOpnd->IsAddrOpnd())
    {TRACE_IT(6159);
        const auto srcAddrOpnd = srcOpnd->AsAddrOpnd();
        if(srcAddrOpnd->IsVar() && Js::TaggedInt::Is(srcAddrOpnd->m_address))
        {TRACE_IT(6160);
            this->blockData.liveInt32Syms->Set(copySym->m_id);
            if (!this->IsLoopPrePass())
            {TRACE_IT(6161);
                StackSym * int32CopySym = copySym->GetInt32EquivSym(storeFldFunc);
                IR::RegOpnd * int32CopyOpnd = IR::RegOpnd::New(int32CopySym, TyInt32, storeFldFunc);
                IR::IntConstOpnd * int32SrcOpnd =
                    IR::IntConstOpnd::New(Js::TaggedInt::ToInt32(srcAddrOpnd->m_address), TyInt32, storeFldFunc);
                newInstr = IR::Instr::New(Js::OpCode::Ld_I4, int32CopyOpnd, int32SrcOpnd, storeFldFunc);
                int32SrcOpnd->SetIsJITOptimizedReg(true);
                storeFldInstr->InsertBefore(newInstr);
            }
            neededCopySymDef = true;
        }
    }

    if(IsLoopPrePass() && neededCopySymDef)
    {TRACE_IT(6162);
        // Record the def that would have been added
        rootLoopPrePass->symsDefInLoop->Set(copySym->m_id);
    }

    this->KillLiveFields(copySym, this->blockData.liveFields);

#if DBG_DUMP
    if (!this->IsLoopPrePass())
    {TRACE_IT(6163);
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {TRACE_IT(6164);
            Output::Print(_u("FieldHoist: Copy field store "));
            Output::SkipToColumn(30);
            Output::Print(_u(" : "));
            storeFldInstr->Dump();
        }
    }
#endif
}

bool
GlobOpt::NeedBailOnImplicitCallWithFieldOpts(Loop *loop, bool hasLiveFields) const
{TRACE_IT(6165);
    if (!((this->DoFieldHoisting(loop) && loop->hasHoistedFields) ||
          ((this->DoFieldRefOpts(loop) ||
            this->DoFieldCopyProp(loop)) &&
           hasLiveFields)))
    {TRACE_IT(6166);
        return false;
    }

    return true;
}

IR::Instr *
GlobOpt::EnsureDisableImplicitCallRegion(Loop * loop)
{TRACE_IT(6167);
    Assert(loop->bailOutInfo != nullptr);
    IR::Instr * endDisableImplicitCall = loop->endDisableImplicitCall;
    if (endDisableImplicitCall)
    {TRACE_IT(6168);
        return endDisableImplicitCall;
    }

    IR::Instr * bailOutTarget = EnsureBailTarget(loop);

    Func * bailOutFunc = loop->GetFunc();
    Assert(loop->bailOutInfo->bailOutFunc == bailOutFunc);

    IR::MemRefOpnd * disableImplicitCallAddress = IR::MemRefOpnd::New(this->func->GetThreadContextInfo()->GetDisableImplicitFlagsAddr(), TyInt8, bailOutFunc);
    IR::IntConstOpnd * disableImplicitCallAndExceptionValue = IR::IntConstOpnd::New(DisableImplicitCallAndExceptionFlag, TyInt8, bailOutFunc, true);
    IR::IntConstOpnd * enableImplicitCallAndExceptionValue = IR::IntConstOpnd::New(DisableImplicitNoFlag, TyInt8, bailOutFunc, true);

    IR::Opnd * implicitCallFlags = Lowerer::GetImplicitCallFlagsOpnd(bailOutFunc);
    IR::IntConstOpnd * noImplicitCall = IR::IntConstOpnd::New(Js::ImplicitCall_None, TyInt8, bailOutFunc, true);

    // Consider: if we are already doing implicit call in the outer loop, we don't need to clear the implicit call bit again
    IR::Instr * clearImplicitCall = IR::Instr::New(Js::OpCode::Ld_A, implicitCallFlags, noImplicitCall, bailOutFunc);
    bailOutTarget->InsertBefore(clearImplicitCall);

    IR::Instr * disableImplicitCall = IR::Instr::New(Js::OpCode::Ld_A, disableImplicitCallAddress, disableImplicitCallAndExceptionValue, bailOutFunc);
    bailOutTarget->InsertBefore(disableImplicitCall);

    endDisableImplicitCall = IR::Instr::New(Js::OpCode::Ld_A, disableImplicitCallAddress, enableImplicitCallAndExceptionValue, bailOutFunc);
    bailOutTarget->InsertBefore(endDisableImplicitCall);

    IR::BailOutInstr * bailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotEqual, IR::BailOutOnImplicitCalls, loop->bailOutInfo, loop->bailOutInfo->bailOutFunc);
    bailOutInstr->SetSrc1(implicitCallFlags);
    bailOutInstr->SetSrc2(noImplicitCall);
    bailOutTarget->InsertBefore(bailOutInstr);

    loop->endDisableImplicitCall = endDisableImplicitCall;
    return endDisableImplicitCall;
}

#if DBG
bool
GlobOpt::IsHoistedPropertySym(PropertySym * sym) const
{TRACE_IT(6169);
    return IsHoistedPropertySym(sym->m_id, this->currentBlock->loop);
}

bool
GlobOpt::IsHoistedPropertySym(SymID symId, Loop * loop) const
{TRACE_IT(6170);
    StackSym * copySym;
    return this->FindFieldHoistStackSym(loop, symId, &copySym) != nullptr;
}

bool
GlobOpt::IsPropertySymId(SymID symId) const
{TRACE_IT(6171);
    return this->func->m_symTable->Find(symId)->IsPropertySym();
}

void
GlobOpt::AssertCanCopyPropOrCSEFieldLoad(IR::Instr * instr)
{TRACE_IT(6172);
    // Consider: Hoisting LdRootFld may have complication with exception if the field doesn't exist.
    // We need to have another opcode for the hoisted version to avoid the exception and bailout.

    // Consider: Theoretically, we can copy prop/field hoist ScopedLdFld/ScopedStFld
    // but GlobOtp::TransferSrcValue blocks that now, and copy prop into that instruction is not supported yet.
    Assert(instr->m_opcode == Js::OpCode::LdSlot || instr->m_opcode == Js::OpCode::LdSlotArr
        || instr->m_opcode == Js::OpCode::LdFld || instr->m_opcode == Js::OpCode::LdFldForCallApplyTarget
        || instr->m_opcode == Js::OpCode::LdRootFld  || instr->m_opcode == Js::OpCode::LdSuperFld
        || instr->m_opcode == Js::OpCode::LdFldForTypeOf || instr->m_opcode == Js::OpCode::LdRootFldForTypeOf
        || instr->m_opcode == Js::OpCode::LdMethodFld || instr->m_opcode == Js::OpCode::LdMethodFldPolyInlineMiss
        || instr->m_opcode == Js::OpCode::LdRootMethodFld
        || instr->m_opcode == Js::OpCode::LdMethodFromFlags
        || instr->m_opcode == Js::OpCode::ScopedLdMethodFld
        || instr->m_opcode == Js::OpCode::CheckFixedFld
        || instr->m_opcode == Js::OpCode::CheckPropertyGuardAndLoadType);

    Assert(instr->m_opcode == Js::OpCode::CheckFixedFld || instr->GetDst()->GetType() == TyVar);
    Assert(instr->GetSrc1()->GetType() == TyVar);
    Assert(instr->GetSrc1()->AsSymOpnd()->m_sym->IsPropertySym());
    Assert(instr->GetSrc2() == nullptr);
}
#endif

StackSym *
GlobOpt::EnsureObjectTypeSym(StackSym * objectSym)
{TRACE_IT(6173);
    Assert(!objectSym->IsTypeSpec());

    objectSym->EnsureObjectInfo(this->func);

    if (objectSym->HasObjectTypeSym())
    {TRACE_IT(6174);
        Assert(this->objectTypeSyms);
        return objectSym->GetObjectTypeSym();
    }

    if (this->objectTypeSyms == nullptr)
    {TRACE_IT(6175);
        this->objectTypeSyms = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
    }

    StackSym * typeSym = StackSym::New(TyVar, this->func);

    objectSym->GetObjectInfo()->m_typeSym = typeSym;

    this->objectTypeSyms->Set(typeSym->m_id);

    return typeSym;
}

PropertySym *
GlobOpt::EnsurePropertyWriteGuardSym(PropertySym * propertySym)
{TRACE_IT(6176);
    // Make sure that the PropertySym has a proto cache sym which is chained into the propertySym list.
    if (!propertySym->m_writeGuardSym)
    {TRACE_IT(6177);
        propertySym->m_writeGuardSym = PropertySym::New(propertySym->m_stackSym, propertySym->m_propertyId, (uint32)-1, (uint)-1, PropertyKindWriteGuard, this->func);
    }

    return propertySym->m_writeGuardSym;
}

void
GlobOpt::PreparePropertySymForTypeCheckSeq(PropertySym *propertySym)
{TRACE_IT(6178);
    Assert(!propertySym->m_stackSym->IsTypeSpec());
    EnsureObjectTypeSym(propertySym->m_stackSym);
    EnsurePropertyWriteGuardSym(propertySym);
}

bool
GlobOpt::IsPropertySymPreparedForTypeCheckSeq(PropertySym *propertySym)
{TRACE_IT(6179);
    Assert(!propertySym->m_stackSym->IsTypeSpec());

    // The following doesn't need to be true. We may copy prop a constant into an object sym, which has
    // previously been prepared for type check sequence optimization.
    // Assert(!propertySym->m_stackSym->m_isIntConst || !propertySym->HasObjectTypeSym());

    // The following doesn't need to be true. We may copy prop the object sym into a field load or store
    // that doesn't have object type spec info and hence the operand wasn't prepared and doesn't have a write
    // guard. The object sym, however, may have other field operations which are object type specialized and
    // thus the type sym for it has been created.
    // Assert(propertySym->HasObjectTypeSym() == propertySym->HasWriteGuardSym());

    return propertySym->HasObjectTypeSym();
}

bool
GlobOpt::PreparePropertySymOpndForTypeCheckSeq(IR::PropertySymOpnd * propertySymOpnd, IR::Instr* instr, Loop * loop)
{TRACE_IT(6180);
    if (!DoFieldRefOpts(loop) || !OpCodeAttr::FastFldInstr(instr->m_opcode) || instr->CallsAccessor())
    {TRACE_IT(6181);
        return false;
    }

    if (!propertySymOpnd->HasObjTypeSpecFldInfo())
    {TRACE_IT(6182);
        return false;
    }

    JITObjTypeSpecFldInfo* info = propertySymOpnd->GetObjTypeSpecInfo();

    if (info->UsesAccessor() || info->IsRootObjectNonConfigurableFieldLoad())
    {TRACE_IT(6183);
        return false;
    }

    if (info->IsPoly() && !info->GetEquivalentTypeSet())
    {TRACE_IT(6184);
        return false;
    }

    PropertySym * propertySym = propertySymOpnd->m_sym->AsPropertySym();

    PreparePropertySymForTypeCheckSeq(propertySym);
    propertySymOpnd->SetTypeCheckSeqCandidate(true);
    propertySymOpnd->SetIsBeingStored(propertySymOpnd == instr->GetDst());

    return true;
}

bool
GlobOpt::CheckIfPropOpEmitsTypeCheck(IR::Instr *instr, IR::PropertySymOpnd *opnd)
{TRACE_IT(6185);
    if (!DoFieldRefOpts() || !OpCodeAttr::FastFldInstr(instr->m_opcode))
    {TRACE_IT(6186);
        return false;
    }

    if (!opnd->IsTypeCheckSeqCandidate())
    {TRACE_IT(6187);
        return false;
    }

    return CheckIfInstrInTypeCheckSeqEmitsTypeCheck(instr, opnd);
}

IR::PropertySymOpnd *
GlobOpt::CreateOpndForTypeCheckOnly(IR::PropertySymOpnd* opnd, Func* func)
{TRACE_IT(6188);
    // Used only for CheckObjType instruction today. Future users should make a call
    // whether the new operand is jit optimized in their scenario or not.

    Assert(!opnd->IsRootObjectNonConfigurableFieldLoad());
    IR::PropertySymOpnd *newOpnd = opnd->CopyCommon(func);

    newOpnd->SetObjTypeSpecFldInfo(opnd->GetObjTypeSpecInfo());
    newOpnd->SetUsesAuxSlot(opnd->UsesAuxSlot());
    newOpnd->SetSlotIndex(opnd->GetSlotIndex());

    newOpnd->objTypeSpecFlags = opnd->objTypeSpecFlags;
    // If we're turning the instruction owning this operand into a CheckObjType, we will do a type check here
    // only for the sake of downstream instructions, so the flags pertaining to this property access are
    // irrelevant, because we don't do a property access here.
    newOpnd->SetTypeCheckOnly(true);
    newOpnd->usesFixedValue = false;

    newOpnd->finalType = opnd->finalType;
    newOpnd->guardedPropOps = opnd->guardedPropOps != nullptr ? opnd->guardedPropOps->CopyNew() : nullptr;
    newOpnd->writeGuards = opnd->writeGuards != nullptr ? opnd->writeGuards->CopyNew() : nullptr;

    newOpnd->SetIsJITOptimizedReg(true);

    return newOpnd;
}

bool
GlobOpt::FinishOptPropOp(IR::Instr *instr, IR::PropertySymOpnd *opnd, BasicBlock* block, bool updateExistingValue, bool* emitsTypeCheckOut, bool* changesTypeValueOut)
{TRACE_IT(6189);
    if (!DoFieldRefOpts() || !OpCodeAttr::FastFldInstr(instr->m_opcode))
    {TRACE_IT(6190);
        return false;
    }

    bool isTypeCheckSeqCandidate = opnd->IsTypeCheckSeqCandidate();
    bool isObjTypeSpecialized = false;
    bool isObjTypeChecked = false;

    if (isTypeCheckSeqCandidate)
    {TRACE_IT(6191);
        isObjTypeSpecialized = ProcessPropOpInTypeCheckSeq<true>(instr, opnd, block, updateExistingValue, emitsTypeCheckOut, changesTypeValueOut, &isObjTypeChecked);
    }

    if (opnd == instr->GetDst() && this->objectTypeSyms)
    {TRACE_IT(6192);
        if (block == nullptr)
        {TRACE_IT(6193);
            block = this->currentBlock;
        }

        // This is a property store that may change the layout of the object that it stores to. This means that
        // it may change any aliased object. Do two things to address this:
        // - Add all object types in this function to the set that may have had a property added. This will prevent
        //   final type optimization across this instruction. (Only needed here for non-specialized stores.)
        // - Kill all type symbols that currently hold object-header-inlined types. Any of them may have their layout
        //   changed by the addition of a property.

        SymID opndId = opnd->HasObjectTypeSym() ? opnd->GetObjectTypeSym()->m_id : -1;
        if (!isObjTypeChecked)
        {TRACE_IT(6194);
            if (block->globOptData.maybeWrittenTypeSyms == nullptr)
            {TRACE_IT(6195);
                block->globOptData.maybeWrittenTypeSyms = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
            }
            if (isObjTypeSpecialized)
            {TRACE_IT(6196);
                // The current object will be protected by a type check, unless no further accesses to it are
                // protected by this access.
                Assert(this->objectTypeSyms->Test(opndId));
                this->objectTypeSyms->Clear(opndId);
            }
            block->globOptData.maybeWrittenTypeSyms->Or(this->objectTypeSyms);
            if (isObjTypeSpecialized)
            {TRACE_IT(6197);
                this->objectTypeSyms->Set(opndId);
            }
        }

        if (!isObjTypeSpecialized || opnd->ChangesObjectLayout())
        {TRACE_IT(6198);
            this->KillObjectHeaderInlinedTypeSyms(block, isObjTypeSpecialized, opndId);
        }
    }

    return isObjTypeSpecialized;
}

void
GlobOpt::KillObjectHeaderInlinedTypeSyms(BasicBlock *block, bool isObjTypeSpecialized, SymID opndId)
{TRACE_IT(6199);
    if (this->objectTypeSyms == nullptr)
    {TRACE_IT(6200);
        return;
    }

    FOREACH_BITSET_IN_SPARSEBV(symId, this->objectTypeSyms)
    {TRACE_IT(6201);
        if (symId == opndId && isObjTypeSpecialized)
        {TRACE_IT(6202);
            // The current object will be protected by a type check, unless no further accesses to it are
            // protected by this access.
            continue;
        }
        Value *value = this->FindObjectTypeValue(symId, block);
        if (value)
        {TRACE_IT(6203);
            JsTypeValueInfo *valueInfo = value->GetValueInfo()->AsJsType();
            Assert(valueInfo);
            if (valueInfo->GetJsType() != nullptr)
            {TRACE_IT(6204);
                JITTypeHolder type(valueInfo->GetJsType());
                if (Js::DynamicType::Is(type->GetTypeId()))
                {TRACE_IT(6205);
                    if (type->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler())
                    {TRACE_IT(6206);
                        this->blockData.liveFields->Clear(symId);
                    }
                }
            }
            else if (valueInfo->GetJsTypeSet())
            {TRACE_IT(6207);
                Js::EquivalentTypeSet *typeSet = valueInfo->GetJsTypeSet();
                for (uint16 i = 0; i < typeSet->GetCount(); i++)
                {TRACE_IT(6208);
                    JITTypeHolder type = typeSet->GetType(i);
                    if (type != nullptr && Js::DynamicType::Is(type->GetTypeId()))
                    {TRACE_IT(6209);
                        if (type->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler())
                        {TRACE_IT(6210);
                            this->blockData.liveFields->Clear(symId);
                            break;
                        }
                    }
                }
            }
        }
    }
    NEXT_BITSET_IN_SPARSEBV;
}

bool
GlobOpt::AreTypeSetsIdentical(Js::EquivalentTypeSet * leftTypeSet, Js::EquivalentTypeSet * rightTypeSet)
{TRACE_IT(6211);
    return Js::EquivalentTypeSet::AreIdentical(leftTypeSet, rightTypeSet);
}

bool
GlobOpt::IsSubsetOf(Js::EquivalentTypeSet * leftTypeSet, Js::EquivalentTypeSet * rightTypeSet)
{TRACE_IT(6212);
    return Js::EquivalentTypeSet::IsSubsetOf(leftTypeSet, rightTypeSet);
}

bool
GlobOpt::ProcessPropOpInTypeCheckSeq(IR::Instr* instr, IR::PropertySymOpnd *opnd)
{TRACE_IT(6213);
    return ProcessPropOpInTypeCheckSeq<true>(instr, opnd, this->currentBlock, false);
}

bool GlobOpt::CheckIfInstrInTypeCheckSeqEmitsTypeCheck(IR::Instr* instr, IR::PropertySymOpnd *opnd)
{TRACE_IT(6214);
    bool emitsTypeCheck;
    ProcessPropOpInTypeCheckSeq<false>(instr, opnd, this->currentBlock, false, &emitsTypeCheck);
    return emitsTypeCheck;
}

template<bool makeChanges>
bool
GlobOpt::ProcessPropOpInTypeCheckSeq(IR::Instr* instr, IR::PropertySymOpnd *opnd, BasicBlock* block, bool updateExistingValue, bool* emitsTypeCheckOut, bool* changesTypeValueOut, bool *isTypeCheckedOut)
{TRACE_IT(6215);
    // We no longer mark types as dead in the backward pass, so we should never see an instr with a dead type here
    // during the forward pass. For the time being we've retained the logic below to deal with dead types in case
    // we ever wanted to revert back to more aggressive type killing that we had before.
    Assert(!opnd->IsTypeDead());

    Assert(opnd->IsTypeCheckSeqCandidate());
    Assert(opnd->HasObjectTypeSym());

    bool isStore = opnd == instr->GetDst();
    bool isTypeDead = opnd->IsTypeDead();
    bool consumeType = makeChanges && !IsLoopPrePass();
    bool produceType = makeChanges && !isTypeDead;
    bool isSpecialized = false;
    bool emitsTypeCheck = false;
    bool addsProperty = false;

    if (block == nullptr)
    {TRACE_IT(6216);
        block = this->currentBlock;
    }

    StackSym * typeSym = opnd->GetObjectTypeSym();

#if DBG
    uint16 typeCheckSeqFlagsBefore;
    Value* valueBefore = nullptr;
    JsTypeValueInfo* valueInfoBefore = nullptr;
    if (!makeChanges)
    {TRACE_IT(6217);
        typeCheckSeqFlagsBefore = opnd->GetTypeCheckSeqFlags();
        valueBefore = FindObjectTypeValue(typeSym, block);
        if (valueBefore != nullptr)
        {TRACE_IT(6218);
            Assert(valueBefore->GetValueInfo() != nullptr && valueBefore->GetValueInfo()->IsJsType());
            valueInfoBefore = valueBefore->GetValueInfo()->AsJsType();
        }
    }
#endif

    Value *value = this->FindObjectTypeValue(typeSym, block);
    JsTypeValueInfo* valueInfo = value != nullptr ? value->GetValueInfo()->AsJsType() : nullptr;

    if (consumeType && valueInfo != nullptr)
    {TRACE_IT(6219);
        opnd->SetTypeAvailable(true);
    }

    bool doEquivTypeCheck = opnd->HasEquivalentTypeSet() && !opnd->NeedsMonoCheck();
    if (!doEquivTypeCheck)
    {TRACE_IT(6220);
        // We need a monomorphic type check here (e.g., final type opt, fixed field check on non-proto property).
        JITTypeHolder opndType = opnd->GetType();

        if (valueInfo == nullptr || (valueInfo->GetJsType() == nullptr && valueInfo->GetJsTypeSet() == nullptr))
        {TRACE_IT(6221);
            // This is the initial type check.
            opnd->SetTypeAvailable(false);
            isSpecialized = !isTypeDead;
            emitsTypeCheck = isSpecialized;
            addsProperty = isStore && isSpecialized && opnd->HasInitialType();
            if (produceType)
            {
                SetObjectTypeFromTypeSym(typeSym, opndType, nullptr, block, updateExistingValue);
            }
        }
        else if (valueInfo->GetJsType() != nullptr)
        {TRACE_IT(6222);
            // We have a monomorphic type check upstream. Check against initial/final type.
            const JITTypeHolder valueType(valueInfo->GetJsType());
            if (valueType == opndType)
            {TRACE_IT(6223);
                // The type on this instruction matches the live value in the value table, so there is no need to
                // refresh the value table.
                isSpecialized = true;
                if (isTypeCheckedOut)
                {TRACE_IT(6224);
                    *isTypeCheckedOut = true;
                }
                if (consumeType)
                {TRACE_IT(6225);
                    opnd->SetTypeChecked(true);
                }
            }
            else if (opnd->HasInitialType() && valueType == opnd->GetInitialType())
            {TRACE_IT(6226);
                // Checked type matches the initial type at this store.
                bool objectMayHaveAcquiredAdditionalProperties =
                    block->globOptData.maybeWrittenTypeSyms &&
                    block->globOptData.maybeWrittenTypeSyms->Test(typeSym->m_id);
                if (consumeType)
                {TRACE_IT(6227);
                    opnd->SetTypeChecked(!objectMayHaveAcquiredAdditionalProperties);
                    opnd->SetInitialTypeChecked(!objectMayHaveAcquiredAdditionalProperties);
                }
                if (produceType)
                {
                    SetObjectTypeFromTypeSym(typeSym, opndType, nullptr, block, updateExistingValue);
                }
                isSpecialized = !isTypeDead || !objectMayHaveAcquiredAdditionalProperties;
                emitsTypeCheck = isSpecialized && objectMayHaveAcquiredAdditionalProperties;
                addsProperty = isSpecialized;
                if (isTypeCheckedOut)
                {TRACE_IT(6228);
                    *isTypeCheckedOut = !objectMayHaveAcquiredAdditionalProperties;
                }
            }
            else
            {TRACE_IT(6229);
                // This must be a type mismatch situation, because the value is available, but doesn't match either
                // the current type or the initial type. We will not optimize this instruction and we do not produce
                // a new type value here.
                isSpecialized = false;

                if (consumeType)
                {TRACE_IT(6230);
                    opnd->SetTypeMismatch(true);
                }
            }
        }
        else
        {TRACE_IT(6231);
            // We have an equivalent type check upstream, but we require a particular type at this point. We
            // can't treat it as "checked", but we may benefit from checking for the required type.
            Assert(valueInfo->GetJsTypeSet());
            Js::EquivalentTypeSet *valueTypeSet = valueInfo->GetJsTypeSet();
            if (valueTypeSet->Contains(opndType))
            {TRACE_IT(6232);
                // Required type is in the type set we've checked. Check for the required type here, and
                // note in the value info that we've narrowed down to this type. (But leave the type set in the
                // value info so it can be merged with the same type set on other paths.)
                isSpecialized = !isTypeDead;
                emitsTypeCheck = isSpecialized;
                if (produceType)
                {
                    SetSingleTypeOnObjectTypeValue(value, opndType);
                }
            }
            else if (opnd->HasInitialType() && valueTypeSet->Contains(opnd->GetInitialType()))
            {TRACE_IT(6233);
                // Required initial type is in the type set we've checked. Check for the initial type here, and
                // note in the value info that we've narrowed down to this type. (But leave the type set in the
                // value info so it can be merged with the same type set on other paths.)
                isSpecialized = !isTypeDead;
                emitsTypeCheck = isSpecialized;
                addsProperty = isSpecialized;
                if (produceType)
                {
                    SetSingleTypeOnObjectTypeValue(value, opndType);
                }
            }
            else
            {TRACE_IT(6234);
                // This must be a type mismatch situation, because the value is available, but doesn't match either
                // the current type or the initial type. We will not optimize this instruction and we do not produce
                // a new type value here.
                isSpecialized = false;

                if (consumeType)
                {TRACE_IT(6235);
                    opnd->SetTypeMismatch(true);
                }
            }
        }
    }
    else
    {TRACE_IT(6236);
        Assert(!opnd->NeedsMonoCheck());

        Js::EquivalentTypeSet * opndTypeSet = opnd->GetEquivalentTypeSet();
        uint16 checkedTypeSetIndex = (uint16)-1;

        if (valueInfo == nullptr || (valueInfo->GetJsType() == nullptr && valueInfo->GetJsTypeSet() == nullptr))
        {TRACE_IT(6237);
            // If we don't have a value for the type we will have to emit a type check and we produce a new type value here.
            if (produceType)
            {TRACE_IT(6238);
                if (opnd->IsMono())
                {
                    SetObjectTypeFromTypeSym(typeSym, opnd->GetFirstEquivalentType(), nullptr, block, updateExistingValue);
                }
                else
                {
                    SetObjectTypeFromTypeSym(typeSym, nullptr, opndTypeSet, block, updateExistingValue);
                }
            }
            isSpecialized = !isTypeDead;
            emitsTypeCheck = isSpecialized;
        }
        else if (valueInfo->GetJsType() != nullptr ?
                 opndTypeSet->Contains(valueInfo->GetJsType(), &checkedTypeSetIndex) :
                 IsSubsetOf(valueInfo->GetJsTypeSet(), opndTypeSet))
        {TRACE_IT(6239);
            // All the types in the value info are contained in the set required by this access,
            // meaning that they're equivalent to the opnd's type set.
            // We won't have a type check, and we don't need to touch the type value.
            isSpecialized = true;
            if (isTypeCheckedOut)
            {TRACE_IT(6240);
                *isTypeCheckedOut = true;
            }
            if (consumeType)
            {TRACE_IT(6241);
                opnd->SetTypeChecked(true);
            }
            if (checkedTypeSetIndex != (uint16)-1)
            {TRACE_IT(6242);
                opnd->SetCheckedTypeSetIndex(checkedTypeSetIndex);
            }
        }
        else if (valueInfo->GetJsTypeSet() &&
                 (opnd->IsMono() ? 
                      valueInfo->GetJsTypeSet()->Contains(opnd->GetFirstEquivalentType()) : 
                      IsSubsetOf(opndTypeSet, valueInfo->GetJsTypeSet())
                 )
            )
        {TRACE_IT(6243);
            // We have an equivalent type check upstream, but we require a tighter type check at this point.
            // We can't treat the operand as "checked", but check for equivalence with the tighter set and update the
            // value info.
            if (produceType)
            {TRACE_IT(6244);
                if (opnd->IsMono())
                {
                    SetObjectTypeFromTypeSym(typeSym, opnd->GetFirstEquivalentType(), nullptr, block, updateExistingValue);
                }
                else
                {
                    SetObjectTypeFromTypeSym(typeSym, nullptr, opndTypeSet, block, updateExistingValue);
                }
            }
            isSpecialized = !isTypeDead;
            emitsTypeCheck = isSpecialized;
        }
        else
        {TRACE_IT(6245);
            // This must be a type mismatch situation, because the value is available, but doesn't match either
            // the current type or the initial type. We will not optimize this instruction and we do not produce
            // a new type value here.
            isSpecialized = false;

            if (consumeType)
            {TRACE_IT(6246);
                opnd->SetTypeMismatch(true);
            }
        }
    }

    Assert(isSpecialized || (!emitsTypeCheck && !addsProperty));

    if (consumeType && opnd->MayNeedWriteGuardProtection())
    {TRACE_IT(6247);
        Assert(!isStore);
        PropertySym *propertySym = opnd->m_sym->AsPropertySym();
        Assert(propertySym->m_writeGuardSym);
        opnd->SetWriteGuardChecked(!!block->globOptData.liveFields->Test(propertySym->m_writeGuardSym->m_id));
    }

    // Even specialized property adds must kill all types for other property adds. That's because any other object sym
    // may, in fact, be an alias of the instance whose type is being modified here. (see Windows Blue Bug 541876)
    if (makeChanges && addsProperty)
    {TRACE_IT(6248);
        Assert(isStore && isSpecialized);
        Assert(this->objectTypeSyms != nullptr);
        Assert(this->objectTypeSyms->Test(typeSym->m_id));

        if (block->globOptData.maybeWrittenTypeSyms == nullptr)
        {TRACE_IT(6249);
            block->globOptData.maybeWrittenTypeSyms = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        }

        this->objectTypeSyms->Clear(typeSym->m_id);
        block->globOptData.maybeWrittenTypeSyms->Or(this->objectTypeSyms);
        this->objectTypeSyms->Set(typeSym->m_id);
    }

    if (produceType && emitsTypeCheck && opnd->IsMono())
    {TRACE_IT(6250);
        // Consider (ObjTypeSpec): Represent maybeWrittenTypeSyms as a flag on value info of the type sym.
        if (block->globOptData.maybeWrittenTypeSyms != nullptr)
        {TRACE_IT(6251);
            // We're doing a type check here, so objtypespec of property adds is safe for this type
            // from this point forward.
            block->globOptData.maybeWrittenTypeSyms->Clear(typeSym->m_id);
        }
    }

    // Consider (ObjTypeSpec): Enable setting write guards live on instructions hoisted out of loops. Note that produceType
    // is false if the type values on loop back edges don't match (see earlier comments).
    // This means that hoisted instructions won't set write guards live if the type changes in the loop, even if
    // the corresponding properties have not been written inside the loop. This may result in some unnecessary type
    // checks and bailouts inside the loop. To enable this, we would need to verify the write guards are still live
    // on the back edge (much like we're doing for types above).

    // Consider (ObjTypeSpec): Support polymorphic write guards as well. We can't currently distinguish between mono and
    // poly write guards, and a type check can only protect operations matching with respect to polymorphism (see
    // BackwardPass::TrackObjTypeSpecProperties for details), so for now we only target monomorphic operations.
    if (produceType && emitsTypeCheck && opnd->IsMono())
    {TRACE_IT(6252);
        // If the type check we'll emit here protects some property operations that require a write guard (i.e.
        // they must do an extra type check and property guard check, if they have been written to in this
        // function), let's mark the write guards as live here, so we can accurately track if their properties
        // have been written to. Make sure we only set those that we'll actually guard, i.e. those that match
        // with respect to polymorphism.
        if (opnd->GetWriteGuards() != nullptr)
        {TRACE_IT(6253);
            block->globOptData.liveFields->Or(opnd->GetWriteGuards());
        }
    }

    if (makeChanges && isTypeDead)
    {TRACE_IT(6254);
        this->KillObjectType(opnd->GetObjectSym(), block->globOptData.liveFields);
    }

#if DBG
    if (!makeChanges)
    {TRACE_IT(6255);
        uint16 typeCheckSeqFlagsAfter = opnd->GetTypeCheckSeqFlags();
        Assert(typeCheckSeqFlagsBefore == typeCheckSeqFlagsAfter);

        Value* valueAfter = FindObjectTypeValue(typeSym, block);
        Assert(valueBefore == valueAfter);
        if (valueAfter != nullptr)
        {TRACE_IT(6256);
            Assert(valueBefore != nullptr);
            Assert(valueAfter->GetValueInfo() != nullptr && valueAfter->GetValueInfo()->IsJsType());
            JsTypeValueInfo* valueInfoAfter = valueAfter->GetValueInfo()->AsJsType();
            Assert(valueInfoBefore == valueInfoAfter);
            Assert(valueInfoBefore->GetJsType() == valueInfoAfter->GetJsType());
            Assert(valueInfoBefore->GetJsTypeSet() == valueInfoAfter->GetJsTypeSet());
        }
    }
#endif

    if (emitsTypeCheckOut != nullptr)
    {TRACE_IT(6257);
        *emitsTypeCheckOut = emitsTypeCheck;
    }

    if (changesTypeValueOut != nullptr)
    {TRACE_IT(6258);
        *changesTypeValueOut = isSpecialized && (emitsTypeCheck || addsProperty);
    }

    return isSpecialized;
}

IR::Instr*
GlobOpt::OptNewScObject(IR::Instr** instrPtr, Value* srcVal)
{TRACE_IT(6259);
    IR::Instr *&instr = *instrPtr;

    if (IsLoopPrePass())
    {TRACE_IT(6260);
        return instr;
    }

    if (PHASE_OFF(Js::ObjTypeSpecNewObjPhase, this->func) || !this->DoFieldRefOpts())
    {TRACE_IT(6261);
        return instr;
    }

    if (!instr->IsNewScObjectInstr())
    {TRACE_IT(6262);
        return nullptr;
    }

    bool isCtorInlined = instr->m_opcode == Js::OpCode::NewScObjectNoCtor;
    const JITTimeConstructorCache * ctorCache = instr->IsProfiledInstr() ?
        instr->m_func->GetConstructorCache(static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId)) : nullptr;

    // TODO: OOP JIT, enable assert
    //Assert(ctorCache == nullptr || srcVal->GetValueInfo()->IsVarConstant() && Js::JavascriptFunction::Is(srcVal->GetValueInfo()->AsVarConstant()->VarValue()));
    Assert(ctorCache == nullptr || !ctorCache->IsTypeFinal() || ctorCache->CtorHasNoExplicitReturnValue());

    if (ctorCache != nullptr && !ctorCache->SkipNewScObject() && (isCtorInlined || ctorCache->IsTypeFinal()))
    {
        GenerateBailAtOperation(instrPtr, IR::BailOutFailedCtorGuardCheck);
    }

    return instr;
}

void
GlobOpt::ValueNumberObjectType(IR::Opnd *dstOpnd, IR::Instr *instr)
{TRACE_IT(6263);
    if (!dstOpnd->IsRegOpnd())
    {TRACE_IT(6264);
        return;
    }

    if (dstOpnd->AsRegOpnd()->m_sym->IsTypeSpec())
    {TRACE_IT(6265);
        return;
    }

    if (instr->IsNewScObjectInstr())
    {TRACE_IT(6266);
        // If we have a NewScObj* for which we have a valid constructor cache we know what type the created object will have.
        // Let's produce the type value accordingly so we don't insert a type check and bailout in the constructor and
        // potentially further downstream.
        Assert(!PHASE_OFF(Js::ObjTypeSpecNewObjPhase, this->func) || !instr->HasBailOutInfo());

        if (instr->HasBailOutInfo())
        {TRACE_IT(6267);
            Assert(instr->IsProfiledInstr());
            Assert(instr->GetBailOutKind() == IR::BailOutFailedCtorGuardCheck);

            bool isCtorInlined = instr->m_opcode == Js::OpCode::NewScObjectNoCtor;
            JITTimeConstructorCache * ctorCache = instr->m_func->GetConstructorCache(static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId));
            Assert(ctorCache != nullptr && (isCtorInlined || ctorCache->IsTypeFinal()));

            StackSym* objSym = dstOpnd->AsRegOpnd()->m_sym;
            StackSym* dstTypeSym = EnsureObjectTypeSym(objSym);
            Assert(this->FindValue(dstTypeSym) == nullptr);

            SetObjectTypeFromTypeSym(dstTypeSym, ctorCache->GetType(), nullptr);
        }
    }
    else
    {TRACE_IT(6268);
        // If the dst opnd is a reg that has a type sym associated with it, then we are either killing
        // the type's existing value or (in the case of a reg copy) assigning it the value of
        // the src's type sym (if any). If the dst doesn't have a type sym, but the src does, let's
        // give dst a new type sym and transfer the value.
        Value *newValue = nullptr;
        IR::Opnd * srcOpnd = instr->GetSrc1();

        if (instr->m_opcode == Js::OpCode::Ld_A && srcOpnd->IsRegOpnd() &&
            !srcOpnd->AsRegOpnd()->m_sym->IsTypeSpec() && srcOpnd->AsRegOpnd()->m_sym->HasObjectTypeSym())
        {TRACE_IT(6269);
            StackSym *srcTypeSym = srcOpnd->AsRegOpnd()->m_sym->GetObjectTypeSym();
            newValue = this->FindValue(srcTypeSym);
        }

        if (newValue == nullptr)
        {TRACE_IT(6270);
            if (dstOpnd->AsRegOpnd()->m_sym->HasObjectTypeSym())
            {TRACE_IT(6271);
                StackSym * typeSym = dstOpnd->AsRegOpnd()->m_sym->GetObjectTypeSym();
                this->blockData.symToValueMap->Clear(typeSym->m_id);
            }
        }
        else
        {TRACE_IT(6272);
            Assert(newValue->GetValueInfo()->IsJsType());
            StackSym * typeSym;
            if (!dstOpnd->AsRegOpnd()->m_sym->HasObjectTypeSym())
            {TRACE_IT(6273);
                typeSym = nullptr;
            }
            typeSym = EnsureObjectTypeSym(dstOpnd->AsRegOpnd()->m_sym);
            this->SetValue(&this->blockData, newValue, typeSym);
        }
    }
}

IR::Instr *
GlobOpt::SetTypeCheckBailOut(IR::Opnd *opnd, IR::Instr *instr, BailOutInfo *bailOutInfo)
{TRACE_IT(6274);
    if (this->IsLoopPrePass() || !opnd->IsSymOpnd())
    {TRACE_IT(6275);
        return instr;
    }

    if (!opnd->AsSymOpnd()->IsPropertySymOpnd())
    {TRACE_IT(6276);
        return instr;
    }

    IR::PropertySymOpnd * propertySymOpnd = opnd->AsPropertySymOpnd();

    AssertMsg(propertySymOpnd->TypeCheckSeqBitsSetOnlyIfCandidate(), "Property sym operand optimized despite not being a candidate?");
    AssertMsg(bailOutInfo == nullptr || !instr->HasBailOutInfo(), "Why are we adding new bailout info to an instruction that already has it?");

    auto HandleBailout = [&](IR::BailOutKind bailOutKind)->void {
        // At this point, we have a cached type that is live downstream or the type check is required
        // for a fixed field load. If we can't do away with the type check, then we're going to need bailout,
        // so lets add bailout info if we don't already have it.
        if (!instr->HasBailOutInfo())
        {TRACE_IT(6277);
            if (bailOutInfo)
            {TRACE_IT(6278);
                instr = instr->ConvertToBailOutInstr(bailOutInfo, bailOutKind);
            }
            else
            {
                GenerateBailAtOperation(&instr, bailOutKind);
                BailOutInfo *bailOutInfo = instr->GetBailOutInfo();

                // Consider (ObjTypeSpec): If we're checking a fixed field here the bailout could be due to polymorphism or
                // due to a fixed field turning non-fixed. Consider distinguishing between the two.
                bailOutInfo->polymorphicCacheIndex = propertySymOpnd->m_inlineCacheIndex;
            }
        }
        else if (instr->GetBailOutKind() == IR::BailOutMarkTempObject)
        {TRACE_IT(6279);
            Assert(!bailOutInfo);
            Assert(instr->GetBailOutInfo()->polymorphicCacheIndex == -1);
            instr->SetBailOutKind(bailOutKind | IR::BailOutMarkTempObject);
            instr->GetBailOutInfo()->polymorphicCacheIndex = propertySymOpnd->m_inlineCacheIndex;
        }
        else
        {TRACE_IT(6280);
            Assert(bailOutKind == instr->GetBailOutKind());
        }
    };

    bool isTypeCheckProtected;
    IR::BailOutKind bailOutKind;
    if (GlobOpt::NeedsTypeCheckBailOut(instr, propertySymOpnd, opnd == instr->GetDst(), &isTypeCheckProtected, &bailOutKind))
    {TRACE_IT(6281);
        HandleBailout(bailOutKind);
    }
    else
    {TRACE_IT(6282);
        if (instr->m_opcode == Js::OpCode::LdMethodFromFlags)
        {TRACE_IT(6283);
            // If LdMethodFromFlags is hoisted to the top of the loop, we should share the same bailout Info.
            // We don't need to do anything for LdMethodFromFlags that cannot be field hoisted.
            HandleBailout(IR::BailOutFailedInlineTypeCheck);
        }
        else if (instr->HasBailOutInfo())
        {TRACE_IT(6284);
            // If we already have a bailout info, but don't actually need it, let's remove it. This can happen if
            // a CheckFixedFld added by the inliner (with bailout info) determined that the object's type has
            // been checked upstream and no bailout is necessary here.
            if (instr->m_opcode == Js::OpCode::CheckFixedFld)
            {TRACE_IT(6285);
                AssertMsg(!PHASE_OFF(Js::FixedMethodsPhase, instr->m_func) ||
                    !PHASE_OFF(Js::UseFixedDataPropsPhase, instr->m_func), "CheckFixedFld with fixed method/data phase disabled?");
                Assert(isTypeCheckProtected);
                AssertMsg(instr->GetBailOutKind() == IR::BailOutFailedFixedFieldTypeCheck || instr->GetBailOutKind() == IR::BailOutFailedEquivalentFixedFieldTypeCheck,
                    "Only BailOutFailed[Equivalent]FixedFieldTypeCheck can be safely removed.  Why does CheckFixedFld carry a different bailout kind?.");
                instr->ClearBailOutInfo();
            }
            else if (propertySymOpnd->MayNeedTypeCheckProtection() && propertySymOpnd->IsTypeCheckProtected())
            {TRACE_IT(6286);
                // Both the type and (if necessary) the proto object have been checked.
                // We're doing a direct slot access. No possibility of bailout here (not even implicit call).
                Assert(instr->GetBailOutKind() == IR::BailOutMarkTempObject);
                instr->ClearBailOutInfo();
            }
        }
    }

    return instr;
}

void
GlobOpt::SetSingleTypeOnObjectTypeValue(Value* value, const JITTypeHolder type)
{
    UpdateObjectTypeValue(value, type, true, nullptr, false);
}

void
GlobOpt::SetTypeSetOnObjectTypeValue(Value* value, Js::EquivalentTypeSet* typeSet)
{
    UpdateObjectTypeValue(value, nullptr, false, typeSet, true);
}

void
GlobOpt::UpdateObjectTypeValue(Value* value, const JITTypeHolder type, bool setType, Js::EquivalentTypeSet* typeSet, bool setTypeSet)
{TRACE_IT(6287);
    Assert(value->GetValueInfo() != nullptr && value->GetValueInfo()->IsJsType());
    JsTypeValueInfo* valueInfo = value->GetValueInfo()->AsJsType();

    if (valueInfo->GetIsShared())
    {TRACE_IT(6288);
        valueInfo = valueInfo->Copy(this->alloc);
        value->SetValueInfo(valueInfo);
    }

    if (setType)
    {TRACE_IT(6289);
        valueInfo->SetJsType(type);
    }
    if (setTypeSet)
    {TRACE_IT(6290);
        valueInfo->SetJsTypeSet(typeSet);
    }
}

void
GlobOpt::SetObjectTypeFromTypeSym(StackSym *typeSym, Value* value, BasicBlock* block)
{TRACE_IT(6291);
    Assert(typeSym != nullptr);
    Assert(value != nullptr);
    Assert(value->GetValueInfo() != nullptr && value->GetValueInfo()->IsJsType());

    SymID typeSymId = typeSym->m_id;

    if (block == nullptr)
    {TRACE_IT(6292);
        block = this->currentBlock;
    }

    SetValue(&block->globOptData, value, typeSym);
    block->globOptData.liveFields->Set(typeSymId);
}

void
GlobOpt::SetObjectTypeFromTypeSym(StackSym *typeSym, const JITTypeHolder type, Js::EquivalentTypeSet * typeSet, BasicBlock* block, bool updateExistingValue)
{TRACE_IT(6293);
    if (block == nullptr)
    {TRACE_IT(6294);
        block = this->currentBlock;
    }

    SetObjectTypeFromTypeSym(typeSym, type, typeSet, &block->globOptData, updateExistingValue);
}

void
GlobOpt::SetObjectTypeFromTypeSym(StackSym *typeSym, const JITTypeHolder type, Js::EquivalentTypeSet * typeSet, GlobOptBlockData *blockData, bool updateExistingValue)
{TRACE_IT(6295);
    Assert(typeSym != nullptr);

    SymID typeSymId = typeSym->m_id;

    if (blockData == nullptr)
    {TRACE_IT(6296);
        blockData = &this->blockData;
    }

    if (updateExistingValue)
    {TRACE_IT(6297);
        Value* value = FindValueFromHashTable(blockData->symToValueMap, typeSymId);

        // If we're trying to update an existing value, the value better exist. We only do this when updating a generic
        // value created during loop pre-pass for field hoisting, so we expect the value info to still be blank.
        Assert(value != nullptr && value->GetValueInfo() != nullptr && value->GetValueInfo()->IsJsType());
        JsTypeValueInfo* valueInfo = value->GetValueInfo()->AsJsType();
        Assert(valueInfo->GetJsType() == nullptr && valueInfo->GetJsTypeSet() == nullptr);
        UpdateObjectTypeValue(value, type, true, typeSet, true);
    }
    else
    {TRACE_IT(6298);
        JsTypeValueInfo* valueInfo = JsTypeValueInfo::New(this->alloc, type, typeSet);
        this->SetSymStoreDirect(valueInfo, typeSym);
        Value* value = NewValue(valueInfo);
        SetValue(blockData, value, typeSym);
    }

    blockData->liveFields->Set(typeSymId);
}

void
GlobOpt::KillObjectType(StackSym* objectSym, BVSparse<JitArenaAllocator>* liveFields)
{TRACE_IT(6299);
    if (objectSym->IsTypeSpec())
    {TRACE_IT(6300);
        objectSym = objectSym->GetVarEquivSym(this->func);
    }

    Assert(objectSym);

    // We may be conservatively attempting to kill type syms from object syms that don't actually
    // participate in object type specialization and hence don't actually have type syms (yet).
    if (!objectSym->HasObjectTypeSym())
    {TRACE_IT(6301);
        return;
    }

    if (liveFields == nullptr)
    {TRACE_IT(6302);
        liveFields = this->blockData.liveFields;
    }

    liveFields->Clear(objectSym->GetObjectTypeSym()->m_id);
}

void
GlobOpt::KillAllObjectTypes(BVSparse<JitArenaAllocator>* liveFields)
{TRACE_IT(6303);
    if (this->objectTypeSyms)
    {TRACE_IT(6304);
        if (liveFields == nullptr)
        {TRACE_IT(6305);
            liveFields = this->blockData.liveFields;
        }

        liveFields->Minus(this->objectTypeSyms);
    }
}

void
GlobOpt::EndFieldLifetime(IR::SymOpnd *symOpnd)
{TRACE_IT(6306);
    this->blockData.liveFields->Clear(symOpnd->m_sym->m_id);
}

PropertySym *
GlobOpt::CopyPropPropertySymObj(IR::SymOpnd *symOpnd, IR::Instr *instr)
{TRACE_IT(6307);
    Assert(symOpnd->m_sym->IsPropertySym());

    PropertySym *propertySym = symOpnd->m_sym->AsPropertySym();

    StackSym *objSym = propertySym->m_stackSym;

    Value * val = this->FindValue(objSym);

    if (val && !PHASE_OFF(Js::ObjPtrCopyPropPhase, this->func))
    {TRACE_IT(6308);
        StackSym *copySym = this->GetCopyPropSym(objSym, val);
        if (copySym != nullptr)
        {TRACE_IT(6309);
            PropertySym *newProp = PropertySym::FindOrCreate(
                copySym->m_id, propertySym->m_propertyId, propertySym->GetPropertyIdIndex(), propertySym->GetInlineCacheIndex(), propertySym->m_fieldKind, this->func);

            if (!this->IsLoopPrePass() || (objSym->IsSingleDef() && copySym->IsSingleDef()))
            {TRACE_IT(6310);
#if DBG_DUMP
                if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::GlobOptPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
                {TRACE_IT(6311);
                    Output::Print(_u("TRACE: "));
                    symOpnd->Dump();
                    Output::Print(_u(" : "));
                    Output::Print(_u("Copy prop obj ptr s%d, new property: "), copySym->m_id);
                    newProp->Dump();
                    Output::Print(_u("\n"));
                }
#endif

                // Copy prop
                this->CaptureByteCodeSymUses(instr);

                // If the old sym was part of an object type spec type check sequence,
                // let's make sure the new one is prepped for it as well.
                if (IsPropertySymPreparedForTypeCheckSeq(propertySym))
                {TRACE_IT(6312);
                    PreparePropertySymForTypeCheckSeq(newProp);
                }
                symOpnd->m_sym = newProp;
                symOpnd->SetIsJITOptimizedReg(true);

                if (symOpnd->IsPropertySymOpnd())
                {TRACE_IT(6313);
                    IR::PropertySymOpnd *propertySymOpnd = symOpnd->AsPropertySymOpnd();

                    // This is no longer strictly necessary, since we don't set the type dead bits in the initial
                    // backward pass, but let's keep it around for now in case we choose to revert to the old model.
                    propertySymOpnd->SetTypeDeadIfTypeCheckSeqCandidate(false);
                }

                if (this->IsLoopPrePass())
                {TRACE_IT(6314);
                    this->prePassCopyPropSym->Set(copySym->m_id);
                }
            }
            propertySym = newProp;

            if(instr->GetDst() && symOpnd->IsEqual(instr->GetDst()))
            {TRACE_IT(6315);
                // Make sure any stack sym uses in the new destination property sym are unspecialized
                instr = ToVarUses(instr, symOpnd, true, nullptr);
            }
        }
    }

    return propertySym;
}

void
GlobOpt::UpdateObjPtrValueType(IR::Opnd * opnd, IR::Instr * instr)
{TRACE_IT(6316);
    if (!opnd->IsSymOpnd() || !opnd->AsSymOpnd()->IsPropertySymOpnd())
    {TRACE_IT(6317);
        return;
    }

    if (!instr->HasTypeCheckBailOut())
    {TRACE_IT(6318);
        // No type check bailout, we didn't check that type of the object pointer.
        return;
    }

    // Only check that fixed field should have type check bailout in loop prepass.
    Assert(instr->m_opcode == Js::OpCode::CheckFixedFld || !this->IsLoopPrePass());

    if (instr->m_opcode != Js::OpCode::CheckFixedFld)
    {TRACE_IT(6319);
        // DeadStore pass may remove type check bailout, except CheckFixedFld which always needs
        // type check bailout. So we can only change the type for CheckFixedFld.
        // Consider: See if we can expand that in the future.
        return;
    }

    IR::PropertySymOpnd * propertySymOpnd = opnd->AsPropertySymOpnd();
    StackSym * objectSym = propertySymOpnd->GetObjectSym();
    Value * objVal = this->FindValue(objectSym);
    if (!objVal)
    {TRACE_IT(6320);
        return;
    }

    ValueType objValueType = objVal->GetValueInfo()->Type();
    if (objValueType.IsDefinite())
    {TRACE_IT(6321);
        return;
    }

    // Verify that the types we're checking for here have been locked so that the type ID's can't be changed
    // without changing the type.
    if (!propertySymOpnd->HasObjectTypeSym())
    {TRACE_IT(6322);
        return;
    }

    StackSym * typeSym = propertySymOpnd->GetObjectTypeSym();
    Assert(typeSym);
    Value * typeValue = this->FindObjectTypeValue(typeSym, currentBlock);
    if (!typeValue)
    {TRACE_IT(6323);
        return;
    }
    JsTypeValueInfo * typeValueInfo = typeValue->GetValueInfo()->AsJsType();
    JITTypeHolder type = typeValueInfo->GetJsType();
    if (type != nullptr)
    {TRACE_IT(6324);
        if (Js::DynamicType::Is(type->GetTypeId()) &&
            !type->GetTypeHandler()->IsLocked())
        {TRACE_IT(6325);
            return;
        }
    }
    else
    {TRACE_IT(6326);
        Js::EquivalentTypeSet * typeSet = typeValueInfo->GetJsTypeSet();
        Assert(typeSet);
        for (uint16 i = 0; i < typeSet->GetCount(); i++)
        {TRACE_IT(6327);
            type = typeSet->GetType(i);
            if (Js::DynamicType::Is(type->GetTypeId()) &&
                !type->GetTypeHandler()->IsLocked())
            {TRACE_IT(6328);
                return;
            }
        }
    }

    AnalysisAssert(type != nullptr);
    Js::TypeId typeId = type->GetTypeId();

    // Passing false for useVirtual as we would never have a virtual typed array hitting this code path
    ValueType newValueType = ValueType::FromTypeId(typeId, false);

    if (newValueType == ValueType::Uninitialized)
    {TRACE_IT(6329);
        switch (typeId)
        {
        default:
            if (typeId > Js::TypeIds_LastStaticType)
            {TRACE_IT(6330);
                Assert(typeId != Js::TypeIds_Proxy);
                if (objValueType.IsLikelyArrayOrObjectWithArray())
                {TRACE_IT(6331);
                    // If we have likely object with array before, we can't make it definite object with array
                    // since we have only proved that it is an object.
                    // Keep the likely array or object with array.
                }
                else
                {TRACE_IT(6332);
                    newValueType = ValueType::GetObject(ObjectType::Object);
                }
            }
            break;
        case Js::TypeIds_Array:
            // Because array can change type id, we can only make it definite if we are doing array check hoist
            // so that implicit call will be installed between the array checks.
            if (!DoArrayCheckHoist() ||
                (currentBlock->loop
                ? !this->ImplicitCallFlagsAllowOpts(currentBlock->loop)
                : !this->ImplicitCallFlagsAllowOpts(this->func)))
            {TRACE_IT(6333);
                break;
            }
            if (objValueType.IsLikelyArrayOrObjectWithArray())
            {TRACE_IT(6334);
                // If we have likely no missing values before, keep the likely, because, we haven't proven that
                // the array really has no missing values
                if (!objValueType.HasNoMissingValues())
                {TRACE_IT(6335);
                    newValueType = ValueType::GetObject(ObjectType::Array).SetArrayTypeId(typeId);
                }
            }
            else
            {TRACE_IT(6336);
                newValueType = ValueType::GetObject(ObjectType::Array).SetArrayTypeId(typeId);
            }
            break;
        }
    }
    if (newValueType != ValueType::Uninitialized)
    {
        ChangeValueType(currentBlock, objVal, newValueType, false, true);
    }
}
