//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptWeakSet::JavascriptWeakSet(DynamicType* type)
        : DynamicObject(type),
        keySet(type->GetScriptContext()->GetRecycler())
    {TRACE_IT(62283);
    }

    bool JavascriptWeakSet::Is(Var aValue)
    {TRACE_IT(62284);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_WeakSet;
    }

    JavascriptWeakSet* JavascriptWeakSet::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptWeakSet'");

        return static_cast<JavascriptWeakSet *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptWeakSet::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);
        CHAKRATEL_LANGSTATS_INC_DATACOUNT(ES6_WeakSet);

        JavascriptWeakSet* weakSetObject = nullptr;

        if (callInfo.Flags & CallFlags_New)
        {TRACE_IT(62285);
             weakSetObject = library->CreateWeakSet();
        }
        else
        {TRACE_IT(62286);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakSet"), _u("WeakSet"));
        }
        Assert(weakSetObject != nullptr);

        Var iterable = (args.Info.Count > 1) ? args[1] : library->GetUndefined();

        RecyclableObject* iter = nullptr;
        RecyclableObject* adder = nullptr;

        if (JavascriptConversion::CheckObjectCoercible(iterable, scriptContext))
        {TRACE_IT(62287);
            iter = JavascriptOperators::GetIterator(iterable, scriptContext);
            Var adderVar = JavascriptOperators::GetProperty(weakSetObject, PropertyIds::add, scriptContext);
            if (!JavascriptConversion::IsCallable(adderVar))
            {TRACE_IT(62288);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
            }
            adder = RecyclableObject::FromVar(adderVar);
        }

        if (iter != nullptr)
        {TRACE_IT(62289);
            JavascriptOperators::DoIteratorStepAndValue(iter, scriptContext, [&](Var nextItem) {
                CALL_FUNCTION(adder, CallInfo(CallFlags_Value, 2), weakSetObject, nextItem);
            });
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), weakSetObject, nullptr, scriptContext) :
            weakSetObject;
    }

    Var JavascriptWeakSet::EntryAdd(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptWeakSet::Is(args[0]))
        {TRACE_IT(62290);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakSet.prototype.add"), _u("WeakSet"));
        }

        JavascriptWeakSet* weakSet = JavascriptWeakSet::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        if (!JavascriptOperators::IsObject(key) || JavascriptOperators::GetTypeId(key) == TypeIds_HostDispatch)
        {TRACE_IT(62291);
            // HostDispatch is not expanded so can't have internal property added to it.
            // TODO: Support HostDispatch as WeakSet key
            JavascriptError::ThrowTypeError(scriptContext, JSERR_WeakMapSetKeyNotAnObject, _u("WeakSet.prototype.add"));
        }

        DynamicObject* keyObj = DynamicObject::FromVar(key);

#if ENABLE_TTD
        //
        //This makes the set decidedly less weak -- forces it to only release when we clean the tracking set but determinizes the behavior nicely
        //      We want to improve this.
        //
        if(scriptContext->IsTTDRecordOrReplayModeEnabled())
        {TRACE_IT(62292);
            scriptContext->TTDContextInfo->TTDWeakReferencePinSet->Add(keyObj);
        }
#endif

        weakSet->Add(keyObj);

        return weakSet;
    }

    Var JavascriptWeakSet::EntryDelete(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptWeakSet::Is(args[0]))
        {TRACE_IT(62293);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakSet.prototype.delete"), _u("WeakSet"));
        }

        JavascriptWeakSet* weakSet = JavascriptWeakSet::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        bool didDelete = false;

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {TRACE_IT(62294);
            DynamicObject* keyObj = DynamicObject::FromVar(key);

            didDelete = weakSet->Delete(keyObj);
        }

        return scriptContext->GetLibrary()->CreateBoolean(didDelete);
    }

    Var JavascriptWeakSet::EntryHas(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptWeakSet::Is(args[0]))
        {TRACE_IT(62295);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakSet.prototype.has"), _u("WeakSet"));
        }

        JavascriptWeakSet* weakSet = JavascriptWeakSet::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        bool hasValue = false;

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {TRACE_IT(62296);
            DynamicObject* keyObj = DynamicObject::FromVar(key);

            hasValue = weakSet->Has(keyObj);
        }

        return scriptContext->GetLibrary()->CreateBoolean(hasValue);
    }

    void JavascriptWeakSet::Add(DynamicObject* key)
    {TRACE_IT(62297);
        keySet.Item(key, true);
    }

    bool JavascriptWeakSet::Delete(DynamicObject* key)
    {TRACE_IT(62298);
        bool unused = false;
        return keySet.TryGetValueAndRemove(key, &unused);
    }

    bool JavascriptWeakSet::Has(DynamicObject* key)
    {TRACE_IT(62299);
        bool unused = false;
        return keySet.TryGetValue(key, &unused);
    }

    BOOL JavascriptWeakSet::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(62300);
        stringBuilder->AppendCppLiteral(_u("WeakSet"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptWeakSet::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(62301);
        this->Map([&](DynamicObject* key)
        {
            extractor->MarkVisitVar(key);
        });
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptWeakSet::GetSnapTag_TTD() const
    {TRACE_IT(62302);
        return TTD::NSSnapObjects::SnapObjectType::SnapSetObject;
    }

    void JavascriptWeakSet::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(62303);
        TTD::NSSnapObjects::SnapSetInfo* ssi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapSetInfo>();
        uint32 setCountEst = this->Size();

        ssi->SetSize = 0;
        ssi->SetValueArray = alloc.SlabReserveArraySpace<TTD::TTDVar>(setCountEst + 1); //always reserve at least 1 element

        this->Map([&](DynamicObject* key)
        {
            AssertMsg(ssi->SetSize < setCountEst, "We are writting junk");

            ssi->SetValueArray[ssi->SetSize] = key;
            ssi->SetSize++;
        });

        if(ssi->SetSize == 0)
        {TRACE_IT(62304);
            ssi->SetValueArray = nullptr;
            alloc.SlabAbortArraySpace<TTD::TTDVar>(setCountEst + 1);
        }
        else
        {TRACE_IT(62305);
            alloc.SlabCommitArraySpace<TTD::TTDVar>(ssi->SetSize, setCountEst + 1);
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapSetInfo*, TTD::NSSnapObjects::SnapObjectType::SnapSetObject>(objData, ssi);
    }
#endif
}
