//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSet::JavascriptSet(DynamicType* type)
        : DynamicObject(type)
    {LOGMEIN("JavascriptSet.cpp] 10\n");
    }

    JavascriptSet* JavascriptSet::New(ScriptContext* scriptContext)
    {LOGMEIN("JavascriptSet.cpp] 14\n");
        JavascriptSet* set = scriptContext->GetLibrary()->CreateSet();
        set->set = RecyclerNew(scriptContext->GetRecycler(), SetDataSet, scriptContext->GetRecycler());

        return set;
    }

    bool JavascriptSet::Is(Var aValue)
    {LOGMEIN("JavascriptSet.cpp] 22\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Set;
    }

    JavascriptSet* JavascriptSet::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSet'");

        return static_cast<JavascriptSet *>(RecyclableObject::FromVar(aValue));
    }

    JavascriptSet::SetDataList::Iterator JavascriptSet::GetIterator()
    {LOGMEIN("JavascriptSet.cpp] 34\n");
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
        {LOGMEIN("JavascriptSet.cpp] 55\n");
            setObject = library->CreateSet();
        }
        else
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set"), _u("Set"));
        }
        Assert(setObject != nullptr);

        Var iterable = (args.Info.Count > 1) ? args[1] : library->GetUndefined();

        RecyclableObject* iter = nullptr;
        RecyclableObject* adder = nullptr;

        if (JavascriptConversion::CheckObjectCoercible(iterable, scriptContext))
        {LOGMEIN("JavascriptSet.cpp] 70\n");
            iter = JavascriptOperators::GetIterator(iterable, scriptContext);
            Var adderVar = JavascriptOperators::GetProperty(setObject, PropertyIds::add, scriptContext);
            if (!JavascriptConversion::IsCallable(adderVar))
            {LOGMEIN("JavascriptSet.cpp] 74\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
            }
            adder = RecyclableObject::FromVar(adderVar);
        }

        if (setObject->set != nullptr)
        {LOGMEIN("JavascriptSet.cpp] 81\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_ObjectIsAlreadyInitialized, _u("Set"), _u("Set"));
        }


        setObject->set = RecyclerNew(scriptContext->GetRecycler(), SetDataSet, scriptContext->GetRecycler());

        if (iter != nullptr)
        {LOGMEIN("JavascriptSet.cpp] 89\n");
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
        {LOGMEIN("JavascriptSet.cpp] 108\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.add"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        Var value = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptNumber::Is(value) && JavascriptNumber::IsNegZero(JavascriptNumber::GetValue(value)))
        {LOGMEIN("JavascriptSet.cpp] 117\n");
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
        {LOGMEIN("JavascriptSet.cpp] 135\n");
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
        {LOGMEIN("JavascriptSet.cpp] 154\n");
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
        {LOGMEIN("JavascriptSet.cpp] 176\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Set.prototype.forEach"), _u("Set"));
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);


        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptSet.cpp] 184\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Set.prototype.forEach"));
        }
        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);

        Var thisArg = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        auto iterator = set->GetIterator();

        while (iterator.Next())
        {LOGMEIN("JavascriptSet.cpp] 194\n");
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
        {LOGMEIN("JavascriptSet.cpp] 211\n");
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
        {LOGMEIN("JavascriptSet.cpp] 233\n");
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
        {LOGMEIN("JavascriptSet.cpp] 252\n");
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
        {LOGMEIN("JavascriptSet.cpp] 269\n");
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
    {LOGMEIN("JavascriptSet.cpp] 288\n");
        if (!set->ContainsKey(value))
        {LOGMEIN("JavascriptSet.cpp] 290\n");
            SetDataNode* node = list.Append(value, GetScriptContext()->GetRecycler());
            set->Add(value, node);
        }
    }

    void JavascriptSet::Clear()
    {LOGMEIN("JavascriptSet.cpp] 297\n");
        // TODO: (Consider) Should we clear the set here and leave it as large as it has grown, or
        // toss it away and create a new empty set, letting it grow as needed?
        list.Clear();
        set->Clear();
    }

    bool JavascriptSet::Delete(Var value)
    {LOGMEIN("JavascriptSet.cpp] 305\n");
        if (set->ContainsKey(value))
        {LOGMEIN("JavascriptSet.cpp] 307\n");
            SetDataNode* node = set->Item(value);
            list.Remove(node);
            return set->Remove(value);
        }
        return false;
    }

    bool JavascriptSet::Has(Var value)
    {LOGMEIN("JavascriptSet.cpp] 316\n");
        return set->ContainsKey(value);
    }

    int JavascriptSet::Size()
    {LOGMEIN("JavascriptSet.cpp] 321\n");
        return set->Count();
    }

    BOOL JavascriptSet::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSet.cpp] 326\n");
        stringBuilder->AppendCppLiteral(_u("Set"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptSet::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptSet.cpp] 333\n");
        auto iterator = this->GetIterator();
        while(iterator.Next())
        {LOGMEIN("JavascriptSet.cpp] 336\n");
            extractor->MarkVisitVar(iterator.Current());
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptSet::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptSet.cpp] 342\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapSetObject;
    }

    void JavascriptSet::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptSet.cpp] 347\n");
        TTD::NSSnapObjects::SnapSetInfo* ssi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapSetInfo>();
        ssi->SetSize = 0;

        if(this->Size() == 0)
        {LOGMEIN("JavascriptSet.cpp] 352\n");
            ssi->SetValueArray = nullptr;
        }
        else
        {
            ssi->SetValueArray = alloc.SlabAllocateArray<TTD::TTDVar>(this->Size());

            auto iter = this->GetIterator();
            while(iter.Next())
            {LOGMEIN("JavascriptSet.cpp] 361\n");
                ssi->SetValueArray[ssi->SetSize] = iter.Current();
                ssi->SetSize++;
            }
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapSetInfo*, TTD::NSSnapObjects::SnapObjectType::SnapSetObject>(objData, ssi);
    }

    JavascriptSet* JavascriptSet::CreateForSnapshotRestore(ScriptContext* ctx)
    {LOGMEIN("JavascriptSet.cpp] 371\n");
        JavascriptSet* res = ctx->GetLibrary()->CreateSet();
        res->set = RecyclerNew(ctx->GetRecycler(), SetDataSet, ctx->GetRecycler());

        return res;
    }
#endif
}
