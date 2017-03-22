//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimeFunctionBody::JITTimeFunctionBody(FunctionBodyDataIDL * bodyData) :
    m_bodyData(*bodyData)
{LOGMEIN("JITTimeFunctionBody.cpp] 9\n");
    CompileAssert(sizeof(JITTimeFunctionBody) == sizeof(FunctionBodyDataIDL));
}

/* static */
void
JITTimeFunctionBody::InitializeJITFunctionData(
    __in ArenaAllocator * arena,
    __in Js::FunctionBody *functionBody,
    __out FunctionBodyDataIDL * jitBody)
{LOGMEIN("JITTimeFunctionBody.cpp] 19\n");
    Assert(functionBody != nullptr);

    // const table
    jitBody->constCount = functionBody->GetConstantCount();
    if (functionBody->GetConstantCount() > 0)
    {LOGMEIN("JITTimeFunctionBody.cpp] 25\n");
        jitBody->constTable = (intptr_t *)PointerValue(functionBody->GetConstTable());
        if (!functionBody->GetIsAsmJsFunction())
        {LOGMEIN("JITTimeFunctionBody.cpp] 28\n");
            jitBody->constTableContent = AnewStructZ(arena, ConstTableContentIDL);
            jitBody->constTableContent->count = functionBody->GetConstantCount();
            jitBody->constTableContent->content = AnewArrayZ(arena, RecyclableObjectIDL*, functionBody->GetConstantCount());

            for (Js::RegSlot reg = Js::FunctionBody::FirstRegSlot; reg < functionBody->GetConstantCount(); ++reg)
            {LOGMEIN("JITTimeFunctionBody.cpp] 34\n");
                Js::Var varConst = functionBody->GetConstantVar(reg);
                Assert(varConst != nullptr);
                if (Js::TaggedInt::Is(varConst) ||
                    varConst == (Js::Var)&Js::NullFrameDisplay ||
                    varConst == (Js::Var)&Js::StrictNullFrameDisplay)
                {LOGMEIN("JITTimeFunctionBody.cpp] 40\n");
                    // don't need TypeId for these
                }
                else
                {
                    if (Js::TaggedNumber::Is(varConst))
                    {LOGMEIN("JITTimeFunctionBody.cpp] 46\n");
                        // the typeid should be TypeIds_Number, determine this directly from const table
                    }
                    else
                    {
                        jitBody->constTableContent->content[reg - Js::FunctionBody::FirstRegSlot] = (RecyclableObjectIDL*)varConst;
                    }
                }
            }
        }
        else if (functionBody->IsWasmFunction())
        {LOGMEIN("JITTimeFunctionBody.cpp] 57\n");
            // no consts in wasm
            Assert(jitBody->constTable == nullptr);
            jitBody->constCount = 0;
        }
    }

    Js::SmallSpanSequence * statementMap = functionBody->GetStatementMapSpanSequence();

    // REVIEW: OOP JIT, is it possible for this to not match with isJitInDebugMode?
    if (functionBody->IsInDebugMode())
    {LOGMEIN("JITTimeFunctionBody.cpp] 68\n");
        Assert(!statementMap);

        jitBody->byteCodeLength = functionBody->GetOriginalByteCode()->GetLength();
        jitBody->byteCodeBuffer = functionBody->GetOriginalByteCode()->GetBuffer();

        auto fullStatementMaps = functionBody->GetStatementMaps();
        jitBody->fullStatementMapCount = fullStatementMaps->Count();
        jitBody->fullStatementMaps = AnewArrayZ(arena, StatementMapIDL, jitBody->fullStatementMapCount);
        fullStatementMaps->Map([jitBody](int index, Js::FunctionBody::StatementMap * map) {

            jitBody->fullStatementMaps[index] = *(StatementMapIDL*)map;

            Assert(jitBody->fullStatementMaps[index].byteCodeSpanBegin == map->byteCodeSpan.Begin());
            Assert(jitBody->fullStatementMaps[index].byteCodeSpanEnd == map->byteCodeSpan.End());
            Assert(jitBody->fullStatementMaps[index].sourceSpanBegin == map->sourceSpan.Begin());
            Assert(jitBody->fullStatementMaps[index].sourceSpanEnd == map->sourceSpan.End());
            Assert((jitBody->fullStatementMaps[index].isSubExpression != FALSE) == map->isSubexpression);
        });

        Js::PropertyIdOnRegSlotsContainer * propOnRegSlots = functionBody->GetPropertyIdOnRegSlotsContainerWithLock();
        if (propOnRegSlots)
        {LOGMEIN("JITTimeFunctionBody.cpp] 90\n");
            jitBody->propertyIdsForRegSlotsCount = propOnRegSlots->length;
            jitBody->propertyIdsForRegSlots = propOnRegSlots->propertyIdsForRegSlots;
        }
    }
    else
    {
        jitBody->byteCodeLength = functionBody->GetByteCode()->GetLength();
        jitBody->byteCodeBuffer = functionBody->GetByteCode()->GetBuffer();
        if (!functionBody->IsWasmFunction()) {LOGMEIN("JITTimeFunctionBody.cpp] 99\n");
            Assert(statementMap);
            jitBody->statementMap = AnewStructZ(arena, SmallSpanSequenceIDL);
            jitBody->statementMap->baseValue = statementMap->baseValue;

            if (statementMap->pActualOffsetList)
            {LOGMEIN("JITTimeFunctionBody.cpp] 105\n");
                jitBody->statementMap->actualOffsetLength = statementMap->pActualOffsetList->Count();
                jitBody->statementMap->actualOffsetList = statementMap->pActualOffsetList->GetBuffer();
            }

            if (statementMap->pStatementBuffer)
            {LOGMEIN("JITTimeFunctionBody.cpp] 111\n");
                jitBody->statementMap->statementLength = statementMap->pStatementBuffer->Count();
                jitBody->statementMap->statementBuffer = statementMap->pStatementBuffer->GetBuffer();
            }
        }
    }

    jitBody->inlineCacheCount = functionBody->GetInlineCacheCount();
    if (functionBody->GetInlineCacheCount() > 0)
    {LOGMEIN("JITTimeFunctionBody.cpp] 120\n");
        jitBody->cacheIdToPropertyIdMap = functionBody->GetCacheIdToPropertyIdMap();
    }

    jitBody->inlineCaches = reinterpret_cast<intptr_t*>(functionBody->GetInlineCaches());

    // body data
    jitBody->functionBodyAddr = (intptr_t)functionBody;

    jitBody->funcNumber = functionBody->GetFunctionNumber();
    jitBody->sourceContextId = functionBody->GetSourceContextId();
    jitBody->nestedCount = functionBody->GetNestedCount();
    if (functionBody->GetNestedCount() > 0)
    {LOGMEIN("JITTimeFunctionBody.cpp] 133\n");
        jitBody->nestedFuncArrayAddr = (intptr_t)functionBody->GetNestedFuncArray();
    }
    jitBody->scopeSlotArraySize = functionBody->scopeSlotArraySize;
    jitBody->paramScopeSlotArraySize = functionBody->paramScopeSlotArraySize;
    jitBody->attributes = functionBody->GetAttributes();
    jitBody->isInstInlineCacheCount = functionBody->GetIsInstInlineCacheCount();

    jitBody->byteCodeCount = functionBody->GetByteCodeCount();
    jitBody->byteCodeInLoopCount = functionBody->GetByteCodeInLoopCount();
    jitBody->nonLoadByteCodeCount = functionBody->GetByteCodeWithoutLDACount();
    jitBody->loopCount = functionBody->GetLoopCount();

    Js::LoopHeader * loopHeaders = functionBody->GetLoopHeaderArrayWithLock();
    if (loopHeaders != nullptr)
    {LOGMEIN("JITTimeFunctionBody.cpp] 148\n");
        jitBody->loopHeaderArrayAddr = (intptr_t)loopHeaders;
        jitBody->loopHeaderArrayLength = functionBody->GetLoopCount();
        jitBody->loopHeaders = AnewArray(arena, JITLoopHeaderIDL, functionBody->GetLoopCount());
        for (uint i = 0; i < functionBody->GetLoopCount(); ++i)
        {LOGMEIN("JITTimeFunctionBody.cpp] 153\n");
            jitBody->loopHeaders[i].startOffset = loopHeaders[i].startOffset;
            jitBody->loopHeaders[i].endOffset = loopHeaders[i].endOffset;
            jitBody->loopHeaders[i].isNested = loopHeaders[i].isNested;
            jitBody->loopHeaders[i].isInTry = loopHeaders[i].isInTry;
            jitBody->loopHeaders[i].interpretCount = functionBody->GetLoopInterpretCount(&loopHeaders[i]);
        }
    }

    jitBody->localFrameDisplayReg = functionBody->GetLocalFrameDisplayRegister();
    jitBody->localClosureReg = functionBody->GetLocalClosureRegister();
    jitBody->envReg = functionBody->GetEnvRegister();
    jitBody->firstTmpReg = functionBody->GetFirstTmpReg();
    jitBody->varCount = functionBody->GetVarCount();
    jitBody->innerScopeCount = functionBody->GetInnerScopeCount();
    if (functionBody->GetInnerScopeCount() > 0)
    {LOGMEIN("JITTimeFunctionBody.cpp] 169\n");
        jitBody->firstInnerScopeReg = functionBody->GetFirstInnerScopeRegister();
    }
    jitBody->envDepth = functionBody->GetEnvDepth();
    jitBody->profiledCallSiteCount = functionBody->GetProfiledCallSiteCount();
    jitBody->inParamCount = functionBody->GetInParamsCount();
    jitBody->thisRegisterForEventHandler = functionBody->GetThisRegisterForEventHandler();
    jitBody->funcExprScopeRegister = functionBody->GetFuncExprScopeRegister();
    jitBody->recursiveCallSiteCount = functionBody->GetNumberOfRecursiveCallSites();
    jitBody->forInLoopDepth = functionBody->GetForInLoopDepth();
    jitBody->argUsedForBranch = functionBody->m_argUsedForBranch;

    jitBody->flags = functionBody->GetFlags();

    jitBody->doBackendArgumentsOptimization = functionBody->GetDoBackendArgumentsOptimization();
    jitBody->isLibraryCode = functionBody->GetUtf8SourceInfo()->GetIsLibraryCode();
    jitBody->isAsmJsMode = functionBody->GetIsAsmjsMode();
    jitBody->isWasmFunction = functionBody->IsWasmFunction();
    jitBody->isStrictMode = functionBody->GetIsStrictMode();
    jitBody->isEval = functionBody->IsEval();
    jitBody->isGlobalFunc = functionBody->GetIsGlobalFunc();
    jitBody->isInlineApplyDisabled = functionBody->IsInlineApplyDisabled();
    jitBody->doJITLoopBody = functionBody->DoJITLoopBody();
    jitBody->hasScopeObject = functionBody->HasScopeObject();
    jitBody->hasImplicitArgIns = functionBody->GetHasImplicitArgIns();
    jitBody->hasCachedScopePropIds = functionBody->HasCachedScopePropIds();
    jitBody->inlineCachesOnFunctionObject = functionBody->GetInlineCachesOnFunctionObject();
    jitBody->doInterruptProbe = functionBody->GetScriptContext()->GetThreadContext()->DoInterruptProbe(functionBody);
    jitBody->disableInlineSpread = functionBody->IsInlineSpreadDisabled();
    jitBody->hasNestedLoop = functionBody->GetHasNestedLoop();
    jitBody->isParamAndBodyScopeMerged = functionBody->IsParamAndBodyScopeMerged();
    jitBody->paramClosureReg = functionBody->GetParamClosureRegister();
    jitBody->usesArgumentsObject = functionBody->GetUsesArgumentsObject();
    jitBody->doScopeObjectCreation = functionBody->GetDoScopeObjectCreation();
    
    //CompileAssert(sizeof(PropertyIdArrayIDL) == sizeof(Js::PropertyIdArray));
    jitBody->formalsPropIdArray = (PropertyIdArrayIDL*)functionBody->GetFormalsPropIdArray(false);
    jitBody->formalsPropIdArrayAddr = (intptr_t)functionBody->GetFormalsPropIdArray(false);
    jitBody->forInCacheArrayAddr = (intptr_t)functionBody->GetForInCacheArray();

    if (functionBody->HasDynamicProfileInfo() && Js::DynamicProfileInfo::HasCallSiteInfo(functionBody))
    {LOGMEIN("JITTimeFunctionBody.cpp] 210\n");
        jitBody->hasNonBuiltInCallee = functionBody->HasNonBuiltInCallee();
    }

    Js::ByteBlock * auxData = functionBody->GetAuxiliaryDataWithLock();
    if (auxData != nullptr)
    {LOGMEIN("JITTimeFunctionBody.cpp] 216\n");
        jitBody->auxDataCount = auxData->GetLength();
        jitBody->auxData = auxData->GetBuffer();
        jitBody->auxDataBufferAddr = (intptr_t)auxData->GetBuffer();
    }
    Js::ByteBlock * auxContextData = functionBody->GetAuxiliaryContextDataWithLock();
    if (auxContextData != nullptr)
    {LOGMEIN("JITTimeFunctionBody.cpp] 223\n");
        jitBody->auxContextDataCount = auxContextData->GetLength();
        jitBody->auxContextData = auxContextData->GetBuffer();
    }

    jitBody->scriptIdAddr = (intptr_t)functionBody->GetAddressOfScriptId();
    jitBody->flagsAddr = (intptr_t)functionBody->GetAddressOfFlags();
    jitBody->probeCountAddr = (intptr_t)&functionBody->GetSourceInfo()->m_probeCount;
    jitBody->regAllocLoadCountAddr = (intptr_t)&functionBody->regAllocLoadCount;
    jitBody->regAllocStoreCountAddr = (intptr_t)&functionBody->regAllocStoreCount;
    jitBody->callCountStatsAddr = (intptr_t)&functionBody->callCountStats;

    jitBody->referencedPropertyIdCount = functionBody->GetReferencedPropertyIdCount();
    jitBody->referencedPropertyIdMap = functionBody->GetReferencedPropertyIdMapWithLock();
    jitBody->hasFinally = functionBody->GetHasFinally();

    jitBody->nameLength = functionBody->GetDisplayNameLength() + 1; // +1 for null terminator
    jitBody->displayName = (char16 *)functionBody->GetDisplayName();
    jitBody->objectLiteralTypesAddr = (intptr_t)functionBody->GetObjectLiteralTypesWithLock();
    jitBody->literalRegexCount = functionBody->GetLiteralRegexCount();
    jitBody->literalRegexes = (intptr_t*)functionBody->GetLiteralRegexesWithLock();

#ifdef ASMJS_PLAT
    if (functionBody->GetIsAsmJsFunction())
    {LOGMEIN("JITTimeFunctionBody.cpp] 247\n");
        jitBody->asmJsData = Anew(arena, AsmJsDataIDL);
        Js::AsmJsFunctionInfo * asmFuncInfo = functionBody->GetAsmJsFunctionInfoWithLock();
        // 5 is hard coded in JITTypes.h
        CompileAssert(WAsmJs::LIMIT == 5);
        for (int i = 0; i < WAsmJs::LIMIT; ++i)
        {LOGMEIN("JITTimeFunctionBody.cpp] 253\n");
            WAsmJs::Types type = (WAsmJs::Types)i;
            const auto typedInfo = asmFuncInfo->GetTypedSlotInfo(type);
            jitBody->asmJsData->typedSlotInfos[i].byteOffset = typedInfo->byteOffset;
            jitBody->asmJsData->typedSlotInfos[i].constCount = typedInfo->constCount;
            jitBody->asmJsData->typedSlotInfos[i].constSrcByteOffset = typedInfo->constSrcByteOffset;
            jitBody->asmJsData->typedSlotInfos[i].tmpCount = typedInfo->tmpCount;
            jitBody->asmJsData->typedSlotInfos[i].varCount = typedInfo->varCount;
        }
        jitBody->asmJsData->argCount = asmFuncInfo->GetArgCount();
        jitBody->asmJsData->argTypeArray = (byte*)asmFuncInfo->GetArgTypeArray();
        jitBody->asmJsData->argByteSize = asmFuncInfo->GetArgByteSize();
        jitBody->asmJsData->retType = asmFuncInfo->GetReturnType().which();
        jitBody->asmJsData->isHeapBufferConst = asmFuncInfo->IsHeapBufferConst();
        jitBody->asmJsData->usesHeapBuffer = asmFuncInfo->UsesHeapBuffer();
        jitBody->asmJsData->totalSizeInBytes = asmFuncInfo->GetTotalSizeinBytes();

#ifdef ENABLE_WASM
        if (functionBody->IsWasmFunction())
        {LOGMEIN("JITTimeFunctionBody.cpp] 272\n");
            jitBody->asmJsData->wasmSignatureCount = asmFuncInfo->GetWebAssemblyModule()->GetSignatureCount();
            jitBody->asmJsData->wasmSignaturesBaseAddr = (intptr_t)asmFuncInfo->GetWebAssemblyModule()->GetSignatures();
            jitBody->asmJsData->wasmSignatures = (WasmSignatureIDL*)asmFuncInfo->GetWebAssemblyModule()->GetSignatures();
        }
#endif
    }
#endif
}

intptr_t
JITTimeFunctionBody::GetAddr() const
{LOGMEIN("JITTimeFunctionBody.cpp] 284\n");
    return m_bodyData.functionBodyAddr;
}

uint
JITTimeFunctionBody::GetFunctionNumber() const
{LOGMEIN("JITTimeFunctionBody.cpp] 290\n");
    return m_bodyData.funcNumber;
}

uint
JITTimeFunctionBody::GetSourceContextId() const
{LOGMEIN("JITTimeFunctionBody.cpp] 296\n");
    return m_bodyData.sourceContextId;
}

uint
JITTimeFunctionBody::GetNestedCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 302\n");
    return m_bodyData.nestedCount;
}

uint
JITTimeFunctionBody::GetScopeSlotArraySize() const
{LOGMEIN("JITTimeFunctionBody.cpp] 308\n");
    return m_bodyData.scopeSlotArraySize;
}

uint
JITTimeFunctionBody::GetParamScopeSlotArraySize() const
{LOGMEIN("JITTimeFunctionBody.cpp] 314\n");
    return m_bodyData.paramScopeSlotArraySize;
}

uint
JITTimeFunctionBody::GetByteCodeCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 320\n");
    return m_bodyData.byteCodeCount;
}

uint
JITTimeFunctionBody::GetByteCodeInLoopCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 326\n");
    return m_bodyData.byteCodeInLoopCount;
}

uint
JITTimeFunctionBody::GetNonLoadByteCodeCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 332\n");
    return m_bodyData.nonLoadByteCodeCount;
}

uint
JITTimeFunctionBody::GetLoopCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 338\n");
    return m_bodyData.loopCount;
}

bool
JITTimeFunctionBody::HasLoops() const
{LOGMEIN("JITTimeFunctionBody.cpp] 344\n");
    return GetLoopCount() != 0;
}

uint
JITTimeFunctionBody::GetByteCodeLength() const
{LOGMEIN("JITTimeFunctionBody.cpp] 350\n");
    return m_bodyData.byteCodeLength;
}

uint
JITTimeFunctionBody::GetInnerScopeCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 356\n");
    return m_bodyData.innerScopeCount;
}

uint
JITTimeFunctionBody::GetInlineCacheCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 362\n");
    return m_bodyData.inlineCacheCount;
}

uint
JITTimeFunctionBody::GetRecursiveCallSiteCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 368\n");
    return m_bodyData.recursiveCallSiteCount;
}

uint
JITTimeFunctionBody::GetForInLoopDepth() const
{LOGMEIN("JITTimeFunctionBody.cpp] 374\n");
    return m_bodyData.forInLoopDepth;
}

Js::RegSlot
JITTimeFunctionBody::GetLocalFrameDisplayReg() const
{LOGMEIN("JITTimeFunctionBody.cpp] 380\n");
    return static_cast<Js::RegSlot>(m_bodyData.localFrameDisplayReg);
}

Js::RegSlot
JITTimeFunctionBody::GetLocalClosureReg() const
{LOGMEIN("JITTimeFunctionBody.cpp] 386\n");
    return static_cast<Js::RegSlot>(m_bodyData.localClosureReg);
}

Js::RegSlot
JITTimeFunctionBody::GetEnvReg() const
{LOGMEIN("JITTimeFunctionBody.cpp] 392\n");
    return static_cast<Js::RegSlot>(m_bodyData.envReg);
}

Js::RegSlot
JITTimeFunctionBody::GetFirstTmpReg() const
{LOGMEIN("JITTimeFunctionBody.cpp] 398\n");
    return static_cast<Js::RegSlot>(m_bodyData.firstTmpReg);
}

Js::RegSlot
JITTimeFunctionBody::GetFirstInnerScopeReg() const
{LOGMEIN("JITTimeFunctionBody.cpp] 404\n");
    return static_cast<Js::RegSlot>(m_bodyData.firstInnerScopeReg);
}

Js::RegSlot
JITTimeFunctionBody::GetVarCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 410\n");
    return static_cast<Js::RegSlot>(m_bodyData.varCount);
}

Js::RegSlot
JITTimeFunctionBody::GetConstCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 416\n");
    return static_cast<Js::RegSlot>(m_bodyData.constCount);
}

Js::RegSlot
JITTimeFunctionBody::GetLocalsCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 422\n");
    return GetConstCount() + GetVarCount();
}

Js::RegSlot
JITTimeFunctionBody::GetTempCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 428\n");
    return GetLocalsCount() - GetFirstTmpReg();
}

Js::RegSlot
JITTimeFunctionBody::GetFuncExprScopeReg() const
{LOGMEIN("JITTimeFunctionBody.cpp] 434\n");
    return static_cast<Js::RegSlot>(m_bodyData.funcExprScopeRegister);
}

Js::RegSlot
JITTimeFunctionBody::GetThisRegForEventHandler() const
{LOGMEIN("JITTimeFunctionBody.cpp] 440\n");
    return static_cast<Js::RegSlot>(m_bodyData.thisRegisterForEventHandler);
}

Js::RegSlot
JITTimeFunctionBody::GetParamClosureReg() const
{LOGMEIN("JITTimeFunctionBody.cpp] 446\n");
    return static_cast<Js::RegSlot>(m_bodyData.paramClosureReg);
}

Js::RegSlot
JITTimeFunctionBody::GetFirstNonTempLocalIndex() const
{LOGMEIN("JITTimeFunctionBody.cpp] 452\n");
    // First local var starts when the const vars end.
    return GetConstCount();
}

Js::RegSlot
JITTimeFunctionBody::GetEndNonTempLocalIndex() const
{LOGMEIN("JITTimeFunctionBody.cpp] 459\n");
    // It will give the index on which current non temp locals ends, which is a first temp reg.
    return GetFirstTmpReg() != Js::Constants::NoRegister ? GetFirstTmpReg() : GetLocalsCount();
}

Js::RegSlot
JITTimeFunctionBody::GetNonTempLocalVarCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 466\n");
    Assert(GetEndNonTempLocalIndex() >= GetFirstNonTempLocalIndex());
    return GetEndNonTempLocalIndex() - GetFirstNonTempLocalIndex();
}

Js::RegSlot
JITTimeFunctionBody::GetRestParamRegSlot() const
{LOGMEIN("JITTimeFunctionBody.cpp] 473\n");
    Js::RegSlot dstRegSlot = GetConstCount();
    if (HasImplicitArgIns())
    {LOGMEIN("JITTimeFunctionBody.cpp] 476\n");
        dstRegSlot += GetInParamsCount() - 1;
    }
    return dstRegSlot;
}

Js::PropertyId
JITTimeFunctionBody::GetPropertyIdFromCacheId(uint cacheId) const
{LOGMEIN("JITTimeFunctionBody.cpp] 484\n");
    Assert(m_bodyData.cacheIdToPropertyIdMap);
    Assert(cacheId < GetInlineCacheCount());
    return static_cast<Js::PropertyId>(m_bodyData.cacheIdToPropertyIdMap[cacheId]);
}

Js::PropertyId
JITTimeFunctionBody::GetReferencedPropertyId(uint index) const
{LOGMEIN("JITTimeFunctionBody.cpp] 492\n");
    if (index < (uint)TotalNumberOfBuiltInProperties)
    {LOGMEIN("JITTimeFunctionBody.cpp] 494\n");
        return index;
    }
    uint mapIndex = index - TotalNumberOfBuiltInProperties;

    Assert(m_bodyData.referencedPropertyIdMap != nullptr);
    Assert(mapIndex < m_bodyData.referencedPropertyIdCount);

    return m_bodyData.referencedPropertyIdMap[mapIndex];
}

uint16
JITTimeFunctionBody::GetArgUsedForBranch() const
{LOGMEIN("JITTimeFunctionBody.cpp] 507\n");
    return m_bodyData.argUsedForBranch;
}

uint16
JITTimeFunctionBody::GetEnvDepth() const
{LOGMEIN("JITTimeFunctionBody.cpp] 513\n");
    return m_bodyData.envDepth;
}

Js::ProfileId
JITTimeFunctionBody::GetProfiledCallSiteCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 519\n");
    return static_cast<Js::ProfileId>(m_bodyData.profiledCallSiteCount);
}

Js::ArgSlot
JITTimeFunctionBody::GetInParamsCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 525\n");
    return static_cast<Js::ArgSlot>(m_bodyData.inParamCount);
}

bool
JITTimeFunctionBody::DoStackNestedFunc() const
{LOGMEIN("JITTimeFunctionBody.cpp] 531\n");
    return Js::FunctionBody::DoStackNestedFunc(GetFlags());
}

bool
JITTimeFunctionBody::DoStackClosure() const
{LOGMEIN("JITTimeFunctionBody.cpp] 537\n");
    return Js::FunctionBody::DoStackClosure(this);
}

bool
JITTimeFunctionBody::HasTry() const
{LOGMEIN("JITTimeFunctionBody.cpp] 543\n");
    return Js::FunctionBody::GetHasTry(GetFlags());
}

bool
JITTimeFunctionBody::HasThis() const
{LOGMEIN("JITTimeFunctionBody.cpp] 549\n");
    return Js::FunctionBody::GetHasThis(GetFlags());
}

bool
JITTimeFunctionBody::HasFinally() const
{LOGMEIN("JITTimeFunctionBody.cpp] 555\n");
    return m_bodyData.hasFinally != FALSE;
}

bool
JITTimeFunctionBody::HasOrParentHasArguments() const
{LOGMEIN("JITTimeFunctionBody.cpp] 561\n");
    return Js::FunctionBody::GetHasOrParentHasArguments(GetFlags());
}

bool
JITTimeFunctionBody::DoBackendArgumentsOptimization() const
{LOGMEIN("JITTimeFunctionBody.cpp] 567\n");
    return m_bodyData.doBackendArgumentsOptimization != FALSE;
}

bool
JITTimeFunctionBody::IsLibraryCode() const
{LOGMEIN("JITTimeFunctionBody.cpp] 573\n");
    return m_bodyData.isLibraryCode != FALSE;
}

bool
JITTimeFunctionBody::IsAsmJsMode() const
{LOGMEIN("JITTimeFunctionBody.cpp] 579\n");
    return m_bodyData.isAsmJsMode != FALSE;
}

bool
JITTimeFunctionBody::IsWasmFunction() const
{LOGMEIN("JITTimeFunctionBody.cpp] 585\n");
    return m_bodyData.isWasmFunction != FALSE;
}

bool
JITTimeFunctionBody::IsStrictMode() const
{LOGMEIN("JITTimeFunctionBody.cpp] 591\n");
    return m_bodyData.isStrictMode != FALSE;
}

bool
JITTimeFunctionBody::IsEval() const
{LOGMEIN("JITTimeFunctionBody.cpp] 597\n");
    return m_bodyData.isEval != FALSE;
}

bool
JITTimeFunctionBody::HasScopeObject() const
{LOGMEIN("JITTimeFunctionBody.cpp] 603\n");
    return m_bodyData.hasScopeObject != FALSE;
}

bool
JITTimeFunctionBody::HasNestedLoop() const
{LOGMEIN("JITTimeFunctionBody.cpp] 609\n");
    return m_bodyData.hasNestedLoop != FALSE;
}

bool
JITTimeFunctionBody::UsesArgumentsObject() const
{LOGMEIN("JITTimeFunctionBody.cpp] 615\n");
    return m_bodyData.usesArgumentsObject != FALSE;
}

bool
JITTimeFunctionBody::IsParamAndBodyScopeMerged() const
{LOGMEIN("JITTimeFunctionBody.cpp] 621\n");
    return m_bodyData.isParamAndBodyScopeMerged != FALSE;
}

bool
JITTimeFunctionBody::IsCoroutine() const
{LOGMEIN("JITTimeFunctionBody.cpp] 627\n");
    return Js::FunctionInfo::IsCoroutine(GetAttributes());
}

bool
JITTimeFunctionBody::IsGenerator() const
{LOGMEIN("JITTimeFunctionBody.cpp] 633\n");
    return Js::FunctionInfo::IsGenerator(GetAttributes());
}

bool
JITTimeFunctionBody::IsLambda() const
{LOGMEIN("JITTimeFunctionBody.cpp] 639\n");
    return Js::FunctionInfo::IsLambda(GetAttributes());
}

bool
JITTimeFunctionBody::HasImplicitArgIns() const
{LOGMEIN("JITTimeFunctionBody.cpp] 645\n");
    return m_bodyData.hasImplicitArgIns != FALSE;
}

bool
JITTimeFunctionBody::HasCachedScopePropIds() const
{LOGMEIN("JITTimeFunctionBody.cpp] 651\n");
    return m_bodyData.hasCachedScopePropIds != FALSE;
}

bool
JITTimeFunctionBody::HasInlineCachesOnFunctionObject() const
{LOGMEIN("JITTimeFunctionBody.cpp] 657\n");
    return m_bodyData.inlineCachesOnFunctionObject != FALSE;
}

bool
JITTimeFunctionBody::DoInterruptProbe() const
{LOGMEIN("JITTimeFunctionBody.cpp] 663\n");
    // TODO michhol: this is technically a threadcontext flag,
    // may want to pass all these when initializing thread context
    return m_bodyData.doInterruptProbe != FALSE;
}

bool
JITTimeFunctionBody::HasRestParameter() const
{LOGMEIN("JITTimeFunctionBody.cpp] 671\n");
    return Js::FunctionBody::GetHasRestParameter(GetFlags());
}

bool
JITTimeFunctionBody::IsGlobalFunc() const
{LOGMEIN("JITTimeFunctionBody.cpp] 677\n");
    return m_bodyData.isGlobalFunc != FALSE;
}

void
JITTimeFunctionBody::DisableInlineApply() 
{LOGMEIN("JITTimeFunctionBody.cpp] 683\n");
    m_bodyData.isInlineApplyDisabled = TRUE;
}

bool
JITTimeFunctionBody::IsInlineApplyDisabled() const
{LOGMEIN("JITTimeFunctionBody.cpp] 689\n");
    return m_bodyData.isInlineApplyDisabled != FALSE;
}

bool
JITTimeFunctionBody::IsNonTempLocalVar(uint32 varIndex) const
{LOGMEIN("JITTimeFunctionBody.cpp] 695\n");
    return GetFirstNonTempLocalIndex() <= varIndex && varIndex < GetEndNonTempLocalIndex();
}

bool
JITTimeFunctionBody::DoJITLoopBody() const
{LOGMEIN("JITTimeFunctionBody.cpp] 701\n");
    return m_bodyData.doJITLoopBody != FALSE;
}

void
JITTimeFunctionBody::DisableInlineSpread()
{LOGMEIN("JITTimeFunctionBody.cpp] 707\n");
    m_bodyData.disableInlineSpread = TRUE;
}

bool
JITTimeFunctionBody::IsInlineSpreadDisabled() const
{LOGMEIN("JITTimeFunctionBody.cpp] 713\n");
    return m_bodyData.disableInlineSpread != FALSE;
}

bool
JITTimeFunctionBody::HasNonBuiltInCallee() const
{LOGMEIN("JITTimeFunctionBody.cpp] 719\n");
    return m_bodyData.hasNonBuiltInCallee != FALSE;
}

bool
JITTimeFunctionBody::CanInlineRecursively(uint depth, bool tryAggressive) const
{LOGMEIN("JITTimeFunctionBody.cpp] 725\n");
    uint recursiveInlineSpan = GetRecursiveCallSiteCount();

    uint minRecursiveInlineDepth = (uint)CONFIG_FLAG(RecursiveInlineDepthMin);

    if (recursiveInlineSpan != GetProfiledCallSiteCount() || tryAggressive == false)
    {LOGMEIN("JITTimeFunctionBody.cpp] 731\n");
        return depth < minRecursiveInlineDepth;
    }

    uint maxRecursiveInlineDepth = (uint)CONFIG_FLAG(RecursiveInlineDepthMax);
    uint maxRecursiveBytecodeBudget = (uint)CONFIG_FLAG(RecursiveInlineThreshold);
    uint numberOfAllowedFuncs = maxRecursiveBytecodeBudget / GetNonLoadByteCodeCount();
    uint maxDepth;

    if (recursiveInlineSpan == 1)
    {LOGMEIN("JITTimeFunctionBody.cpp] 741\n");
        maxDepth = numberOfAllowedFuncs;
    }
    else
    {
        maxDepth = (uint)ceil(log((double)((double)numberOfAllowedFuncs) / log((double)recursiveInlineSpan)));
    }
    maxDepth = maxDepth < minRecursiveInlineDepth ? minRecursiveInlineDepth : maxDepth;
    maxDepth = maxDepth < maxRecursiveInlineDepth ? maxDepth : maxRecursiveInlineDepth;
    return depth < maxDepth;
}

bool
JITTimeFunctionBody::NeedScopeObjectForArguments(bool hasNonSimpleParams) const
{LOGMEIN("JITTimeFunctionBody.cpp] 755\n");
    // TODO: OOP JIT, enable assert
    //Assert(HasReferenceableBuiltInArguments());
    // We can avoid creating a scope object with arguments present if:
    bool dontNeedScopeObject =
        // Either we are in strict mode, or have strict mode formal semantics from a non-simple parameter list, and
        (IsStrictMode() || hasNonSimpleParams)
        // Neither of the scopes are objects
        && !HasScopeObject();

    return
        // Regardless of the conditions above, we won't need a scope object if there aren't any formals.
        (GetInParamsCount() > 1 || HasRestParameter())
        && !dontNeedScopeObject;
}

bool
JITTimeFunctionBody::GetDoScopeObjectCreation() const
{LOGMEIN("JITTimeFunctionBody.cpp] 773\n");
    return !!m_bodyData.doScopeObjectCreation;
}

const byte *
JITTimeFunctionBody::GetByteCodeBuffer() const
{LOGMEIN("JITTimeFunctionBody.cpp] 779\n");
    return m_bodyData.byteCodeBuffer;
}

StatementMapIDL *
JITTimeFunctionBody::GetFullStatementMap() const
{LOGMEIN("JITTimeFunctionBody.cpp] 785\n");
    return m_bodyData.fullStatementMaps;
}

uint
JITTimeFunctionBody::GetFullStatementMapCount() const
{LOGMEIN("JITTimeFunctionBody.cpp] 791\n");
    return m_bodyData.fullStatementMapCount;
}

intptr_t
JITTimeFunctionBody::GetScriptIdAddr() const
{LOGMEIN("JITTimeFunctionBody.cpp] 797\n");
    return m_bodyData.scriptIdAddr;
}

intptr_t
JITTimeFunctionBody::GetProbeCountAddr() const
{LOGMEIN("JITTimeFunctionBody.cpp] 803\n");
    return m_bodyData.probeCountAddr;
}

intptr_t
JITTimeFunctionBody::GetFlagsAddr() const
{LOGMEIN("JITTimeFunctionBody.cpp] 809\n");
    return m_bodyData.flagsAddr;
}

intptr_t
JITTimeFunctionBody::GetRegAllocLoadCountAddr() const
{LOGMEIN("JITTimeFunctionBody.cpp] 815\n");
    return m_bodyData.regAllocLoadCountAddr;
}

intptr_t
JITTimeFunctionBody::GetFormalsPropIdArrayAddr() const
{LOGMEIN("JITTimeFunctionBody.cpp] 821\n");
    return m_bodyData.formalsPropIdArrayAddr;
}

intptr_t
JITTimeFunctionBody::GetRegAllocStoreCountAddr() const
{LOGMEIN("JITTimeFunctionBody.cpp] 827\n");
    return m_bodyData.regAllocStoreCountAddr;
}

intptr_t
JITTimeFunctionBody::GetCallCountStatsAddr() const
{LOGMEIN("JITTimeFunctionBody.cpp] 833\n");
    return m_bodyData.callCountStatsAddr;
}

intptr_t
JITTimeFunctionBody::GetObjectLiteralTypeRef(uint index) const
{LOGMEIN("JITTimeFunctionBody.cpp] 839\n");
    Assert(m_bodyData.objectLiteralTypesAddr != 0);
    return m_bodyData.objectLiteralTypesAddr + index * MachPtr;
}

intptr_t
JITTimeFunctionBody::GetConstantVar(Js::RegSlot location) const
{LOGMEIN("JITTimeFunctionBody.cpp] 846\n");
    Assert(m_bodyData.constTable != nullptr);
    Assert(location < GetConstCount());
    Assert(location != 0);

    return static_cast<intptr_t>(m_bodyData.constTable[location - Js::FunctionBody::FirstRegSlot]);
}

JITRecyclableObject *
JITTimeFunctionBody::GetConstantContent(Js::RegSlot location) const
{LOGMEIN("JITTimeFunctionBody.cpp] 856\n");
    Assert(m_bodyData.constTableContent != nullptr);
    Assert(m_bodyData.constTableContent->content != nullptr);
    Assert(location < GetConstCount());
    Assert(location != 0);

    JITRecyclableObject * obj = (JITRecyclableObject *)m_bodyData.constTableContent->content[location - Js::FunctionBody::FirstRegSlot];
    Assert(obj);
    return obj;
}

intptr_t
JITTimeFunctionBody::GetInlineCache(uint index) const
{LOGMEIN("JITTimeFunctionBody.cpp] 869\n");
    Assert(m_bodyData.inlineCaches != nullptr);
    Assert(index < GetInlineCacheCount());
#if 0 // TODO: michhol OOP JIT, add these asserts
    Assert(this->m_inlineCacheTypes[index] == InlineCacheTypeNone ||
        this->m_inlineCacheTypes[index] == InlineCacheTypeInlineCache);
    this->m_inlineCacheTypes[index] = InlineCacheTypeInlineCache;
#endif
    return static_cast<intptr_t>(m_bodyData.inlineCaches[index]);
}

intptr_t
JITTimeFunctionBody::GetIsInstInlineCache(uint index) const
{LOGMEIN("JITTimeFunctionBody.cpp] 882\n");
    Assert(m_bodyData.inlineCaches != nullptr);
    Assert(index < m_bodyData.isInstInlineCacheCount);
    index += GetInlineCacheCount();
#if 0 // TODO: michhol OOP JIT, add these asserts
    Assert(this->m_inlineCacheTypes[index] == InlineCacheTypeNone ||
        this->m_inlineCacheTypes[index] == InlineCacheTypeIsInst);
    this->m_inlineCacheTypes[index] = InlineCacheTypeIsInst;
#endif
    return static_cast<intptr_t>(m_bodyData.inlineCaches[index]);
}

Js::TypeId
JITTimeFunctionBody::GetConstantType(Js::RegSlot location) const
{LOGMEIN("JITTimeFunctionBody.cpp] 896\n");
    Assert(m_bodyData.constTable != nullptr);
    Assert(m_bodyData.constTableContent != nullptr);
    Assert(location < GetConstCount());
    Assert(location != 0);
    auto obj = m_bodyData.constTableContent->content[location - Js::FunctionBody::FirstRegSlot];

    if (obj == nullptr)
    {LOGMEIN("JITTimeFunctionBody.cpp] 904\n");
        if (Js::TaggedNumber::Is((Js::Var)GetConstantVar(location)))
        {LOGMEIN("JITTimeFunctionBody.cpp] 906\n");
            // tagged float
            return Js::TypeId::TypeIds_Number;
        }
        else
        {
            return Js::TypeId::TypeIds_Limit;
        }
    }

    return static_cast<Js::TypeId>(*(obj->typeId));
}

intptr_t
JITTimeFunctionBody::GetLiteralRegexAddr(uint index) const
{LOGMEIN("JITTimeFunctionBody.cpp] 921\n");
    Assert(index < m_bodyData.literalRegexCount);

    return m_bodyData.literalRegexes[index];
}

void *
JITTimeFunctionBody::GetConstTable() const
{LOGMEIN("JITTimeFunctionBody.cpp] 929\n");
    return m_bodyData.constTable;
}

bool
JITTimeFunctionBody::IsConstRegPropertyString(Js::RegSlot reg, ScriptContextInfo * context) const
{LOGMEIN("JITTimeFunctionBody.cpp] 935\n");
    RecyclableObjectIDL * content = m_bodyData.constTableContent->content[reg - Js::FunctionBody::FirstRegSlot];
    if (content != nullptr && content->vtbl == context->GetVTableAddress(VtablePropertyString))
    {LOGMEIN("JITTimeFunctionBody.cpp] 938\n");
        return true;
    }
    return false;
}

intptr_t
JITTimeFunctionBody::GetRootObject() const
{LOGMEIN("JITTimeFunctionBody.cpp] 946\n");
    Assert(m_bodyData.constTable != nullptr);
    return m_bodyData.constTable[Js::FunctionBody::RootObjectRegSlot - Js::FunctionBody::FirstRegSlot];
}

Js::FunctionInfoPtrPtr
JITTimeFunctionBody::GetNestedFuncRef(uint index) const
{LOGMEIN("JITTimeFunctionBody.cpp] 953\n");
    Assert(index < GetNestedCount());
    Js::FunctionInfoPtrPtr baseAddr = (Js::FunctionInfoPtrPtr)m_bodyData.nestedFuncArrayAddr;
    return baseAddr + index;
}

intptr_t
JITTimeFunctionBody::GetLoopHeaderAddr(uint loopNum) const
{LOGMEIN("JITTimeFunctionBody.cpp] 961\n");
    Assert(loopNum < GetLoopCount());
    intptr_t baseAddr = m_bodyData.loopHeaderArrayAddr;
    return baseAddr + (loopNum * sizeof(Js::LoopHeader));
}

const JITLoopHeaderIDL *
JITTimeFunctionBody::GetLoopHeaderData(uint loopNum) const
{LOGMEIN("JITTimeFunctionBody.cpp] 969\n");
    Assert(loopNum < GetLoopCount());
    return &m_bodyData.loopHeaders[loopNum];
}

const AsmJsJITInfo *
JITTimeFunctionBody::GetAsmJsInfo() const
{LOGMEIN("JITTimeFunctionBody.cpp] 976\n");
    return reinterpret_cast<const AsmJsJITInfo *>(m_bodyData.asmJsData);
}

JITTimeProfileInfo *
JITTimeFunctionBody::GetProfileInfo() const
{LOGMEIN("JITTimeFunctionBody.cpp] 982\n");
    return reinterpret_cast<JITTimeProfileInfo *>(m_bodyData.profileData);
}

const JITTimeProfileInfo *
JITTimeFunctionBody::GetReadOnlyProfileInfo() const
{LOGMEIN("JITTimeFunctionBody.cpp] 988\n");
    return reinterpret_cast<const JITTimeProfileInfo *>(m_bodyData.profileData);
}

bool
JITTimeFunctionBody::HasProfileInfo() const
{LOGMEIN("JITTimeFunctionBody.cpp] 994\n");
    return m_bodyData.profileData != nullptr;
}

bool
JITTimeFunctionBody::HasPropIdToFormalsMap() const
{LOGMEIN("JITTimeFunctionBody.cpp] 1000\n");
    return m_bodyData.propertyIdsForRegSlotsCount > 0 && GetFormalsPropIdArray() != nullptr;
}

bool
JITTimeFunctionBody::IsRegSlotFormal(Js::RegSlot reg) const
{LOGMEIN("JITTimeFunctionBody.cpp] 1006\n");
    Assert(reg < m_bodyData.propertyIdsForRegSlotsCount);
    Js::PropertyId propId = (Js::PropertyId)m_bodyData.propertyIdsForRegSlots[reg];
    Js::PropertyIdArray * formalProps = GetFormalsPropIdArray();
    for (uint32 i = 0; i < formalProps->count; i++)
    {LOGMEIN("JITTimeFunctionBody.cpp] 1011\n");
        if (formalProps->elements[i] == propId)
        {LOGMEIN("JITTimeFunctionBody.cpp] 1013\n");
            return true;
        }
    }
    return false;
}

/* static */
bool
JITTimeFunctionBody::LoopContains(const JITLoopHeaderIDL * loop1, const JITLoopHeaderIDL * loop2)
{LOGMEIN("JITTimeFunctionBody.cpp] 1023\n");
    return (loop1->startOffset <= loop2->startOffset && loop2->endOffset <= loop1->endOffset);
}

Js::FunctionBody::FunctionBodyFlags
JITTimeFunctionBody::GetFlags() const
{LOGMEIN("JITTimeFunctionBody.cpp] 1029\n");
    return static_cast<Js::FunctionBody::FunctionBodyFlags>(m_bodyData.flags);
}

Js::FunctionInfo::Attributes
JITTimeFunctionBody::GetAttributes() const
{LOGMEIN("JITTimeFunctionBody.cpp] 1035\n");
    return static_cast<Js::FunctionInfo::Attributes>(m_bodyData.attributes);
}

intptr_t
JITTimeFunctionBody::GetAuxDataAddr(uint offset) const
{LOGMEIN("JITTimeFunctionBody.cpp] 1041\n");
    return m_bodyData.auxDataBufferAddr + offset;
}

void *
JITTimeFunctionBody::ReadFromAuxData(uint offset) const
{LOGMEIN("JITTimeFunctionBody.cpp] 1047\n");
    return (void *)(m_bodyData.auxData + offset);
}

void *
JITTimeFunctionBody::ReadFromAuxContextData(uint offset) const
{LOGMEIN("JITTimeFunctionBody.cpp] 1053\n");
    return (void *)(m_bodyData.auxContextData + offset);
}

const Js::PropertyIdArray *
JITTimeFunctionBody::ReadPropertyIdArrayFromAuxData(uint offset) const
{LOGMEIN("JITTimeFunctionBody.cpp] 1059\n");
    Js::PropertyIdArray * auxArray = (Js::PropertyIdArray *)(m_bodyData.auxData + offset);
    Assert(offset + auxArray->GetDataSize() <= m_bodyData.auxDataCount);
    return auxArray;
}

Js::PropertyIdArray *
JITTimeFunctionBody::GetFormalsPropIdArray() const
{LOGMEIN("JITTimeFunctionBody.cpp] 1067\n");
    return  (Js::PropertyIdArray *)m_bodyData.formalsPropIdArray;
}

Js::ForInCache *
JITTimeFunctionBody::GetForInCache(uint profileId) const
{LOGMEIN("JITTimeFunctionBody.cpp] 1073\n");
    return  &((Js::ForInCache *)m_bodyData.forInCacheArrayAddr)[profileId];
}

bool
JITTimeFunctionBody::InitializeStatementMap(Js::SmallSpanSequence * statementMap, ArenaAllocator* alloc) const
{LOGMEIN("JITTimeFunctionBody.cpp] 1079\n");
    if (!m_bodyData.statementMap)
    {LOGMEIN("JITTimeFunctionBody.cpp] 1081\n");
        return false;
    }
    const uint statementsLength = m_bodyData.statementMap->statementLength;
    const uint offsetsLength = m_bodyData.statementMap->actualOffsetLength;

    statementMap->baseValue = m_bodyData.statementMap->baseValue;

    // TODO: (leish OOP JIT) using arena to prevent memory leak, fix to really implement GrowingUint32ArenaArray::Create()
    // or find other way to reuse like michhol's comments
    typedef JsUtil::GrowingArray<uint32, ArenaAllocator> GrowingUint32ArenaArray;

    if (statementsLength > 0)
    {LOGMEIN("JITTimeFunctionBody.cpp] 1094\n");
        // TODO: (michhol OOP JIT) should be able to directly use statementMap.statementBuffer        
        statementMap->pStatementBuffer = (JsUtil::GrowingUint32HeapArray*)Anew(alloc, GrowingUint32ArenaArray, alloc, statementsLength);
        statementMap->pStatementBuffer->SetCount(statementsLength);
        js_memcpy_s(
            statementMap->pStatementBuffer->GetBuffer(),
            statementMap->pStatementBuffer->Count() * sizeof(uint32),
            m_bodyData.statementMap->statementBuffer,
            statementsLength * sizeof(uint32));
    }

    if (offsetsLength > 0)
    {LOGMEIN("JITTimeFunctionBody.cpp] 1106\n");
        statementMap->pActualOffsetList = (JsUtil::GrowingUint32HeapArray*)Anew(alloc, GrowingUint32ArenaArray, alloc, offsetsLength);
        statementMap->pActualOffsetList->SetCount(offsetsLength);
        js_memcpy_s(
            statementMap->pActualOffsetList->GetBuffer(),
            statementMap->pActualOffsetList->Count() * sizeof(uint32),
            m_bodyData.statementMap->actualOffsetList,
            offsetsLength * sizeof(uint32));
    }
    return true;
}

char16*
JITTimeFunctionBody::GetDisplayName() const
{LOGMEIN("JITTimeFunctionBody.cpp] 1120\n");
    return m_bodyData.displayName;
}
