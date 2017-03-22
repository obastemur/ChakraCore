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
{LOGMEIN("MutationBreakpoint.cpp] 18\n");
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
{LOGMEIN("MutationBreakpoint.cpp] 33\n");}

bool Js::MutationBreakpoint::HandleSetProperty(Js::ScriptContext *scriptContext, RecyclableObject *object, Js::PropertyId propertyId, Var newValue)
{LOGMEIN("MutationBreakpoint.cpp] 36\n");
    Assert(scriptContext);
    Assert(object);
    ScriptContext *objectContext = object->GetScriptContext();
    if (IsFeatureEnabled(scriptContext)
        && objectContext->HasMutationBreakpoints())
    {LOGMEIN("MutationBreakpoint.cpp] 42\n");
        MutationBreakpoint *bp = nullptr;
        DynamicObject *dynObj = DynamicObject::FromVar(object);

        if (dynObj->GetInternalProperty(object, InternalPropertyIds::MutationBp, reinterpret_cast<Var*>(&bp), nullptr, objectContext)
            && bp)
        {LOGMEIN("MutationBreakpoint.cpp] 48\n");
            if (!bp->IsValid())
            {LOGMEIN("MutationBreakpoint.cpp] 50\n");
                bp->Reset();
            }
            else
            {
                MutationType mutationType = MutationTypeUpdate;
                if (!object->HasProperty(propertyId))
                {LOGMEIN("MutationBreakpoint.cpp] 57\n");
                    mutationType = MutationTypeAdd;
                }
                if (bp->ShouldBreak(mutationType, propertyId))
                {LOGMEIN("MutationBreakpoint.cpp] 61\n");
                    const PropertyRecord *pr = scriptContext->GetPropertyName(propertyId);
                    bp->newValue = newValue;
                    bp->Break(scriptContext, mutationType, pr);
                    bp->newValue = nullptr;
                    return true;
                }
                else
                {
                    // Mutation breakpoint exists; do not update cache
                    return true;
                }
            }
        }
    }
    return false;
}

void Js::MutationBreakpoint::HandleDeleteProperty(ScriptContext *scriptContext, Var instance, PropertyId propertyId)
{LOGMEIN("MutationBreakpoint.cpp] 80\n");
    Assert(scriptContext);
    Assert(instance);
    if (MutationBreakpoint::CanSet(instance))
    {LOGMEIN("MutationBreakpoint.cpp] 84\n");
        DynamicObject *obj = DynamicObject::FromVar(instance);
        if (obj->GetScriptContext()->HasMutationBreakpoints())
        {LOGMEIN("MutationBreakpoint.cpp] 87\n");
            MutationBreakpoint *bp = nullptr;
            if (obj->GetInternalProperty(obj, InternalPropertyIds::MutationBp, reinterpret_cast<Var *>(&bp), nullptr, scriptContext)
                && bp)
            {LOGMEIN("MutationBreakpoint.cpp] 91\n");
                if (!bp->IsValid())
                {LOGMEIN("MutationBreakpoint.cpp] 93\n");
                    bp->Reset();
                }
                else if (bp->ShouldBreak(MutationTypeDelete, propertyId))
                {LOGMEIN("MutationBreakpoint.cpp] 97\n");
                    const PropertyRecord *pr = scriptContext->GetPropertyName(propertyId);
                    bp->Break(scriptContext, MutationTypeDelete, pr);
                }
            }
        }
    }
}

void Js::MutationBreakpoint::HandleDeleteProperty(ScriptContext *scriptContext, Var instance, Js::JavascriptString *propertyNameString)
{LOGMEIN("MutationBreakpoint.cpp] 107\n");
    PropertyRecord const *propertyRecord = nullptr;
    DynamicObject *obj = DynamicObject::FromVar(instance);
    
    if (JavascriptOperators::ShouldTryDeleteProperty(obj, propertyNameString, &propertyRecord))
    {LOGMEIN("MutationBreakpoint.cpp] 112\n");
        Assert(propertyRecord);
        HandleDeleteProperty(scriptContext, instance, propertyRecord->GetPropertyId());
    }
}

bool Js::MutationBreakpoint::DeleteProperty(PropertyRecord *pr)
{LOGMEIN("MutationBreakpoint.cpp] 119\n");
    Assert(pr != nullptr);

    for (int i = 0; i < this->properties->Count(); i++)
    {LOGMEIN("MutationBreakpoint.cpp] 123\n");
        PropertyMutation pm = this->properties->Item(i);
        if (pm.pr == pr)
        {LOGMEIN("MutationBreakpoint.cpp] 126\n");
            this->properties->RemoveAt(i);
            return true;
        }
    }
    return false;
}

const Js::Var Js::MutationBreakpoint::GetMutationObjectVar() const
{LOGMEIN("MutationBreakpoint.cpp] 135\n");
    Var retVar = nullptr;
    Assert(this->didCauseBreak);
    if (this->obj != nullptr)
    {LOGMEIN("MutationBreakpoint.cpp] 139\n");
        DynamicObject *dynObj = this->obj->Get();
        if (dynObj != nullptr)
        {LOGMEIN("MutationBreakpoint.cpp] 142\n");
            retVar = static_cast<Js::Var>(dynObj);
        }
    }
    return retVar;
}

const Js::Var Js::MutationBreakpoint::GetBreakNewValueVar() const
{LOGMEIN("MutationBreakpoint.cpp] 150\n");

    Assert(this->didCauseBreak);
    return this->newValue;
}

bool Js::MutationBreakpoint::IsFeatureEnabled(ScriptContext *scriptContext)
{LOGMEIN("MutationBreakpoint.cpp] 157\n");
    Assert(scriptContext != nullptr);
    return scriptContext->IsScriptContextInDebugMode() && !PHASE_OFF1(Js::ObjectMutationBreakpointPhase);
}

bool Js::MutationBreakpoint::CanSet(Var object)
{LOGMEIN("MutationBreakpoint.cpp] 163\n");
    if (!object)
    {LOGMEIN("MutationBreakpoint.cpp] 165\n");
        return false;
    }

    TypeId id = JavascriptOperators::GetTypeId(object);
    return JavascriptOperators::IsObjectType(id) && !JavascriptOperators::IsSpecialObjectType(id);
}

Js::MutationBreakpoint * Js::MutationBreakpoint::New(ScriptContext *scriptContext, DynamicObject *obj, const PropertyRecord *pr, MutationType type, Js::PropertyId propertyId)
{LOGMEIN("MutationBreakpoint.cpp] 174\n");
    return RecyclerNewFinalized(scriptContext->GetRecycler(), MutationBreakpoint, scriptContext, obj, pr, type, propertyId);
}

Js::MutationBreakpointDelegate * Js::MutationBreakpoint::GetDelegate()
{LOGMEIN("MutationBreakpoint.cpp] 179\n");
    // Create a new breakpoint object if needed
    if (!mutationBreakpointDelegate)
    {LOGMEIN("MutationBreakpoint.cpp] 182\n");
        mutationBreakpointDelegate = Js::MutationBreakpointDelegate::New(this);
        // NOTE: no need to add ref here, as a new MutationBreakpointDelegate is initialized with 1 ref count
    }
    mutationBreakpointDelegate->AddRef();
    return mutationBreakpointDelegate;
}

bool Js::MutationBreakpoint::IsValid() const
{LOGMEIN("MutationBreakpoint.cpp] 191\n");
    return isValid;
}

void Js::MutationBreakpoint::Invalidate()
{
    AssertMsg(isValid, "Breakpoint already invalid");
    isValid = false;
}

// Return true if breakpoint should break on object with a specific mutation type
bool Js::MutationBreakpoint::ShouldBreak(MutationType type)
{LOGMEIN("MutationBreakpoint.cpp] 203\n");
    return ShouldBreak(type, Constants::NoProperty);
}

// Return true if breakpoint should break on object, or a property pid, with
// a specific mutation type
bool Js::MutationBreakpoint::ShouldBreak(MutationType type, PropertyId pid)
{LOGMEIN("MutationBreakpoint.cpp] 210\n");
    Assert(isValid);
    if (mFlag == MutationTypeNone && pid == Constants::NoProperty)
    {LOGMEIN("MutationBreakpoint.cpp] 213\n");
        return false;
    }
    else if (type != MutationTypeNone && (type & mFlag) == type)
    {LOGMEIN("MutationBreakpoint.cpp] 217\n");
        // Break on object
        return true;
    }

    // search properties vector
    for (int i = 0; i < properties->Count(); i++)
    {LOGMEIN("MutationBreakpoint.cpp] 224\n");
        PropertyMutation pm = properties->Item(i);

        if (pm.pr->GetPropertyId() == pid)
        {LOGMEIN("MutationBreakpoint.cpp] 228\n");
            if (pm.mFlag == MutationTypeNone)
            {LOGMEIN("MutationBreakpoint.cpp] 230\n");
                return false;
            }
            else if (type != MutationTypeNone && (pm.mFlag & type) == type)
            {LOGMEIN("MutationBreakpoint.cpp] 234\n");
                return true;
            }
            break;
        }
    }
    return false;
}

bool Js::MutationBreakpoint::Reset()
{LOGMEIN("MutationBreakpoint.cpp] 244\n");
    // Invalidate breakpoint
    if (isValid)
    {LOGMEIN("MutationBreakpoint.cpp] 247\n");
        this->Invalidate();
    }

    // Release existing delegate object
    if (mutationBreakpointDelegate)
    {LOGMEIN("MutationBreakpoint.cpp] 253\n");
        mutationBreakpointDelegate->Release();
        mutationBreakpointDelegate = nullptr;
    }

    // Clear all property records
    if (properties)
    {LOGMEIN("MutationBreakpoint.cpp] 260\n");
        properties->ClearAndZero();
    }

    // Get object and remove strong ref
    DynamicObject *obj = this->obj->Get();
    return obj && obj->SetInternalProperty(InternalPropertyIds::MutationBp, nullptr, PropertyOperation_SpecialValue, NULL);
}

void Js::MutationBreakpoint::SetBreak(MutationType type, const PropertyRecord *pr)
{LOGMEIN("MutationBreakpoint.cpp] 270\n");
    // Break on entire object if pid is NoProperty
    if (!pr)
    {LOGMEIN("MutationBreakpoint.cpp] 273\n");
        if (type == MutationTypeNone)
        {LOGMEIN("MutationBreakpoint.cpp] 275\n");
            mFlag = MutationTypeNone;
        }
        else
        {
            mFlag = static_cast<MutationType>(mFlag | type);
        }
        return;
    }

    // Check if property is already added
    for (int i = 0; i < properties->Count(); i++)
    {LOGMEIN("MutationBreakpoint.cpp] 287\n");
        PropertyMutation& pm = properties->Item(i);
        if (pm.pr == pr)
        {LOGMEIN("MutationBreakpoint.cpp] 290\n");
            // added to existing property mutation struct
            if (type == MutationTypeNone)
            {LOGMEIN("MutationBreakpoint.cpp] 293\n");
                pm.mFlag = MutationTypeNone;
            }
            else
            {
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
{LOGMEIN("MutationBreakpoint.cpp] 312\n");
    this->didCauseBreak = true;
    this->breakMutationType = mutationType;
    this->propertyRecord = pr;

    InterpreterHaltState haltState(STOP_MUTATIONBREAKPOINT, /*executingFunction*/nullptr, this);
    scriptContext->GetDebugContext()->GetProbeContainer()->DispatchMutationBreakpoint(&haltState);

    this->didCauseBreak = false;
}

bool Js::MutationBreakpoint::GetDidCauseBreak() const
{LOGMEIN("MutationBreakpoint.cpp] 324\n");
    return this->didCauseBreak;
}

const char16 * Js::MutationBreakpoint::GetBreakPropertyName() const
{LOGMEIN("MutationBreakpoint.cpp] 329\n");
    Assert(this->didCauseBreak);
    Assert(this->propertyRecord);
    return this->propertyRecord->GetBuffer();
}

const Js::PropertyId Js::MutationBreakpoint::GetParentPropertyId() const
{LOGMEIN("MutationBreakpoint.cpp] 336\n");
    Assert(this->didCauseBreak);
    return this->parentPropertyId;
}

const char16 * Js::MutationBreakpoint::GetParentPropertyName() const
{LOGMEIN("MutationBreakpoint.cpp] 342\n");
    Assert(this->didCauseBreak);
    const PropertyRecord *pr = nullptr;
    if ((this->parentPropertyId != Constants::NoProperty) && (this->obj != nullptr))
    {LOGMEIN("MutationBreakpoint.cpp] 346\n");
        DynamicObject *dynObj = this->obj->Get();
        if (dynObj != nullptr)
        {LOGMEIN("MutationBreakpoint.cpp] 349\n");
            pr = dynObj->GetScriptContext()->GetPropertyName(this->parentPropertyId);
            return pr->GetBuffer();
        }
    }
    return _u("");
}

MutationType Js::MutationBreakpoint::GetBreakMutationType() const
{LOGMEIN("MutationBreakpoint.cpp] 358\n");
    Assert(this->didCauseBreak);
    return this->breakMutationType;
}

const Js::PropertyId Js::MutationBreakpoint::GetBreakPropertyId() const
{LOGMEIN("MutationBreakpoint.cpp] 364\n");
    Assert(this->didCauseBreak);
    Assert(this->propertyRecord);
    return this->propertyRecord->GetPropertyId();
}

const char16 * Js::MutationBreakpoint::GetBreakMutationTypeName(MutationType mutationType)
{LOGMEIN("MutationBreakpoint.cpp] 371\n");
    switch (mutationType)
    {LOGMEIN("MutationBreakpoint.cpp] 373\n");
    case MutationTypeUpdate: return _u("Changing");
    case MutationTypeDelete: return _u("Deleting");
    case MutationTypeAdd: return _u("Adding");
    default: AssertMsg(false, "Unhandled break reason mutation type. Did we add a new mutation type?"); return _u("");
    }
}

const char16 * Js::MutationBreakpoint::GetMutationTypeForConditionalEval(MutationType mutationType)
{LOGMEIN("MutationBreakpoint.cpp] 382\n");
    switch (mutationType)
    {LOGMEIN("MutationBreakpoint.cpp] 384\n");
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
{LOGMEIN("MutationBreakpoint.cpp] 397\n");
    Assert(obj);
    Assert(scriptContext);

    if (!CanSet(obj))
    {LOGMEIN("MutationBreakpoint.cpp] 402\n");
        return nullptr;
    }
    DynamicObject *dynObj = static_cast<DynamicObject*>(obj);

    const PropertyRecord *pr = nullptr;

    if (!setOnObject && (propertyId != Constants::NoProperty))
    {LOGMEIN("MutationBreakpoint.cpp] 410\n");
        pr = scriptContext->GetPropertyName(propertyId);
        Assert(pr);
    }

    MutationBreakpoint *bp = nullptr;

    // Breakpoint exists; update it.
    if (dynObj->GetInternalProperty(dynObj, InternalPropertyIds::MutationBp, reinterpret_cast<Var*>(&bp), nullptr, scriptContext)
        && bp)
    {LOGMEIN("MutationBreakpoint.cpp] 420\n");
        // Valid bp; update it.
        if (bp->IsValid())
        {LOGMEIN("MutationBreakpoint.cpp] 423\n");
            Assert(bp->mutationBreakpointDelegate); // Delegate must already exist
            // set breakpoint
            bp->SetBreak(type, pr);

            return bp->GetDelegate();
        }
        // bp invalidated by haven't got cleaned up yet, so reset it right here
        // and then create new bp
        else
        {
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

void Js::MutationBreakpoint::Finalize(bool isShutdown) {LOGMEIN("MutationBreakpoint.cpp] 450\n");}
void Js::MutationBreakpoint::Dispose(bool isShutdown)
{LOGMEIN("MutationBreakpoint.cpp] 452\n");
    // TODO (t-shchan): If removed due to detaching debugger, do not fire event
    // TODO (t-shchan): Fire debugger event for breakpoint removal
    if (mutationBreakpointDelegate)
    {LOGMEIN("MutationBreakpoint.cpp] 456\n");
        mutationBreakpointDelegate->Release();
    }
}
void Js::MutationBreakpoint::Mark(Recycler * recycler) {LOGMEIN("MutationBreakpoint.cpp] 460\n");}


/*
    MutationBreakpointDelegate definition
*/

Js::MutationBreakpointDelegate::MutationBreakpointDelegate(Js::MutationBreakpoint *bp)
    : m_refCount(1), m_breakpoint(bp), m_didCauseBreak(false), m_propertyRecord(nullptr), m_type(MutationTypeNone)
{LOGMEIN("MutationBreakpoint.cpp] 469\n");
    Assert(bp != nullptr);
}

Js::MutationBreakpointDelegate * Js::MutationBreakpointDelegate::New(Js::MutationBreakpoint *bp)
{LOGMEIN("MutationBreakpoint.cpp] 474\n");
    return HeapNew(Js::MutationBreakpointDelegate, bp);
}

/*
    IMutationBreakpoint interface definition
*/

STDMETHODIMP_(ULONG) Js::MutationBreakpointDelegate::AddRef()
{LOGMEIN("MutationBreakpoint.cpp] 483\n");
    return (ULONG)InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) Js::MutationBreakpointDelegate::Release()
{LOGMEIN("MutationBreakpoint.cpp] 488\n");
    ULONG refCount = (ULONG)InterlockedDecrement(&m_refCount);

    if (0 == refCount)
    {LOGMEIN("MutationBreakpoint.cpp] 492\n");
        HeapDelete(this);
    }

    return refCount;
}

STDMETHODIMP Js::MutationBreakpointDelegate::QueryInterface(REFIID iid, void ** ppv)
{LOGMEIN("MutationBreakpoint.cpp] 500\n");
    if (!ppv)
    {LOGMEIN("MutationBreakpoint.cpp] 502\n");
        return E_INVALIDARG;
    }

    if (__uuidof(IUnknown) == iid || __uuidof(IMutationBreakpoint) == iid)
    {LOGMEIN("MutationBreakpoint.cpp] 507\n");
        *ppv = static_cast<IUnknown*>(static_cast<IMutationBreakpoint*>(this));
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP Js::MutationBreakpointDelegate::Delete(void)
{LOGMEIN("MutationBreakpoint.cpp] 521\n");
    this->m_breakpoint->Invalidate();
    return S_OK;
}

STDMETHODIMP Js::MutationBreakpointDelegate::DidCauseBreak(
    /* [out] */ __RPC__out BOOL *didCauseBreak)
{LOGMEIN("MutationBreakpoint.cpp] 528\n");
    if (!didCauseBreak)
    {LOGMEIN("MutationBreakpoint.cpp] 530\n");
        return E_INVALIDARG;
    }
    *didCauseBreak = this->m_breakpoint->GetDidCauseBreak();
    return S_OK;
}
#endif
