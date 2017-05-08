//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSet::JavascriptSet(DynamicType* type)
        : DynamicObject(type)
    {TRACE_IT(61356);
    }

    JavascriptSet* JavascriptSet::New(ScriptContext* scriptContext)
    {TRACE_IT(61357);
        JavascriptSet* set = scriptContext->GetLibrary()->CreateSet();
        set->set = RecyclerNew(scriptContext->GetRecycler(), SetDataSet, scriptContext->GetRecycler());

        return set;
    }

    bool JavascriptSet::Is(Var aValue)
    {TRACE_IT(61358);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Set;
    }

    JavascriptSet* JavascriptSet::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSet'");

        return static_cast<JavascriptSet *>(RecyclableObject::FromVar(aValue));
    }

    JavascriptSet::SetDataList::Iterator JavascriptSet::GetIterator()
    {TRACE_IT(61359);
        return list.GetIterator();
    }

    Var JavascriptSet::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Set"));

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);
        CHAKRATEL_LANGSTATS_INC_DATACOUNT(ES6_Set);

        JavascriptSet* setObject = nullptr;

        if (callInfo.Flags & CallFlags_New)
        {TRACE_IT(61360);
            setObject = library->CreateSet();
        }
        else
        {TRACE_IT(61361);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set"), _u("Set"));
        }
        Assert(setObject != nullptr);

        Var iterable = (args.Info.Count > 1) ? args[1] : library->GetUndefined();

        RecyclableObject* iter = nullptr;
        RecyclableObject* adder = nullptr;

        if (JavascriptConversion::CheckObjectCoercible(iterable, scriptContext))
        {TRACE_IT(61362);
            iter = JavascriptOperators::GetIterator(iterable, scriptContext);
            Var adderVar = JavascriptOperators::GetProperty(setObject, PropertyIds::add, scriptContext);
            if (!JavascriptConversion::IsCallable(adderVar))
            {TRACE_IT(61363);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
            }
            adder = RecyclableObject::FromVar(adderVar);
        }

        if (setObject->set != nullptr)
        {TRACE_IT(61364);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_ObjectIsAlreadyInitialized, _u("Set"), _u("Set"));
        }


        setObject->set = RecyclerNew(scriptContext->GetRecycler(), SetDataSet, scriptContext->GetRecycler());

        if (iter != nullptr)
        {TRACE_IT(61365);
            JavascriptOperators::DoIteratorStepAndValue(iter, scriptContext, [&](Var nextItem) {
                CALL_FUNCTION(adder, CallInfo(CallFlags_Value, 2), setObject, nextItem);
            });
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), setObject, nullptr, scriptContext) :
            setObject;
    }

    Var JavascriptSet::EntryAdd(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {TRACE_IT(61366);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.add"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        Var value = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptNumber::Is(value) && JavascriptNumber::IsNegZero(JavascriptNumber::GetValue(value)))
        {TRACE_IT(61367);
            // Normalize -0 to +0
            value = JavascriptNumber::New(0.0, scriptContext);
        }

        set->Add(value);

        return set;
    }

    Var JavascriptSet::EntryClear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {TRACE_IT(61368);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.clear"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        set->Clear();

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptSet::EntryDelete(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {TRACE_IT(61369);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.delete"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        Var value = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        bool didDelete = set->Delete(value);

        return scriptContext->GetLibrary()->CreateBoolean(didDelete);
    }

    Var JavascriptSet::EntryForEach(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Set.prototype.forEach"));

        if (!JavascriptSet::Is(args[0]))
        {TRACE_IT(61370);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.forEach"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);


        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(61371);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Set.prototype.forEach"));
        }
        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);

        Var thisArg = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        auto iterator = set->GetIterator();

        while (iterator.Next())
        {TRACE_IT(61372);
            Var value = iterator.Current();

            CALL_FUNCTION(callBackFn, CallInfo(CallFlags_Value, 4), thisArg, value, value, args[0]);
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptSet::EntryHas(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {TRACE_IT(61373);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.has"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);


        Var value = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        bool hasValue = set->Has(value);

        return scriptContext->GetLibrary()->CreateBoolean(hasValue);
    }

    Var JavascriptSet::EntrySizeGetter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {TRACE_IT(61374);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.size"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        int size = set->Size();

        return JavascriptNumber::ToVar(size, scriptContext);
    }

    Var JavascriptSet::EntryEntries(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {TRACE_IT(61375);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.entries"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        return scriptContext->GetLibrary()->CreateSetIterator(set, JavascriptSetIteratorKind::KeyAndValue);
    }

    Var JavascriptSet::EntryValues(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {TRACE_IT(61376);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.values"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        return scriptContext->GetLibrary()->CreateSetIterator(set, JavascriptSetIteratorKind::Value);
    }

    Var JavascriptSet::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

    void JavascriptSet::Add(Var value)
    {TRACE_IT(61377);
        if (!set->ContainsKey(value))
        {TRACE_IT(61378);
            SetDataNode* node = list.Append(value, GetScriptContext()->GetRecycler());
            set->Add(value, node);
        }
    }

    void JavascriptSet::Clear()
    {TRACE_IT(61379);
        // TODO: (Consider) Should we clear the set here and leave it as large as it has grown, or
        // toss it away and create a new empty set, letting it grow as needed?
        list.Clear();
        set->Clear();
    }

    bool JavascriptSet::Delete(Var value)
    {TRACE_IT(61380);
        if (set->ContainsKey(value))
        {TRACE_IT(61381);
            SetDataNode* node = set->Item(value);
            list.Remove(node);
            return set->Remove(value);
        }
        return false;
    }

    bool JavascriptSet::Has(Var value)
    {TRACE_IT(61382);
        return set->ContainsKey(value);
    }

    int JavascriptSet::Size()
    {TRACE_IT(61383);
        return set->Count();
    }

    BOOL JavascriptSet::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(61384);
        stringBuilder->AppendCppLiteral(_u("Set"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptSet::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(61385);
        auto iterator = this->GetIterator();
        while(iterator.Next())
        {TRACE_IT(61386);
            extractor->MarkVisitVar(iterator.Current());
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptSet::GetSnapTag_TTD() const
    {TRACE_IT(61387);
        return TTD::NSSnapObjects::SnapObjectType::SnapSetObject;
    }

    void JavascriptSet::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(61388);
        TTD::NSSnapObjects::SnapSetInfo* ssi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapSetInfo>();
        ssi->SetSize = 0;

        if(this->Size() == 0)
        {TRACE_IT(61389);
            ssi->SetValueArray = nullptr;
        }
        else
        {TRACE_IT(61390);
            ssi->SetValueArray = alloc.SlabAllocateArray<TTD::TTDVar>(this->Size());

            auto iter = this->GetIterator();
            while(iter.Next())
            {TRACE_IT(61391);
                ssi->SetValueArray[ssi->SetSize] = iter.Current();
                ssi->SetSize++;
            }
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapSetInfo*, TTD::NSSnapObjects::SnapObjectType::SnapSetObject>(objData, ssi);
    }

    JavascriptSet* JavascriptSet::CreateForSnapshotRestore(ScriptContext* ctx)
    {TRACE_IT(61392);
        JavascriptSet* res = ctx->GetLibrary()->CreateSet();
        res->set = RecyclerNew(ctx->GetRecycler(), SetDataSet, ctx->GetRecycler());

        return res;
    }
#endif
}
