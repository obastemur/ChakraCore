//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptWeakMap::JavascriptWeakMap(DynamicType* type)
        : DynamicObject(type),
        keySet(type->GetScriptContext()->GetRecycler())
    {TRACE_IT(62237);
    }

    bool JavascriptWeakMap::Is(Var aValue)
    {TRACE_IT(62238);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_WeakMap;
    }

    JavascriptWeakMap* JavascriptWeakMap::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptWeakMap'");

        return static_cast<JavascriptWeakMap *>(RecyclableObject::FromVar(aValue));
    }

    JavascriptWeakMap::WeakMapKeyMap* JavascriptWeakMap::GetWeakMapKeyMapFromKey(DynamicObject* key) const
    {TRACE_IT(62239);
        Var weakMapKeyData = nullptr;
        if (!key->GetInternalProperty(key, InternalPropertyIds::WeakMapKeyMap, &weakMapKeyData, nullptr, key->GetScriptContext()))
        {TRACE_IT(62240);
            return nullptr;
        }

        if (key->GetScriptContext()->GetLibrary()->GetUndefined() == weakMapKeyData)
        {TRACE_IT(62241);
            // Assert to find out where this can happen.
            Assert(false);
            return nullptr;
        }

        return static_cast<WeakMapKeyMap*>(weakMapKeyData);
    }

    JavascriptWeakMap::WeakMapKeyMap* JavascriptWeakMap::AddWeakMapKeyMapToKey(DynamicObject* key)
    {TRACE_IT(62242);
        // The internal property may exist on an object that has had DynamicObject::ResetObject called on itself.
        // In that case the value stored in the property slot should be null.
        DebugOnly(Var unused = nullptr);
        Assert(!key->GetInternalProperty(key, InternalPropertyIds::WeakMapKeyMap, &unused, nullptr, key->GetScriptContext()) || unused == nullptr);

        WeakMapKeyMap* weakMapKeyData = RecyclerNew(GetScriptContext()->GetRecycler(), WeakMapKeyMap, GetScriptContext()->GetRecycler());
        BOOL success = key->SetInternalProperty(InternalPropertyIds::WeakMapKeyMap, weakMapKeyData, PropertyOperation_Force, nullptr);
        Assert(success);

        return weakMapKeyData;
    }

    bool JavascriptWeakMap::KeyMapGet(WeakMapKeyMap* map, Var* value) const
    {TRACE_IT(62243);
        if (map->ContainsKey(GetWeakMapId()))
        {TRACE_IT(62244);
            *value = map->Item(GetWeakMapId());
            return true;
        }

        return false;
    }

    Var JavascriptWeakMap::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);
        CHAKRATEL_LANGSTATS_INC_DATACOUNT(ES6_WeakMap);

        JavascriptWeakMap* weakMapObject = nullptr;

        if (callInfo.Flags & CallFlags_New)
        {TRACE_IT(62245);
            weakMapObject = library->CreateWeakMap();
        }
        else
        {TRACE_IT(62246);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap"), _u("WeakMap"));
        }
        Assert(weakMapObject != nullptr);

        Var iterable = (args.Info.Count > 1) ? args[1] : library->GetUndefined();

        RecyclableObject* iter = nullptr;
        RecyclableObject* adder = nullptr;

        if (JavascriptConversion::CheckObjectCoercible(iterable, scriptContext))
        {TRACE_IT(62247);
            iter = JavascriptOperators::GetIterator(iterable, scriptContext);
            Var adderVar = JavascriptOperators::GetProperty(weakMapObject, PropertyIds::set, scriptContext);
            if (!JavascriptConversion::IsCallable(adderVar))
            {TRACE_IT(62248);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
            }
            adder = RecyclableObject::FromVar(adderVar);
        }

        if (iter != nullptr)
        {TRACE_IT(62249);
            Var undefined = library->GetUndefined();

            JavascriptOperators::DoIteratorStepAndValue(iter, scriptContext, [&](Var nextItem) {
                if (!JavascriptOperators::IsObject(nextItem))
                {TRACE_IT(62250);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                }

                RecyclableObject* obj = RecyclableObject::FromVar(nextItem);

                Var key, value;

                if (!JavascriptOperators::GetItem(obj, 0u, &key, scriptContext))
                {TRACE_IT(62251);
                    key = undefined;
                }

                if (!JavascriptOperators::GetItem(obj, 1u, &value, scriptContext))
                {TRACE_IT(62252);
                    value = undefined;
                }

                CALL_FUNCTION(adder, CallInfo(CallFlags_Value, 3), weakMapObject, key, value);
            });
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), weakMapObject, nullptr, scriptContext) :
            weakMapObject;
    }

    Var JavascriptWeakMap::EntryDelete(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptWeakMap::Is(args[0]))
        {TRACE_IT(62253);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap.prototype.delete"), _u("WeakMap"));
        }

        JavascriptWeakMap* weakMap = JavascriptWeakMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        bool didDelete = false;

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {TRACE_IT(62254);
            DynamicObject* keyObj = DynamicObject::FromVar(key);

            didDelete = weakMap->Delete(keyObj);
        }

        return scriptContext->GetLibrary()->CreateBoolean(didDelete);
    }

    Var JavascriptWeakMap::EntryGet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptWeakMap::Is(args[0]))
        {TRACE_IT(62255);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap.prototype.get"), _u("WeakMap"));
        }

        JavascriptWeakMap* weakMap = JavascriptWeakMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {TRACE_IT(62256);
            DynamicObject* keyObj = DynamicObject::FromVar(key);
            Var value = nullptr;

            if (weakMap->Get(keyObj, &value))
            {TRACE_IT(62257);
                return value;
            }
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptWeakMap::EntryHas(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptWeakMap::Is(args[0]))
        {TRACE_IT(62258);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap.prototype.has"), _u("WeakMap"));
        }

        JavascriptWeakMap* weakMap = JavascriptWeakMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        bool hasValue = false;

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {TRACE_IT(62259);
            DynamicObject* keyObj = DynamicObject::FromVar(key);

            hasValue = weakMap->Has(keyObj);
        }

        return scriptContext->GetLibrary()->CreateBoolean(hasValue);
    }

    Var JavascriptWeakMap::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptWeakMap::Is(args[0]))
        {TRACE_IT(62260);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap.prototype.set"), _u("WeakMap"));
        }

        JavascriptWeakMap* weakMap = JavascriptWeakMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        Var value = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        if (!JavascriptOperators::IsObject(key) || JavascriptOperators::GetTypeId(key) == TypeIds_HostDispatch)
        {TRACE_IT(62261);
            // HostDispatch can not expand so can't have internal property added to it.
            // TODO: Support HostDispatch as WeakMap key
            JavascriptError::ThrowTypeError(scriptContext, JSERR_WeakMapSetKeyNotAnObject, _u("WeakMap.prototype.set"));
        }

        DynamicObject* keyObj = DynamicObject::FromVar(key);

#if ENABLE_TTD
        //
        //TODO: This makes the map decidedly less weak -- forces it to only release when we clean the tracking set but determinizes the behavior nicely
        //      We want to improve this.
        //
        if(scriptContext->IsTTDRecordOrReplayModeEnabled())
        {TRACE_IT(62262);
            scriptContext->TTDContextInfo->TTDWeakReferencePinSet->Add(keyObj);
        }
#endif

        weakMap->Set(keyObj, value);

        return weakMap;
    }

    void JavascriptWeakMap::Clear()
    {TRACE_IT(62263);
        keySet.Map([&](DynamicObject* key, bool value, const RecyclerWeakReference<DynamicObject>* weakRef) {
            WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

            // It may be the case that a CEO has been reset and the keyMap is now null.
            // Just ignore it in this case, the keyMap has already been collected.
            if (keyMap != nullptr)
            {TRACE_IT(62264);
                // It may also be the case that a CEO has been reset and then added to a separate WeakMap,
                // creating a new WeakMapKeyMap on the CEO.  In this case GetWeakMapId() may not be in the
                // keyMap, so don't assert successful removal here.
                keyMap->Remove(GetWeakMapId());
            }
        });
        keySet.Clear();
    }

    bool JavascriptWeakMap::Delete(DynamicObject* key)
    {TRACE_IT(62265);
        WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

        if (keyMap != nullptr)
        {TRACE_IT(62266);
            bool unused = false;
            bool inSet = keySet.TryGetValueAndRemove(key, &unused);
            bool inData = keyMap->Remove(GetWeakMapId());
            Assert(inSet == inData);

            return inData;
        }

        return false;
    }

    bool JavascriptWeakMap::Get(DynamicObject* key, Var* value) const
    {TRACE_IT(62267);
        WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

        if (keyMap != nullptr)
        {TRACE_IT(62268);
            return KeyMapGet(keyMap, value);
        }

        return false;
    }

    bool JavascriptWeakMap::Has(DynamicObject* key) const
    {TRACE_IT(62269);
        WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

        if (keyMap != nullptr)
        {TRACE_IT(62270);
            return keyMap->ContainsKey(GetWeakMapId());
        }

        return false;
    }

    void JavascriptWeakMap::Set(DynamicObject* key, Var value)
    {TRACE_IT(62271);
        WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

        if (keyMap == nullptr)
        {TRACE_IT(62272);
            keyMap = AddWeakMapKeyMapToKey(key);
        }

        keyMap->Item(GetWeakMapId(), value);
        keySet.Item(key, true);
    }

    BOOL JavascriptWeakMap::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(62273);
        stringBuilder->AppendCppLiteral(_u("WeakMap"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptWeakMap::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(62274);
        this->Map([&](DynamicObject* key, Js::Var value)
        {
            extractor->MarkVisitVar(key);
            extractor->MarkVisitVar(value);
        });
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptWeakMap::GetSnapTag_TTD() const
    {TRACE_IT(62275);
        return TTD::NSSnapObjects::SnapObjectType::SnapMapObject;
    }

    void JavascriptWeakMap::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(62276);
        TTD::NSSnapObjects::SnapMapInfo* smi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapMapInfo>();
        uint32 mapCountEst = this->Size() * 2;

        smi->MapSize = 0;
        smi->MapKeyValueArray = alloc.SlabReserveArraySpace<TTD::TTDVar>(mapCountEst + 1); //always reserve at least 1 element

        this->Map([&](DynamicObject* key, Js::Var value)
        {
            AssertMsg(smi->MapSize + 1 < mapCountEst, "We are writting junk");

            smi->MapKeyValueArray[smi->MapSize] = key;
            smi->MapKeyValueArray[smi->MapSize + 1] = value;
            smi->MapSize += 2;
        });

        if(smi->MapSize == 0)
        {TRACE_IT(62277);
            smi->MapKeyValueArray = nullptr;
            alloc.SlabAbortArraySpace<TTD::TTDVar>(mapCountEst + 1);
        }
        else
        {TRACE_IT(62278);
            alloc.SlabCommitArraySpace<TTD::TTDVar>(smi->MapSize, mapCountEst + 1);
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapMapInfo*, TTD::NSSnapObjects::SnapObjectType::SnapMapObject>(objData, smi);
    }
#endif
}
