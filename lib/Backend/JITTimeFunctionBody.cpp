//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimeFunctionBody::JITTimeFunctionBody(FunctionBodyDataIDL * bodyData) :
    m_bodyData(*bodyData)
{TRACE_IT(9780);
    CompileAssert(sizeof(JITTimeFunctionBody) == sizeof(FunctionBodyDataIDL));
}

/* static */
void
JITTimeFunctionBody::InitializeJITFunctionData(
    __in ArenaAllocator * arena,
    __in Js::FunctionBody *functionBody,
    __out FunctionBodyDataIDL * jitBody)
{TRACE_IT(9781);
    Assert(functionBody != nullptr);

    // const table
    jitBody->constCount = functionBody->GetConstantCount();
    if (functionBody->GetConstantCount() > 0)
    {TRACE_IT(9782);
        jitBody->constTable = (intptr_t *)PointerValue(functionBody->GetConstTable());
        if (!functionBody->GetIsAsmJsFunction())
        {TRACE_IT(9783);
            jitBody->constTableContent = AnewStructZ(arena, ConstTableContentIDL);
            jitBody->constTableContent->count = functionBody->GetConstantCount();
            jitBody->constTableContent->content = AnewArrayZ(arena, RecyclableObjectIDL*, functionBody->GetConstantCount());

            for (Js::RegSlot reg = Js::FunctionBody::FirstRegSlot; reg < functionBody->GetConstantCount(); ++reg)
            {TRACE_IT(9784);
                Js::Var varConst = functionBody->GetConstantVar(reg);
                Assert(varConst != nullptr);
                if (Js::TaggedInt::Is(varConst) ||
                    varConst == (Js::Var)&Js::NullFrameDisplay ||
                    varConst == (Js::Var)&Js::StrictNullFrameDisplay)
                {TRACE_IT(9785);
                    // don't need TypeId for these
                }
                else
                {TRACE_IT(9786);
                    if (Js::TaggedNumber::Is(varConst))
                    {TRACE_IT(9787);
                        // the typeid should be TypeIds_Number, determine this directly from const table
                    }
                    else
                    {TRACE_IT(9788);
                        jitBody->constTableContent->content[reg - Js::FunctionBody::FirstRegSlot] = (RecyclableObjectIDL*)varConst;
                    }
                }
            }
        }
        else if (functionBody->IsWasmFunction())
        {TRACE_IT(9789);
            // no consts in wasm
            Assert(jitBody->constTable == nullptr);
            jitBody->constCount = 0;
        }
    }

    Js::SmallSpanSequence * statementMap = functionBody->GetStatementMapSpanSequence();

    // REVIEW: OOP JIT, is it possible for this to not match with isJitInDebugMode?
    if (functionBody->IsInDebugMode())
    {TRACE_IT(9790);
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
        {TRACE_IT(9791);
            jitBody->propertyIdsForRegSlotsCount = propOnRegSlots->length;
            jitBody->propertyIdsForRegSlots = propOnRegSlots->propertyIdsForRegSlots;
        }
    }
    else
    {TRACE_IT(9792);
        jitBody->byteCodeLength = functionBody->GetByteCode()->GetLength();
        jitBody->byteCodeBuffer = functionBody->GetByteCode()->GetBuffer();
        if (!functionBody->IsWasmFunction()) {TRACE_IT(9793);
            Assert(statementMap);
            jitBody->statementMap = AnewStructZ(arena, SmallSpanSequenceIDL);
            jitBody->statementMap->baseValue = statementMap->baseValue;

            if (statementMap->pActualOffsetList)
            {TRACE_IT(9794);
                jitBody->statementMap->actualOffsetLength = statementMap->pActualOffsetList->Count();
                jitBody->statementMap->actualOffsetList = statementMap->pActualOffsetList->GetBuffer();
            }

            if (statementMap->pStatementBuffer)
            {TRACE_IT(9795);
                jitBody->statementMap->statementLength = statementMap->pStatementBuffer->Count();
                jitBody->statementMap->statementBuffer = statementMap->pStatementBuffer->GetBuffer();
            }
        }
    }

    jitBody->inlineCacheCount = functionBody->GetInlineCacheCount();
    if (functionBody->GetInlineCacheCount() > 0)
    {TRACE_IT(9796);
        jitBody->cacheIdToPropertyIdMap = functionBody->GetCacheIdToPropertyIdMap();
    }

    jitBody->inlineCaches = reinterpret_cast<intptr_t*>(functionBody->GetInlineCaches());

    // body data
    jitBody->functionBodyAddr = (intptr_t)functionBody;

    jitBody->funcNumber = functionBody->GetFunctionNumber();
    jitBody->sourceContextId = functionBody->GetSourceContextId();
    jitBody->nestedCount = functionBody->GetNestedCount();
    if (functionBody->GetNestedCount() > 0)
    {TRACE_IT(9797);
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
    {TRACE_IT(9798);
        jitBody->loopHeaderArrayAddr = (intptr_t)loopHeaders;
        jitBody->loopHeaderArrayLength = functionBody->GetLoopCount();
        jitBody->loopHeaders = AnewArray(arena, JITLoopHeaderIDL, functionBody->GetLoopCount());
        for (uint i = 0; i < functionBody->GetLoopCount(); ++i)
        {TRACE_IT(9799);
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
    {TRACE_IT(9800);
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
    {TRACE_IT(9801);
        jitBody->hasNonBuiltInCallee = functionBody->HasNonBuiltInCallee();
    }

    Js::ByteBlock * auxData = functionBody->GetAuxiliaryDataWithLock();
    if (auxData != nullptr)
    {TRACE_IT(9802);
        jitBody->auxDataCount = auxData->GetLength();
        jitBody->auxData = auxData->GetBuffer();
        jitBody->auxDataBufferAddr = (intptr_t)auxData->GetBuffer();
    }
    Js::ByteBlock * auxContextData = functionBody->GetAuxiliaryContextDataWithLock();
    if (auxContextData != nullptr)
    {TRACE_IT(9803);
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
    {TRACE_IT(9804);
        jitBody->asmJsData = Anew(arena, AsmJsDataIDL);
        Js::AsmJsFunctionInfo * asmFuncInfo = functionBody->GetAsmJsFunctionInfoWithLock();
        // 5 is hard coded in JITTypes.h
        CompileAssert(WAsmJs::LIMIT == 5);
        for (int i = 0; i < WAsmJs::LIMIT; ++i)
        {TRACE_IT(9805);
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
        {TRACE_IT(9806);
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
{TRACE_IT(9807);
    return m_bodyData.functionBodyAddr;
}

uint
JITTimeFunctionBody::GetFunctionNumber() const
{TRACE_IT(9808);
    return m_bodyData.funcNumber;
}

uint
JITTimeFunctionBody::GetSourceContextId() const
{TRACE_IT(9809);
    return m_bodyData.sourceContextId;
}

uint
JITTimeFunctionBody::GetNestedCount() const
{TRACE_IT(9810);
    return m_bodyData.nestedCount;
}

uint
JITTimeFunctionBody::GetScopeSlotArraySize() const
{TRACE_IT(9811);
    return m_bodyData.scopeSlotArraySize;
}

uint
JITTimeFunctionBody::GetParamScopeSlotArraySize() const
{TRACE_IT(9812);
    return m_bodyData.paramScopeSlotArraySize;
}

uint
JITTimeFunctionBody::GetByteCodeCount() const
{TRACE_IT(9813);
    return m_bodyData.byteCodeCount;
}

uint
JITTimeFunctionBody::GetByteCodeInLoopCount() const
{TRACE_IT(9814);
    return m_bodyData.byteCodeInLoopCount;
}

uint
JITTimeFunctionBody::GetNonLoadByteCodeCount() const
{TRACE_IT(9815);
    return m_bodyData.nonLoadByteCodeCount;
}

uint
JITTimeFunctionBody::GetLoopCount() const
{TRACE_IT(9816);
    return m_bodyData.loopCount;
}

bool
JITTimeFunctionBody::HasLoops() const
{TRACE_IT(9817);
    return GetLoopCount() != 0;
}

uint
JITTimeFunctionBody::GetByteCodeLength() const
{TRACE_IT(9818);
    return m_bodyData.byteCodeLength;
}

uint
JITTimeFunctionBody::GetInnerScopeCount() const
{TRACE_IT(9819);
    return m_bodyData.innerScopeCount;
}

uint
JITTimeFunctionBody::GetInlineCacheCount() const
{TRACE_IT(9820);
    return m_bodyData.inlineCacheCount;
}

uint
JITTimeFunctionBody::GetRecursiveCallSiteCount() const
{TRACE_IT(9821);
    return m_bodyData.recursiveCallSiteCount;
}

uint
JITTimeFunctionBody::GetForInLoopDepth() const
{TRACE_IT(9822);
    return m_bodyData.forInLoopDepth;
}

Js::RegSlot
JITTimeFunctionBody::GetLocalFrameDisplayReg() const
{TRACE_IT(9823);
    return static_cast<Js::RegSlot>(m_bodyData.localFrameDisplayReg);
}

Js::RegSlot
JITTimeFunctionBody::GetLocalClosureReg() const
{TRACE_IT(9824);
    return static_cast<Js::RegSlot>(m_bodyData.localClosureReg);
}

Js::RegSlot
JITTimeFunctionBody::GetEnvReg() const
{TRACE_IT(9825);
    return static_cast<Js::RegSlot>(m_bodyData.envReg);
}

Js::RegSlot
JITTimeFunctionBody::GetFirstTmpReg() const
{TRACE_IT(9826);
    return static_cast<Js::RegSlot>(m_bodyData.firstTmpReg);
}

Js::RegSlot
JITTimeFunctionBody::GetFirstInnerScopeReg() const
{TRACE_IT(9827);
    return static_cast<Js::RegSlot>(m_bodyData.firstInnerScopeReg);
}

Js::RegSlot
JITTimeFunctionBody::GetVarCount() const
{TRACE_IT(9828);
    return static_cast<Js::RegSlot>(m_bodyData.varCount);
}

Js::RegSlot
JITTimeFunctionBody::GetConstCount() const
{TRACE_IT(9829);
    return static_cast<Js::RegSlot>(m_bodyData.constCount);
}

Js::RegSlot
JITTimeFunctionBody::GetLocalsCount() const
{TRACE_IT(9830);
    return GetConstCount() + GetVarCount();
}

Js::RegSlot
JITTimeFunctionBody::GetTempCount() const
{TRACE_IT(9831);
    return GetLocalsCount() - GetFirstTmpReg();
}

Js::RegSlot
JITTimeFunctionBody::GetFuncExprScopeReg() const
{TRACE_IT(9832);
    return static_cast<Js::RegSlot>(m_bodyData.funcExprScopeRegister);
}

Js::RegSlot
JITTimeFunctionBody::GetThisRegForEventHandler() const
{TRACE_IT(9833);
    return static_cast<Js::RegSlot>(m_bodyData.thisRegisterForEventHandler);
}

Js::RegSlot
JITTimeFunctionBody::GetParamClosureReg() const
{TRACE_IT(9834);
    return static_cast<Js::RegSlot>(m_bodyData.paramClosureReg);
}

Js::RegSlot
JITTimeFunctionBody::GetFirstNonTempLocalIndex() const
{TRACE_IT(9835);
    // First local var starts when the const vars end.
    return GetConstCount();
}

Js::RegSlot
JITTimeFunctionBody::GetEndNonTempLocalIndex() const
{TRACE_IT(9836);
    // It will give the index on which current non temp locals ends, which is a first temp reg.
    return GetFirstTmpReg() != Js::Constants::NoRegister ? GetFirstTmpReg() : GetLocalsCount();
}

Js::RegSlot
JITTimeFunctionBody::GetNonTempLocalVarCount() const
{TRACE_IT(9837);
    Assert(GetEndNonTempLocalIndex() >= GetFirstNonTempLocalIndex());
    return GetEndNonTempLocalIndex() - GetFirstNonTempLocalIndex();
}

Js::RegSlot
JITTimeFunctionBody::GetRestParamRegSlot() const
{TRACE_IT(9838);
    Js::RegSlot dstRegSlot = GetConstCount();
    if (HasImplicitArgIns())
    {TRACE_IT(9839);
        dstRegSlot += GetInParamsCount() - 1;
    }
    return dstRegSlot;
}

Js::PropertyId
JITTimeFunctionBody::GetPropertyIdFromCacheId(uint cacheId) const
{TRACE_IT(9840);
    Assert(m_bodyData.cacheIdToPropertyIdMap);
    Assert(cacheId < GetInlineCacheCount());
    return static_cast<Js::PropertyId>(m_bodyData.cacheIdToPropertyIdMap[cacheId]);
}

Js::PropertyId
JITTimeFunctionBody::GetReferencedPropertyId(uint index) const
{TRACE_IT(9841);
    if (index < (uint)TotalNumberOfBuiltInProperties)
    {TRACE_IT(9842);
        return index;
    }
    uint mapIndex = index - TotalNumberOfBuiltInProperties;

    Assert(m_bodyData.referencedPropertyIdMap != nullptr);
    Assert(mapIndex < m_bodyData.referencedPropertyIdCount);

    return m_bodyData.referencedPropertyIdMap[mapIndex];
}

uint16
JITTimeFunctionBody::GetArgUsedForBranch() const
{TRACE_IT(9843);
    return m_bodyData.argUsedForBranch;
}

uint16
JITTimeFunctionBody::GetEnvDepth() const
{TRACE_IT(9844);
    return m_bodyData.envDepth;
}

Js::ProfileId
JITTimeFunctionBody::GetProfiledCallSiteCount() const
{TRACE_IT(9845);
    return static_cast<Js::ProfileId>(m_bodyData.profiledCallSiteCount);
}

Js::ArgSlot
JITTimeFunctionBody::GetInParamsCount() const
{TRACE_IT(9846);
    return static_cast<Js::ArgSlot>(m_bodyData.inParamCount);
}

bool
JITTimeFunctionBody::DoStackNestedFunc() const
{TRACE_IT(9847);
    return Js::FunctionBody::DoStackNestedFunc(GetFlags());
}

bool
JITTimeFunctionBody::DoStackClosure() const
{TRACE_IT(9848);
    return Js::FunctionBody::DoStackClosure(this);
}

bool
JITTimeFunctionBody::HasTry() const
{TRACE_IT(9849);
    return Js::FunctionBody::GetHasTry(GetFlags());
}

bool
JITTimeFunctionBody::HasThis() const
{TRACE_IT(9850);
    return Js::FunctionBody::GetHasThis(GetFlags());
}

bool
JITTimeFunctionBody::HasFinally() const
{TRACE_IT(9851);
    return m_bodyData.hasFinally != FALSE;
}

bool
JITTimeFunctionBody::HasOrParentHasArguments() const
{TRACE_IT(9852);
    return Js::FunctionBody::GetHasOrParentHasArguments(GetFlags());
}

bool
JITTimeFunctionBody::DoBackendArgumentsOptimization() const
{TRACE_IT(9853);
    return m_bodyData.doBackendArgumentsOptimization != FALSE;
}

bool
JITTimeFunctionBody::IsLibraryCode() const
{TRACE_IT(9854);
    return m_bodyData.isLibraryCode != FALSE;
}

bool
JITTimeFunctionBody::IsAsmJsMode() const
{TRACE_IT(9855);
    return m_bodyData.isAsmJsMode != FALSE;
}

bool
JITTimeFunctionBody::IsWasmFunction() const
{TRACE_IT(9856);
    return m_bodyData.isWasmFunction != FALSE;
}

bool
JITTimeFunctionBody::IsStrictMode() const
{TRACE_IT(9857);
    return m_bodyData.isStrictMode != FALSE;
}

bool
JITTimeFunctionBody::IsEval() const
{TRACE_IT(9858);
    return m_bodyData.isEval != FALSE;
}

bool
JITTimeFunctionBody::HasScopeObject() const
{TRACE_IT(9859);
    return m_bodyData.hasScopeObject != FALSE;
}

bool
JITTimeFunctionBody::HasNestedLoop() const
{TRACE_IT(9860);
    return m_bodyData.hasNestedLoop != FALSE;
}

bool
JITTimeFunctionBody::UsesArgumentsObject() const
{TRACE_IT(9861);
    return m_bodyData.usesArgumentsObject != FALSE;
}

bool
JITTimeFunctionBody::IsParamAndBodyScopeMerged() const
{TRACE_IT(9862);
    return m_bodyData.isParamAndBodyScopeMerged != FALSE;
}

bool
JITTimeFunctionBody::IsCoroutine() const
{TRACE_IT(9863);
    return Js::FunctionInfo::IsCoroutine(GetAttributes());
}

bool
JITTimeFunctionBody::IsGenerator() const
{TRACE_IT(9864);
    return Js::FunctionInfo::IsGenerator(GetAttributes());
}

bool
JITTimeFunctionBody::IsLambda() const
{TRACE_IT(9865);
    return Js::FunctionInfo::IsLambda(GetAttributes());
}

bool
JITTimeFunctionBody::HasImplicitArgIns() const
{TRACE_IT(9866);
    return m_bodyData.hasImplicitArgIns != FALSE;
}

bool
JITTimeFunctionBody::HasCachedScopePropIds() const
{TRACE_IT(9867);
    return m_bodyData.hasCachedScopePropIds != FALSE;
}

bool
JITTimeFunctionBody::HasInlineCachesOnFunctionObject() const
{TRACE_IT(9868);
    return m_bodyData.inlineCachesOnFunctionObject != FALSE;
}

bool
JITTimeFunctionBody::DoInterruptProbe() const
{TRACE_IT(9869);
    // TODO michhol: this is technically a threadcontext flag,
    // may want to pass all these when initializing thread context
    return m_bodyData.doInterruptProbe != FALSE;
}

bool
JITTimeFunctionBody::HasRestParameter() const
{TRACE_IT(9870);
    return Js::FunctionBody::GetHasRestParameter(GetFlags());
}

bool
JITTimeFunctionBody::IsGlobalFunc() const
{TRACE_IT(9871);
    return m_bodyData.isGlobalFunc != FALSE;
}

void
JITTimeFunctionBody::DisableInlineApply() 
{TRACE_IT(9872);
    m_bodyData.isInlineApplyDisabled = TRUE;
}

bool
JITTimeFunctionBody::IsInlineApplyDisabled() const
{TRACE_IT(9873);
    return m_bodyData.isInlineApplyDisabled != FALSE;
}

bool
JITTimeFunctionBody::IsNonTempLocalVar(uint32 varIndex) const
{TRACE_IT(9874);
    return GetFirstNonTempLocalIndex() <= varIndex && varIndex < GetEndNonTempLocalIndex();
}

bool
JITTimeFunctionBody::DoJITLoopBody() const
{TRACE_IT(9875);
    return m_bodyData.doJITLoopBody != FALSE;
}

void
JITTimeFunctionBody::DisableInlineSpread()
{TRACE_IT(9876);
    m_bodyData.disableInlineSpread = TRUE;
}

bool
JITTimeFunctionBody::IsInlineSpreadDisabled() const
{TRACE_IT(9877);
    return m_bodyData.disableInlineSpread != FALSE;
}

bool
JITTimeFunctionBody::HasNonBuiltInCallee() const
{TRACE_IT(9878);
    return m_bodyData.hasNonBuiltInCallee != FALSE;
}

bool
JITTimeFunctionBody::CanInlineRecursively(uint depth, bool tryAggressive) const
{TRACE_IT(9879);
    uint recursiveInlineSpan = GetRecursiveCallSiteCount();

    uint minRecursiveInlineDepth = (uint)CONFIG_FLAG(RecursiveInlineDepthMin);

    if (recursiveInlineSpan != GetProfiledCallSiteCount() || tryAggressive == false)
    {TRACE_IT(9880);
        return depth < minRecursiveInlineDepth;
    }

    uint maxRecursiveInlineDepth = (uint)CONFIG_FLAG(RecursiveInlineDepthMax);
    uint maxRecursiveBytecodeBudget = (uint)CONFIG_FLAG(RecursiveInlineThreshold);
    uint numberOfAllowedFuncs = maxRecursiveBytecodeBudget / GetNonLoadByteCodeCount();
    uint maxDepth;

    if (recursiveInlineSpan == 1)
    {TRACE_IT(9881);
        maxDepth = numberOfAllowedFuncs;
    }
    else
    {TRACE_IT(9882);
        maxDepth = (uint)ceil(log((double)((double)numberOfAllowedFuncs) / log((double)recursiveInlineSpan)));
    }
    maxDepth = maxDepth < minRecursiveInlineDepth ? minRecursiveInlineDepth : maxDepth;
    maxDepth = maxDepth < maxRecursiveInlineDepth ? maxDepth : maxRecursiveInlineDepth;
    return depth < maxDepth;
}

bool
JITTimeFunctionBody::NeedScopeObjectForArguments(bool hasNonSimpleParams) const
{TRACE_IT(9883);
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
{TRACE_IT(9884);
    return !!m_bodyData.doScopeObjectCreation;
}

const byte *
JITTimeFunctionBody::GetByteCodeBuffer() const
{TRACE_IT(9885);
    return m_bodyData.byteCodeBuffer;
}

StatementMapIDL *
JITTimeFunctionBody::GetFullStatementMap() const
{TRACE_IT(9886);
    return m_bodyData.fullStatementMaps;
}

uint
JITTimeFunctionBody::GetFullStatementMapCount() const
{TRACE_IT(9887);
    return m_bodyData.fullStatementMapCount;
}

intptr_t
JITTimeFunctionBody::GetScriptIdAddr() const
{TRACE_IT(9888);
    return m_bodyData.scriptIdAddr;
}

intptr_t
JITTimeFunctionBody::GetProbeCountAddr() const
{TRACE_IT(9889);
    return m_bodyData.probeCountAddr;
}

intptr_t
JITTimeFunctionBody::GetFlagsAddr() const
{TRACE_IT(9890);
    return m_bodyData.flagsAddr;
}

intptr_t
JITTimeFunctionBody::GetRegAllocLoadCountAddr() const
{TRACE_IT(9891);
    return m_bodyData.regAllocLoadCountAddr;
}

intptr_t
JITTimeFunctionBody::GetFormalsPropIdArrayAddr() const
{TRACE_IT(9892);
    return m_bodyData.formalsPropIdArrayAddr;
}

intptr_t
JITTimeFunctionBody::GetRegAllocStoreCountAddr() const
{TRACE_IT(9893);
    return m_bodyData.regAllocStoreCountAddr;
}

intptr_t
JITTimeFunctionBody::GetCallCountStatsAddr() const
{TRACE_IT(9894);
    return m_bodyData.callCountStatsAddr;
}

intptr_t
JITTimeFunctionBody::GetObjectLiteralTypeRef(uint index) const
{TRACE_IT(9895);
    Assert(m_bodyData.objectLiteralTypesAddr != 0);
    return m_bodyData.objectLiteralTypesAddr + index * MachPtr;
}

intptr_t
JITTimeFunctionBody::GetConstantVar(Js::RegSlot location) const
{TRACE_IT(9896);
    Assert(m_bodyData.constTable != nullptr);
    Assert(location < GetConstCount());
    Assert(location != 0);

    return static_cast<intptr_t>(m_bodyData.constTable[location - Js::FunctionBody::FirstRegSlot]);
}

JITRecyclableObject *
JITTimeFunctionBody::GetConstantContent(Js::RegSlot location) const
{TRACE_IT(9897);
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
{TRACE_IT(9898);
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
{TRACE_IT(9899);
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
{TRACE_IT(9900);
    Assert(m_bodyData.constTable != nullptr);
    Assert(m_bodyData.constTableContent != nullptr);
    Assert(location < GetConstCount());
    Assert(location != 0);
    auto obj = m_bodyData.constTableContent->content[location - Js::FunctionBody::FirstRegSlot];

    if (obj == nullptr)
    {TRACE_IT(9901);
        if (Js::TaggedNumber::Is((Js::Var)GetConstantVar(location)))
        {TRACE_IT(9902);
            // tagged float
            return Js::TypeId::TypeIds_Number;
        }
        else
        {TRACE_IT(9903);
            return Js::TypeId::TypeIds_Limit;
        }
    }

    return static_cast<Js::TypeId>(*(obj->typeId));
}

intptr_t
JITTimeFunctionBody::GetLiteralRegexAddr(uint index) const
{TRACE_IT(9904);
    Assert(index < m_bodyData.literalRegexCount);

    return m_bodyData.literalRegexes[index];
}

void *
JITTimeFunctionBody::GetConstTable() const
{TRACE_IT(9905);
    return m_bodyData.constTable;
}

bool
JITTimeFunctionBody::IsConstRegPropertyString(Js::RegSlot reg, ScriptContextInfo * context) const
{TRACE_IT(9906);
    RecyclableObjectIDL * content = m_bodyData.constTableContent->content[reg - Js::FunctionBody::FirstRegSlot];
    if (content != nullptr && content->vtbl == context->GetVTableAddress(VtablePropertyString))
    {TRACE_IT(9907);
        return true;
    }
    return false;
}

intptr_t
JITTimeFunctionBody::GetRootObject() const
{TRACE_IT(9908);
    Assert(m_bodyData.constTable != nullptr);
    return m_bodyData.constTable[Js::FunctionBody::RootObjectRegSlot - Js::FunctionBody::FirstRegSlot];
}

Js::FunctionInfoPtrPtr
JITTimeFunctionBody::GetNestedFuncRef(uint index) const
{TRACE_IT(9909);
    Assert(index < GetNestedCount());
    Js::FunctionInfoPtrPtr baseAddr = (Js::FunctionInfoPtrPtr)m_bodyData.nestedFuncArrayAddr;
    return baseAddr + index;
}

intptr_t
JITTimeFunctionBody::GetLoopHeaderAddr(uint loopNum) const
{TRACE_IT(9910);
    Assert(loopNum < GetLoopCount());
    intptr_t baseAddr = m_bodyData.loopHeaderArrayAddr;
    return baseAddr + (loopNum * sizeof(Js::LoopHeader));
}

const JITLoopHeaderIDL *
JITTimeFunctionBody::GetLoopHeaderData(uint loopNum) const
{TRACE_IT(9911);
    Assert(loopNum < GetLoopCount());
    return &m_bodyData.loopHeaders[loopNum];
}

const AsmJsJITInfo *
JITTimeFunctionBody::GetAsmJsInfo() const
{TRACE_IT(9912);
    return reinterpret_cast<const AsmJsJITInfo *>(m_bodyData.asmJsData);
}

JITTimeProfileInfo *
JITTimeFunctionBody::GetProfileInfo() const
{TRACE_IT(9913);
    return reinterpret_cast<JITTimeProfileInfo *>(m_bodyData.profileData);
}

const JITTimeProfileInfo *
JITTimeFunctionBody::GetReadOnlyProfileInfo() const
{TRACE_IT(9914);
    return reinterpret_cast<const JITTimeProfileInfo *>(m_bodyData.profileData);
}

bool
JITTimeFunctionBody::HasProfileInfo() const
{TRACE_IT(9915);
    return m_bodyData.profileData != nullptr;
}

bool
JITTimeFunctionBody::HasPropIdToFormalsMap() const
{TRACE_IT(9916);
    return m_bodyData.propertyIdsForRegSlotsCount > 0 && GetFormalsPropIdArray() != nullptr;
}

bool
JITTimeFunctionBody::IsRegSlotFormal(Js::RegSlot reg) const
{TRACE_IT(9917);
    Assert(reg < m_bodyData.propertyIdsForRegSlotsCount);
    Js::PropertyId propId = (Js::PropertyId)m_bodyData.propertyIdsForRegSlots[reg];
    Js::PropertyIdArray * formalProps = GetFormalsPropIdArray();
    for (uint32 i = 0; i < formalProps->count; i++)
    {TRACE_IT(9918);
        if (formalProps->elements[i] == propId)
        {TRACE_IT(9919);
            return true;
        }
    }
    return false;
}

/* static */
bool
JITTimeFunctionBody::LoopContains(const JITLoopHeaderIDL * loop1, const JITLoopHeaderIDL * loop2)
{TRACE_IT(9920);
    return (loop1->startOffset <= loop2->startOffset && loop2->endOffset <= loop1->endOffset);
}

Js::FunctionBody::FunctionBodyFlags
JITTimeFunctionBody::GetFlags() const
{TRACE_IT(9921);
    return static_cast<Js::FunctionBody::FunctionBodyFlags>(m_bodyData.flags);
}

Js::FunctionInfo::Attributes
JITTimeFunctionBody::GetAttributes() const
{TRACE_IT(9922);
    return static_cast<Js::FunctionInfo::Attributes>(m_bodyData.attributes);
}

intptr_t
JITTimeFunctionBody::GetAuxDataAddr(uint offset) const
{TRACE_IT(9923);
    return m_bodyData.auxDataBufferAddr + offset;
}

void *
JITTimeFunctionBody::ReadFromAuxData(uint offset) const
{TRACE_IT(9924);
    return (void *)(m_bodyData.auxData + offset);
}

void *
JITTimeFunctionBody::ReadFromAuxContextData(uint offset) const
{TRACE_IT(9925);
    return (void *)(m_bodyData.auxContextData + offset);
}

const Js::PropertyIdArray *
JITTimeFunctionBody::ReadPropertyIdArrayFromAuxData(uint offset) const
{TRACE_IT(9926);
    Js::PropertyIdArray * auxArray = (Js::PropertyIdArray *)(m_bodyData.auxData + offset);
    Assert(offset + auxArray->GetDataSize() <= m_bodyData.auxDataCount);
    return auxArray;
}

Js::PropertyIdArray *
JITTimeFunctionBody::GetFormalsPropIdArray() const
{TRACE_IT(9927);
    return  (Js::PropertyIdArray *)m_bodyData.formalsPropIdArray;
}

Js::ForInCache *
JITTimeFunctionBody::GetForInCache(uint profileId) const
{TRACE_IT(9928);
    return  &((Js::ForInCache *)m_bodyData.forInCacheArrayAddr)[profileId];
}

bool
JITTimeFunctionBody::InitializeStatementMap(Js::SmallSpanSequence * statementMap, ArenaAllocator* alloc) const
{TRACE_IT(9929);
    if (!m_bodyData.statementMap)
    {TRACE_IT(9930);
        return false;
    }
    const uint statementsLength = m_bodyData.statementMap->statementLength;
    const uint offsetsLength = m_bodyData.statementMap->actualOffsetLength;

    statementMap->baseValue = m_bodyData.statementMap->baseValue;

    // TODO: (leish OOP JIT) using arena to prevent memory leak, fix to really implement GrowingUint32ArenaArray::Create()
    // or find other way to reuse like michhol's comments
    typedef JsUtil::GrowingArray<uint32, ArenaAllocator> GrowingUint32ArenaArray;

    if (statementsLength > 0)
    {TRACE_IT(9931);
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
    {TRACE_IT(9932);
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
{TRACE_IT(9933);
    return m_bodyData.displayName;
}
