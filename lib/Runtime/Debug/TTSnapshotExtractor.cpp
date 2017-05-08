//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#if ENABLE_TTD

namespace TTD
{
    void SnapshotExtractor::MarkVisitHandler(Js::DynamicTypeHandler* handler)
    {TRACE_IT(45342);
        this->m_marks.MarkAndTestAddr<MarkTableTag::TypeHandlerTag>(handler);
    }

    void SnapshotExtractor::MarkVisitType(Js::Type* type)
    {TRACE_IT(45343);
        //Must ensure this is de-serialized before you call this

        if(this->m_marks.MarkAndTestAddr<MarkTableTag::TypeTag>(type))
        {TRACE_IT(45344);
            if(Js::DynamicType::Is(type->GetTypeId()))
            {TRACE_IT(45345);
                Js::DynamicTypeHandler* handler = (static_cast<Js::DynamicType*>(type))->GetTypeHandler();

                this->MarkVisitHandler(handler);
            }

            Js::RecyclableObject* proto = type->GetPrototype();
            if(proto != nullptr)
            {TRACE_IT(45346);
                this->MarkVisitVar(proto);
            }
        }
    }

    void SnapshotExtractor::MarkVisitStandardProperties(Js::RecyclableObject* obj)
    {TRACE_IT(45347);
        TTDAssert(Js::DynamicType::Is(obj->GetTypeId()) || obj->GetPropertyCount() == 0, "Only dynamic objects should have standard properties.");

        if(Js::DynamicType::Is(obj->GetTypeId()))
        {TRACE_IT(45348);
            Js::DynamicObject* dynObj = Js::DynamicObject::FromVar(obj);

            dynObj->GetDynamicType()->GetTypeHandler()->MarkObjectSlots_TTD(this, dynObj);

            Js::ArrayObject* parray = dynObj->GetObjectArray();
            if(parray != nullptr)
            {TRACE_IT(45349);
                this->MarkVisitVar(parray);
            }
        }
    }

    void SnapshotExtractor::ExtractHandlerIfNeeded(Js::DynamicTypeHandler* handler, ThreadContext* threadContext)
    {TRACE_IT(45350);
        if(this->m_marks.IsMarked(handler))
        {TRACE_IT(45351);
            NSSnapType::SnapHandler* sHandler = this->m_pendingSnap->GetNextAvailableHandlerEntry();
            handler->ExtractSnapHandler(sHandler, threadContext, this->m_pendingSnap->GetSnapshotSlabAllocator());

            this->m_idToHandlerMap.AddItem(sHandler->HandlerId, sHandler);
            this->m_marks.ClearMark(handler);
        }
    }

    void SnapshotExtractor::ExtractTypeIfNeeded(Js::Type* jstype, ThreadContext* threadContext)
    {TRACE_IT(45352);
        if(this->m_marks.IsMarked(jstype))
        {TRACE_IT(45353);
            if(Js::DynamicType::Is(jstype->GetTypeId()))
            {TRACE_IT(45354);
                this->ExtractHandlerIfNeeded(static_cast<Js::DynamicType*>(jstype)->GetTypeHandler(), threadContext);
            }

            NSSnapType::SnapHandler* sHandler = nullptr;
            if(Js::DynamicType::Is(jstype->GetTypeId()))
            {TRACE_IT(45355);
                Js::DynamicTypeHandler* dhandler = static_cast<const Js::DynamicType*>(jstype)->GetTypeHandler();

                TTD_PTR_ID handlerId = TTD_CONVERT_TYPEINFO_TO_PTR_ID(dhandler);
                sHandler = this->m_idToHandlerMap.LookupKnownItem(handlerId);
            }

            NSSnapType::SnapType* sType = this->m_pendingSnap->GetNextAvailableTypeEntry();
            jstype->ExtractSnapType(sType, sHandler, this->m_pendingSnap->GetSnapshotSlabAllocator());

            this->m_idToTypeMap.AddItem(sType->TypePtrId, sType);
            this->m_marks.ClearMark(jstype);
        }
    }

    void SnapshotExtractor::ExtractSlotArrayIfNeeded(Js::ScriptContext* ctx, Js::Var* scope)
    {TRACE_IT(45356);
        if(this->m_marks.IsMarked(scope))
        {TRACE_IT(45357);
            NSSnapValues::SlotArrayInfo* slotInfo = this->m_pendingSnap->GetNextAvailableSlotArrayEntry();

            Js::ScopeSlots slots(scope);
            slotInfo->SlotId = TTD_CONVERT_VAR_TO_PTR_ID(scope);
            slotInfo->ScriptContextLogId = ctx->ScriptContextLogTag;

            slotInfo->SlotCount = slots.GetCount();
            slotInfo->Slots = this->m_pendingSnap->GetSnapshotSlabAllocator().SlabAllocateArray<TTDVar>(slotInfo->SlotCount);

            for(uint32 j = 0; j < slotInfo->SlotCount; ++j)
            {TRACE_IT(45358);
                slotInfo->Slots[j] = slots.Get(j);
            }

            if(slots.IsFunctionScopeSlotArray())
            {TRACE_IT(45359);
                Js::FunctionBody* fb = slots.GetFunctionInfo()->GetFunctionBody();

                slotInfo->isFunctionBodyMetaData = true;
                slotInfo->OptFunctionBodyId = TTD_CONVERT_FUNCTIONBODY_TO_PTR_ID(fb);
                slotInfo->OptDebugScopeId = TTD_INVALID_PTR_ID;
                slotInfo->OptWellKnownDbgScope = TTD_INVALID_WELLKNOWN_TOKEN;

                Js::PropertyId* propertyIds = fb->GetPropertyIdsForScopeSlotArray();
                slotInfo->PIDArray = this->m_pendingSnap->GetSnapshotSlabAllocator().SlabAllocateArray<Js::PropertyId>(slotInfo->SlotCount);
                js_memcpy_s(slotInfo->PIDArray, sizeof(Js::PropertyId) * slotInfo->SlotCount, propertyIds, sizeof(Js::PropertyId) * slots.GetCount());
            }
            else
            {TRACE_IT(45360);
                Js::DebuggerScope* dbgScope = slots.GetDebuggerScope();
                slotInfo->isFunctionBodyMetaData = false;
                slotInfo->OptFunctionBodyId = TTD_INVALID_PTR_ID;

                TTD_WELLKNOWN_TOKEN wellKnownToken = ctx->TTDWellKnownInfo->ResolvePathForKnownDbgScopeIfExists(dbgScope);
                if(wellKnownToken == TTD_INVALID_WELLKNOWN_TOKEN)
                {TRACE_IT(45361);
                    slotInfo->OptDebugScopeId = TTD_CONVERT_DEBUGSCOPE_TO_PTR_ID(dbgScope);
                    slotInfo->OptWellKnownDbgScope = TTD_INVALID_WELLKNOWN_TOKEN;
                }
                else
                {TRACE_IT(45362);
                    slotInfo->OptDebugScopeId = TTD_INVALID_PTR_ID;
                    slotInfo->OptWellKnownDbgScope = this->m_pendingSnap->GetSnapshotSlabAllocator().CopyRawNullTerminatedStringInto(wellKnownToken);
                }

                slotInfo->PIDArray = this->m_pendingSnap->GetSnapshotSlabAllocator().SlabAllocateArray<Js::PropertyId>(slotInfo->SlotCount);

                for(uint32 j = 0; j < slotInfo->SlotCount; ++j)
                {TRACE_IT(45363);
                    slotInfo->PIDArray[j] = dbgScope->GetPropertyIdForSlotIndex_TTD(j);
                }
            }

            this->m_marks.ClearMark(scope);
        }
    }

    void SnapshotExtractor::ExtractScopeIfNeeded(Js::ScriptContext* ctx, Js::FrameDisplay* environment)
    {TRACE_IT(45364);
        if(this->m_marks.IsMarked(environment))
        {TRACE_IT(45365);
            TTDAssert(environment->GetLength() > 0, "This doesn't make sense");

            NSSnapValues::ScriptFunctionScopeInfo* funcScopeInfo = this->m_pendingSnap->GetNextAvailableFunctionScopeEntry();
            funcScopeInfo->ScopeId = TTD_CONVERT_ENV_TO_PTR_ID(environment);
            funcScopeInfo->ScriptContextLogId = ctx->ScriptContextLogTag;

            funcScopeInfo->ScopeCount = environment->GetLength();
            funcScopeInfo->ScopeArray = this->m_pendingSnap->GetSnapshotSlabAllocator().SlabAllocateArray<NSSnapValues::ScopeInfoEntry>(funcScopeInfo->ScopeCount);

            for(uint16 i = 0; i < funcScopeInfo->ScopeCount; ++i)
            {TRACE_IT(45366);
                void* scope = environment->GetItem(i);
                NSSnapValues::ScopeInfoEntry* entryInfo = (funcScopeInfo->ScopeArray + i);

                entryInfo->Tag = environment->GetScopeType(scope);
                switch(entryInfo->Tag)
                {
                case Js::ScopeType::ScopeType_ActivationObject:
                case Js::ScopeType::ScopeType_WithScope:
                    entryInfo->IDValue = TTD_CONVERT_VAR_TO_PTR_ID((Js::Var)scope);
                    break;
                case Js::ScopeType::ScopeType_SlotArray:
                {TRACE_IT(45367);
                    this->ExtractSlotArrayIfNeeded(ctx, (Js::Var*)scope);

                    entryInfo->IDValue = TTD_CONVERT_SLOTARRAY_TO_PTR_ID((Js::Var*)scope);
                    break;
                }
                default:
                    TTDAssert(false, "Unknown scope kind");
                    entryInfo->IDValue = TTD_INVALID_PTR_ID;
                    break;
                }
            }

            this->m_marks.ClearMark(environment);
        }
    }

    void SnapshotExtractor::ExtractScriptFunctionEnvironmentIfNeeded(Js::ScriptFunction* function)
    {TRACE_IT(45368);
        Js::FrameDisplay* environment = function->GetEnvironment();
        if(environment->GetLength() != 0)
        {TRACE_IT(45369);
            this->ExtractScopeIfNeeded(function->GetScriptContext(), environment);
        }
    }

    void SnapshotExtractor::ExtractRootInfo(const ThreadContextTTD* tctx, const JsUtil::BaseDictionary<Js::RecyclableObject*, TTD_LOG_PTR_ID, HeapAllocator>& objToLogIdMap) const
    {
        UnorderedArrayList<SnapRootPinEntry, TTD_ARRAY_LIST_SIZE_MID>& glist = this->m_pendingSnap->GetGlobalRootList();

        //Extract special roots
        const JsUtil::List<Js::ScriptContext*, HeapAllocator>& ctxList = tctx->GetTTDContexts();
        for(int32 i = 0; i < ctxList.Count(); ++i)
        {TRACE_IT(45370);
            Js::ScriptContext* ctx = ctxList.Item(i);

            SnapRootPinEntry* speg = glist.NextOpenEntry();
            speg->LogObject = TTD_CONVERT_VAR_TO_PTR_ID(ctx->GetGlobalObject());
            speg->LogId = objToLogIdMap.Item(ctx->GetGlobalObject());

            SnapRootPinEntry* speu = glist.NextOpenEntry();
            speu->LogObject = TTD_CONVERT_VAR_TO_PTR_ID(ctx->GetLibrary()->GetUndefined());
            speu->LogId = objToLogIdMap.Item(ctx->GetLibrary()->GetUndefined());

            SnapRootPinEntry* spen = glist.NextOpenEntry();
            spen->LogObject = TTD_CONVERT_VAR_TO_PTR_ID(ctx->GetLibrary()->GetNull());
            spen->LogId = objToLogIdMap.Item(ctx->GetLibrary()->GetNull());

            SnapRootPinEntry* spet = glist.NextOpenEntry();
            spet->LogObject = TTD_CONVERT_VAR_TO_PTR_ID(ctx->GetLibrary()->GetTrue());
            spet->LogId = objToLogIdMap.Item(ctx->GetLibrary()->GetTrue());

            SnapRootPinEntry* spef = glist.NextOpenEntry();
            spef->LogObject = TTD_CONVERT_VAR_TO_PTR_ID(ctx->GetLibrary()->GetFalse());
            spef->LogId = objToLogIdMap.Item(ctx->GetLibrary()->GetFalse());
        }

        //Extract global roots
        for(auto iter = tctx->GetRootSet()->GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(45371);
            TTDAssert(objToLogIdMap.ContainsKey(iter.CurrentValue()), "We are missing a value mapping!!!");

            SnapRootPinEntry* spe = glist.NextOpenEntry();
            spe->LogObject = TTD_CONVERT_VAR_TO_PTR_ID(iter.CurrentValue());
            spe->LogId = objToLogIdMap.Item(iter.CurrentValue());
        }


        //Extract local roots
        UnorderedArrayList<SnapRootPinEntry, TTD_ARRAY_LIST_SIZE_SMALL>& llist = this->m_pendingSnap->GetLocalRootList();
        for(auto iter = tctx->GetLocalRootSet()->GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(45372);
            if(objToLogIdMap.ContainsKey(iter.CurrentValue()))
            {TRACE_IT(45373);
                SnapRootPinEntry* spe = llist.NextOpenEntry();

                spe->LogObject = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(iter.CurrentValue());
                spe->LogId = objToLogIdMap.Item(iter.CurrentValue());
            }
        }
    }

    void SnapshotExtractor::UnloadDataFromExtractor()
    {TRACE_IT(45374);
        this->m_marks.Clear();
        this->m_worklist.Clear();

        this->m_idToHandlerMap.Unload();
        this->m_idToTypeMap.Unload();

        this->m_pendingSnap = nullptr;
    }

    SnapshotExtractor::SnapshotExtractor()
        : m_marks(), m_worklist(&HeapAllocator::Instance),
        m_idToHandlerMap(), m_idToTypeMap(),
        m_pendingSnap(nullptr),
        m_snapshotsTakenCount(0),
        m_totalMarkMillis(0.0), m_totalExtractMillis(0.0),
        m_maxMarkMillis(0.0), m_maxExtractMillis(0.0),
        m_lastMarkMillis(0.0), m_lastExtractMillis(0.0)
    {TRACE_IT(45375);
        ;
    }

    SnapshotExtractor::~SnapshotExtractor()
    {TRACE_IT(45376);
        this->UnloadDataFromExtractor();
    }

    SnapShot* SnapshotExtractor::GetPendingSnapshot()
    {TRACE_IT(45377);
        TTDAssert(this->m_pendingSnap != nullptr, "Should only call if we are extracting a snapshot");

        return this->m_pendingSnap;
    }

    SlabAllocator& SnapshotExtractor::GetActiveSnapshotSlabAllocator()
    {TRACE_IT(45378);
        TTDAssert(this->m_pendingSnap != nullptr, "Should only call if we are extracting a snapshot");

        return this->m_pendingSnap->GetSnapshotSlabAllocator();
    }

    void SnapshotExtractor::MarkVisitVar(Js::Var var)
    {TRACE_IT(45379);
        TTDAssert(var != nullptr, "I don't think this should happen but not 100% sure.");
        TTDAssert(Js::JavascriptOperators::GetTypeId(var) < Js::TypeIds_Limit || Js::RecyclableObject::FromVar(var)->CanHaveInterceptors(), "Not cool.");

        //We don't need to visit tagged things
        if(JsSupport::IsVarTaggedInline(var))
        {TRACE_IT(45380);
            return;
        }

        if(JsSupport::IsVarPrimitiveKind(var))
        {TRACE_IT(45381);
            if(this->m_marks.MarkAndTestAddr<MarkTableTag::PrimitiveObjectTag>(var))
            {TRACE_IT(45382);
                Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(var);
                this->MarkVisitType(obj->GetType());
            }
        }
        else
        {TRACE_IT(45383);
            TTDAssert(JsSupport::IsVarComplexKind(var), "Shouldn't be anything else");

            if(this->m_marks.MarkAndTestAddr<MarkTableTag::CompoundObjectTag>(var))
            {TRACE_IT(45384);
                Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(var);

                //do this here instead of in mark visit type as it wants the dynamic object as well
                if(Js::DynamicType::Is(obj->GetTypeId()))
                {TRACE_IT(45385);
                    Js::DynamicObject* dynObj = Js::DynamicObject::FromVar(obj);
                    if(dynObj->GetDynamicType()->GetTypeHandler()->IsDeferredTypeHandler())
                    {TRACE_IT(45386);
                        dynObj->GetDynamicType()->GetTypeHandler()->EnsureObjectReady(dynObj);
                    }
                }
                this->MarkVisitType(obj->GetType());

                this->m_worklist.Enqueue(obj);
            }
        }
    }

    void SnapshotExtractor::MarkFunctionBody(Js::FunctionBody* fb)
    {TRACE_IT(45387);
        if(this->m_marks.MarkAndTestAddr<MarkTableTag::FunctionBodyTag>(fb))
        {TRACE_IT(45388);
            Js::FunctionBody* currfb = fb->GetScriptContext()->TTDContextInfo->ResolveParentBody(fb);

            while(currfb != nullptr && this->m_marks.MarkAndTestAddr<MarkTableTag::FunctionBodyTag>(currfb))
            {TRACE_IT(45389);
                currfb = currfb->GetScriptContext()->TTDContextInfo->ResolveParentBody(currfb);
            }
        }
    }

    void SnapshotExtractor::MarkScriptFunctionScopeInfo(Js::FrameDisplay* environment)
    {TRACE_IT(45390);
        if(this->m_marks.MarkAndTestAddr<MarkTableTag::EnvironmentTag>(environment))
        {TRACE_IT(45391);
            uint32 scopeCount = environment->GetLength();

            for(uint32 i = 0; i < scopeCount; ++i)
            {TRACE_IT(45392);
                void* scope = environment->GetItem(i);

                switch(environment->GetScopeType(scope))
                {
                case Js::ScopeType::ScopeType_ActivationObject:
                case Js::ScopeType::ScopeType_WithScope:
                {TRACE_IT(45393);
                    this->MarkVisitVar((Js::Var)scope);
                    break;
                }
                case Js::ScopeType::ScopeType_SlotArray:
                {TRACE_IT(45394);
                    if(this->m_marks.MarkAndTestAddr<MarkTableTag::SlotArrayTag>(scope))
                    {TRACE_IT(45395);
                        Js::ScopeSlots slotArray = (Js::Var*)scope;
                        uint slotArrayCount = slotArray.GetCount();

                        if(slotArray.IsFunctionScopeSlotArray())
                        {TRACE_IT(45396);
                            this->MarkFunctionBody(slotArray.GetFunctionInfo()->GetFunctionBody());
                        }

                        for(uint j = 0; j < slotArrayCount; j++)
                        {TRACE_IT(45397);
                            Js::Var sval = slotArray.Get(j);
                            this->MarkVisitVar(sval);
                        }
                    }
                    break;
                }
                default:
                    TTDAssert(false, "Unknown scope kind");
                }
            }
        }
    }

    void SnapshotExtractor::BeginSnapshot(ThreadContext* threadContext)
    {TRACE_IT(45398);
        TTDAssert((this->m_pendingSnap == nullptr) & this->m_worklist.Empty(), "Something went wrong.");

        this->m_pendingSnap = TT_HEAP_NEW(SnapShot);
    }

    void SnapshotExtractor::DoMarkWalk(ThreadContext* threadContext)
    {TRACE_IT(45399);
        TTDTimer timer;
        double startTime = timer.Now();

        //Add the special roots
        const JsUtil::List<Js::ScriptContext*, HeapAllocator>& ctxList = threadContext->TTDContext->GetTTDContexts();
        for(int32 i = 0; i < ctxList.Count(); ++i)
        {TRACE_IT(45400);
            Js::ScriptContext* ctx = ctxList.Item(i);
            this->MarkVisitVar(ctx->GetGlobalObject());
            this->MarkVisitVar(ctx->GetLibrary()->GetUndefined());
            this->MarkVisitVar(ctx->GetLibrary()->GetNull());
            this->MarkVisitVar(ctx->GetLibrary()->GetTrue());
            this->MarkVisitVar(ctx->GetLibrary()->GetFalse());
        }

        //Add the global roots
        for(auto iter = threadContext->TTDContext->GetRootSet()->GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(45401);
            Js::Var root = iter.CurrentValue();
            this->MarkVisitVar(root);
        }

        //Add the local roots
        for(auto iter = threadContext->TTDContext->GetLocalRootSet()->GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(45402);
            Js::Var root = iter.CurrentValue();
            this->MarkVisitVar(root);
        }

        while(!this->m_worklist.Empty())
        {TRACE_IT(45403);
            Js::RecyclableObject* nobj = this->m_worklist.Dequeue();
            TTDAssert(JsSupport::IsVarComplexKind(nobj), "Should only be these two options");

            this->MarkVisitStandardProperties(nobj);
            nobj->MarkVisitKindSpecificPtrs(this);
        }

        //Mark all of the well known objects/types
        for(int32 i = 0; i < threadContext->TTDContext->GetTTDContexts().Count(); ++i)
        {TRACE_IT(45404);
            threadContext->TTDContext->GetTTDContexts().Item(i)->TTDWellKnownInfo->MarkWellKnownObjects_TTD(this->m_marks);
        }

        double endTime = timer.Now();
        this->m_pendingSnap->MarkTime = (endTime - startTime) / 1000.0;
    }

    void SnapshotExtractor::EvacuateMarkedIntoSnapshot(ThreadContext* threadContext)
    {TRACE_IT(45405);
        TTDTimer timer;
        double startTime = timer.Now();

        SnapShot* snap = this->m_pendingSnap;
        SlabAllocator& alloc = this->m_pendingSnap->GetSnapshotSlabAllocator();

        //invert the root map for extracting
        JsUtil::BaseDictionary<Js::RecyclableObject*, TTD_LOG_PTR_ID, HeapAllocator> objToLogIdMap(&HeapAllocator::Instance);
        threadContext->TTDContext->LoadInvertedRootMap(objToLogIdMap);

        UnorderedArrayList<NSSnapValues::SnapContext, TTD_ARRAY_LIST_SIZE_XSMALL>& snpCtxs = this->m_pendingSnap->GetContextList();
        for(int32 i = 0; i < threadContext->TTDContext->GetTTDContexts().Count(); ++i)
        {TRACE_IT(45406);
            NSSnapValues::SnapContext* snpCtx = snpCtxs.NextOpenEntry();
            NSSnapValues::ExtractScriptContext(snpCtx, threadContext->TTDContext->GetTTDContexts().Item(i), objToLogIdMap, snap->GetSnapshotSlabAllocator());
        }

        //extract the thread context symbol map info
        JsUtil::BaseDictionary<const char16*, const Js::PropertyRecord*, Recycler>* tcSymbolRegistrationMap = threadContext->GetSymbolRegistrationMap_TTD();
        UnorderedArrayList<Js::PropertyId, TTD_ARRAY_LIST_SIZE_XSMALL>& tcSymbolMapInfo = this->m_pendingSnap->GetTCSymbolMapInfoList();
        for(auto iter = tcSymbolRegistrationMap->GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(45407);
            Js::PropertyId* tcpid = tcSymbolMapInfo.NextOpenEntry();
            *tcpid = iter.CurrentValue()->GetPropertyId();
        }

        //We extract all the global code function bodies with the context so clear their marks now
        JsUtil::List<TopLevelFunctionInContextRelation, HeapAllocator> topLevelScriptLoad(&HeapAllocator::Instance);
        JsUtil::List<TopLevelFunctionInContextRelation, HeapAllocator> topLevelNewFunction(&HeapAllocator::Instance);
        JsUtil::List<TopLevelFunctionInContextRelation, HeapAllocator> topLevelEval(&HeapAllocator::Instance);

        for(int32 i = 0; i < threadContext->TTDContext->GetTTDContexts().Count(); ++i)
        {TRACE_IT(45408);
            topLevelScriptLoad.Clear();
            topLevelNewFunction.Clear();
            topLevelEval.Clear();

            Js::ScriptContext* ctx = threadContext->TTDContext->GetTTDContexts().Item(i);
            ctx->TTDContextInfo->GetLoadedSources(topLevelScriptLoad, topLevelNewFunction, topLevelEval);

            for(int32 j = 0; j < topLevelScriptLoad.Count(); ++j)
            {TRACE_IT(45409);
                Js::FunctionBody* body = TTD_COERCE_PTR_ID_TO_FUNCTIONBODY(topLevelScriptLoad.Item(j).ContextSpecificBodyPtrId);
                this->m_marks.ClearMark(body);
            }

            for(int32 j = 0; j < topLevelNewFunction.Count(); ++j)
            {TRACE_IT(45410);
                Js::FunctionBody* body = TTD_COERCE_PTR_ID_TO_FUNCTIONBODY(topLevelNewFunction.Item(j).ContextSpecificBodyPtrId);
                this->m_marks.ClearMark(body);
            }

            for(int32 j = 0; j < topLevelEval.Count(); ++j)
            {TRACE_IT(45411);
                Js::FunctionBody* body = TTD_COERCE_PTR_ID_TO_FUNCTIONBODY(topLevelEval.Item(j).ContextSpecificBodyPtrId);
                this->m_marks.ClearMark(body);
            }
        }

        this->m_idToHandlerMap.Initialize(this->m_marks.GetCountForTag<MarkTableTag::TypeHandlerTag>());
        this->m_idToTypeMap.Initialize(this->m_marks.GetCountForTag<MarkTableTag::TypeTag>());

        //walk all the marked objects
        this->m_marks.InitializeIter();
        MarkTableTag tag = this->m_marks.GetTagValue();
        while(tag != MarkTableTag::Clear)
        {TRACE_IT(45412);
            switch(tag & MarkTableTag::AllKindMask)
            {
            case MarkTableTag::TypeHandlerTag:
                this->ExtractHandlerIfNeeded(this->m_marks.GetPtrValue<Js::DynamicTypeHandler*>(), threadContext);
                break;
            case MarkTableTag::TypeTag:
                this->ExtractTypeIfNeeded(this->m_marks.GetPtrValue<Js::Type*>(), threadContext);
                break;
            case MarkTableTag::PrimitiveObjectTag:
            {TRACE_IT(45413);
                this->ExtractTypeIfNeeded(this->m_marks.GetPtrValue<Js::RecyclableObject*>()->GetType(), threadContext);
                NSSnapValues::ExtractSnapPrimitiveValue(snap->GetNextAvailablePrimitiveObjectEntry(), this->m_marks.GetPtrValue<Js::RecyclableObject*>(), this->m_marks.GetTagValueIsWellKnown(), this->m_idToTypeMap, alloc);
                break;
            }
            case MarkTableTag::CompoundObjectTag:
            {TRACE_IT(45414);
                this->ExtractTypeIfNeeded(this->m_marks.GetPtrValue<Js::RecyclableObject*>()->GetType(), threadContext);
                if(Js::ScriptFunction::Is(this->m_marks.GetPtrValue<Js::RecyclableObject*>()))
                {TRACE_IT(45415);
                    this->ExtractScriptFunctionEnvironmentIfNeeded(this->m_marks.GetPtrValue<Js::ScriptFunction*>());
                }
                NSSnapObjects::ExtractCompoundObject(snap->GetNextAvailableCompoundObjectEntry(), this->m_marks.GetPtrValue<Js::RecyclableObject*>(), this->m_marks.GetTagValueIsWellKnown(), this->m_idToTypeMap, alloc);
                break;
            }
            case MarkTableTag::FunctionBodyTag:
                NSSnapValues::ExtractFunctionBodyInfo(snap->GetNextAvailableFunctionBodyResolveInfoEntry(), this->m_marks.GetPtrValue<Js::FunctionBody*>(), this->m_marks.GetTagValueIsWellKnown(), alloc);
                break;
            case MarkTableTag::EnvironmentTag:
            case MarkTableTag::SlotArrayTag:
                break; //should be handled with the associated script function
            default:
                TTDAssert(false, "If this isn't true then we have an unknown tag");
                break;
            }

            this->m_marks.MoveToNextAddress();
            tag = this->m_marks.GetTagValue();
        }

        this->ExtractRootInfo(threadContext->TTDContext, objToLogIdMap);

        if(threadContext->TTDContext->GetActiveScriptContext() == nullptr)
        {TRACE_IT(45416);
            this->m_pendingSnap->SetActiveScriptContext(TTD_INVALID_LOG_PTR_ID);
        }
        else
        {TRACE_IT(45417);
            TTD_LOG_PTR_ID ctxId = threadContext->TTDContext->GetActiveScriptContext()->ScriptContextLogTag;
            this->m_pendingSnap->SetActiveScriptContext(ctxId);
        }

        double endTime = timer.Now();
        snap->ExtractTime = (endTime - startTime) / 1000.0;
    }

    SnapShot* SnapshotExtractor::CompleteSnapshot()
    {TRACE_IT(45418);
        SnapShot* snap = this->m_pendingSnap;
        this->UnloadDataFromExtractor();

        this->m_snapshotsTakenCount++;
        this->m_totalMarkMillis += snap->MarkTime;
        this->m_totalExtractMillis += snap->ExtractTime;

        if(this->m_maxMarkMillis < snap->MarkTime)
        {TRACE_IT(45419);
            this->m_maxMarkMillis = snap->MarkTime;
        }

        if(this->m_maxExtractMillis < snap->ExtractTime)
        {TRACE_IT(45420);
            this->m_maxExtractMillis = snap->ExtractTime;
        }

        this->m_lastMarkMillis = snap->MarkTime;
        this->m_lastExtractMillis = snap->ExtractTime;

        return snap;
    }
}

#endif
