//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptMap::JavascriptMap(DynamicType* type)
        : DynamicObject(type)
    {LOGMEIN("JavascriptMap.cpp] 10\n");
    }

    JavascriptMap* JavascriptMap::New(ScriptContext* scriptContext)
    {LOGMEIN("JavascriptMap.cpp] 14\n");
        JavascriptMap* map = scriptContext->GetLibrary()->CreateMap();
        map->map = RecyclerNew(scriptContext->GetRecycler(), MapDataMap, scriptContext->GetRecycler());

        return map;
    }

    bool JavascriptMap::Is(Var aValue)
    {LOGMEIN("JavascriptMap.cpp] 22\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Map;
    }

    JavascriptMap* JavascriptMap::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptMap'");

        return static_cast<JavascriptMap *>(RecyclableObject::FromVar(aValue));
    }

    JavascriptMap::MapDataList::Iterator JavascriptMap::GetIterator()
    {LOGMEIN("JavascriptMap.cpp] 34\n");
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
        {LOGMEIN("JavascriptMap.cpp] 55\n");
            mapObject = library->CreateMap();
        }
        else
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map"), _u("Map"));
        }
        Assert(mapObject != nullptr);

        Var iterable = (args.Info.Count > 1) ? args[1] : library->GetUndefined();

        RecyclableObject* iter = nullptr;
        RecyclableObject* adder = nullptr;

        if (JavascriptConversion::CheckObjectCoercible(iterable, scriptContext))
        {LOGMEIN("JavascriptMap.cpp] 70\n");
            iter = JavascriptOperators::GetIterator(iterable, scriptContext);
            Var adderVar = JavascriptOperators::GetProperty(mapObject, PropertyIds::set, scriptContext);
            if (!JavascriptConversion::IsCallable(adderVar))
            {LOGMEIN("JavascriptMap.cpp] 74\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
            }
            adder = RecyclableObject::FromVar(adderVar);
        }

        if (mapObject->map != nullptr)
        {LOGMEIN("JavascriptMap.cpp] 81\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_ObjectIsAlreadyInitialized, _u("Map"), _u("Map"));
        }

        mapObject->map = RecyclerNew(scriptContext->GetRecycler(), MapDataMap, scriptContext->GetRecycler());

        if (iter != nullptr)
        {LOGMEIN("JavascriptMap.cpp] 88\n");
            Var undefined = library->GetUndefined();

            JavascriptOperators::DoIteratorStepAndValue(iter, scriptContext, [&](Var nextItem) {
                if (!JavascriptOperators::IsObject(nextItem))
                {LOGMEIN("JavascriptMap.cpp] 93\n");
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                }

                RecyclableObject* obj = RecyclableObject::FromVar(nextItem);

                Var key, value;

                if (!JavascriptOperators::GetItem(obj, 0u, &key, scriptContext))
                {LOGMEIN("JavascriptMap.cpp] 102\n");
                    key = undefined;
                }

                if (!JavascriptOperators::GetItem(obj, 1u, &value, scriptContext))
                {LOGMEIN("JavascriptMap.cpp] 107\n");
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
        {LOGMEIN("JavascriptMap.cpp] 129\n");
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
        {LOGMEIN("JavascriptMap.cpp] 148\n");
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
        {LOGMEIN("JavascriptMap.cpp] 170\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.forEach"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptMap.cpp] 177\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Map.prototype.forEach"));
        }
        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);

        Var thisArg = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        auto iterator = map->GetIterator();

        while (iterator.Next())
        {LOGMEIN("JavascriptMap.cpp] 187\n");
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
        {LOGMEIN("JavascriptMap.cpp] 205\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.get"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        Var value = nullptr;

        if (map->Get(key, &value))
        {LOGMEIN("JavascriptMap.cpp] 215\n");
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
        {LOGMEIN("JavascriptMap.cpp] 230\n");
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
        {LOGMEIN("JavascriptMap.cpp] 251\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.set"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        Var value = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptNumber::Is(key) && JavascriptNumber::IsNegZero(JavascriptNumber::GetValue(key)))
        {LOGMEIN("JavascriptMap.cpp] 261\n");
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
        {LOGMEIN("JavascriptMap.cpp] 279\n");
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
        {LOGMEIN("JavascriptMap.cpp] 298\n");
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
        {LOGMEIN("JavascriptMap.cpp] 315\n");
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
        {LOGMEIN("JavascriptMap.cpp] 332\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Map.prototype.values"), _u("Map"));
        }

        JavascriptMap* map = JavascriptMap::FromVar(args[0]);
        return scriptContext->GetLibrary()->CreateMapIterator(map, JavascriptMapIteratorKind::Value);
    }

    void JavascriptMap::Clear()
    {LOGMEIN("JavascriptMap.cpp] 341\n");
        list.Clear();
        map->Clear();
    }

    bool JavascriptMap::Delete(Var key)
    {LOGMEIN("JavascriptMap.cpp] 347\n");
        if (map->ContainsKey(key))
        {LOGMEIN("JavascriptMap.cpp] 349\n");
            MapDataNode* node = map->Item(key);
            list.Remove(node);
            return map->Remove(key);
        }
        return false;
    }

    bool JavascriptMap::Get(Var key, Var* value)
    {LOGMEIN("JavascriptMap.cpp] 358\n");
        if (map->ContainsKey(key))
        {LOGMEIN("JavascriptMap.cpp] 360\n");
            MapDataNode* node = map->Item(key);
            *value = node->data.Value();
            return true;
        }
        return false;
    }

    bool JavascriptMap::Has(Var key)
    {LOGMEIN("JavascriptMap.cpp] 369\n");
        return map->ContainsKey(key);
    }

    void JavascriptMap::Set(Var key, Var value)
    {LOGMEIN("JavascriptMap.cpp] 374\n");
        if (map->ContainsKey(key))
        {LOGMEIN("JavascriptMap.cpp] 376\n");
            MapDataNode* node = map->Item(key);
            node->data = MapDataKeyValuePair(key, value);
        }
        else
        {
            MapDataKeyValuePair pair(key, value);
            MapDataNode* node = list.Append(pair, GetScriptContext()->GetRecycler());
            map->Add(key, node);
        }
    }

    int JavascriptMap::Size()
    {LOGMEIN("JavascriptMap.cpp] 389\n");
        return map->Count();
    }

    BOOL JavascriptMap::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptMap.cpp] 394\n");
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
    {LOGMEIN("JavascriptMap.cpp] 410\n");
        auto iterator = GetIterator();
        while(iterator.Next())
        {LOGMEIN("JavascriptMap.cpp] 413\n");
            extractor->MarkVisitVar(iterator.Current().Key());
            extractor->MarkVisitVar(iterator.Current().Value());
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptMap::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptMap.cpp] 420\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapMapObject;
    }

    void JavascriptMap::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptMap.cpp] 425\n");
        TTD::NSSnapObjects::SnapMapInfo* smi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapMapInfo>();
        smi->MapSize = 0;

        if(this->Size() == 0)
        {LOGMEIN("JavascriptMap.cpp] 430\n");
            smi->MapKeyValueArray = nullptr;
        }
        else
        {
            smi->MapKeyValueArray = alloc.SlabAllocateArray<TTD::TTDVar>(this->Size() * 2);

            auto iter = this->GetIterator();
            while(iter.Next())
            {LOGMEIN("JavascriptMap.cpp] 439\n");
                smi->MapKeyValueArray[smi->MapSize] = iter.Current().Key();
                smi->MapKeyValueArray[smi->MapSize + 1] = iter.Current().Value();
                smi->MapSize += 2;
            }
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapMapInfo*, TTD::NSSnapObjects::SnapObjectType::SnapMapObject>(objData, smi);
    }

    JavascriptMap* JavascriptMap::CreateForSnapshotRestore(ScriptContext* ctx)
    {LOGMEIN("JavascriptMap.cpp] 450\n");
        JavascriptMap* res = ctx->GetLibrary()->CreateMap();
        res->map = RecyclerNew(ctx->GetRecycler(), MapDataMap, ctx->GetRecycler());

        return res;
    }
#endif
}
