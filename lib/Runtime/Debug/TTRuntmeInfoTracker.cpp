//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#if ENABLE_TTD

namespace TTD
{
    void ThreadContextTTD::AddNewScriptContext_Helper(Js::ScriptContext* ctx, HostScriptContextCallbackFunctor& callbackFunctor, bool noNative, bool debugMode)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 11\n");
        ////
        //First just setup the standard things needed for a script context
        ctx->TTDHostCallbackFunctor = callbackFunctor;
        if(noNative)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 16\n");
            ctx->ForceNoNative();
        }

        if(debugMode)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 21\n");
#ifdef _WIN32
            ctx->InitializeDebugging();
#else
            //
            //TODO: x-plat does not like some parts of initiallize debugging so just set the flag we need 
            //
            ctx->GetDebugContext()->SetDebuggerMode(Js::DebuggerMode::Debugging);
#endif
        }

        ctx->InitializeCoreImage_TTD();

        TTDAssert(!this->m_contextList.Contains(ctx), "We should only be adding at creation time!!!");
        this->m_contextList.Add(ctx);
    }

    ThreadContextTTD::ThreadContextTTD(ThreadContext* threadContext, void* runtimeHandle, uint32 snapInterval, uint32 snapHistoryLength)
        : m_threadCtx(threadContext), m_runtimeHandle(runtimeHandle), m_contextCreatedOrDestoyedInReplay(false),
        SnapInterval(snapInterval), SnapHistoryLength(snapHistoryLength),
        m_activeContext(nullptr), m_contextList(&HeapAllocator::Instance), m_deadScriptRecordList(&HeapAllocator::Instance), m_ttdContextToExternalRefMap(&HeapAllocator::Instance),
        m_ttdRootSet(), m_ttdLocalRootSet(), m_ttdRootTagIdMap(&HeapAllocator::Instance),
        TTDataIOInfo({ 0 }), TTDExternalObjectFunctions({ 0 })
    {
        Recycler* tctxRecycler = this->m_threadCtx->GetRecycler();

        this->m_ttdRootSet.Root(RecyclerNew(tctxRecycler, TTD::ObjectPinSet, tctxRecycler), tctxRecycler);
        this->m_ttdLocalRootSet.Root(RecyclerNew(tctxRecycler, TTD::ObjectPinSet, tctxRecycler), tctxRecycler);
    }

    ThreadContextTTD::~ThreadContextTTD()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 52\n");
        for(auto iter = this->m_ttdContextToExternalRefMap.GetIterator(); iter.IsValid(); iter.MoveNext())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 54\n");
            this->m_threadCtx->GetRecycler()->RootRelease(iter.CurrentValue());
        }
        this->m_ttdContextToExternalRefMap.Clear();

        this->m_activeContext = nullptr;
        this->m_contextList.Clear();
        this->m_deadScriptRecordList.Clear();

        if(this->m_ttdRootSet != nullptr)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 64\n");
            this->m_ttdRootSet.Unroot(this->m_ttdRootSet->GetAllocator());
        }

        if(this->m_ttdLocalRootSet != nullptr)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 69\n");
            this->m_ttdLocalRootSet.Unroot(this->m_ttdLocalRootSet->GetAllocator());
        }

        this->m_ttdRootTagIdMap.Clear();
    }

    ThreadContext* ThreadContextTTD::GetThreadContext()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 77\n");
        return this->m_threadCtx;
    }

    void* ThreadContextTTD::GetRuntimeHandle()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 82\n");
        return this->m_runtimeHandle;
    }

    FinalizableObject* ThreadContextTTD::GetRuntimeContextForScriptContext(Js::ScriptContext* ctx)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 87\n");
        return this->m_ttdContextToExternalRefMap.Lookup(ctx, nullptr);
    }

    bool ThreadContextTTD::ContextCreatedOrDestoyedInReplay() const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 92\n");
        return this->m_contextCreatedOrDestoyedInReplay;
    }

    void ThreadContextTTD::ResetContextCreatedOrDestoyedInReplay()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 97\n");
        this->m_contextCreatedOrDestoyedInReplay = false;
    }

    const JsUtil::List<Js::ScriptContext*, HeapAllocator>& ThreadContextTTD::GetTTDContexts() const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 102\n");
        return this->m_contextList;
    }

    JsUtil::List<DeadScriptLogTagInfo, HeapAllocator>& ThreadContextTTD::GetTTDDeadContextsForRecord()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 107\n");
        return this->m_deadScriptRecordList;
    }

    void ThreadContextTTD::AddNewScriptContextRecord(FinalizableObject* externalCtx, Js::ScriptContext* ctx, HostScriptContextCallbackFunctor& callbackFunctor, bool noNative, bool debugMode)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 112\n");
        this->AddNewScriptContext_Helper(ctx, callbackFunctor, noNative, debugMode);

        this->AddTrackedRootSpecial(TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetGlobalObject()), ctx->GetGlobalObject());
        ctx->ScriptContextLogTag = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetGlobalObject());

        this->AddTrackedRootSpecial(TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetLibrary()->GetUndefined()), ctx->GetLibrary()->GetUndefined());
        this->AddTrackedRootSpecial(TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetLibrary()->GetNull()), ctx->GetLibrary()->GetNull());
        this->AddTrackedRootSpecial(TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetLibrary()->GetTrue()), ctx->GetLibrary()->GetTrue());
        this->AddTrackedRootSpecial(TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetLibrary()->GetFalse()), ctx->GetLibrary()->GetFalse());
    }

    void ThreadContextTTD::AddNewScriptContextReplay(FinalizableObject* externalCtx, Js::ScriptContext* ctx, HostScriptContextCallbackFunctor& callbackFunctor, bool noNative, bool debugMode)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 125\n");
        this->AddNewScriptContext_Helper(ctx, callbackFunctor, noNative, debugMode);

        this->m_contextCreatedOrDestoyedInReplay = true;

        this->m_threadCtx->GetRecycler()->RootAddRef(externalCtx);
        this->m_ttdContextToExternalRefMap.Add(ctx, externalCtx);
    }

    void ThreadContextTTD::SetActiveScriptContext(Js::ScriptContext* ctx)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 135\n");
        TTDAssert(ctx == nullptr || this->m_contextList.Contains(ctx), "Missing value!!!");

        this->m_activeContext = ctx;
    }

    Js::ScriptContext* ThreadContextTTD::GetActiveScriptContext()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 142\n");
        return this->m_activeContext;
    }

    void ThreadContextTTD::NotifyCtxDestroyInRecord(Js::ScriptContext* ctx)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 147\n");
        if(this->m_contextList.Contains(ctx))
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 149\n");
            DeadScriptLogTagInfo deadInfo = { 0 };
            deadInfo.GlobalLogTag = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetGlobalObject());
            deadInfo.UndefinedLogTag = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetLibrary()->GetUndefined());
            deadInfo.NullLogTag = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetLibrary()->GetNull());
            deadInfo.TrueLogTag = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetLibrary()->GetTrue());
            deadInfo.FalseLogTag = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(ctx->GetLibrary()->GetFalse());

            this->m_deadScriptRecordList.Add(deadInfo);

            this->RemoveTrackedRootSpecial(deadInfo.GlobalLogTag);
            this->RemoveTrackedRootSpecial(deadInfo.UndefinedLogTag);
            this->RemoveTrackedRootSpecial(deadInfo.NullLogTag);
            this->RemoveTrackedRootSpecial(deadInfo.TrueLogTag);
            this->RemoveTrackedRootSpecial(deadInfo.FalseLogTag);

            this->m_contextList.Remove(ctx);
        }
    }

    void ThreadContextTTD::NotifyCtxDestroyedInReplay(TTD_LOG_PTR_ID globalId, TTD_LOG_PTR_ID undefId, TTD_LOG_PTR_ID nullId, TTD_LOG_PTR_ID trueId, TTD_LOG_PTR_ID falseId)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 170\n");
        this->m_contextCreatedOrDestoyedInReplay = true;

        Js::ScriptContext* ctx = nullptr;
        for(int32 i = 0; i < this->m_contextList.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 175\n");
            if(this->m_contextList.Item(i)->ScriptContextLogTag == globalId)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 177\n");
                ctx = this->m_contextList.Item(i);
                break;
            }
        }
        TTDAssert(ctx != nullptr, "We lost a context somewhere!");

        this->RemoveTrackedRootSpecial(globalId);
        this->RemoveTrackedRootSpecial(undefId);
        this->RemoveTrackedRootSpecial(nullId);
        this->RemoveTrackedRootSpecial(trueId);
        this->RemoveTrackedRootSpecial(falseId);

        this->m_contextList.Remove(ctx);

        FinalizableObject* externalCtx = this->m_ttdContextToExternalRefMap.Item(ctx);
        this->m_ttdContextToExternalRefMap.Remove(ctx);

        this->m_threadCtx->GetRecycler()->RootRelease(externalCtx);
    }

    void ThreadContextTTD::ClearContextsForSnapRestore(JsUtil::List<FinalizableObject*, HeapAllocator>& deadCtxs)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 199\n");
        for(int32 i = 0; i < this->m_contextList.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 201\n");
            Js::ScriptContext* ctx = this->m_contextList.Item(i);
            FinalizableObject* externalCtx = this->m_ttdContextToExternalRefMap.Item(ctx);

            deadCtxs.Add(externalCtx);
        }
        this->m_ttdContextToExternalRefMap.Clear();
        this->m_contextList.Clear();

        this->m_activeContext = nullptr;
    }

    bool ThreadContextTTD::IsSpecialRootObject(Js::RecyclableObject* obj)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 214\n");
        Js::TypeId tid = obj->GetTypeId();
        return (tid <= Js::TypeIds_Boolean) | (tid == Js::TypeIds_GlobalObject);
    }

    //Get all of the roots for a script context (roots are currently any recyclableObjects exposed to the host)
    void ThreadContextTTD::AddTrackedRootGeneral(TTD_LOG_PTR_ID origId, Js::RecyclableObject* newRoot)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 221\n");
        TTDAssert(!ThreadContextTTD::IsSpecialRootObject(newRoot), "Should add these with special path!!!");
        TTDAssert(!this->m_ttdRootSet->ContainsKey(newRoot), "Should not have duplicate inserts.");

        this->m_ttdRootSet->AddNew(newRoot);
        this->m_ttdRootTagIdMap.Item(origId, newRoot);
    }

    void ThreadContextTTD::RemoveTrackedRootGeneral(TTD_LOG_PTR_ID origId, Js::RecyclableObject* deleteRoot)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 230\n");
        TTDAssert(!ThreadContextTTD::IsSpecialRootObject(deleteRoot), "Should add these with special path!!!");
        TTDAssert(this->m_ttdRootSet->ContainsKey(deleteRoot), "Should not have delete elements that are not in the root set.");

        this->m_ttdRootSet->Remove(deleteRoot);
        if(!this->m_ttdLocalRootSet->ContainsKey(deleteRoot))
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 236\n");
            this->m_ttdRootTagIdMap.Remove(origId);
        }
    }

    void ThreadContextTTD::AddTrackedRootSpecial(TTD_LOG_PTR_ID origId, Js::RecyclableObject* newRoot)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 242\n");
        TTDAssert(ThreadContextTTD::IsSpecialRootObject(newRoot), "Should add these with special path!!!");
        TTDAssert(!this->m_ttdRootSet->ContainsKey(newRoot), "Should not have duplicate inserts.");

        this->m_ttdRootTagIdMap.Item(origId, newRoot);
    }

    void ThreadContextTTD::RemoveTrackedRootSpecial(TTD_LOG_PTR_ID origId)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 250\n");
        this->m_ttdRootTagIdMap.Remove(origId);
    }

    const ObjectPinSet* ThreadContextTTD::GetRootSet() const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 255\n");
        return this->m_ttdRootSet;
    }

    void ThreadContextTTD::AddLocalRoot(TTD_LOG_PTR_ID origId, Js::RecyclableObject* newRoot)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 260\n");
        TTDAssert(!ThreadContextTTD::IsSpecialRootObject(newRoot), "Should not be adding these as local roots!!!");

        this->m_ttdLocalRootSet->AddNew(newRoot);

        //if the pinned root set already has an entry then don't overwrite that one with a local entry (e.g. we will keep the current value)
        if(!this->m_ttdRootSet->ContainsKey(newRoot))
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 267\n");
            this->m_ttdRootTagIdMap.Item(origId, newRoot);
        }
    }

    void ThreadContextTTD::ClearLocalRootsAndRefreshMap()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 273\n");
        this->m_ttdLocalRootSet->Clear();

        this->m_ttdRootTagIdMap.MapAndRemoveIf([&](JsUtil::SimpleDictionaryEntry<TTD_LOG_PTR_ID, Js::RecyclableObject*>& entry) -> bool
        {
            Js::RecyclableObject* obj = entry.Value();
            return !(ThreadContextTTD::IsSpecialRootObject(obj) || this->m_ttdRootSet->Contains(obj));
        });
    }

    const ObjectPinSet* ThreadContextTTD::GetLocalRootSet() const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 284\n");
        return this->m_ttdLocalRootSet;
    }

    void ThreadContextTTD::LoadInvertedRootMap(JsUtil::BaseDictionary<Js::RecyclableObject*, TTD_LOG_PTR_ID, HeapAllocator>& objToLogIdMap) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 289\n");
        for(auto iter = this->m_ttdRootTagIdMap.GetIterator(); iter.IsValid(); iter.MoveNext())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 291\n");
            objToLogIdMap.AddNew(iter.CurrentValue(), iter.CurrentKey());
        }
    }

    void ThreadContextTTD::ExtractSnapshotRoots(JsUtil::List<Js::Var, HeapAllocator>& roots)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 297\n");
        for(auto rootIter = this->m_ttdRootSet->GetIterator(); rootIter.IsValid(); rootIter.MoveNext())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 299\n");
            Js::RecyclableObject* obj = rootIter.CurrentValue();
            roots.Add(obj);
        }

        for(auto localIter = this->m_ttdLocalRootSet->GetIterator(); localIter.IsValid(); localIter.MoveNext())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 305\n");
            Js::RecyclableObject* obj = localIter.CurrentValue();
            if(!this->m_ttdRootSet->Contains(obj))
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 308\n");
                roots.Add(obj);
            }
        }
    }

    Js::RecyclableObject* ThreadContextTTD::LookupObjectForLogID(TTD_LOG_PTR_ID origId)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 315\n");
        //Local root always has mappings for all the ids
        return this->m_ttdRootTagIdMap.Item(origId);
    }

    void ThreadContextTTD::ClearRootsForSnapRestore()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 321\n");
        this->m_ttdRootSet->Clear();
        this->m_ttdLocalRootSet->Clear();

        this->m_ttdRootTagIdMap.Clear();
    }

    Js::ScriptContext* ThreadContextTTD::LookupContextForScriptId(TTD_LOG_PTR_ID ctxId) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 329\n");
        for(int i = 0; i < this->m_contextList.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 331\n");
            if(this->m_contextList.Item(i)->ScriptContextLogTag == ctxId)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 333\n");
                return this->m_contextList.Item(i);
            }
        }

        return nullptr;
    }

    ScriptContextTTD::ScriptContextTTD(Js::ScriptContext* ctx)
        : m_ctx(ctx),
        m_ttdPendingAsyncModList(&HeapAllocator::Instance),
        m_ttdTopLevelScriptLoad(&HeapAllocator::Instance), m_ttdTopLevelNewFunction(&HeapAllocator::Instance), m_ttdTopLevelEval(&HeapAllocator::Instance),
        m_ttdFunctionBodyParentMap(&HeapAllocator::Instance)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 346\n");
        Recycler* ctxRecycler = this->m_ctx->GetRecycler();

        this->m_ttdPinnedRootFunctionSet.Root(RecyclerNew(ctxRecycler, TTD::FunctionBodyPinSet, ctxRecycler), ctxRecycler);
        this->TTDWeakReferencePinSet.Root(RecyclerNew(ctxRecycler, TTD::ObjectPinSet, ctxRecycler), ctxRecycler);
    }

    ScriptContextTTD::~ScriptContextTTD()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 354\n");
        this->m_ttdPendingAsyncModList.Clear();

        this->m_ttdTopLevelScriptLoad.Clear();
        this->m_ttdTopLevelNewFunction.Clear();
        this->m_ttdTopLevelEval.Clear();

        if(this->m_ttdPinnedRootFunctionSet != nullptr)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 362\n");
            this->m_ttdPinnedRootFunctionSet.Unroot(this->m_ttdPinnedRootFunctionSet->GetAllocator());
        }

        this->m_ttdFunctionBodyParentMap.Clear();

        if(this->TTDWeakReferencePinSet != nullptr)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 369\n");
            this->TTDWeakReferencePinSet.Unroot(this->TTDWeakReferencePinSet->GetAllocator());
        }
    }

    void ScriptContextTTD::AddToAsyncPendingList(Js::ArrayBuffer* trgt, uint32 index)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 375\n");
        TTDPendingAsyncBufferModification pending = { trgt, index };
        this->m_ttdPendingAsyncModList.Add(pending);
    }

    void ScriptContextTTD::GetFromAsyncPendingList(TTDPendingAsyncBufferModification* pendingInfo, byte* finalModPos)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 381\n");
        pendingInfo->ArrayBufferVar = nullptr;
        pendingInfo->Index = 0;

        const byte* currentBegin = nullptr;
        int32 pos = -1;
        for(int32 i = 0; i < this->m_ttdPendingAsyncModList.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 388\n");
            const TTDPendingAsyncBufferModification& pi = this->m_ttdPendingAsyncModList.Item(i);
            const Js::ArrayBuffer* pbuff = Js::ArrayBuffer::FromVar(pi.ArrayBufferVar);
            const byte* pbuffBegin = pbuff->GetBuffer() + pi.Index;
            const byte* pbuffMax = pbuff->GetBuffer() + pbuff->GetByteLength();

            //if the final mod is less than the start of this buffer + index or off then end then this definitely isn't it so skip
            if(pbuffBegin > finalModPos || pbuffMax < finalModPos)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 396\n");
                continue;
            }

            //it is in the right range so now we assume non-overlapping so we see if this pbuffBegin is closer than the current best
            TTDAssert(finalModPos != currentBegin, "We have something strange!!!");
            if(currentBegin == nullptr || finalModPos < currentBegin)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 403\n");
                currentBegin = pbuffBegin;
                pos = (int32)i;
            }
        }
        TTDAssert(pos != -1, "Missing matching register!!!");

        *pendingInfo = this->m_ttdPendingAsyncModList.Item(pos);
        this->m_ttdPendingAsyncModList.RemoveAt(pos);
    }

    const JsUtil::List<TTDPendingAsyncBufferModification, HeapAllocator>& ScriptContextTTD::GetPendingAsyncModListForSnapshot() const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 415\n");
        return this->m_ttdPendingAsyncModList;
    }

    void ScriptContextTTD::ClearPendingAsyncModListForSnapRestore()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 420\n");
        this->m_ttdPendingAsyncModList.Clear();
    }

    void ScriptContextTTD::GetLoadedSources(JsUtil::List<TTD::TopLevelFunctionInContextRelation, HeapAllocator>& topLevelScriptLoad, JsUtil::List<TTD::TopLevelFunctionInContextRelation, HeapAllocator>& topLevelNewFunction, JsUtil::List<TTD::TopLevelFunctionInContextRelation, HeapAllocator>& topLevelEval)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 425\n");
        TTDAssert(topLevelScriptLoad.Count() == 0 && topLevelNewFunction.Count() == 0 && topLevelEval.Count() == 0, "Should be empty when you call this.");

        topLevelScriptLoad.AddRange(this->m_ttdTopLevelScriptLoad);
        topLevelNewFunction.AddRange(this->m_ttdTopLevelNewFunction);
        topLevelEval.AddRange(this->m_ttdTopLevelEval);
    }

    bool ScriptContextTTD::IsBodyAlreadyLoadedAtTopLevel(Js::FunctionBody* body) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 434\n");
        return this->m_ttdPinnedRootFunctionSet->Contains(body);
    }

    void ScriptContextTTD::ProcessFunctionBodyOnLoad(Js::FunctionBody* body, Js::FunctionBody* parent)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 439\n");
        //if this is a root (parent is null) then put this in the rootbody pin set so it isn't reclaimed on us
        if(parent == nullptr)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 442\n");
            TTDAssert(!this->m_ttdPinnedRootFunctionSet->Contains(body), "We already added this function!!!");
            this->m_ttdPinnedRootFunctionSet->AddNew(body);
        }

        this->m_ttdFunctionBodyParentMap.AddNew(body, parent);

        for(uint32 i = 0; i < body->GetNestedCount(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 450\n");
            Js::ParseableFunctionInfo* pfiMid = body->GetNestedFunctionForExecution(i);
            Js::FunctionBody* currfb = TTD::JsSupport::ForceAndGetFunctionBody(pfiMid);

            this->ProcessFunctionBodyOnLoad(currfb, body);
        }
    }

    void ScriptContextTTD::RegisterLoadedScript(Js::FunctionBody* body, uint64 bodyCtrId)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 459\n");
        TTD::TopLevelFunctionInContextRelation relation;
        relation.TopLevelBodyCtr = bodyCtrId;
        relation.ContextSpecificBodyPtrId = TTD_CONVERT_FUNCTIONBODY_TO_PTR_ID(body);

        this->m_ttdTopLevelScriptLoad.Add(relation);
    }

    void ScriptContextTTD::RegisterNewScript(Js::FunctionBody* body, uint64 bodyCtrId)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 468\n");
        TTD::TopLevelFunctionInContextRelation relation;
        relation.TopLevelBodyCtr = bodyCtrId;
        relation.ContextSpecificBodyPtrId = TTD_CONVERT_FUNCTIONBODY_TO_PTR_ID(body);

        this->m_ttdTopLevelNewFunction.Add(relation);
    }

    void ScriptContextTTD::RegisterEvalScript(Js::FunctionBody* body, uint64 bodyCtrId)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 477\n");
        TTD::TopLevelFunctionInContextRelation relation;
        relation.TopLevelBodyCtr = bodyCtrId;
        relation.ContextSpecificBodyPtrId = TTD_CONVERT_FUNCTIONBODY_TO_PTR_ID(body);

        this->m_ttdTopLevelEval.Add(relation);
    }

    Js::FunctionBody* ScriptContextTTD::ResolveParentBody(Js::FunctionBody* body) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 486\n");
        //Want to return null if this has no parent (or this is an internal function and we don't care about parents)
        return this->m_ttdFunctionBodyParentMap.LookupWithKey(body, nullptr);
    }

    uint64 ScriptContextTTD::FindTopLevelCtrForBody(Js::FunctionBody* body) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 492\n");
        Js::FunctionBody* rootBody = body;
        while(this->ResolveParentBody(rootBody) != nullptr)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 495\n");
            rootBody = this->ResolveParentBody(rootBody);
        }

        TTD_PTR_ID trgtid = TTD_CONVERT_FUNCTIONBODY_TO_PTR_ID(rootBody);

        for(int32 i = 0; i < this->m_ttdTopLevelScriptLoad.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 502\n");
            if(this->m_ttdTopLevelScriptLoad.Item(i).ContextSpecificBodyPtrId == trgtid)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 504\n");
                return this->m_ttdTopLevelScriptLoad.Item(i).TopLevelBodyCtr;
            }
        }

        for(int32 i = 0; i < this->m_ttdTopLevelNewFunction.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 510\n");
            if(this->m_ttdTopLevelNewFunction.Item(i).ContextSpecificBodyPtrId == trgtid)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 512\n");
                return this->m_ttdTopLevelNewFunction.Item(i).TopLevelBodyCtr;
            }
        }

        for(int32 i = 0; i < this->m_ttdTopLevelEval.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 518\n");
            if(this->m_ttdTopLevelEval.Item(i).ContextSpecificBodyPtrId == trgtid)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 520\n");
                return this->m_ttdTopLevelEval.Item(i).TopLevelBodyCtr;
            }
        }

        TTDAssert(false, "We are missing a top-level function reference.");
        return 0;
    }

    Js::FunctionBody* ScriptContextTTD::FindRootBodyByTopLevelCtr(uint64 bodyCtrId) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 530\n");
        for(int32 i = 0; i < this->m_ttdTopLevelScriptLoad.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 532\n");
            if(this->m_ttdTopLevelScriptLoad.Item(i).TopLevelBodyCtr == bodyCtrId)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 534\n");
                return TTD_COERCE_PTR_ID_TO_FUNCTIONBODY(this->m_ttdTopLevelScriptLoad.Item(i).ContextSpecificBodyPtrId);
            }
        }

        for(int32 i = 0; i < this->m_ttdTopLevelNewFunction.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 540\n");
            if(this->m_ttdTopLevelNewFunction.Item(i).TopLevelBodyCtr == bodyCtrId)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 542\n");
                return TTD_COERCE_PTR_ID_TO_FUNCTIONBODY(this->m_ttdTopLevelNewFunction.Item(i).ContextSpecificBodyPtrId);
            }
        }

        for(int32 i = 0; i < this->m_ttdTopLevelEval.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 548\n");
            if(this->m_ttdTopLevelEval.Item(i).TopLevelBodyCtr == bodyCtrId)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 550\n");
                return TTD_COERCE_PTR_ID_TO_FUNCTIONBODY(this->m_ttdTopLevelEval.Item(i).ContextSpecificBodyPtrId);
            }
        }

        //TTDAssert(false, "We are missing a top level body counter.");
        return nullptr;
    }

    void ScriptContextTTD::ClearLoadedSourcesForSnapshotRestore()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 560\n");
        this->m_ttdTopLevelScriptLoad.Clear();
        this->m_ttdTopLevelNewFunction.Clear();
        this->m_ttdTopLevelEval.Clear();

        this->m_ttdPinnedRootFunctionSet->Clear();

        this->m_ttdFunctionBodyParentMap.Clear();
    }

    //////////////////

    void RuntimeContextInfo::BuildPathString(UtilSupport::TTAutoString rootpath, const char16* name, const char16* optaccessortag, UtilSupport::TTAutoString& into)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 573\n");
        into.Append(rootpath);
        into.Append(_u("."));
        into.Append(name);

        if(optaccessortag != nullptr)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 579\n");
           into.Append(optaccessortag);
        }
    }

    void RuntimeContextInfo::LoadAndOrderPropertyNames(Js::RecyclableObject* obj, JsUtil::List<const Js::PropertyRecord*, HeapAllocator>& propertyList)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 585\n");
        TTDAssert(propertyList.Count() == 0, "This should be empty.");

        Js::ScriptContext* ctx = obj->GetScriptContext();
        uint32 propcount = (uint32)obj->GetPropertyCount();

        //get all of the properties
        for(uint32 i = 0; i < propcount; ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 593\n");
            Js::PropertyIndex propertyIndex = (Js::PropertyIndex)i;
            Js::PropertyId propertyId = obj->GetPropertyId(propertyIndex);

            if((propertyId != Js::Constants::NoProperty) & (!Js::IsInternalPropertyId(propertyId)))
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 598\n");
                TTDAssert(obj->HasOwnProperty(propertyId), "We are assuming this is own property count.");

                propertyList.Add(ctx->GetPropertyName(propertyId));
            }
        }

        //now sort the list so the traversal order is stable
        //Rock a custom shell sort!!!!
        const int32 gaps[6] = { 132, 57, 23, 10, 4, 1 };

        int32 llen = propertyList.Count();
        for(uint32 gapi = 0; gapi < 6; ++gapi)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 611\n");
            int32 gap = gaps[gapi];

            for(int32 i = gap; i < llen; i++)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 615\n");
                const Js::PropertyRecord* temp = propertyList.Item(i);

                int32 j = 0;
                for(j = i; j >= gap && PropertyNameCmp(propertyList.Item(j - gap), temp); j -= gap)
                {LOGMEIN("TTRuntmeInfoTracker.cpp] 620\n");
                    const Js::PropertyRecord* shiftElem = propertyList.Item(j - gap);
                    propertyList.SetItem(j, shiftElem);
                }

                propertyList.SetItem(j, temp);
            }
        }
    }

    bool RuntimeContextInfo::PropertyNameCmp(const Js::PropertyRecord* p1, const Js::PropertyRecord* p2)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 631\n");
        if(p1->GetLength() != p2->GetLength())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 633\n");
            return p1->GetLength() > p2->GetLength();
        }
        else
        {
            return wcscmp(p1->GetBuffer(), p2->GetBuffer()) > 0;
        }
    }
    
    RuntimeContextInfo::RuntimeContextInfo()
        : m_worklist(&HeapAllocator::Instance), m_nullString(),
        m_coreObjToPathMap(&HeapAllocator::Instance, TTD_CORE_OBJECT_COUNT), m_coreBodyToPathMap(&HeapAllocator::Instance, TTD_CORE_FUNCTION_BODY_COUNT), m_coreDbgScopeToPathMap(&HeapAllocator::Instance, TTD_CORE_FUNCTION_BODY_COUNT),
        m_sortedObjectList(&HeapAllocator::Instance, TTD_CORE_OBJECT_COUNT), m_sortedFunctionBodyList(&HeapAllocator::Instance, TTD_CORE_FUNCTION_BODY_COUNT), m_sortedDbgScopeList(&HeapAllocator::Instance, TTD_CORE_FUNCTION_BODY_COUNT)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 646\n");
        ;
    }

    RuntimeContextInfo::~RuntimeContextInfo()
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 651\n");
        for(auto iter = this->m_coreObjToPathMap.GetIterator(); iter.IsValid(); iter.MoveNext())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 653\n");
            TT_HEAP_DELETE(UtilSupport::TTAutoString, iter.CurrentValue());
        }

        for(auto iter = this->m_coreBodyToPathMap.GetIterator(); iter.IsValid(); iter.MoveNext())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 658\n");
            TT_HEAP_DELETE(UtilSupport::TTAutoString, iter.CurrentValue());
        }

        for(auto iter = this->m_coreDbgScopeToPathMap.GetIterator(); iter.IsValid(); iter.MoveNext())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 663\n");
            TT_HEAP_DELETE(UtilSupport::TTAutoString, iter.CurrentValue());
        }
    }

    //Mark all the well-known objects/values/types from this script context
    void RuntimeContextInfo::MarkWellKnownObjects_TTD(MarkTable& marks) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 670\n");
        for(int32 i = 0; i < this->m_sortedObjectList.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 672\n");
            Js::RecyclableObject* obj = this->m_sortedObjectList.Item(i);
            marks.MarkAddrWithSpecialInfo<MarkTableTag::JsWellKnownObj>(obj);
        }

        for(int32 i = 0; i < this->m_sortedFunctionBodyList.Count(); ++i)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 678\n");
            Js::FunctionBody* body = this->m_sortedFunctionBodyList.Item(i);
            marks.MarkAddrWithSpecialInfo<MarkTableTag::JsWellKnownObj>(body);
        }
    }

    TTD_WELLKNOWN_TOKEN RuntimeContextInfo::ResolvePathForKnownObject(Js::RecyclableObject* obj) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 685\n");
        const UtilSupport::TTAutoString* res = this->m_coreObjToPathMap.Item(obj);

        return res->GetStrValue();
    }

    TTD_WELLKNOWN_TOKEN RuntimeContextInfo::ResolvePathForKnownFunctionBody(Js::FunctionBody* fbody) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 692\n");
        const UtilSupport::TTAutoString* res = this->m_coreBodyToPathMap.Item(fbody);

        return res->GetStrValue();
    }

    TTD_WELLKNOWN_TOKEN RuntimeContextInfo::ResolvePathForKnownDbgScopeIfExists(Js::DebuggerScope* dbgScope) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 699\n");
        const UtilSupport::TTAutoString* res = this->m_coreDbgScopeToPathMap.LookupWithKey(dbgScope, nullptr);

        if(res == nullptr)
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 703\n");
            return nullptr;
        }

        return res->GetStrValue();
    }

    Js::RecyclableObject* RuntimeContextInfo::LookupKnownObjectFromPath(TTD_WELLKNOWN_TOKEN pathIdString) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 711\n");
        int32 pos = LookupPositionInDictNameList<Js::RecyclableObject*, true>(pathIdString, this->m_coreObjToPathMap, this->m_sortedObjectList, this->m_nullString);
        TTDAssert(pos != -1, "This isn't a well known object!");

        return this->m_sortedObjectList.Item(pos);
    }

    Js::FunctionBody* RuntimeContextInfo::LookupKnownFunctionBodyFromPath(TTD_WELLKNOWN_TOKEN pathIdString) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 719\n");
        int32 pos = LookupPositionInDictNameList<Js::FunctionBody*, true>(pathIdString, this->m_coreBodyToPathMap, this->m_sortedFunctionBodyList, this->m_nullString);
        TTDAssert(pos != -1, "Missing function.");

        return (pos != -1) ? this->m_sortedFunctionBodyList.Item(pos) : nullptr;
    }

    Js::DebuggerScope* RuntimeContextInfo::LookupKnownDebuggerScopeFromPath(TTD_WELLKNOWN_TOKEN pathIdString) const
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 727\n");
        int32 pos = LookupPositionInDictNameList<Js::DebuggerScope*, true>(pathIdString, this->m_coreDbgScopeToPathMap, this->m_sortedDbgScopeList, this->m_nullString);
        TTDAssert(pos != -1, "Missing debug scope.");

        return (pos != -1) ? this->m_sortedDbgScopeList.Item(pos) : nullptr;
    }

    void RuntimeContextInfo::GatherKnownObjectToPathMap(Js::ScriptContext* ctx)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 735\n");
        JsUtil::List<const Js::PropertyRecord*, HeapAllocator> propertyRecordList(&HeapAllocator::Instance);

        this->EnqueueRootPathObject(_u("global"), ctx->GetGlobalObject());
        this->EnqueueRootPathObject(_u("null"), ctx->GetLibrary()->GetNull());

        this->EnqueueRootPathObject(_u("_defaultAccessor"), ctx->GetLibrary()->GetDefaultAccessorFunction());

        if(ctx->GetConfig()->IsErrorStackTraceEnabled())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 744\n");
            this->EnqueueRootPathObject(_u("_stackTraceAccessor"), ctx->GetLibrary()->GetStackTraceAccessorFunction());
            this->EnqueueRootPathObject(_u("_throwTypeErrorRestrictedPropertyAccessor"), ctx->GetLibrary()->GetThrowTypeErrorRestrictedPropertyAccessorFunction());
        }

        if(ctx->GetConfig()->IsES6PromiseEnabled())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 750\n");
            this->EnqueueRootPathObject(_u("_identityFunction"), ctx->GetLibrary()->GetIdentityFunction());
            this->EnqueueRootPathObject(_u("_throwerFunction"), ctx->GetLibrary()->GetThrowerFunction());
        }

        this->EnqueueRootPathObject(_u("_arrayIteratorPrototype"), ctx->GetLibrary()->GetArrayIteratorPrototype());

        uint32 counter = 0;
        while(!this->m_worklist.Empty())
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 759\n");
            Js::RecyclableObject* curr = this->m_worklist.Dequeue();

            counter++;

            ////
            //Handle the standard properties for all object types

            //load propArray with all property names
            propertyRecordList.Clear();
            LoadAndOrderPropertyNames(curr, propertyRecordList);

            //access each property and process the target objects as needed
            for(int32 i = 0; i < propertyRecordList.Count(); ++i)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 773\n");
                const Js::PropertyRecord* precord = propertyRecordList.Item(i);

                Js::Var getter = nullptr;
                Js::Var setter = nullptr;
                if(curr->GetAccessors(precord->GetPropertyId(), &getter, &setter, ctx))
                {LOGMEIN("TTRuntmeInfoTracker.cpp] 779\n");
                    if(getter != nullptr && !Js::JavascriptOperators::IsUndefinedObject(getter))
                    {LOGMEIN("TTRuntmeInfoTracker.cpp] 781\n");
                        TTDAssert(Js::JavascriptFunction::Is(getter), "The getter is not a function?");
                        this->EnqueueNewPathVarAsNeeded(curr, getter, precord, _u(">"));
                    }

                    if(setter != nullptr && !Js::JavascriptOperators::IsUndefinedObject(setter))
                    {LOGMEIN("TTRuntmeInfoTracker.cpp] 787\n");
                        TTDAssert(Js::JavascriptFunction::Is(setter), "The setter is not a function?");
                        this->EnqueueNewPathVarAsNeeded(curr, Js::RecyclableObject::FromVar(setter), precord, _u("<"));
                    }
                }
                else
                {
                    Js::Var pitem = nullptr;
                    BOOL isproperty = Js::JavascriptOperators::GetOwnProperty(curr, precord->GetPropertyId(), &pitem, ctx);
                    TTDAssert(isproperty, "Not sure what went wrong.");

                    this->EnqueueNewPathVarAsNeeded(curr, pitem, precord, nullptr);
                }
            }

            //shouldn't have any dynamic array valued properties
            TTDAssert(!Js::DynamicType::Is(curr->GetTypeId()) || (Js::DynamicObject::FromVar(curr))->GetObjectArray() == nullptr || (Js::DynamicObject::FromVar(curr))->GetObjectArray()->GetLength() == 0, "Shouldn't have any dynamic array valued properties at this point.");

            Js::RecyclableObject* proto = curr->GetPrototype();
            bool skipProto = (proto == nullptr) || Js::JavascriptOperators::IsUndefinedOrNullType(proto->GetTypeId());
            if(!skipProto)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 808\n");
                this->EnqueueNewPathVarAsNeeded(curr, proto, _u("_proto_"));
            }

            curr->ProcessCorePaths();
        }

        SortDictIntoListOnNames<Js::RecyclableObject*>(this->m_coreObjToPathMap, this->m_sortedObjectList, this->m_nullString);
        SortDictIntoListOnNames<Js::FunctionBody*>(this->m_coreBodyToPathMap, this->m_sortedFunctionBodyList, this->m_nullString);
        SortDictIntoListOnNames<Js::DebuggerScope*>(this->m_coreDbgScopeToPathMap, this->m_sortedDbgScopeList, this->m_nullString);
    }

    ////

    void RuntimeContextInfo::EnqueueRootPathObject(const char16* rootName, Js::RecyclableObject* obj)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 823\n");
        this->m_worklist.Enqueue(obj);

        UtilSupport::TTAutoString* rootStr = TT_HEAP_NEW(UtilSupport::TTAutoString, rootName);

        TTDAssert(!this->m_coreObjToPathMap.ContainsKey(obj), "Already in map!!!");
        this->m_coreObjToPathMap.AddNew(obj, rootStr);
    }

    void RuntimeContextInfo::EnqueueNewPathVarAsNeeded(Js::RecyclableObject* parent, Js::Var val, const Js::PropertyRecord* prop, const char16* optacessortag)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 833\n");
        this->EnqueueNewPathVarAsNeeded(parent, val, prop->GetBuffer(), optacessortag);
    }

    void RuntimeContextInfo::EnqueueNewPathVarAsNeeded(Js::RecyclableObject* parent, Js::Var val, const char16* propName, const char16* optacessortag)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 838\n");
        if(JsSupport::IsVarTaggedInline(val))
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 840\n");
            return;
        }

        if(JsSupport::IsVarPrimitiveKind(val) && !Js::GlobalObject::Is(parent))
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 845\n");
            return; //we keep primitives from global object only -- may need others but this is a simple way to start to get undefined, null, infy, etc.
        }

        Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(val);
        if(!this->m_coreObjToPathMap.ContainsKey(obj))
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 851\n");   
            const UtilSupport::TTAutoString* ppath = this->m_coreObjToPathMap.Item(parent);

            this->m_worklist.Enqueue(obj);

            UtilSupport::TTAutoString* tpath = TT_HEAP_NEW(UtilSupport::TTAutoString, *ppath);
            tpath->Append(_u("."));
            tpath->Append(propName);

            if(optacessortag != nullptr)
            {LOGMEIN("TTRuntmeInfoTracker.cpp] 861\n");
                tpath->Append(optacessortag);
            }

            TTDAssert(!this->m_coreObjToPathMap.ContainsKey(obj), "Already in map!!!");
            this->m_coreObjToPathMap.AddNew(obj, tpath);
        }
    }

    void RuntimeContextInfo::EnqueueNewFunctionBodyObject(Js::RecyclableObject* parent, Js::FunctionBody* fbody, const char16* name)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 871\n");
        if(!this->m_coreBodyToPathMap.ContainsKey(fbody))
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 873\n");
            fbody->EnsureDeserialized();
            const UtilSupport::TTAutoString* ppath = this->m_coreObjToPathMap.Item(parent);

            UtilSupport::TTAutoString* fpath = TT_HEAP_NEW(UtilSupport::TTAutoString, *ppath);

            fpath->Append(_u("."));
            fpath->Append(name);

            this->m_coreBodyToPathMap.AddNew(fbody, fpath);
        }
    }

    void RuntimeContextInfo::AddWellKnownDebuggerScopePath(Js::RecyclableObject* parent, Js::DebuggerScope* dbgScope, uint32 index)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 887\n");
        if(!this->m_coreDbgScopeToPathMap.ContainsKey(dbgScope))
        {LOGMEIN("TTRuntmeInfoTracker.cpp] 889\n");
            const UtilSupport::TTAutoString* ppath = this->m_coreObjToPathMap.Item(parent);

            UtilSupport::TTAutoString* scpath = TT_HEAP_NEW(UtilSupport::TTAutoString, *ppath);

            scpath->Append(_u(".!scope["));
            scpath->Append(index);
            scpath->Append(_u("]"));

            this->m_coreDbgScopeToPathMap.AddNew(dbgScope, scpath);
        }
    }

    void RuntimeContextInfo::BuildArrayIndexBuffer(uint32 arrayidx, UtilSupport::TTAutoString& res)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 903\n");
        res.Append(_u("!arrayContents["));
        res.Append(arrayidx);
        res.Append(_u("]"));
    }

    void RuntimeContextInfo::BuildEnvironmentIndexBuffer(uint32 envidx, UtilSupport::TTAutoString& res)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 910\n");
        res.Append(_u("!env["));
        res.Append(envidx);
        res.Append(_u("]"));
    }

    void RuntimeContextInfo::BuildEnvironmentIndexAndSlotBuffer(uint32 envidx, uint32 slotidx, UtilSupport::TTAutoString& res)
    {LOGMEIN("TTRuntmeInfoTracker.cpp] 917\n");
        res.Append(_u("!env["));
        res.Append(envidx);
        res.Append(_u("].!slot["));
        res.Append(slotidx);
        res.Append(_u("]"));
    }
}

#endif
