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
    {LOGMEIN("JavascriptWeakMap.cpp] 11\n");
    }

    bool JavascriptWeakMap::Is(Var aValue)
    {LOGMEIN("JavascriptWeakMap.cpp] 15\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_WeakMap;
    }

    JavascriptWeakMap* JavascriptWeakMap::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptWeakMap'");

        return static_cast<JavascriptWeakMap *>(RecyclableObject::FromVar(aValue));
    }

    JavascriptWeakMap::WeakMapKeyMap* JavascriptWeakMap::GetWeakMapKeyMapFromKey(DynamicObject* key) const
    {LOGMEIN("JavascriptWeakMap.cpp] 27\n");
        Var weakMapKeyData = nullptr;
        if (!key->GetInternalProperty(key, InternalPropertyIds::WeakMapKeyMap, &weakMapKeyData, nullptr, key->GetScriptContext()))
        {LOGMEIN("JavascriptWeakMap.cpp] 30\n");
            return nullptr;
        }

        if (key->GetScriptContext()->GetLibrary()->GetUndefined() == weakMapKeyData)
        {LOGMEIN("JavascriptWeakMap.cpp] 35\n");
            // Assert to find out where this can happen.
            Assert(false);
            return nullptr;
        }

        return static_cast<WeakMapKeyMap*>(weakMapKeyData);
    }

    JavascriptWeakMap::WeakMapKeyMap* JavascriptWeakMap::AddWeakMapKeyMapToKey(DynamicObject* key)
    {LOGMEIN("JavascriptWeakMap.cpp] 45\n");
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
    {LOGMEIN("JavascriptWeakMap.cpp] 59\n");
        if (map->ContainsKey(GetWeakMapId()))
        {LOGMEIN("JavascriptWeakMap.cpp] 61\n");
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
        {LOGMEIN("JavascriptWeakMap.cpp] 85\n");
            weakMapObject = library->CreateWeakMap();
        }
        else
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap"), _u("WeakMap"));
        }
        Assert(weakMapObject != nullptr);

        Var iterable = (args.Info.Count > 1) ? args[1] : library->GetUndefined();

        RecyclableObject* iter = nullptr;
        RecyclableObject* adder = nullptr;

        if (JavascriptConversion::CheckObjectCoercible(iterable, scriptContext))
        {LOGMEIN("JavascriptWeakMap.cpp] 100\n");
            iter = JavascriptOperators::GetIterator(iterable, scriptContext);
            Var adderVar = JavascriptOperators::GetProperty(weakMapObject, PropertyIds::set, scriptContext);
            if (!JavascriptConversion::IsCallable(adderVar))
            {LOGMEIN("JavascriptWeakMap.cpp] 104\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
            }
            adder = RecyclableObject::FromVar(adderVar);
        }

        if (iter != nullptr)
        {LOGMEIN("JavascriptWeakMap.cpp] 111\n");
            Var undefined = library->GetUndefined();

            JavascriptOperators::DoIteratorStepAndValue(iter, scriptContext, [&](Var nextItem) {
                if (!JavascriptOperators::IsObject(nextItem))
                {LOGMEIN("JavascriptWeakMap.cpp] 116\n");
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                }

                RecyclableObject* obj = RecyclableObject::FromVar(nextItem);

                Var key, value;

                if (!JavascriptOperators::GetItem(obj, 0u, &key, scriptContext))
                {LOGMEIN("JavascriptWeakMap.cpp] 125\n");
                    key = undefined;
                }

                if (!JavascriptOperators::GetItem(obj, 1u, &value, scriptContext))
                {LOGMEIN("JavascriptWeakMap.cpp] 130\n");
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
        {LOGMEIN("JavascriptWeakMap.cpp] 151\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap.prototype.delete"), _u("WeakMap"));
        }

        JavascriptWeakMap* weakMap = JavascriptWeakMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        bool didDelete = false;

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {LOGMEIN("JavascriptWeakMap.cpp] 161\n");
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
        {LOGMEIN("JavascriptWeakMap.cpp] 178\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap.prototype.get"), _u("WeakMap"));
        }

        JavascriptWeakMap* weakMap = JavascriptWeakMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {LOGMEIN("JavascriptWeakMap.cpp] 187\n");
            DynamicObject* keyObj = DynamicObject::FromVar(key);
            Var value = nullptr;

            if (weakMap->Get(keyObj, &value))
            {LOGMEIN("JavascriptWeakMap.cpp] 192\n");
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
        {LOGMEIN("JavascriptWeakMap.cpp] 208\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap.prototype.has"), _u("WeakMap"));
        }

        JavascriptWeakMap* weakMap = JavascriptWeakMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        bool hasValue = false;

        if (JavascriptOperators::IsObject(key) && JavascriptOperators::GetTypeId(key) != TypeIds_HostDispatch)
        {LOGMEIN("JavascriptWeakMap.cpp] 218\n");
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
        {LOGMEIN("JavascriptWeakMap.cpp] 235\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("WeakMap.prototype.set"), _u("WeakMap"));
        }

        JavascriptWeakMap* weakMap = JavascriptWeakMap::FromVar(args[0]);

        Var key = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        Var value = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        if (!JavascriptOperators::IsObject(key) || JavascriptOperators::GetTypeId(key) == TypeIds_HostDispatch)
        {LOGMEIN("JavascriptWeakMap.cpp] 245\n");
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
        {LOGMEIN("JavascriptWeakMap.cpp] 259\n");
            scriptContext->TTDContextInfo->TTDWeakReferencePinSet->Add(keyObj);
        }
#endif

        weakMap->Set(keyObj, value);

        return weakMap;
    }

    void JavascriptWeakMap::Clear()
    {LOGMEIN("JavascriptWeakMap.cpp] 270\n");
        keySet.Map([&](DynamicObject* key, bool value, const RecyclerWeakReference<DynamicObject>* weakRef) {
            WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

            // It may be the case that a CEO has been reset and the keyMap is now null.
            // Just ignore it in this case, the keyMap has already been collected.
            if (keyMap != nullptr)
            {LOGMEIN("JavascriptWeakMap.cpp] 277\n");
                // It may also be the case that a CEO has been reset and then added to a separate WeakMap,
                // creating a new WeakMapKeyMap on the CEO.  In this case GetWeakMapId() may not be in the
                // keyMap, so don't assert successful removal here.
                keyMap->Remove(GetWeakMapId());
            }
        });
        keySet.Clear();
    }

    bool JavascriptWeakMap::Delete(DynamicObject* key)
    {LOGMEIN("JavascriptWeakMap.cpp] 288\n");
        WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

        if (keyMap != nullptr)
        {LOGMEIN("JavascriptWeakMap.cpp] 292\n");
            bool unused = false;
            bool inSet = keySet.TryGetValueAndRemove(key, &unused);
            bool inData = keyMap->Remove(GetWeakMapId());
            Assert(inSet == inData);

            return inData;
        }

        return false;
    }

    bool JavascriptWeakMap::Get(DynamicObject* key, Var* value) const
    {LOGMEIN("JavascriptWeakMap.cpp] 305\n");
        WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

        if (keyMap != nullptr)
        {LOGMEIN("JavascriptWeakMap.cpp] 309\n");
            return KeyMapGet(keyMap, value);
        }

        return false;
    }

    bool JavascriptWeakMap::Has(DynamicObject* key) const
    {LOGMEIN("JavascriptWeakMap.cpp] 317\n");
        WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

        if (keyMap != nullptr)
        {LOGMEIN("JavascriptWeakMap.cpp] 321\n");
            return keyMap->ContainsKey(GetWeakMapId());
        }

        return false;
    }

    void JavascriptWeakMap::Set(DynamicObject* key, Var value)
    {LOGMEIN("JavascriptWeakMap.cpp] 329\n");
        WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

        if (keyMap == nullptr)
        {LOGMEIN("JavascriptWeakMap.cpp] 333\n");
            keyMap = AddWeakMapKeyMapToKey(key);
        }

        keyMap->Item(GetWeakMapId(), value);
        keySet.Item(key, true);
    }

    BOOL JavascriptWeakMap::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptWeakMap.cpp] 342\n");
        stringBuilder->AppendCppLiteral(_u("WeakMap"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptWeakMap::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptWeakMap.cpp] 349\n");
        this->Map([&](DynamicObject* key, Js::Var value)
        {
            extractor->MarkVisitVar(key);
            extractor->MarkVisitVar(value);
        });
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptWeakMap::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptWeakMap.cpp] 358\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapMapObject;
    }

    void JavascriptWeakMap::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptWeakMap.cpp] 363\n");
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
        {LOGMEIN("JavascriptWeakMap.cpp] 380\n");
            smi->MapKeyValueArray = nullptr;
            alloc.SlabAbortArraySpace<TTD::TTDVar>(mapCountEst + 1);
        }
        else
        {
            alloc.SlabCommitArraySpace<TTD::TTDVar>(smi->MapSize, mapCountEst + 1);
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapMapInfo*, TTD::NSSnapObjects::SnapObjectType::SnapMapObject>(objData, smi);
    }
#endif
}
