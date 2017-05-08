//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptMap::JavascriptMap(DynamicType* type)
        : DynamicObject(type)
    {TRACE_IT(60094);
    }

    JavascriptMap* JavascriptMap::New(ScriptContext* scriptContext)
    {TRACE_IT(60095);
        JavascriptMap* map = scriptContext->GetLibrary()->CreateMap();
        map->map = RecyclerNew(scriptContext->GetRecycler(), MapDataMap, scriptContext->GetRecycler());

        return map;
    }

    bool JavascriptMap::Is(Var aValue)
    {TRACE_IT(60096);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Map;
    }

    JavascriptMap* JavascriptMap::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptMap'");

        return static_cast<JavascriptMap *>(RecyclableObject::FromVar(aValue));
    }

    JavascriptMap::MapDataList::Iterator JavascriptMap::GetIterator()
    {TRACE_IT(60097);
        return list.GetIterator();
    }

    Var JavascriptMap::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Map"));

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);
        CHAKRATEL_LANGSTATS_INC_DATACOUNT(ES6_Map);

        JavascriptMap* mapObject = nullptr;

        if (callInfo.Flags & CallFlags_New)
        {TRACE_IT(60098);
            mapObject = library->CreateMap();
        }
        else
        {TRACE_IT(60099);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map"), _u("Map"));
        }
        Assert(mapObject != nullptr);

        Var iterable = (args.Info.Count > 1) ? args[1] : library->GetUndefined();

        RecyclableObject* iter = nullptr;
        RecyclableObject* adder = nullptr;

        if (JavascriptConversion::CheckObjectCoercible(iterable, scriptContext))
        {TRACE_IT(60100);
            iter = JavascriptOperators::GetIterator(iterable, scriptContext);
            Var adderVar = JavascriptOperators::GetProperty(mapObject, PropertyIds::set, scriptContext);
            if (!JavascriptConversion::IsCallable(adderVar))
            {TRACE_IT(60101);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
            }
            adder = RecyclableObject::FromVar(adderVar);
        }

        if (mapObject->map != nullptr)
        {TRACE_IT(60102);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_ObjectIsAlreadyInitialized, _u("Map"), _u("Map"));
        }

        mapObject->map = RecyclerNew(scriptContext->GetRecycler(), MapDataMap, scriptContext->GetRecycler());

        if (iter != nullptr)
        {TRACE_IT(60103);
            Var undefined = library->GetUndefined();

            JavascriptOperators::DoIteratorStepAndValue(iter, scriptContext, [&](Var nextItem) {
                if (!JavascriptOperators::IsObject(nextItem))
                {TRACE_IT(60104);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                }

                RecyclableObject* obj = RecyclableObject::FromVar(nextItem);

                Var key, value;

                if (!JavascriptOperators::GetItem(obj, 0u, &key, scriptContext))
                {TRACE_IT(60105);
                    key = undefined;
                }

                if (!JavascriptOperators::GetItem(obj, 1u, &value, scriptContext))
                {TRACE_IT(60106);
                    value = undefined;
                }

                // CONSIDER: if adder is the default built-in, fast path it and skip the JS call?
                CALL_FUNCTION(adder, CallInfo(CallFlags_Value, 3), mapObject, key, value);
            });
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), mapObject, nullptr, scriptContext) :
            mapObject;
    }

    Var JavascriptMap::EntryClear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60107);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.clear"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        map->Clear();

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptMap::EntryDelete(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60108);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.delete"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        bool didDelete = map->Delete(key);

        return scriptContext->GetLibrary()->CreateBoolean(didDelete);
    }

    Var JavascriptMap::EntryForEach(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Map.prototype.forEach"));

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60109);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.forEach"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(60110);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Map.prototype.forEach"));
        }
        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);

        Var thisArg = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        auto iterator = map->GetIterator();

        while (iterator.Next())
        {TRACE_IT(60111);
            Var key = iterator.Current().Key();
            Var value = iterator.Current().Value();

            CALL_FUNCTION(callBackFn, CallInfo(CallFlags_Value, 4), thisArg, value, key, map);
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptMap::EntryGet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60112);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.get"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        Var value = nullptr;

        if (map->Get(key, &value))
        {TRACE_IT(60113);
            return value;
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptMap::EntryHas(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60114);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.has"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        bool hasValue = map->Has(key);

        return scriptContext->GetLibrary()->CreateBoolean(hasValue);
    }

    Var JavascriptMap::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60115);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.set"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        Var value = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptNumber::Is(key) && JavascriptNumber::IsNegZero(JavascriptNumber::GetValue(key)))
        {TRACE_IT(60116);
            // Normalize -0 to +0
            key = JavascriptNumber::New(0.0, scriptContext);
        }

        map->Set(key, value);

        return map;
    }

    Var JavascriptMap::EntrySizeGetter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60117);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.size"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        int size = map->Size();

        return JavascriptNumber::ToVar(size, scriptContext);
    }

    Var JavascriptMap::EntryEntries(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60118);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.entries"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        return scriptContext->GetLibrary()->CreateMapIterator(map, JavascriptMapIteratorKind::KeyAndValue);
    }

    Var JavascriptMap::EntryKeys(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60119);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.keys"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        return scriptContext->GetLibrary()->CreateMapIterator(map, JavascriptMapIteratorKind::Key);
    }

    Var JavascriptMap::EntryValues(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptMap::Is(args[0]))
        {TRACE_IT(60120);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.values"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);
        return scriptContext->GetLibrary()->CreateMapIterator(map, JavascriptMapIteratorKind::Value);
    }

    void JavascriptMap::Clear()
    {TRACE_IT(60121);
        list.Clear();
        map->Clear();
    }

    bool JavascriptMap::Delete(Var key)
    {TRACE_IT(60122);
        if (map->ContainsKey(key))
        {TRACE_IT(60123);
            MapDataNode* node = map->Item(key);
            list.Remove(node);
            return map->Remove(key);
        }
        return false;
    }

    bool JavascriptMap::Get(Var key, Var* value)
    {TRACE_IT(60124);
        if (map->ContainsKey(key))
        {TRACE_IT(60125);
            MapDataNode* node = map->Item(key);
            *value = node->data.Value();
            return true;
        }
        return false;
    }

    bool JavascriptMap::Has(Var key)
    {TRACE_IT(60126);
        return map->ContainsKey(key);
    }

    void JavascriptMap::Set(Var key, Var value)
    {TRACE_IT(60127);
        if (map->ContainsKey(key))
        {TRACE_IT(60128);
            MapDataNode* node = map->Item(key);
            node->data = MapDataKeyValuePair(key, value);
        }
        else
        {TRACE_IT(60129);
            MapDataKeyValuePair pair(key, value);
            MapDataNode* node = list.Append(pair, GetScriptContext()->GetRecycler());
            map->Add(key, node);
        }
    }

    int JavascriptMap::Size()
    {TRACE_IT(60130);
        return map->Count();
    }

    BOOL JavascriptMap::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(60131);
        stringBuilder->AppendCppLiteral(_u("Map"));
        return TRUE;
    }

    Var JavascriptMap::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

#if ENABLE_TTD
    void JavascriptMap::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(60132);
        auto iterator = GetIterator();
        while(iterator.Next())
        {TRACE_IT(60133);
            extractor->MarkVisitVar(iterator.Current().Key());
            extractor->MarkVisitVar(iterator.Current().Value());
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptMap::GetSnapTag_TTD() const
    {TRACE_IT(60134);
        return TTD::NSSnapObjects::SnapObjectType::SnapMapObject;
    }

    void JavascriptMap::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(60135);
        TTD::NSSnapObjects::SnapMapInfo* smi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapMapInfo>();
        smi->MapSize = 0;

        if(this->Size() == 0)
        {TRACE_IT(60136);
            smi->MapKeyValueArray = nullptr;
        }
        else
        {TRACE_IT(60137);
            smi->MapKeyValueArray = alloc.SlabAllocateArray<TTD::TTDVar>(this->Size() * 2);

            auto iter = this->GetIterator();
            while(iter.Next())
            {TRACE_IT(60138);
                smi->MapKeyValueArray[smi->MapSize] = iter.Current().Key();
                smi->MapKeyValueArray[smi->MapSize + 1] = iter.Current().Value();
                smi->MapSize += 2;
            }
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapMapInfo*, TTD::NSSnapObjects::SnapObjectType::SnapMapObject>(objData, smi);
    }

    JavascriptMap* JavascriptMap::CreateForSnapshotRestore(ScriptContext* ctx)
    {TRACE_IT(60139);
        JavascriptMap* res = ctx->GetLibrary()->CreateMap();
        res->map = RecyclerNew(ctx->GetRecycler(), MapDataMap, ctx->GetRecycler());

        return res;
    }
#endif
}
