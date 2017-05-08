//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#ifdef ENABLE_MUTATION_BREAKPOINT
Js::MutationBreakpoint::MutationBreakpoint(ScriptContext *scriptContext, DynamicObject *obj, const PropertyRecord *pr, MutationType type, Js::PropertyId parentPropertyId)
    : isValid(true)
    , didCauseBreak(false)
    , mFlag(MutationTypeNone)
    , obj(nullptr)
    , properties(nullptr)
    , mutationBreakpointDelegate(nullptr)
    , breakMutationType(MutationTypeNone)
    , propertyRecord(nullptr)
    , newValue(nullptr)
    , parentPropertyId(Constants::NoProperty)
{TRACE_IT(43133);
    // create weak reference to object
    this->obj = scriptContext->GetRecycler()->CreateWeakReferenceHandle(obj);

    // initialize property mutation list
    this->properties = RecyclerNew(scriptContext->GetRecycler(), PropertyMutationList, scriptContext->GetRecycler());

    // Save the property id of parent object
    this->parentPropertyId = parentPropertyId;

    // set breakpoint
    this->SetBreak(type, pr);
}

Js::MutationBreakpoint::~MutationBreakpoint()
{TRACE_IT(43134);}

bool Js::MutationBreakpoint::HandleSetProperty(Js::ScriptContext *scriptContext, RecyclableObject *object, Js::PropertyId propertyId, Var newValue)
{TRACE_IT(43135);
    Assert(scriptContext);
    Assert(object);
    ScriptContext *objectContext = object->GetScriptContext();
    if (IsFeatureEnabled(scriptContext)
        && objectContext->HasMutationBreakpoints())
    {TRACE_IT(43136);
        MutationBreakpoint *bp = nullptr;
        DynamicObject *dynObj = DynamicObject::FromVar(object);

        if (dynObj->GetInternalProperty(object, InternalPropertyIds::MutationBp, reinterpret_cast<Var*>(&bp), nullptr, objectContext)
            && bp)
        {TRACE_IT(43137);
            if (!bp->IsValid())
            {TRACE_IT(43138);
                bp->Reset();
            }
            else
            {TRACE_IT(43139);
                MutationType mutationType = MutationTypeUpdate;
                if (!object->HasProperty(propertyId))
                {TRACE_IT(43140);
                    mutationType = MutationTypeAdd;
                }
                if (bp->ShouldBreak(mutationType, propertyId))
                {TRACE_IT(43141);
                    const PropertyRecord *pr = scriptContext->GetPropertyName(propertyId);
                    bp->newValue = newValue;
                    bp->Break(scriptContext, mutationType, pr);
                    bp->newValue = nullptr;
                    return true;
                }
                else
                {TRACE_IT(43142);
                    // Mutation breakpoint exists; do not update cache
                    return true;
                }
            }
        }
    }
    return false;
}

void Js::MutationBreakpoint::HandleDeleteProperty(ScriptContext *scriptContext, Var instance, PropertyId propertyId)
{TRACE_IT(43143);
    Assert(scriptContext);
    Assert(instance);
    if (MutationBreakpoint::CanSet(instance))
    {TRACE_IT(43144);
        DynamicObject *obj = DynamicObject::FromVar(instance);
        if (obj->GetScriptContext()->HasMutationBreakpoints())
        {TRACE_IT(43145);
            MutationBreakpoint *bp = nullptr;
            if (obj->GetInternalProperty(obj, InternalPropertyIds::MutationBp, reinterpret_cast<Var *>(&bp), nullptr, scriptContext)
                && bp)
            {TRACE_IT(43146);
                if (!bp->IsValid())
                {TRACE_IT(43147);
                    bp->Reset();
                }
                else if (bp->ShouldBreak(MutationTypeDelete, propertyId))
                {TRACE_IT(43148);
                    const PropertyRecord *pr = scriptContext->GetPropertyName(propertyId);
                    bp->Break(scriptContext, MutationTypeDelete, pr);
                }
            }
        }
    }
}

void Js::MutationBreakpoint::HandleDeleteProperty(ScriptContext *scriptContext, Var instance, Js::JavascriptString *propertyNameString)
{TRACE_IT(43149);
    PropertyRecord const *propertyRecord = nullptr;
    DynamicObject *obj = DynamicObject::FromVar(instance);
    
    if (JavascriptOperators::ShouldTryDeleteProperty(obj, propertyNameString, &propertyRecord))
    {TRACE_IT(43150);
        Assert(propertyRecord);
        HandleDeleteProperty(scriptContext, instance, propertyRecord->GetPropertyId());
    }
}

bool Js::MutationBreakpoint::DeleteProperty(PropertyRecord *pr)
{TRACE_IT(43151);
    Assert(pr != nullptr);

    for (int i = 0; i < this->properties->Count(); i++)
    {TRACE_IT(43152);
        PropertyMutation pm = this->properties->Item(i);
        if (pm.pr == pr)
        {TRACE_IT(43153);
            this->properties->RemoveAt(i);
            return true;
        }
    }
    return false;
}

const Js::Var Js::MutationBreakpoint::GetMutationObjectVar() const
{TRACE_IT(43154);
    Var retVar = nullptr;
    Assert(this->didCauseBreak);
    if (this->obj != nullptr)
    {TRACE_IT(43155);
        DynamicObject *dynObj = this->obj->Get();
        if (dynObj != nullptr)
        {TRACE_IT(43156);
            retVar = static_cast<Js::Var>(dynObj);
        }
    }
    return retVar;
}

const Js::Var Js::MutationBreakpoint::GetBreakNewValueVar() const
{TRACE_IT(43157);

    Assert(this->didCauseBreak);
    return this->newValue;
}

bool Js::MutationBreakpoint::IsFeatureEnabled(ScriptContext *scriptContext)
{TRACE_IT(43158);
    Assert(scriptContext != nullptr);
    return scriptContext->IsScriptContextInDebugMode() && !PHASE_OFF1(Js::ObjectMutationBreakpointPhase);
}

bool Js::MutationBreakpoint::CanSet(Var object)
{TRACE_IT(43159);
    if (!object)
    {TRACE_IT(43160);
        return false;
    }

    TypeId id = JavascriptOperators::GetTypeId(object);
    return JavascriptOperators::IsObjectType(id) && !JavascriptOperators::IsSpecialObjectType(id);
}

Js::MutationBreakpoint * Js::MutationBreakpoint::New(ScriptContext *scriptContext, DynamicObject *obj, const PropertyRecord *pr, MutationType type, Js::PropertyId propertyId)
{TRACE_IT(43161);
    return RecyclerNewFinalized(scriptContext->GetRecycler(), MutationBreakpoint, scriptContext, obj, pr, type, propertyId);
}

Js::MutationBreakpointDelegate * Js::MutationBreakpoint::GetDelegate()
{TRACE_IT(43162);
    // Create a new breakpoint object if needed
    if (!mutationBreakpointDelegate)
    {TRACE_IT(43163);
        mutationBreakpointDelegate = Js::MutationBreakpointDelegate::New(this);
        // NOTE: no need to add ref here, as a new MutationBreakpointDelegate is initialized with 1 ref count
    }
    mutationBreakpointDelegate->AddRef();
    return mutationBreakpointDelegate;
}

bool Js::MutationBreakpoint::IsValid() const
{TRACE_IT(43164);
    return isValid;
}

void Js::MutationBreakpoint::Invalidate()
{
    AssertMsg(isValid, "Breakpoint already invalid");
    isValid = false;
}

// Return true if breakpoint should break on object with a specific mutation type
bool Js::MutationBreakpoint::ShouldBreak(MutationType type)
{TRACE_IT(43165);
    return ShouldBreak(type, Constants::NoProperty);
}

// Return true if breakpoint should break on object, or a property pid, with
// a specific mutation type
bool Js::MutationBreakpoint::ShouldBreak(MutationType type, PropertyId pid)
{TRACE_IT(43166);
    Assert(isValid);
    if (mFlag == MutationTypeNone && pid == Constants::NoProperty)
    {TRACE_IT(43167);
        return false;
    }
    else if (type != MutationTypeNone && (type & mFlag) == type)
    {TRACE_IT(43168);
        // Break on object
        return true;
    }

    // search properties vector
    for (int i = 0; i < properties->Count(); i++)
    {TRACE_IT(43169);
        PropertyMutation pm = properties->Item(i);

        if (pm.pr->GetPropertyId() == pid)
        {TRACE_IT(43170);
            if (pm.mFlag == MutationTypeNone)
            {TRACE_IT(43171);
                return false;
            }
            else if (type != MutationTypeNone && (pm.mFlag & type) == type)
            {TRACE_IT(43172);
                return true;
            }
            break;
        }
    }
    return false;
}

bool Js::MutationBreakpoint::Reset()
{TRACE_IT(43173);
    // Invalidate breakpoint
    if (isValid)
    {TRACE_IT(43174);
        this->Invalidate();
    }

    // Release existing delegate object
    if (mutationBreakpointDelegate)
    {TRACE_IT(43175);
        mutationBreakpointDelegate->Release();
        mutationBreakpointDelegate = nullptr;
    }

    // Clear all property records
    if (properties)
    {TRACE_IT(43176);
        properties->ClearAndZero();
    }

    // Get object and remove strong ref
    DynamicObject *obj = this->obj->Get();
    return obj && obj->SetInternalProperty(InternalPropertyIds::MutationBp, nullptr, PropertyOperation_SpecialValue, NULL);
}

void Js::MutationBreakpoint::SetBreak(MutationType type, const PropertyRecord *pr)
{TRACE_IT(43177);
    // Break on entire object if pid is NoProperty
    if (!pr)
    {TRACE_IT(43178);
        if (type == MutationTypeNone)
        {TRACE_IT(43179);
            mFlag = MutationTypeNone;
        }
        else
        {TRACE_IT(43180);
            mFlag = static_cast<MutationType>(mFlag | type);
        }
        return;
    }

    // Check if property is already added
    for (int i = 0; i < properties->Count(); i++)
    {TRACE_IT(43181);
        PropertyMutation& pm = properties->Item(i);
        if (pm.pr == pr)
        {TRACE_IT(43182);
            // added to existing property mutation struct
            if (type == MutationTypeNone)
            {TRACE_IT(43183);
                pm.mFlag = MutationTypeNone;
            }
            else
            {TRACE_IT(43184);
                pm.mFlag = static_cast<MutationType>(pm.mFlag | type);
            }
            return;
        }
    }
    // if not in list, add new property mutation
    PropertyMutation pm = {
        pr,
        type
    };
    properties->Add(pm);
}

void Js::MutationBreakpoint::Break(ScriptContext *scriptContext, MutationType mutationType, const PropertyRecord *pr)
{TRACE_IT(43185);
    this->didCauseBreak = true;
    this->breakMutationType = mutationType;
    this->propertyRecord = pr;

    InterpreterHaltState haltState(STOP_MUTATIONBREAKPOINT, /*executingFunction*/nullptr, this);
    scriptContext->GetDebugContext()->GetProbeContainer()->DispatchMutationBreakpoint(&haltState);

    this->didCauseBreak = false;
}

bool Js::MutationBreakpoint::GetDidCauseBreak() const
{TRACE_IT(43186);
    return this->didCauseBreak;
}

const char16 * Js::MutationBreakpoint::GetBreakPropertyName() const
{TRACE_IT(43187);
    Assert(this->didCauseBreak);
    Assert(this->propertyRecord);
    return this->propertyRecord->GetBuffer();
}

const Js::PropertyId Js::MutationBreakpoint::GetParentPropertyId() const
{TRACE_IT(43188);
    Assert(this->didCauseBreak);
    return this->parentPropertyId;
}

const char16 * Js::MutationBreakpoint::GetParentPropertyName() const
{TRACE_IT(43189);
    Assert(this->didCauseBreak);
    const PropertyRecord *pr = nullptr;
    if ((this->parentPropertyId != Constants::NoProperty) && (this->obj != nullptr))
    {TRACE_IT(43190);
        DynamicObject *dynObj = this->obj->Get();
        if (dynObj != nullptr)
        {TRACE_IT(43191);
            pr = dynObj->GetScriptContext()->GetPropertyName(this->parentPropertyId);
            return pr->GetBuffer();
        }
    }
    return _u("");
}

MutationType Js::MutationBreakpoint::GetBreakMutationType() const
{TRACE_IT(43192);
    Assert(this->didCauseBreak);
    return this->breakMutationType;
}

const Js::PropertyId Js::MutationBreakpoint::GetBreakPropertyId() const
{TRACE_IT(43193);
    Assert(this->didCauseBreak);
    Assert(this->propertyRecord);
    return this->propertyRecord->GetPropertyId();
}

const char16 * Js::MutationBreakpoint::GetBreakMutationTypeName(MutationType mutationType)
{TRACE_IT(43194);
    switch (mutationType)
    {
    case MutationTypeUpdate: return _u("Changing");
    case MutationTypeDelete: return _u("Deleting");
    case MutationTypeAdd: return _u("Adding");
    default: AssertMsg(false, "Unhandled break reason mutation type. Did we add a new mutation type?"); return _u("");
    }
}

const char16 * Js::MutationBreakpoint::GetMutationTypeForConditionalEval(MutationType mutationType)
{TRACE_IT(43195);
    switch (mutationType)
    {
    case MutationTypeUpdate: return _u("update");
    case MutationTypeDelete: return _u("delete");
    case MutationTypeAdd: return _u("add");
    default: AssertMsg(false, "Unhandled mutation type in conditional object mutation breakpoint."); return _u("");
    }
}

// setOnObject - Is true if we are watching the parent object
// parentPropertyId - Property ID of object on which Mutation is set
// propertyId - Property ID of property. If setOnObject is false we are watching a specific property (propertyId)

Js::MutationBreakpointDelegate * Js::MutationBreakpoint::Set(ScriptContext *scriptContext, Var obj, BOOL setOnObject, MutationType type, PropertyId parentPropertyId, PropertyId propertyId)
{TRACE_IT(43196);
    Assert(obj);
    Assert(scriptContext);

    if (!CanSet(obj))
    {TRACE_IT(43197);
        return nullptr;
    }
    DynamicObject *dynObj = static_cast<DynamicObject*>(obj);

    const PropertyRecord *pr = nullptr;

    if (!setOnObject && (propertyId != Constants::NoProperty))
    {TRACE_IT(43198);
        pr = scriptContext->GetPropertyName(propertyId);
        Assert(pr);
    }

    MutationBreakpoint *bp = nullptr;

    // Breakpoint exists; update it.
    if (dynObj->GetInternalProperty(dynObj, InternalPropertyIds::MutationBp, reinterpret_cast<Var*>(&bp), nullptr, scriptContext)
        && bp)
    {TRACE_IT(43199);
        // Valid bp; update it.
        if (bp->IsValid())
        {TRACE_IT(43200);
            Assert(bp->mutationBreakpointDelegate); // Delegate must already exist
            // set breakpoint
            bp->SetBreak(type, pr);

            return bp->GetDelegate();
        }
        // bp invalidated by haven't got cleaned up yet, so reset it right here
        // and then create new bp
        else
        {TRACE_IT(43201);
            bp->Reset();
        }
    }

    // Create new breakpoint
    bp = MutationBreakpoint::New(scriptContext, dynObj, const_cast<PropertyRecord *>(pr), type, parentPropertyId);

    // Adding reference to dynamic object
    dynObj->SetInternalProperty(InternalPropertyIds::MutationBp, bp, PropertyOperation_SpecialValue, nullptr);

    // Track in object's own script context
    dynObj->GetScriptContext()->InsertMutationBreakpoint(bp);

    return bp->GetDelegate();
}

void Js::MutationBreakpoint::Finalize(bool isShutdown) {TRACE_IT(43202);}
void Js::MutationBreakpoint::Dispose(bool isShutdown)
{TRACE_IT(43203);
    // TODO (t-shchan): If removed due to detaching debugger, do not fire event
    // TODO (t-shchan): Fire debugger event for breakpoint removal
    if (mutationBreakpointDelegate)
    {TRACE_IT(43204);
        mutationBreakpointDelegate->Release();
    }
}
void Js::MutationBreakpoint::Mark(Recycler * recycler) {TRACE_IT(43205);}


/*
    MutationBreakpointDelegate definition
*/

Js::MutationBreakpointDelegate::MutationBreakpointDelegate(Js::MutationBreakpoint *bp)
    : m_refCount(1), m_breakpoint(bp), m_didCauseBreak(false), m_propertyRecord(nullptr), m_type(MutationTypeNone)
{TRACE_IT(43206);
    Assert(bp != nullptr);
}

Js::MutationBreakpointDelegate * Js::MutationBreakpointDelegate::New(Js::MutationBreakpoint *bp)
{TRACE_IT(43207);
    return HeapNew(Js::MutationBreakpointDelegate, bp);
}

/*
    IMutationBreakpoint interface definition
*/

STDMETHODIMP_(ULONG) Js::MutationBreakpointDelegate::AddRef()
{TRACE_IT(43208);
    return (ULONG)InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) Js::MutationBreakpointDelegate::Release()
{TRACE_IT(43209);
    ULONG refCount = (ULONG)InterlockedDecrement(&m_refCount);

    if (0 == refCount)
    {TRACE_IT(43210);
        HeapDelete(this);
    }

    return refCount;
}

STDMETHODIMP Js::MutationBreakpointDelegate::QueryInterface(REFIID iid, void ** ppv)
{TRACE_IT(43211);
    if (!ppv)
    {TRACE_IT(43212);
        return E_INVALIDARG;
    }

    if (__uuidof(IUnknown) == iid || __uuidof(IMutationBreakpoint) == iid)
    {TRACE_IT(43213);
        *ppv = static_cast<IUnknown*>(static_cast<IMutationBreakpoint*>(this));
    }
    else
    {TRACE_IT(43214);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP Js::MutationBreakpointDelegate::Delete(void)
{TRACE_IT(43215);
    this->m_breakpoint->Invalidate();
    return S_OK;
}

STDMETHODIMP Js::MutationBreakpointDelegate::DidCauseBreak(
    /* [out] */ __RPC__out BOOL *didCauseBreak)
{TRACE_IT(43216);
    if (!didCauseBreak)
    {TRACE_IT(43217);
        return E_INVALIDARG;
    }
    *didCauseBreak = this->m_breakpoint->GetDidCauseBreak();
    return S_OK;
}
#endif
