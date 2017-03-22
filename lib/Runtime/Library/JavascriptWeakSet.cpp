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
    {LOGMEIN("JavascriptWeakSet.cpp] 11\n");
    }

    bool JavascriptWeakSet::Is(Var aValue)
    {LOGMEIN("JavascriptWeakSet.cpp] 15\n");
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
        {LOGMEIN("JavascriptWeakSet.cpp] 42\n");
             weakSetObject = library->CreateWeakSet();
        }
        else
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakSet"), _u("WeakSet"));
        }
        Assert(weakSetObject != nullptr);

        Var iterable = (args.Info.Count > 1) ? args[1] : library->GetUndefined();

        RecyclableObject* iter = nullptr;
        RecyclableObject* adder = nullptr;

        if (JavascriptConversion::CheckObjectCoercible(iterable, scriptContext))
        {LOGMEIN("JavascriptWeakSet.cpp] 57\n");
            iter = JavascriptOperators::GetIterator(iterable, scriptContext);
            Var adderVar = JavascriptOperators::GetProperty(weakSetObject, PropertyIds::add, scriptContext);
            if (!JavascriptConversion::IsCallable(adderVar))
            {LOGMEIN("JavascriptWeakSet.cpp] 61\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
            }
            adder = RecyclableObject::FromVar(adderVar);
        }

        if (iter != nullptr)
        {LOGMEIN("JavascriptWeakSet.cpp] 68\n");
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
        {LOGMEIN("JavascriptWeakSet.cpp] 87\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakSet.prototype.add"), _u("WeakSet"));
        }

        JavascriptWeakSet* weakSet = JavascriptWeakSet::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        if (!JavascriptOperators::IsObject(key) || JavascriptOperators::GetTypeId(key) == TypeIds_HostDispatch)
        {LOGMEIN("JavascriptWeakSet.cpp] 96\n");
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
        {LOGMEIN("JavascriptWeakSet.cpp] 110\n");
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
        {LOGMEIN("JavascriptWeakSet.cpp] 128\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakSet.prototype.delete"), _u("WeakSet"));
        }

        JavascriptWeakSet* weakSet = JavascriptWeakSet::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        bool didDelete = false;

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {LOGMEIN("JavascriptWeakSet.cpp] 138\n");
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
        {LOGMEIN("JavascriptWeakSet.cpp] 155\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakSet.prototype.has"), _u("WeakSet"));
        }

        JavascriptWeakSet* weakSet = JavascriptWeakSet::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        bool hasValue = false;

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {LOGMEIN("JavascriptWeakSet.cpp] 165\n");
            DynamicObject* keyObj = DynamicObject::FromVar(key);

            hasValue = weakSet->Has(keyObj);
        }

        return scriptContext->GetLibrary()->CreateBoolean(hasValue);
    }

    void JavascriptWeakSet::Add(DynamicObject* key)
    {LOGMEIN("JavascriptWeakSet.cpp] 175\n");
        keySet.Item(key, true);
    }

    bool JavascriptWeakSet::Delete(DynamicObject* key)
    {LOGMEIN("JavascriptWeakSet.cpp] 180\n");
        bool unused = false;
        return keySet.TryGetValueAndRemove(key, &unused);
    }

    bool JavascriptWeakSet::Has(DynamicObject* key)
    {LOGMEIN("JavascriptWeakSet.cpp] 186\n");
        bool unused = false;
        return keySet.TryGetValue(key, &unused);
    }

    BOOL JavascriptWeakSet::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptWeakSet.cpp] 192\n");
        stringBuilder->AppendCppLiteral(_u("WeakSet"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptWeakSet::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptWeakSet.cpp] 199\n");
        this->Map([&](DynamicObject* key)
        {
            extractor->MarkVisitVar(key);
        });
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptWeakSet::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptWeakSet.cpp] 207\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapSetObject;
    }

    void JavascriptWeakSet::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptWeakSet.cpp] 212\n");
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
        {LOGMEIN("JavascriptWeakSet.cpp] 228\n");
            ssi->SetValueArray = nullptr;
            alloc.SlabAbortArraySpace<TTD::TTDVar>(setCountEst + 1);
        }
        else
        {
            alloc.SlabCommitArraySpace<TTD::TTDVar>(ssi->SetSize, setCountEst + 1);
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapSetInfo*, TTD::NSSnapObjects::SnapObjectType::SnapSetObject>(objData, ssi);
    }
#endif
}
