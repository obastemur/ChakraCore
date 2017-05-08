//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"
#include "FormalsUtil.h"
#include "../Runtime/Language/SourceDynamicProfileManager.h"

#if DBG_DUMP
void PrintPnodeWIndent(ParseNode *pnode,int indentAmt);
#endif

const char* const nopNames[knopLim]= {
#define PTNODE(nop,sn,pc,nk,grfnop,json) sn,
#include "ptlist.h"
};
void printNop(int nop) {TRACE_IT(29344);
  printf("%s\n",nopNames[nop]);
}

const uint ParseNode::mpnopgrfnop[knopLim] =
{
#define PTNODE(nop,sn,pc,nk,grfnop,json) grfnop,
#include "ptlist.h"
};

bool Parser::IsES6DestructuringEnabled() const
{TRACE_IT(29345);
    return m_scriptContext->GetConfig()->IsES6DestructuringEnabled();
}

struct DeferredFunctionStub
{
    Field(RestorePoint) restorePoint;
    Field(uint) fncFlags;
    Field(uint) nestedCount;
    Field(DeferredFunctionStub *) deferredStubs;
    Field(charcount_t) ichMin;
};

struct StmtNest
{
    union
    {
        struct
        {
            ParseNodePtr pnodeStmt; // This statement node.
            ParseNodePtr pnodeLab;  // Labels for this statement.
        };
        struct
        {
            bool isDeferred : 1;
            OpCode op;              // This statement operation.
            LabelId* pLabelId;      // Labels for this statement.
        };
    };
    StmtNest *pstmtOuter;           // Enclosing statement.

    OpCode GetNop() const 
    {TRACE_IT(29346); 
        AnalysisAssert(isDeferred || pnodeStmt != nullptr);
        return isDeferred ? op : pnodeStmt->nop; 
    }
};

struct BlockInfoStack
{
    StmtNest pstmt;
    ParseNode *pnodeBlock;
    ParseNodePtr *m_ppnodeLex;              // lexical variable list tail
    BlockInfoStack *pBlockInfoOuter;        // containing block's BlockInfoStack
    BlockInfoStack *pBlockInfoFunction;     // nearest function's BlockInfoStack (if pnodeBlock is a function, this points to itself)
};

#if DEBUG
Parser::Parser(Js::ScriptContext* scriptContext, BOOL strictMode, PageAllocator *alloc, bool isBackground, size_t size)
#else
Parser::Parser(Js::ScriptContext* scriptContext, BOOL strictMode, PageAllocator *alloc, bool isBackground)
#endif
    : m_nodeAllocator(_u("Parser"), alloc ? alloc : scriptContext->GetThreadContext()->GetPageAllocator(), Parser::OutOfMemory),
    // use the GuestArena directly for keeping the RegexPattern* alive during byte code generation
    m_registeredRegexPatterns(scriptContext->GetGuestArena())
{TRACE_IT(29347);
    AssertMsg(size == sizeof(Parser), "verify conditionals affecting the size of Parser agree");
    Assert(scriptContext != nullptr);
    m_isInBackground = isBackground;
    m_phtbl = nullptr;
    m_pscan = nullptr;
    m_deferringAST = FALSE;
    m_stoppedDeferredParse = FALSE;
    m_hasParallelJob = false;
    m_doingFastScan = false;
    m_scriptContext = scriptContext;
    m_pCurrentAstSize = nullptr;
    m_arrayDepth = 0;
    m_funcInArrayDepth = 0;
    m_parenDepth = 0;
    m_funcInArray = 0;
    m_tryCatchOrFinallyDepth = 0;
    m_UsesArgumentsAtGlobal = false;
    m_currentNodeFunc = nullptr;
    m_currentNodeDeferredFunc = nullptr;
    m_currentNodeNonLambdaFunc = nullptr;
    m_currentNodeNonLambdaDeferredFunc = nullptr;
    m_currentNodeProg = nullptr;
    m_currDeferredStub = nullptr;
    m_prevSiblingDeferredStub = nullptr;
    m_pstmtCur = nullptr;
    m_currentBlockInfo = nullptr;
    m_currentScope = nullptr;
    m_currentDynamicBlock = nullptr;
    m_grfscr = fscrNil;
    m_length = 0;
    m_originalLength = 0;
    m_nextFunctionId = nullptr;
    m_errorCallback = nullptr;
    m_uncertainStructure = FALSE;
    m_reparsingLambdaParams = false;
    m_inFIB = false;
    currBackgroundParseItem = nullptr;
    backgroundParseItems = nullptr;
    fastScannedRegExpNodes = nullptr;

    m_fUseStrictMode = strictMode;
    m_InAsmMode = false;
    m_deferAsmJs = true;
    m_scopeCountNoAst = 0;
    m_fExpectExternalSource = 0;

    m_parseType = ParseType_Upfront;

    m_deferEllipsisError = false;
    m_hasDeferredShorthandInitError = false;
    m_parsingSuperRestrictionState = ParsingSuperRestrictionState_SuperDisallowed;
}

Parser::~Parser(void)
{TRACE_IT(29348);
    if (m_scriptContext == nullptr || m_scriptContext->GetGuestArena() == nullptr)
    {TRACE_IT(29349);
        // If the scriptContext or guestArena have gone away, there is no point clearing each item of this list.
        // Just reset it so that destructor of the SList will be no-op
        m_registeredRegexPatterns.Reset();
    }

    if (this->m_hasParallelJob)
    {TRACE_IT(29350);
#if ENABLE_BACKGROUND_PARSING
        // Let the background threads know that they can decommit their arena pages.
        BackgroundParser *bgp = m_scriptContext->GetBackgroundParser();
        Assert(bgp);
        if (bgp->Processor()->ProcessesInBackground())
        {TRACE_IT(29351);
            JsUtil::BackgroundJobProcessor *processor = static_cast<JsUtil::BackgroundJobProcessor*>(bgp->Processor());

            bool result = processor->IterateBackgroundThreads([&](JsUtil::ParallelThreadData *threadData)->bool {
                threadData->canDecommit = true;
                return false;
            });
            Assert(result);
        }
#endif
    }

    Release();

}

void Parser::OutOfMemory()
{TRACE_IT(29352);
    throw ParseExceptionObject(ERRnoMemory);
}

void Parser::Error(HRESULT hr)
{TRACE_IT(29353);
    Assert(FAILED(hr));
    m_err.Throw(hr);
}

void Parser::Error(HRESULT hr, ParseNodePtr pnode)
{TRACE_IT(29354);
    if (pnode && pnode->ichLim)
    {
        Error(hr, pnode->ichMin, pnode->ichLim);
    }
    else
    {TRACE_IT(29355);
        Error(hr);
    }
}

void Parser::Error(HRESULT hr, charcount_t ichMin, charcount_t ichLim)
{TRACE_IT(29356);
    m_pscan->SetErrorPosition(ichMin, ichLim);
    Error(hr);
}

void Parser::IdentifierExpectedError(const Token& token)
{TRACE_IT(29357);
    Assert(token.tk != tkID);

    HRESULT hr;
    if (token.IsReservedWord())
    {TRACE_IT(29358);
        if (token.IsKeyword())
        {TRACE_IT(29359);
            hr = ERRKeywordNotId;
        }
        else
        {TRACE_IT(29360);
            Assert(token.IsFutureReservedWord(true));
            if (token.IsFutureReservedWord(false))
            {TRACE_IT(29361);
                // Future reserved word in strict and non-strict modes
                hr = ERRFutureReservedWordNotId;
            }
            else
            {TRACE_IT(29362);
                // Future reserved word only in strict mode. The token would have been converted to tkID by the scanner if not
                // in strict mode.
                Assert(IsStrictMode());
                hr = ERRFutureReservedWordInStrictModeNotId;
            }
        }
    }
    else
    {TRACE_IT(29363);
        hr = ERRnoIdent;
    }

    Error(hr);
}

HRESULT Parser::ValidateSyntax(LPCUTF8 pszSrc, size_t encodedCharCount, bool isGenerator, bool isAsync, CompileScriptException *pse, void (Parser::*validateFunction)())
{TRACE_IT(29364);
    AssertPsz(pszSrc);
    AssertMemN(pse);

    if (this->IsBackgroundParser())
    {
        PROBE_STACK_NO_DISPOSE(m_scriptContext, Js::Constants::MinStackDefault);
    }
    else
    {
        PROBE_STACK(m_scriptContext, Js::Constants::MinStackDefault);
    }

    HRESULT hr;
    SmartFPUControl smartFpuControl;

    DebugOnly( m_err.fInited = TRUE; )
    BOOL fDeferSave = m_deferringAST;
    try
    {TRACE_IT(29365);
        hr = NOERROR;

        this->PrepareScanner(false);

        m_length = encodedCharCount;
        m_originalLength = encodedCharCount;

        // make sure deferred parsing is turned off
        ULONG grfscr = fscrNil;

        // Give the scanner the source and get the first token
        m_pscan->SetText(pszSrc, 0, encodedCharCount, 0, grfscr);
        m_pscan->SetYieldIsKeyword(isGenerator);
        m_pscan->SetAwaitIsKeyword(isAsync);
        m_pscan->Scan();

        uint nestedCount = 0;
        m_pnestedCount = &nestedCount;

        ParseNodePtr pnodeScope = nullptr;
        m_ppnodeScope = &pnodeScope;
        m_ppnodeExprScope = nullptr;

        uint nextFunctionId = 0;
        m_nextFunctionId = &nextFunctionId;

        m_inDeferredNestedFunc = false;
        m_deferringAST = true;



        m_nextBlockId = 0;

        ParseNode *pnodeFnc = CreateNode(knopFncDecl);
        pnodeFnc->sxFnc.ClearFlags();
        pnodeFnc->sxFnc.SetDeclaration(false);
        pnodeFnc->sxFnc.functionId   = 0;
        pnodeFnc->sxFnc.astSize      = 0;
        pnodeFnc->sxFnc.pnodeVars    = nullptr;
        pnodeFnc->sxFnc.pnodeParams  = nullptr;
        pnodeFnc->sxFnc.pnodeBody    = nullptr;
        pnodeFnc->sxFnc.pnodeName    = nullptr;
        pnodeFnc->sxFnc.pnodeRest    = nullptr;
        pnodeFnc->sxFnc.deferredStub = nullptr;
        pnodeFnc->sxFnc.SetIsGenerator(isGenerator);
        pnodeFnc->sxFnc.SetIsAsync(isAsync);
        m_ppnodeVar = &pnodeFnc->sxFnc.pnodeVars;
        m_currentNodeFunc = pnodeFnc;
        m_currentNodeDeferredFunc = NULL;
        m_sourceContextInfo = nullptr;
        AssertMsg(m_pstmtCur == NULL, "Statement stack should be empty when we start parse function body");

        ParseNodePtr block = StartParseBlock<false>(PnodeBlockType::Function, ScopeType_FunctionBody);
        (this->*validateFunction)();
        FinishParseBlock(block);

        pnodeFnc->ichLim = m_pscan->IchLimTok();
        pnodeFnc->sxFnc.cbLim = m_pscan->IecpLimTok();
        pnodeFnc->sxFnc.pnodeVars = nullptr;

        // there should be nothing after successful parsing for a given construct
        if (m_token.tk != tkEOF)
            Error(ERRsyntax);

        RELEASEPTR(m_pscan);
        m_deferringAST = fDeferSave;
    }
    catch(ParseExceptionObject& e)
    {TRACE_IT(29366);
        m_deferringAST = fDeferSave;
        m_err.m_hr = e.GetError();
        hr = pse->ProcessError( m_pscan,  m_err.m_hr, /* pnodeBase */ NULL);
    }

    return hr;
}

HRESULT Parser::ParseSourceInternal(
    __out ParseNodePtr* parseTree, LPCUTF8 pszSrc, size_t offsetInBytes, size_t encodedCharCount, charcount_t offsetInChars,
    bool fromExternal, ULONG grfscr, CompileScriptException *pse, Js::LocalFunctionId * nextFunctionId, ULONG lineNumber, SourceContextInfo * sourceContextInfo)
{TRACE_IT(29367);
    AssertMem(parseTree);
    AssertPsz(pszSrc);
    AssertMemN(pse);

    if (this->IsBackgroundParser())
    {
        PROBE_STACK_NO_DISPOSE(m_scriptContext, Js::Constants::MinStackDefault);
    }
    else
    {
        PROBE_STACK(m_scriptContext, Js::Constants::MinStackDefault);
    }

#ifdef PROFILE_EXEC
    m_scriptContext->ProfileBegin(Js::ParsePhase);
#endif
    JS_ETW_INTERNAL(EventWriteJSCRIPT_PARSE_START(m_scriptContext,0));

    *parseTree = NULL;
    m_sourceLim = 0;

    m_grfscr = grfscr;
    m_sourceContextInfo = sourceContextInfo;

    ParseNodePtr pnodeBase = NULL;
    HRESULT hr;
    SmartFPUControl smartFpuControl;

    DebugOnly( m_err.fInited = TRUE; )

    try
    {TRACE_IT(29368);
        this->PrepareScanner(fromExternal);

        if ((grfscr & fscrEvalCode) != 0)
        {TRACE_IT(29369);
            this->m_parsingSuperRestrictionState = Parser::ParsingSuperRestrictionState_SuperPropertyAllowed;
        }

        if ((grfscr & fscrIsModuleCode) != 0)
        {TRACE_IT(29370);
            // Module source flag should not be enabled unless module is enabled
            Assert(m_scriptContext->GetConfig()->IsES6ModuleEnabled());

            // Module code is always strict mode code.
            this->m_fUseStrictMode = TRUE;
        }

        // parse the source
        pnodeBase = Parse(pszSrc, offsetInBytes, encodedCharCount, offsetInChars, grfscr, lineNumber, nextFunctionId, pse);

        AssertNodeMem(pnodeBase);

        // Record the actual number of words parsed.
        m_sourceLim = pnodeBase->ichLim - offsetInChars;

        // TODO: The assert can be false positive in some scenarios and chuckj to fix it later
        // Assert(utf8::ByteIndexIntoCharacterIndex(pszSrc + offsetInBytes, encodedCharCount, fromExternal ? utf8::doDefault : utf8::doAllowThreeByteSurrogates) == m_sourceLim);

#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::ParsePhase))
        {
            PrintPnodeWIndent(pnodeBase,4);
            fflush(stdout);
        }
#endif

        *parseTree = pnodeBase;

        hr = NOERROR;
    }
    catch(ParseExceptionObject& e)
    {TRACE_IT(29371);
        m_err.m_hr = e.GetError();
        hr = pse->ProcessError( m_pscan, m_err.m_hr, pnodeBase);
    }

    if (this->m_hasParallelJob)
    {TRACE_IT(29372);
#if ENABLE_BACKGROUND_PARSING
        ///// Wait here for remaining jobs to finish. Then look for errors, do final const bindings.
        // pleath TODO: If there are remaining jobs, let the main thread help finish them.
        BackgroundParser *bgp = m_scriptContext->GetBackgroundParser();
        Assert(bgp);

        CompileScriptException se;
        this->WaitForBackgroundJobs(bgp, &se);

        BackgroundParseItem *failedItem = bgp->GetFailedBackgroundParseItem();
        if (failedItem)
        {TRACE_IT(29373);
            CompileScriptException *bgPse = failedItem->GetPSE();
            Assert(bgPse);
            *pse = *bgPse;
            hr = failedItem->GetHR();
            bgp->SetFailedBackgroundParseItem(nullptr);
        }

        if (this->fastScannedRegExpNodes != nullptr)
        {TRACE_IT(29374);
            this->FinishBackgroundRegExpNodes();
        }

        for (BackgroundParseItem *item = this->backgroundParseItems; item; item = item->GetNext())
        {TRACE_IT(29375);
            Parser *parser = item->GetParser();
            parser->FinishBackgroundPidRefs(item, this != parser);
        }
#endif
    }

    // done with the scanner
    RELEASEPTR(m_pscan);

#ifdef PROFILE_EXEC
    m_scriptContext->ProfileEnd(Js::ParsePhase);
#endif
    JS_ETW_INTERNAL(EventWriteJSCRIPT_PARSE_STOP(m_scriptContext, 0));

    return hr;
}

#if ENABLE_BACKGROUND_PARSING
void Parser::WaitForBackgroundJobs(BackgroundParser *bgp, CompileScriptException *pse)
{TRACE_IT(29376);
    // The scan of the script is done, but there may be unfinished background jobs in the queue.
    // Enlist the main thread to help with those.
    BackgroundParseItem *item;
    if (!*bgp->GetPendingBackgroundItemsPtr())
    {TRACE_IT(29377);
        // We're done.
        return;
    }

    // Save parser state, since we'll need to restore it in order to bind references correctly later.
    this->m_isInBackground = true;
    this->SetCurrBackgroundParseItem(nullptr);
    uint blockIdSave = this->m_nextBlockId;
    uint functionIdSave = *this->m_nextFunctionId;
    StmtNest *pstmtSave = this->m_pstmtCur;

    if (!bgp->Processor()->ProcessesInBackground())
    {TRACE_IT(29378);
        // No background thread. Just walk the jobs with no locking and process them.
        for (item = bgp->GetNextUnprocessedItem(); item; item = bgp->GetNextUnprocessedItem())
        {TRACE_IT(29379);
            bgp->Processor()->RemoveJob(item);
            bool succeeded = bgp->Process(item, this, pse);
            bgp->JobProcessed(item, succeeded);
        }
        Assert(!*bgp->GetPendingBackgroundItemsPtr());
    }
    else
    {TRACE_IT(29380);
        // Background threads. We need to have the critical section in order to:
        // - Check for unprocessed jobs;
        // - Remove jobs from the processor queue;
        // - Do JobsProcessed work (such as removing jobs from the BackgroundParser's unprocessed list).
        CriticalSection *pcs = static_cast<JsUtil::BackgroundJobProcessor*>(bgp->Processor())->GetCriticalSection();
        pcs->Enter();
        for (;;)
        {TRACE_IT(29381);
            // Grab a job (in lock)
            item = bgp->GetNextUnprocessedItem();
            if (item == nullptr)
            {TRACE_IT(29382);
                break;
            }
            bgp->Processor()->RemoveJob(item);
            pcs->Leave();

            // Process job (if there is one) (outside lock)
            bool succeeded = bgp->Process(item, this, pse);

            pcs->Enter();
            bgp->JobProcessed(item, succeeded);
        }
        pcs->Leave();

        // Wait for the background threads to finish jobs they're already processing (if any).
        // TODO: Replace with a proper semaphore.
        while(*bgp->GetPendingBackgroundItemsPtr());
    }

    Assert(!*bgp->GetPendingBackgroundItemsPtr());

    // Restore parser state.
    this->m_pstmtCur = pstmtSave;
    this->m_isInBackground = false;
    this->m_nextBlockId = blockIdSave;
    *this->m_nextFunctionId = functionIdSave;
}

void Parser::FinishBackgroundPidRefs(BackgroundParseItem *item, bool isOtherParser)
{TRACE_IT(29383);
    for (BlockInfoStack *blockInfo = item->GetParseContext()->currentBlockInfo; blockInfo; blockInfo = blockInfo->pBlockInfoOuter)
    {TRACE_IT(29384);
        if (isOtherParser)
        {TRACE_IT(29385);
            this->BindPidRefs<true>(blockInfo, item->GetMaxBlockId());
        }
        else
        {TRACE_IT(29386);
            this->BindPidRefs<false>(blockInfo, item->GetMaxBlockId());
        }
    }
}

void Parser::FinishBackgroundRegExpNodes()
{TRACE_IT(29387);
    // We have a list of RegExp nodes that we saw on the UI thread in functions we're parallel parsing,
    // and for each background job we have a list of RegExp nodes for which we couldn't allocate patterns.
    // We need to copy the pattern pointers from the UI thread nodes to the corresponding nodes on the
    // background nodes.
    // There may be UI thread nodes for which there are no background thread equivalents, because the UI thread
    // has to assume that the background thread won't defer anything.

    // Note that because these lists (and the list of background jobs) are SList's built by prepending, they are
    // all in reverse lexical order.

    Assert(!this->IsBackgroundParser());
    Assert(this->fastScannedRegExpNodes);
    Assert(this->backgroundParseItems != nullptr);

    BackgroundParseItem *currBackgroundItem;

#if DBG
    for (currBackgroundItem = this->backgroundParseItems;
         currBackgroundItem;
         currBackgroundItem = currBackgroundItem->GetNext())
    {TRACE_IT(29388);
        if (currBackgroundItem->RegExpNodeList())
        {
            FOREACH_DLIST_ENTRY(ParseNodePtr, ArenaAllocator, pnode, currBackgroundItem->RegExpNodeList())
            {TRACE_IT(29389);
                Assert(pnode->sxPid.regexPattern == nullptr);
            }
            NEXT_DLIST_ENTRY;
        }
    }
#endif

    // Hook up the patterns allocated on the main thread to the nodes created on the background thread.
    // Walk the list of foreground nodes, advancing through the work items and looking up each item.
    // Note that the background thread may have chosen to defer a given RegEx literal, so not every foreground
    // node will have a matching background node. Doesn't matter for correctness.
    // (It's inefficient, of course, to have to restart the inner loop from the beginning of the work item's
    // list, but it should be unusual to have many RegExes in a single work item's chunk of code. Figure out how
    // to start the inner loop from a known internal node within the list if that turns out to be important.)
    currBackgroundItem = this->backgroundParseItems;
    FOREACH_DLIST_ENTRY(ParseNodePtr, ArenaAllocator, pnodeFgnd, this->fastScannedRegExpNodes)
    {TRACE_IT(29390);
        Assert(pnodeFgnd->nop == knopRegExp);
        Assert(pnodeFgnd->sxPid.regexPattern != nullptr);
        bool quit = false;

        while (!quit)
        {TRACE_IT(29391);
            // Find the next work item with a RegEx in it.
            while (currBackgroundItem && currBackgroundItem->RegExpNodeList() == nullptr)
            {TRACE_IT(29392);
                currBackgroundItem = currBackgroundItem->GetNext();
            }
            if (!currBackgroundItem)
            {TRACE_IT(29393);
                break;
            }

            // Walk the RegExps in the work item.
            FOREACH_DLIST_ENTRY(ParseNodePtr, ArenaAllocator, pnodeBgnd, currBackgroundItem->RegExpNodeList())
            {TRACE_IT(29394);
                Assert(pnodeBgnd->nop == knopRegExp);

                if (pnodeFgnd->ichMin <= pnodeBgnd->ichMin)
                {TRACE_IT(29395);
                    // Either we found a match, or the next background node is past the foreground node.
                    // In any case, we can stop searching.
                    if (pnodeFgnd->ichMin == pnodeBgnd->ichMin)
                    {TRACE_IT(29396);
                        Assert(pnodeFgnd->ichLim == pnodeBgnd->ichLim);
                        pnodeBgnd->sxPid.regexPattern = pnodeFgnd->sxPid.regexPattern;
                    }
                    quit = true;
                    break;
                }
            }
            NEXT_DLIST_ENTRY;

            if (!quit)
            {TRACE_IT(29397);
                // Need to advance to the next work item.
                currBackgroundItem = currBackgroundItem->GetNext();
            }
        }
    }
    NEXT_DLIST_ENTRY;

#if DBG
    for (currBackgroundItem = this->backgroundParseItems;
         currBackgroundItem;
         currBackgroundItem = currBackgroundItem->GetNext())
    {TRACE_IT(29398);
        if (currBackgroundItem->RegExpNodeList())
        {
            FOREACH_DLIST_ENTRY(ParseNodePtr, ArenaAllocator, pnode, currBackgroundItem->RegExpNodeList())
            {TRACE_IT(29399);
                Assert(pnode->sxPid.regexPattern != nullptr);
            }
            NEXT_DLIST_ENTRY;
        }
    }
#endif
}
#endif

LabelId* Parser::CreateLabelId(IdentToken* pToken)
{TRACE_IT(29400);
    LabelId* pLabelId;

    pLabelId = (LabelId*)m_nodeAllocator.Alloc(sizeof(LabelId));
    if (NULL == pLabelId)
        Error(ERRnoMemory);
    pLabelId->pid = pToken->pid;
    pLabelId->next = NULL;

    return pLabelId;
}

/*****************************************************************************
The following set of routines allocate parse tree nodes of various kinds.
They catch an exception on out of memory.
*****************************************************************************/
static const int g_mpnopcbNode[] =
{
#define PTNODE(nop,sn,pc,nk,ok,json) kcbPn##nk,
#include "ptlist.h"
};

const Js::RegSlot NoRegister = (Js::RegSlot)-1;
const Js::RegSlot OneByteRegister = (Js::RegSlot_OneByte)-1;

void Parser::InitNode(OpCode nop,ParseNodePtr pnode) {TRACE_IT(29401);
    pnode->nop = nop;
    pnode->grfpn = PNodeFlags::fpnNone;
    pnode->location = NoRegister;
    pnode->emitLabels = false;
    pnode->isUsed = true;
    pnode->notEscapedUse = false;
    pnode->isInList = false;
    pnode->isCallApplyTargetLoad = false;
}

// Create nodes using Arena
ParseNodePtr
Parser::StaticCreateBlockNode(ArenaAllocator* alloc, charcount_t ichMin , charcount_t ichLim, int blockId, PnodeBlockType blockType)
{TRACE_IT(29402);
    ParseNodePtr pnode = StaticCreateNodeT<knopBlock>(alloc, ichMin, ichLim);
    InitBlockNode(pnode, blockId, blockType);
    return pnode;
}

void Parser::InitBlockNode(ParseNodePtr pnode, int blockId, PnodeBlockType blockType)
{TRACE_IT(29403);
    Assert(pnode->nop == knopBlock);
    pnode->sxBlock.pnodeScopes = nullptr;
    pnode->sxBlock.pnodeNext = nullptr;
    pnode->sxBlock.scope = nullptr;
    pnode->sxBlock.enclosingBlock = nullptr;
    pnode->sxBlock.pnodeLexVars = nullptr;
    pnode->sxBlock.pnodeStmt = nullptr;
    pnode->sxBlock.pnodeLastValStmt = nullptr;

    pnode->sxBlock.callsEval = false;
    pnode->sxBlock.childCallsEval = false;
    pnode->sxBlock.blockType = blockType;
    pnode->sxBlock.blockId = blockId;

    if (blockType != PnodeBlockType::Regular)
    {TRACE_IT(29404);
        pnode->grfpn |= PNodeFlags::fpnSyntheticNode;
    }
}

// Create Node with limit
template <OpCode nop>
ParseNodePtr Parser::CreateNodeT(charcount_t ichMin,charcount_t ichLim)
{TRACE_IT(29405);
    Assert(!this->m_deferringAST);
    ParseNodePtr pnode = StaticCreateNodeT<nop>(&m_nodeAllocator, ichMin, ichLim);

    Assert(m_pCurrentAstSize != NULL);
    *m_pCurrentAstSize += GetNodeSize<nop>();

    return pnode;
}

ParseNodePtr Parser::CreateDeclNode(OpCode nop, IdentPtr pid, SymbolType symbolType, bool errorOnRedecl, bool *isRedecl)
{TRACE_IT(29406);
    ParseNodePtr pnode = CreateNode(nop);

    pnode->sxVar.InitDeclNode(pid, NULL);

    if (symbolType != STUnknown)
    {TRACE_IT(29407);
        pnode->sxVar.sym = AddDeclForPid(pnode, pid, symbolType, errorOnRedecl, isRedecl);
    }

    return pnode;
}

Symbol* Parser::AddDeclForPid(ParseNodePtr pnode, IdentPtr pid, SymbolType symbolType, bool errorOnRedecl, bool *isRedecl)
{TRACE_IT(29408);
    Assert(pnode->IsVarLetOrConst());

    PidRefStack *refForUse = nullptr, *refForDecl = nullptr;

    if (isRedecl)
    {TRACE_IT(29409);
        *isRedecl = false;
    }

    BlockInfoStack *blockInfo;
    bool fBlockScope = false;
    if (pnode->nop != knopVarDecl || symbolType == STFunction)
    {TRACE_IT(29410);
        Assert(m_pstmtCur);
        if (m_pstmtCur->GetNop() != knopBlock)
        {TRACE_IT(29411);
            // Let/const declared in a bare statement context.
            Error(ERRDeclOutOfStmt);
        }

        if (m_pstmtCur->pstmtOuter && m_pstmtCur->pstmtOuter->GetNop() == knopSwitch)
        {TRACE_IT(29412);
            // Let/const declared inside a switch block (requiring conservative use-before-decl check).
            pnode->sxVar.isSwitchStmtDecl = true;
        }

        fBlockScope = pnode->nop != knopVarDecl ||
            (
                !GetCurrentBlockInfo()->pnodeBlock->sxBlock.scope ||
                GetCurrentBlockInfo()->pnodeBlock->sxBlock.scope->GetScopeType() != ScopeType_GlobalEvalBlock
                );
    }
    if (fBlockScope)
    {TRACE_IT(29413);
        blockInfo = GetCurrentBlockInfo();
    }
    else
    {TRACE_IT(29414);
        blockInfo = GetCurrentFunctionBlockInfo();
    }

    refForDecl = this->FindOrAddPidRef(pid, blockInfo->pnodeBlock->sxBlock.blockId, GetCurrentFunctionNode()->sxFnc.functionId);

    if (refForDecl == nullptr)
    {TRACE_IT(29415);
        Error(ERRnoMemory);
    }

    if (refForDecl->funcId != GetCurrentFunctionNode()->sxFnc.functionId)
    {TRACE_IT(29416);
        // Fix up the function id, which is incorrect if we're reparsing lambda parameters
        Assert(this->m_reparsingLambdaParams);
        refForDecl->funcId = GetCurrentFunctionNode()->sxFnc.functionId;
    }

    if (blockInfo == GetCurrentBlockInfo())
    {TRACE_IT(29417);
        refForUse = refForDecl;
    }
    else
    {TRACE_IT(29418);
        refForUse = this->PushPidRef(pid);
    }
    pnode->sxVar.symRef = refForUse->GetSymRef();
    Symbol *sym = refForDecl->GetSym();
    if (sym != nullptr)
    {TRACE_IT(29419);
        if (isRedecl)
        {TRACE_IT(29420);
            *isRedecl = true;
        }
        // Multiple declarations in the same scope. 3 possibilities: error, existing one wins, new one wins.
        switch (pnode->nop)
        {
        case knopLetDecl:
        case knopConstDecl:
            if (!sym->GetDecl()->sxVar.isBlockScopeFncDeclVar && !sym->GetIsArguments())
            {TRACE_IT(29421);
                // If the built-in arguments is shadowed then don't throw
                Assert(errorOnRedecl);
                // Redeclaration error.
                Error(ERRRedeclaration);
            }
            else
            {TRACE_IT(29422);
                // (New) let/const hides the (old) var
                sym->SetSymbolType(symbolType);
                sym->SetDecl(pnode);
            }
            break;
        case knopVarDecl:
            if (m_currentScope->GetScopeType() == ScopeType_Parameter && !sym->GetIsArguments())
            {TRACE_IT(29423);
                // If this is a parameter list, mark the scope to indicate that it has duplicate definition unless it is shadowing the default arguments symbol.
                // If later this turns out to be a non-simple param list (like function f(a, a, c = 1) {}) then it is a SyntaxError to have duplicate formals.
                m_currentScope->SetHasDuplicateFormals();
            }

            if (sym->GetDecl() == nullptr)
            {TRACE_IT(29424);
                Assert(symbolType == STFunction);
                sym->SetDecl(pnode);
                break;
            }
            switch (sym->GetDecl()->nop)
            {
            case knopLetDecl:
            case knopConstDecl:
                // Destructuring made possible to have the formals to be the let bind. But that shouldn't throw the error.
                if (errorOnRedecl && (!IsES6DestructuringEnabled() || sym->GetSymbolType() != STFormal))
                {TRACE_IT(29425);
                    Error(ERRRedeclaration);
                }
                // If !errorOnRedecl, (old) let/const hides the (new) var, so do nothing.
                break;
            case knopVarDecl:
                // Legal redeclaration. Who wins?
                if (errorOnRedecl || sym->GetDecl()->sxVar.isBlockScopeFncDeclVar || sym->GetIsArguments())
                {TRACE_IT(29426);
                    if (symbolType == STFormal ||
                        (symbolType == STFunction && sym->GetSymbolType() != STFormal) ||
                        sym->GetSymbolType() == STVariable)
                    {TRACE_IT(29427);
                        // New decl wins.
                        sym->SetSymbolType(symbolType);
                        sym->SetDecl(pnode);
                    }
                }
                break;
            }
            break;
        }
    }
    else
    {TRACE_IT(29428);
        Scope *scope = blockInfo->pnodeBlock->sxBlock.scope;
        if (scope == nullptr)
        {TRACE_IT(29429);
            Assert(blockInfo->pnodeBlock->sxBlock.blockType == PnodeBlockType::Regular);
            scope = Anew(&m_nodeAllocator, Scope, &m_nodeAllocator, ScopeType_Block);
            if (this->IsCurBlockInLoop())
            {TRACE_IT(29430);
                scope->SetIsBlockInLoop();
            }
            blockInfo->pnodeBlock->sxBlock.scope = scope;
            PushScope(scope);
        }

        if (scope->GetScopeType() == ScopeType_GlobalEvalBlock)
        {TRACE_IT(29431);
            Assert(fBlockScope);
            Assert(scope->GetEnclosingScope() == m_currentNodeProg->sxProg.scope);
            // Check for same-named decl in Global scope.
            PidRefStack *pidRefOld = pid->GetPidRefForScopeId(0);
            if (pidRefOld && pidRefOld->GetSym())
            {TRACE_IT(29432);
                Error(ERRRedeclaration);
            }
        }
        else if (scope->GetScopeType() == ScopeType_Global && (this->m_grfscr & fscrEvalCode) &&
                 !(m_functionBody && m_functionBody->GetScopeInfo()))
        {TRACE_IT(29433);
            // Check for same-named decl in GlobalEvalBlock scope. Note that this is not necessary
            // if we're compiling a deferred nested function and the global scope was restored from cached info,
            // because in that case we don't need a GlobalEvalScope.
            Assert(!fBlockScope || (this->m_grfscr & fscrConsoleScopeEval) == fscrConsoleScopeEval);
            PidRefStack *pidRefOld = pid->GetPidRefForScopeId(1);
            if (pidRefOld && pidRefOld->GetSym())
            {TRACE_IT(29434);
                Error(ERRRedeclaration);
            }
        }

        if ((scope->GetScopeType() == ScopeType_FunctionBody || scope->GetScopeType() == ScopeType_Parameter) && symbolType != STFunction)
        {TRACE_IT(29435);
            ParseNodePtr pnodeFnc = GetCurrentFunctionNode();
            AnalysisAssert(pnodeFnc);
            if (pnodeFnc->sxFnc.pnodeName &&
                pnodeFnc->sxFnc.pnodeName->nop == knopVarDecl &&
                pnodeFnc->sxFnc.pnodeName->sxVar.pid == pid &&
                (pnodeFnc->sxFnc.IsBodyAndParamScopeMerged() || scope->GetScopeType() == ScopeType_Parameter))
            {TRACE_IT(29436);
                // Named function expression has its name hidden by a local declaration.
                // This is important to know if we don't know whether nested deferred functions refer to it,
                // because if the name has a non-local reference then we have to create a scope object.
                m_currentNodeFunc->sxFnc.SetNameIsHidden();
            }
        }

        if (!sym)
        {TRACE_IT(29437);
            const char16 *name = reinterpret_cast<const char16*>(pid->Psz());
            int nameLength = pid->Cch();
            SymbolName const symName(name, nameLength);

            Assert(!scope->FindLocalSymbol(symName));
            sym = Anew(&m_nodeAllocator, Symbol, symName, pnode, symbolType);
            scope->AddNewSymbol(sym);
            sym->SetPid(pid);
        }
        refForDecl->SetSym(sym);
    }
    return sym;
}

bool Parser::IsCurBlockInLoop() const
{TRACE_IT(29438);
    for (StmtNest *stmt = this->m_pstmtCur; stmt != nullptr; stmt = stmt->pstmtOuter)
    {TRACE_IT(29439);
        OpCode nop = stmt->GetNop();
        if (ParseNode::Grfnop(nop) & fnopContinue)
        {TRACE_IT(29440);
            return true;
        }
        if (nop == knopFncDecl)
        {TRACE_IT(29441);
            return false;
        }
    }
    return false;
}

void Parser::RestorePidRefForSym(Symbol *sym)
{TRACE_IT(29442);
    IdentPtr pid = m_pscan->m_phtbl->PidHashNameLen(sym->GetName().GetBuffer(), sym->GetName().GetLength());
    Assert(pid);
    sym->SetPid(pid);
    PidRefStack *ref = this->PushPidRef(pid);
    ref->SetSym(sym);
}

IdentPtr Parser::PidFromNode(ParseNodePtr pnode)
{TRACE_IT(29443);
    for (;;)
    {TRACE_IT(29444);
        switch (pnode->nop)
        {
        case knopName:
            return pnode->sxPid.pid;

        case knopVarDecl:
            return pnode->sxVar.pid;

        case knopDot:
            Assert(pnode->sxBin.pnode2->nop == knopName);
            return pnode->sxBin.pnode2->sxPid.pid;

        case knopComma:
            // Advance to the RHS and iterate.
            pnode = pnode->sxBin.pnode2;
            break;

        default:
            return nullptr;
        }
    }
}

#if DBG
void VerifyNodeSize(OpCode nop, int size)
{TRACE_IT(29445);
    Assert(nop >= 0 && nop < knopLim);
    __analysis_assume(nop < knopLim);
    Assert(g_mpnopcbNode[nop] == size);
}
#endif

ParseNodePtr Parser::StaticCreateBinNode(OpCode nop, ParseNodePtr pnode1,
                                   ParseNodePtr pnode2,ArenaAllocator* alloc)
{
    DebugOnly(VerifyNodeSize(nop, kcbPnBin));
    ParseNodePtr pnode = (ParseNodePtr)alloc->Alloc(kcbPnBin);
    InitNode(nop, pnode);

    pnode->sxBin.pnodeNext = nullptr;
    pnode->sxBin.pnode1 = pnode1;
    pnode->sxBin.pnode2 = pnode2;

    // Statically detect if the add is a concat
    if (!PHASE_OFF1(Js::ByteCodeConcatExprOptPhase))
    {TRACE_IT(29446);
        // We can't flatten the concat expression if the LHS is not a flatten concat already
        // e.g.  a + (<str> + b)
        //      Side effect of ToStr(b) need to happen first before ToStr(a)
        //      If we flatten the concat expression, we will do ToStr(a) before ToStr(b)
        if ((nop == knopAdd) && (pnode1->CanFlattenConcatExpr() || pnode2->nop == knopStr))
        {TRACE_IT(29447);
            pnode->grfpn |= fpnCanFlattenConcatExpr;
        }
    }

    return pnode;
}

// Create nodes using parser allocator

ParseNodePtr Parser::CreateNode(OpCode nop, charcount_t ichMin)
{TRACE_IT(29448);
    bool nodeAllowed = IsNodeAllowedInCurrentDeferralState(nop);
    Assert(nodeAllowed);

    Assert(nop >= 0 && nop < knopLim);
    ParseNodePtr pnode;
    int cb = (nop >= knopNone && nop < knopLim) ? g_mpnopcbNode[nop] : g_mpnopcbNode[knopEmpty];

    pnode = (ParseNodePtr)m_nodeAllocator.Alloc(cb);
    Assert(pnode != nullptr);

    if (!m_deferringAST)
    {TRACE_IT(29449);
        Assert(m_pCurrentAstSize != nullptr);
        *m_pCurrentAstSize += cb;
    }

    InitNode(nop,pnode);

    // default - may be changed
    pnode->ichMin = ichMin;
    if (m_pscan!= nullptr) {TRACE_IT(29450);
      pnode->ichLim = m_pscan->IchLimTok();
    }
    else pnode->ichLim=0;

    return pnode;
}

ParseNodePtr Parser::CreateUniNode(OpCode nop, ParseNodePtr pnode1)
{TRACE_IT(29451);
    Assert(!this->m_deferringAST);
    DebugOnly(VerifyNodeSize(nop, kcbPnUni));
    ParseNodePtr pnode = (ParseNodePtr)m_nodeAllocator.Alloc(kcbPnUni);

    Assert(m_pCurrentAstSize != nullptr);
    *m_pCurrentAstSize += kcbPnUni;

    InitNode(nop, pnode);

    pnode->sxUni.pnode1 = pnode1;
    if (nullptr == pnode1)
    {TRACE_IT(29452);
        // no ops
        pnode->ichMin = m_pscan->IchMinTok();
        pnode->ichLim = m_pscan->IchLimTok();
    }
    else
    {TRACE_IT(29453);
        // 1 op
        pnode->ichMin = pnode1->ichMin;
        pnode->ichLim = pnode1->ichLim;
        this->CheckArguments(pnode);
    }
    return pnode;
}

ParseNodePtr Parser::CreateBinNode(OpCode nop, ParseNodePtr pnode1, ParseNodePtr pnode2)
{TRACE_IT(29454);
    Assert(!this->m_deferringAST);
    charcount_t ichMin;
    charcount_t ichLim;

    if (nullptr == pnode1)
    {TRACE_IT(29455);
        // no ops
        Assert(nullptr == pnode2);
        ichMin = m_pscan->IchMinTok();
        ichLim = m_pscan->IchLimTok();
    }
    else
    {TRACE_IT(29456);
        if (nullptr == pnode2)
        {TRACE_IT(29457);
            // 1 op
            ichMin = pnode1->ichMin;
            ichLim = pnode1->ichLim;
        }
        else
        {TRACE_IT(29458);
            // 2 ops
            ichMin = pnode1->ichMin;
            ichLim = pnode2->ichLim;
            if (nop != knopDot && nop != knopIndex)
            {TRACE_IT(29459);
                this->CheckArguments(pnode2);
            }
        }
        if (nop != knopDot && nop != knopIndex)
        {TRACE_IT(29460);
            this->CheckArguments(pnode1);
        }
    }

    return CreateBinNode(nop, pnode1, pnode2, ichMin, ichLim);
}

ParseNodePtr Parser::CreateTriNode(OpCode nop, ParseNodePtr pnode1,
                                   ParseNodePtr pnode2, ParseNodePtr pnode3)
{TRACE_IT(29461);
    charcount_t ichMin;
    charcount_t ichLim;

    if (nullptr == pnode1)
    {TRACE_IT(29462);
        // no ops
        Assert(nullptr == pnode2);
        Assert(nullptr == pnode3);
        ichMin = m_pscan->IchMinTok();
        ichLim = m_pscan->IchLimTok();
    }
    else if (nullptr == pnode2)
    {TRACE_IT(29463);
        // 1 op
        Assert(nullptr == pnode3);
        ichMin = pnode1->ichMin;
        ichLim = pnode1->ichLim;
    }
    else if (nullptr == pnode3)
    {TRACE_IT(29464);
        // 2 op
        ichMin = pnode1->ichMin;
        ichLim = pnode2->ichLim;
    }
    else
    {TRACE_IT(29465);
        // 3 ops
        ichMin = pnode1->ichMin;
        ichLim = pnode3->ichLim;
    }

    return CreateTriNode(nop, pnode1, pnode2, pnode3, ichMin, ichLim);
}

ParseNodePtr Parser::CreateBlockNode(charcount_t ichMin,charcount_t ichLim, PnodeBlockType blockType)
{TRACE_IT(29466);
    return StaticCreateBlockNode(&m_nodeAllocator, ichMin, ichLim, this->m_nextBlockId++, blockType);
}

ParseNodePtr
Parser::CreateCallNode(OpCode nop, ParseNodePtr pnode1, ParseNodePtr pnode2,charcount_t ichMin,charcount_t ichLim)
{TRACE_IT(29467);
    Assert(!this->m_deferringAST);
    DebugOnly(VerifyNodeSize(nop, kcbPnCall));
    ParseNodePtr pnode = (ParseNodePtr)m_nodeAllocator.Alloc(kcbPnCall);

    Assert(m_pCurrentAstSize != nullptr);
    *m_pCurrentAstSize += kcbPnCall;

    InitNode(nop, pnode);

    pnode->sxCall.pnodeTarget = pnode1;
    pnode->sxCall.pnodeArgs = pnode2;
    pnode->sxCall.argCount = 0;
    pnode->sxCall.spreadArgCount = 0;
    pnode->sxCall.callOfConstants = false;
    pnode->sxCall.isApplyCall = false;
    pnode->sxCall.isEvalCall = false;

    pnode->ichMin = ichMin;
    pnode->ichLim = ichLim;

    return pnode;
}

ParseNodePtr Parser::CreateStrNode(IdentPtr pid)
{TRACE_IT(29468);
    Assert(!this->m_deferringAST);

    ParseNodePtr pnode = CreateNode(knopStr);
    pnode->sxPid.pid=pid;
    pnode->grfpn |= PNodeFlags::fpnCanFlattenConcatExpr;
    return pnode;
}

ParseNodePtr Parser::CreateIntNode(int32 lw)
{TRACE_IT(29469);
    ParseNodePtr pnode = CreateNode(knopInt);
    pnode->sxInt.lw = lw;
    return pnode;
}

// Create Node with scanner limit
template <OpCode nop>
ParseNodePtr Parser::CreateNodeWithScanner()
{TRACE_IT(29470);
    Assert(m_pscan != nullptr);
    return CreateNodeWithScanner<nop>(m_pscan->IchMinTok());
}

template <OpCode nop>
ParseNodePtr Parser::CreateNodeWithScanner(charcount_t ichMin)
{TRACE_IT(29471);
    Assert(m_pscan != nullptr);
    return CreateNodeT<nop>(ichMin, m_pscan->IchLimTok());
}

ParseNodePtr Parser::CreateProgNodeWithScanner(bool isModuleSource)
{TRACE_IT(29472);
    ParseNodePtr pnodeProg;

    if (isModuleSource)
    {TRACE_IT(29473);
        pnodeProg = CreateNodeWithScanner<knopModule>();

        // knopModule is not actually handled anywhere since we would need to handle it everywhere we could
        // have knopProg and it would be treated exactly the same except for import/export statements.
        // We are only using it as a way to get the correct size for PnModule.
        // Consider: Should we add a flag to PnProg which is false but set to true in PnModule?
        //           If we do, it can't be a virtual method since the parse nodes are all in a union.
        pnodeProg->nop = knopProg;
    }
    else
    {TRACE_IT(29474);
        pnodeProg = CreateNodeWithScanner<knopProg>();
    }

    return pnodeProg;
}

ParseNodePtr Parser::CreateCallNode(OpCode nop, ParseNodePtr pnode1, ParseNodePtr pnode2)
{TRACE_IT(29475);
    charcount_t ichMin;
    charcount_t ichLim;

    if (nullptr == pnode1)
    {TRACE_IT(29476);
        Assert(nullptr == pnode2);
        ichMin = m_pscan->IchMinTok();
        ichLim = m_pscan->IchLimTok();
    }
    else
    {TRACE_IT(29477);
        if (nullptr == pnode2)
        {TRACE_IT(29478);
            ichMin = pnode1->ichMin;
            ichLim = pnode1->ichLim;
        }
        else
        {TRACE_IT(29479);
            ichMin = pnode1->ichMin;
            ichLim = pnode2->ichLim;
        }
        if (pnode1->nop == knopDot || pnode1->nop == knopIndex)
        {TRACE_IT(29480);
            this->CheckArguments(pnode1->sxBin.pnode1);
        }
    }
    return CreateCallNode(nop, pnode1, pnode2, ichMin, ichLim);
}

ParseNodePtr Parser::CreateStrNodeWithScanner(IdentPtr pid)
{TRACE_IT(29481);
    Assert(!this->m_deferringAST);

    ParseNodePtr pnode = CreateNodeWithScanner<knopStr>();
    pnode->sxPid.pid=pid;
    pnode->grfpn |= PNodeFlags::fpnCanFlattenConcatExpr;
    return pnode;
}

ParseNodePtr Parser::CreateIntNodeWithScanner(int32 lw)
{TRACE_IT(29482);
    Assert(!this->m_deferringAST);
    ParseNodePtr pnode = CreateNodeWithScanner<knopInt>();
    pnode->sxInt.lw = lw;
    return pnode;
}

ParseNodePtr Parser::CreateTempNode(ParseNode* initExpr)
{TRACE_IT(29483);
    ParseNodePtr pnode = CreateNode(knopTemp, (charcount_t)0);
    pnode->sxVar.pnodeInit =initExpr;
    pnode->sxVar.pnodeNext = nullptr;
    return pnode;
}

ParseNodePtr Parser::CreateTempRef(ParseNode* tempNode)
{TRACE_IT(29484);
    ParseNodePtr pnode = CreateUniNode(knopTempRef, tempNode);
    return pnode;
}

void Parser::CheckPidIsValid(IdentPtr pid, bool autoArgumentsObject)
{TRACE_IT(29485);
    if (IsStrictMode())
    {TRACE_IT(29486);
        // in strict mode, variable named 'eval' cannot be created
        if (pid == wellKnownPropertyPids.eval)
        {TRACE_IT(29487);
            Error(ERREvalUsage);
        }
        else if (pid == wellKnownPropertyPids.arguments && !autoArgumentsObject)
        {TRACE_IT(29488);
            Error(ERRArgsUsage);
        }
    }
}

// CreateVarDecl needs m_ppnodeVar to be pointing to the right function.
// Post-parsing rewriting during bytecode gen may have m_ppnodeVar pointing to the last parsed function.
// This function sets up m_ppnodeVar to point to the given pnodeFnc and creates the new var declaration.
// This prevents accidentally adding var declarations to the last parsed function.
ParseNodePtr Parser::AddVarDeclNode(IdentPtr pid, ParseNodePtr pnodeFnc)
{TRACE_IT(29489);
    AnalysisAssert(pnodeFnc);

    ParseNodePtr *const ppnodeVarSave = m_ppnodeVar;

    m_ppnodeVar = &pnodeFnc->sxFnc.pnodeVars;
    while (*m_ppnodeVar != nullptr)
    {TRACE_IT(29490);
        m_ppnodeVar = &(*m_ppnodeVar)->sxVar.pnodeNext;
    }

    ParseNodePtr pnode = CreateVarDeclNode(pid, STUnknown, false, 0, /* checkReDecl = */ false);

    m_ppnodeVar = ppnodeVarSave;

    return pnode;
}

ParseNodePtr Parser::CreateModuleImportDeclNode(IdentPtr localName)
{TRACE_IT(29491);
    ParseNodePtr declNode = CreateBlockScopedDeclNode(localName, knopConstDecl);
    Symbol* sym = declNode->sxVar.sym;

    sym->SetIsModuleExportStorage(true);
    sym->SetIsModuleImport(true);

    return declNode;
}

ParseNodePtr Parser::CreateVarDeclNode(IdentPtr pid, SymbolType symbolType, bool autoArgumentsObject, ParseNodePtr pnodeFnc, bool errorOnRedecl, bool *isRedecl)
{TRACE_IT(29492);
    ParseNodePtr pnode = CreateDeclNode(knopVarDecl, pid, symbolType, errorOnRedecl, isRedecl);

    // Append the variable to the end of the current variable list.
    AssertMem(m_ppnodeVar);
    pnode->sxVar.pnodeNext = *m_ppnodeVar;
    *m_ppnodeVar = pnode;
    if (nullptr != pid)
    {TRACE_IT(29493);
        // this is not a temp - make sure temps go after this node
        AssertMem(pid);
        m_ppnodeVar = &pnode->sxVar.pnodeNext;
        CheckPidIsValid(pid, autoArgumentsObject);
    }

    return pnode;
}

ParseNodePtr Parser::CreateBlockScopedDeclNode(IdentPtr pid, OpCode nodeType)
{TRACE_IT(29494);
    Assert(nodeType == knopConstDecl || nodeType == knopLetDecl);

    ParseNodePtr pnode = CreateDeclNode(nodeType, pid, STVariable, true);

    if (nullptr != pid)
    {TRACE_IT(29495);
        AssertMem(pid);
        pid->SetIsLetOrConst();
        AddVarDeclToBlock(pnode);
        CheckPidIsValid(pid);
    }

    return pnode;
}

void Parser::AddVarDeclToBlock(ParseNode *pnode)
{TRACE_IT(29496);
    Assert(pnode->nop == knopConstDecl || pnode->nop == knopLetDecl);

    // Maintain a combined list of let and const declarations to keep
    // track of declaration order.

    AssertMem(m_currentBlockInfo->m_ppnodeLex);
    *m_currentBlockInfo->m_ppnodeLex = pnode;
    m_currentBlockInfo->m_ppnodeLex = &pnode->sxVar.pnodeNext;
    pnode->sxVar.pnodeNext = nullptr;
}

void Parser::SetCurrentStatement(StmtNest *stmt)
{TRACE_IT(29497);
    m_pstmtCur = stmt;
}

template<bool buildAST>
ParseNodePtr Parser::StartParseBlockWithCapacity(PnodeBlockType blockType, ScopeType scopeType, int capacity)
{TRACE_IT(29498);
    Scope *scope = nullptr;
    // Block scopes are created lazily when we discover block-scoped content.
    if (scopeType != ScopeType_Unknown && scopeType != ScopeType_Block)
    {TRACE_IT(29499);
        scope = Anew(&m_nodeAllocator, Scope, &m_nodeAllocator, scopeType, capacity);
        PushScope(scope);
    }

    return StartParseBlockHelper<buildAST>(blockType, scope, nullptr, nullptr);
}

template<bool buildAST>
ParseNodePtr Parser::StartParseBlock(PnodeBlockType blockType, ScopeType scopeType, ParseNodePtr pnodeLabel, LabelId* pLabelId)
{TRACE_IT(29500);
    Scope *scope = nullptr;
    // Block scopes are created lazily when we discover block-scoped content.
    if (scopeType != ScopeType_Unknown && scopeType != ScopeType_Block)
    {TRACE_IT(29501);
        scope = Anew(&m_nodeAllocator, Scope, &m_nodeAllocator, scopeType);
        PushScope(scope);
    }

    return StartParseBlockHelper<buildAST>(blockType, scope, pnodeLabel, pLabelId);
}

template<bool buildAST>
ParseNodePtr Parser::StartParseBlockHelper(PnodeBlockType blockType, Scope *scope, ParseNodePtr pnodeLabel, LabelId* pLabelId)
{TRACE_IT(29502);
    ParseNodePtr pnodeBlock = CreateBlockNode(blockType);
    pnodeBlock->sxBlock.scope = scope;
    BlockInfoStack *newBlockInfo = PushBlockInfo(pnodeBlock);

    PushStmt<buildAST>(&newBlockInfo->pstmt, pnodeBlock, knopBlock, pnodeLabel, pLabelId);

    return pnodeBlock;
}

void Parser::PushScope(Scope *scope)
{TRACE_IT(29503);
    Assert(scope);
    scope->SetEnclosingScope(m_currentScope);
    m_currentScope = scope;
}

void Parser::PopScope(Scope *scope)
{TRACE_IT(29504);
    Assert(scope == m_currentScope);
    m_currentScope = scope->GetEnclosingScope();
    scope->SetEnclosingScope(nullptr);
}

void Parser::PushFuncBlockScope(ParseNodePtr pnodeBlock, ParseNodePtr **ppnodeScopeSave, ParseNodePtr **ppnodeExprScopeSave)
{TRACE_IT(29505);
    // Maintain the scope tree.

    pnodeBlock->sxBlock.pnodeScopes = nullptr;
    pnodeBlock->sxBlock.pnodeNext = nullptr;

    // Insert this block into the active list of scopes (m_ppnodeExprScope or m_ppnodeScope).
    // Save the current block's "next" pointer as the new endpoint of that list.
    if (m_ppnodeExprScope)
    {TRACE_IT(29506);
        *ppnodeScopeSave = m_ppnodeScope;

        Assert(*m_ppnodeExprScope == nullptr);
        *m_ppnodeExprScope = pnodeBlock;
        *ppnodeExprScopeSave = &pnodeBlock->sxBlock.pnodeNext;
    }
    else
    {TRACE_IT(29507);
        Assert(m_ppnodeScope);
        Assert(*m_ppnodeScope == nullptr);
        *m_ppnodeScope = pnodeBlock;
        *ppnodeScopeSave = &pnodeBlock->sxBlock.pnodeNext;

        *ppnodeExprScopeSave = m_ppnodeExprScope;
    }

    // Advance the global scope list pointer to the new block's child list.
    m_ppnodeScope = &pnodeBlock->sxBlock.pnodeScopes;
    // Set m_ppnodeExprScope to NULL to make that list inactive.
    m_ppnodeExprScope = nullptr;
}

void Parser::PopFuncBlockScope(ParseNodePtr *ppnodeScopeSave, ParseNodePtr *ppnodeExprScopeSave)
{TRACE_IT(29508);
    Assert(m_ppnodeExprScope == nullptr || *m_ppnodeExprScope == nullptr);
    m_ppnodeExprScope = ppnodeExprScopeSave;

    AssertMem(m_ppnodeScope);
    Assert(nullptr == *m_ppnodeScope);
    m_ppnodeScope = ppnodeScopeSave;
}

template<bool buildAST>
ParseNodePtr Parser::ParseBlock(ParseNodePtr pnodeLabel, LabelId* pLabelId)
{TRACE_IT(29509);
    ParseNodePtr pnodeBlock = nullptr;
    ParseNodePtr *ppnodeScopeSave = nullptr;
    ParseNodePtr *ppnodeExprScopeSave = nullptr;

    pnodeBlock = StartParseBlock<buildAST>(PnodeBlockType::Regular, ScopeType_Block, pnodeLabel, pLabelId);

    BlockInfoStack* outerBlockInfo = m_currentBlockInfo->pBlockInfoOuter;
    if (outerBlockInfo != nullptr && outerBlockInfo->pnodeBlock != nullptr
        && outerBlockInfo->pnodeBlock->sxBlock.scope != nullptr
        && outerBlockInfo->pnodeBlock->sxBlock.scope->GetScopeType() == ScopeType_CatchParamPattern)
    {TRACE_IT(29510);
        // If we are parsing the catch block then destructured params can have let declrations. Let's add them to the new block.
        for (ParseNodePtr pnode = m_currentBlockInfo->pBlockInfoOuter->pnodeBlock->sxBlock.pnodeLexVars; pnode; pnode = pnode->sxVar.pnodeNext)
        {TRACE_IT(29511);
            PidRefStack* ref = PushPidRef(pnode->sxVar.sym->GetPid());
            ref->SetSym(pnode->sxVar.sym);
        }
    }

    ChkCurTok(tkLCurly, ERRnoLcurly);
    ParseNodePtr * ppnodeList = nullptr;
    if (buildAST)
    {
        PushFuncBlockScope(pnodeBlock, &ppnodeScopeSave, &ppnodeExprScopeSave);
        ppnodeList = &pnodeBlock->sxBlock.pnodeStmt;
    }

    ParseStmtList<buildAST>(ppnodeList);

    if (buildAST)
    {
        PopFuncBlockScope(ppnodeScopeSave, ppnodeExprScopeSave);
    }

    FinishParseBlock(pnodeBlock);

    ChkCurTok(tkRCurly, ERRnoRcurly);


    return pnodeBlock;
}

void Parser::FinishParseBlock(ParseNode *pnodeBlock, bool needScanRCurly)
{TRACE_IT(29512);
    Assert(m_currentBlockInfo != nullptr && pnodeBlock == m_currentBlockInfo->pnodeBlock);

    if (needScanRCurly)
    {TRACE_IT(29513);
        // Only update the ichLim if we were expecting an RCurly. If there is an
        // expression body without a necessary RCurly, the correct ichLim will
        // have been set already.
        pnodeBlock->ichLim = m_pscan->IchLimTok();
    }

    BindPidRefs<false>(GetCurrentBlockInfo(), m_nextBlockId - 1);

    PopStmt(&m_currentBlockInfo->pstmt);

    PopBlockInfo();

    Scope *scope = pnodeBlock->sxBlock.scope;
    if (scope)
    {TRACE_IT(29514);
        PopScope(scope);
    }
}

void Parser::FinishParseFncExprScope(ParseNodePtr pnodeFnc, ParseNodePtr pnodeFncExprScope)
{TRACE_IT(29515);
    int fncExprScopeId = pnodeFncExprScope->sxBlock.blockId;
    ParseNodePtr pnodeName = pnodeFnc->sxFnc.pnodeName;
    if (pnodeName)
    {TRACE_IT(29516);
        Assert(pnodeName->nop == knopVarDecl);
        BindPidRefsInScope(pnodeName->sxVar.pid, pnodeName->sxVar.sym, fncExprScopeId);
    }
    FinishParseBlock(pnodeFncExprScope);
}

template <const bool backgroundPidRef>
void Parser::BindPidRefs(BlockInfoStack *blockInfo, uint maxBlockId)
{TRACE_IT(29517);
    // We need to bind all assignments in order to emit assignment to 'const' error
    int blockId = blockInfo->pnodeBlock->sxBlock.blockId;

    Scope *scope = blockInfo->pnodeBlock->sxBlock.scope;
    if (scope)
    {TRACE_IT(29518);
        auto bindPidRefs = [blockId, maxBlockId, this](Symbol *sym)
        {TRACE_IT(29519);
            ParseNodePtr pnode = sym->GetDecl();
            IdentPtr pid;
#if PROFILE_DICTIONARY
            int depth = 0;
#endif
            Assert(pnode);
            switch (pnode->nop)
            {
            case knopVarDecl:
            case knopLetDecl:
            case knopConstDecl:
                pid = pnode->sxVar.pid;
                if (backgroundPidRef)
                {TRACE_IT(29520);
                    pid = this->m_pscan->m_phtbl->FindExistingPid(pid->Psz(), pid->Psz() + pid->Cch(), pid->Cch(), pid->Hash(), nullptr, nullptr
#if PROFILE_DICTIONARY
                                                                  , depth
#endif
                        );
                    if (pid == nullptr)
                    {TRACE_IT(29521);
                        break;
                    }
                }
                this->BindPidRefsInScope(pid, sym, blockId, maxBlockId);
                break;
            case knopName:
                pid = pnode->sxPid.pid;
                if (backgroundPidRef)
                {TRACE_IT(29522);
                    pid = this->m_pscan->m_phtbl->FindExistingPid(pid->Psz(), pid->Psz() + pid->Cch(), pid->Cch(), pid->Hash(), nullptr, nullptr
#if PROFILE_DICTIONARY
                                                                  , depth
#endif
                        );
                    if (pid == nullptr)
                    {TRACE_IT(29523);
                        break;
                    }
                }
                this->BindPidRefsInScope(pid, sym, blockId, maxBlockId);
                break;
            default:
                Assert(0);
                break;
            }
        };

        scope->ForEachSymbol(bindPidRefs);
    }
}

void Parser::BindPidRefsInScope(IdentPtr pid, Symbol *sym, int blockId, uint maxBlockId)
{TRACE_IT(29524);
    PidRefStack *ref, *nextRef, *lastRef = nullptr;
    Js::LocalFunctionId funcId = GetCurrentFunctionNode()->sxFnc.functionId;
    Assert(sym);

    if (pid->GetIsModuleExport())
    {TRACE_IT(29525);
        sym->SetIsModuleExportStorage(true);
    }

    bool hasFuncAssignment = sym->GetHasFuncAssignment();
    bool doesEscape = false;

    for (ref = pid->GetTopRef(); ref && ref->GetScopeId() >= blockId; ref = nextRef)
    {TRACE_IT(29526);
        // Fix up sym* on PID ref.
        Assert(!ref->GetSym() || ref->GetSym() == sym);
        nextRef = ref->prev;
        Assert(ref->GetScopeId() >= 0);
        if ((uint)ref->GetScopeId() > maxBlockId)
        {TRACE_IT(29527);
            lastRef = ref;
            continue;
        }
        ref->SetSym(sym);
        this->RemovePrevPidRef(pid, lastRef);

        if (ref->IsAssignment())
        {TRACE_IT(29528);
            sym->PromoteAssignmentState();
            if (m_currentNodeFunc && sym->GetIsFormal())
            {TRACE_IT(29529);
                m_currentNodeFunc->sxFnc.SetHasAnyWriteToFormals(true);
            }
        }

        if (ref->GetFuncScopeId() != funcId && !sym->GetIsGlobal() && !sym->GetIsModuleExportStorage())
        {TRACE_IT(29530);
            Assert(ref->GetFuncScopeId() > funcId);
            sym->SetHasNonLocalReference();
        }

        if (ref->IsFuncAssignment())
        {TRACE_IT(29531);
            hasFuncAssignment = true;
        }

        if (ref->IsEscape())
        {TRACE_IT(29532);
            doesEscape = true;
        }

        if (m_currentNodeFunc && doesEscape && hasFuncAssignment)
        {TRACE_IT(29533);
            if (m_sourceContextInfo ?
                    !PHASE_OFF_RAW(Js::DisableStackFuncOnDeferredEscapePhase, m_sourceContextInfo->sourceContextId, m_currentNodeFunc->sxFnc.functionId) :
                    !PHASE_OFF1(Js::DisableStackFuncOnDeferredEscapePhase))
            {TRACE_IT(29534);
                m_currentNodeFunc->sxFnc.SetNestedFuncEscapes();
            }
        }

        if (ref->GetScopeId() == blockId)
        {TRACE_IT(29535);
            break;
        }
    }
}

void Parser::MarkEscapingRef(ParseNodePtr pnode, IdentToken *pToken)
{TRACE_IT(29536);
    if (m_currentNodeFunc == nullptr)
    {TRACE_IT(29537);
        return;
    }
    if (pnode && pnode->nop == knopFncDecl)
    {TRACE_IT(29538);
        this->SetNestedFuncEscapes();
    }
    else if (pToken->pid)
    {TRACE_IT(29539);
        PidRefStack *pidRef = pToken->pid->GetTopRef();
        if (pidRef->sym)
        {TRACE_IT(29540);
            if (pidRef->sym->GetSymbolType() == STFunction)
            {TRACE_IT(29541);
                this->SetNestedFuncEscapes();
            }
        }
        else
        {TRACE_IT(29542);
            pidRef->isEscape = true;
        }
    }
}

void Parser::SetNestedFuncEscapes() const
{TRACE_IT(29543);
    if (m_sourceContextInfo ?
            !PHASE_OFF_RAW(Js::DisableStackFuncOnDeferredEscapePhase, m_sourceContextInfo->sourceContextId, m_currentNodeFunc->sxFnc.functionId) :
            !PHASE_OFF1(Js::DisableStackFuncOnDeferredEscapePhase))
    {TRACE_IT(29544);
        m_currentNodeFunc->sxFnc.SetNestedFuncEscapes();
    }
}

void Parser::PopStmt(StmtNest *pStmt)
{TRACE_IT(29545);
    Assert(pStmt == m_pstmtCur);
    SetCurrentStatement(m_pstmtCur->pstmtOuter);
}

BlockInfoStack *Parser::PushBlockInfo(ParseNodePtr pnodeBlock)
{TRACE_IT(29546);
    BlockInfoStack *newBlockInfo = (BlockInfoStack *)m_nodeAllocator.Alloc(sizeof(BlockInfoStack));
    Assert(nullptr != newBlockInfo);

    newBlockInfo->pnodeBlock = pnodeBlock;
    newBlockInfo->pBlockInfoOuter = m_currentBlockInfo;
    newBlockInfo->m_ppnodeLex = &pnodeBlock->sxBlock.pnodeLexVars;

    if (pnodeBlock->sxBlock.blockType != PnodeBlockType::Regular)
    {TRACE_IT(29547);
        newBlockInfo->pBlockInfoFunction = newBlockInfo;
    }
    else
    {TRACE_IT(29548);
        Assert(m_currentBlockInfo);
        newBlockInfo->pBlockInfoFunction = m_currentBlockInfo->pBlockInfoFunction;
    }

    m_currentBlockInfo = newBlockInfo;
    return newBlockInfo;
}

void Parser::PopBlockInfo()
{TRACE_IT(29549);
    Assert(m_currentBlockInfo);
    PopDynamicBlock();
    m_currentBlockInfo = m_currentBlockInfo->pBlockInfoOuter;
}

void Parser::PushDynamicBlock()
{TRACE_IT(29550);
    Assert(GetCurrentBlock());
    int blockId = GetCurrentBlock()->sxBlock.blockId;
    if (m_currentDynamicBlock && m_currentDynamicBlock->id == blockId)
    {TRACE_IT(29551);
        return;
    }
    BlockIdsStack *info = (BlockIdsStack *)m_nodeAllocator.Alloc(sizeof(BlockIdsStack));
    if (nullptr == info)
    {TRACE_IT(29552);
        Error(ERRnoMemory);
    }

    info->id = blockId;
    info->prev = m_currentDynamicBlock;
    m_currentDynamicBlock = info;
}

void Parser::PopDynamicBlock()
{TRACE_IT(29553);
    int blockId = GetCurrentDynamicBlockId();
    if (GetCurrentBlock()->sxBlock.blockId != blockId || blockId == -1)
    {TRACE_IT(29554);
        return;
    }
    Assert(m_currentDynamicBlock);
    for (BlockInfoStack *blockInfo = m_currentBlockInfo; blockInfo; blockInfo = blockInfo->pBlockInfoOuter)
    {TRACE_IT(29555);
        for (ParseNodePtr pnodeDecl = blockInfo->pnodeBlock->sxBlock.pnodeLexVars;
             pnodeDecl;
             pnodeDecl = pnodeDecl->sxVar.pnodeNext)
        {TRACE_IT(29556);
            this->SetPidRefsInScopeDynamic(pnodeDecl->sxVar.pid, blockId);
        }
    }

    m_currentDynamicBlock = m_currentDynamicBlock->prev;
}

int Parser::GetCurrentDynamicBlockId() const
{TRACE_IT(29557);
    return m_currentDynamicBlock ? m_currentDynamicBlock->id : -1;
}

ParseNode *Parser::GetCurrentFunctionNode()
{TRACE_IT(29558);
    if (m_currentNodeDeferredFunc != nullptr)
    {TRACE_IT(29559);
        return m_currentNodeDeferredFunc;
    }
    else if (m_currentNodeFunc != nullptr)
    {TRACE_IT(29560);
        return m_currentNodeFunc;
    }
    else
    {TRACE_IT(29561);
        AssertMsg(GetFunctionBlock()->sxBlock.blockType == PnodeBlockType::Global,
            "Most likely we are trying to find a syntax error, related to 'let' or 'const' in deferred parsing mode with disabled support of 'let' and 'const'");
        return m_currentNodeProg;
    }
}

ParseNode *Parser::GetCurrentNonLambdaFunctionNode()
{TRACE_IT(29562);
    if (m_currentNodeNonLambdaDeferredFunc != nullptr)
    {TRACE_IT(29563);
        return m_currentNodeNonLambdaDeferredFunc;
    }
    return m_currentNodeNonLambdaFunc;

}
void Parser::RegisterRegexPattern(UnifiedRegex::RegexPattern *const regexPattern)
{TRACE_IT(29564);
    Assert(regexPattern);

    // ensure a no-throw add behavior here, to catch out of memory exceptions, using the guest arena allocator
    if (!m_registeredRegexPatterns.PrependNoThrow(m_scriptContext->GetGuestArena(), regexPattern))
    {TRACE_IT(29565);
        Parser::Error(ERRnoMemory);
    }
}

void Parser::CaptureState(ParserState *state)
{TRACE_IT(29566);
    Assert(state != nullptr);

    state->m_funcInArraySave = m_funcInArray;
    state->m_funcInArrayDepthSave = m_funcInArrayDepth;
    state->m_nestedCountSave = *m_pnestedCount;
    state->m_ppnodeScopeSave = m_ppnodeScope;
    state->m_ppnodeExprScopeSave = m_ppnodeExprScope;
    state->m_pCurrentAstSizeSave = m_pCurrentAstSize;
    state->m_nextBlockId = m_nextBlockId;

    Assert(state->m_ppnodeScopeSave == nullptr || *state->m_ppnodeScopeSave == nullptr);
    Assert(state->m_ppnodeExprScopeSave == nullptr || *state->m_ppnodeExprScopeSave == nullptr);

#if DEBUG
    state->m_currentBlockInfo = m_currentBlockInfo;
#endif
}

void Parser::RestoreStateFrom(ParserState *state)
{TRACE_IT(29567);
    Assert(state != nullptr);
    Assert(state->m_currentBlockInfo == m_currentBlockInfo);

    m_funcInArray = state->m_funcInArraySave;
    m_funcInArrayDepth = state->m_funcInArrayDepthSave;
    *m_pnestedCount = state->m_nestedCountSave;
    m_pCurrentAstSize = state->m_pCurrentAstSizeSave;
    m_nextBlockId = state->m_nextBlockId;

    if (state->m_ppnodeScopeSave != nullptr)
    {TRACE_IT(29568);
        *state->m_ppnodeScopeSave = nullptr;
    }

    if (state->m_ppnodeExprScopeSave != nullptr)
    {TRACE_IT(29569);
        *state->m_ppnodeExprScopeSave = nullptr;
    }

    m_ppnodeScope = state->m_ppnodeScopeSave;
    m_ppnodeExprScope = state->m_ppnodeExprScopeSave;
}

void Parser::AddToNodeListEscapedUse(ParseNode ** ppnodeList, ParseNode *** pppnodeLast,
                           ParseNode * pnodeAdd)
{
    AddToNodeList(ppnodeList, pppnodeLast, pnodeAdd);
    pnodeAdd->SetIsInList();
}

void Parser::AddToNodeList(ParseNode ** ppnodeList, ParseNode *** pppnodeLast,
                           ParseNode * pnodeAdd)
{TRACE_IT(29570);
    Assert(!this->m_deferringAST);
    if (nullptr == *pppnodeLast)
    {TRACE_IT(29571);
        // should be an empty list
        Assert(nullptr == *ppnodeList);

        *ppnodeList = pnodeAdd;
        *pppnodeLast = ppnodeList;
    }
    else
    {TRACE_IT(29572);
        //
        AssertNodeMem(*ppnodeList);
        AssertNodeMem(**pppnodeLast);

        ParseNode *pnodeT = CreateBinNode(knopList, **pppnodeLast, pnodeAdd);
        **pppnodeLast = pnodeT;
        *pppnodeLast = &pnodeT->sxBin.pnode2;
    }
}

// Check reference to "arguments" that indicates the object may escape.
void Parser::CheckArguments(ParseNodePtr pnode)
{TRACE_IT(29573);
    if (m_currentNodeFunc && this->NodeIsIdent(pnode, wellKnownPropertyPids.arguments))
    {TRACE_IT(29574);
        m_currentNodeFunc->sxFnc.SetHasHeapArguments();
    }
}

// Check use of "arguments" that requires instantiation of the object.
void Parser::CheckArgumentsUse(IdentPtr pid, ParseNodePtr pnodeFnc)
{TRACE_IT(29575);
    if (pid == wellKnownPropertyPids.arguments)
    {TRACE_IT(29576);
        if (pnodeFnc != nullptr && pnodeFnc != m_currentNodeProg)
        {TRACE_IT(29577);
            pnodeFnc->sxFnc.SetUsesArguments(TRUE);
        }
        else
        {TRACE_IT(29578);
            m_UsesArgumentsAtGlobal = true;
        }
    }
}

void Parser::CheckStrictModeEvalArgumentsUsage(IdentPtr pid, ParseNodePtr pnode)
{TRACE_IT(29579);
    if (pid != nullptr)
    {TRACE_IT(29580);
        // In strict mode, 'eval' / 'arguments' cannot be assigned to.
        if ( pid == wellKnownPropertyPids.eval)
        {
            Error(ERREvalUsage, pnode);
        }

        if (pid == wellKnownPropertyPids.arguments)
        {
            Error(ERRArgsUsage, pnode);
        }
    }
}

void Parser::ReduceDeferredScriptLength(size_t chars)
{TRACE_IT(29581);
    // If we're in deferred mode, subtract the given char count from the total length,
    // and see if this puts us under the deferral threshold.
    if ((m_grfscr & fscrDeferFncParse) &&
        (
            PHASE_OFF1(Js::DeferEventHandlersPhase) ||
            (m_grfscr & fscrGlobalCode)
        )
    )
    {TRACE_IT(29582);
        if (m_length > chars)
        {TRACE_IT(29583);
            m_length -= chars;
        }
        else
        {TRACE_IT(29584);
            m_length = 0;
        }
        if (m_length < Parser::GetDeferralThreshold(this->m_sourceContextInfo->IsSourceProfileLoaded()))
        {TRACE_IT(29585);
            // Stop deferring.
            m_grfscr &= ~fscrDeferFncParse;
            m_stoppedDeferredParse = TRUE;
        }
    }
}

/***************************************************************************
Look for an existing label with the given name.
***************************************************************************/
BOOL Parser::PnodeLabelNoAST(IdentToken* pToken, LabelId* pLabelIdList)
{TRACE_IT(29586);
    StmtNest* pStmt;
    LabelId* pLabelId;

    // Look in the label stack.
    for (pStmt = m_pstmtCur; pStmt != nullptr; pStmt = pStmt->pstmtOuter)
    {TRACE_IT(29587);
        for (pLabelId = pStmt->pLabelId; pLabelId != nullptr; pLabelId = pLabelId->next)
        {TRACE_IT(29588);
            if (pLabelId->pid == pToken->pid)
                return TRUE;
        }
    }

    // Also look in the pnodeLabels list.
    for (pLabelId = pLabelIdList; pLabelId != nullptr; pLabelId = pLabelId->next)
    {TRACE_IT(29589);
        if (pLabelId->pid == pToken->pid)
            return TRUE;
    }

    return FALSE;
}

void Parser::EnsureStackAvailable()
{TRACE_IT(29590);
    if (!m_scriptContext->GetThreadContext()->IsStackAvailable(Js::Constants::MinStackCompile))
    {TRACE_IT(29591);
        Error(ERRnoMemory);
    }
}

void Parser::ThrowNewTargetSyntaxErrForGlobalScope()
{TRACE_IT(29592);
    if (GetCurrentNonLambdaFunctionNode() != nullptr)
    {TRACE_IT(29593);
        return;
    }

    if ((this->m_grfscr & fscrEval) != 0)
    {TRACE_IT(29594);
        Js::JavascriptFunction * caller = nullptr;
        if (Js::JavascriptStackWalker::GetCaller(&caller, m_scriptContext))
        {TRACE_IT(29595);
            Js::FunctionBody * callerBody = caller->GetFunctionBody();
            Assert(callerBody);
            if (!callerBody->GetIsGlobalFunc() && !(callerBody->IsLambda() && callerBody->GetEnclosedByGlobalFunc()))
            {TRACE_IT(29596);
                return;
            }
        }
    }

    Error(ERRInvalidNewTarget);
 }

template<bool buildAST>
ParseNodePtr Parser::ParseMetaProperty(tokens metaParentKeyword, charcount_t ichMin, _Out_opt_ BOOL* pfCanAssign)
{TRACE_IT(29597);
    AssertMsg(metaParentKeyword == tkNEW, "Only supported for tkNEW parent keywords");
    AssertMsg(this->m_token.tk == tkDot, "We must be currently sitting on the dot after the parent keyword");

    m_pscan->Scan();

    if (this->m_token.tk == tkID && this->m_token.GetIdentifier(m_phtbl) == this->GetTargetPid())
    {TRACE_IT(29598);
        ThrowNewTargetSyntaxErrForGlobalScope();
        if (pfCanAssign)
        {TRACE_IT(29599);
            *pfCanAssign = FALSE;
        }
        if (buildAST)
        {TRACE_IT(29600);
            return CreateNodeWithScanner<knopNewTarget>(ichMin);
        }
    }
    else
    {TRACE_IT(29601);
        Error(ERRsyntax);
    }

    return nullptr;
}

template<bool buildAST>
void Parser::ParseNamedImportOrExportClause(ModuleImportOrExportEntryList* importOrExportEntryList, bool isExportClause)
{TRACE_IT(29602);
    Assert(m_token.tk == tkLCurly);
    Assert(importOrExportEntryList != nullptr);

    m_pscan->Scan();

    while (m_token.tk != tkRCurly && m_token.tk != tkEOF)
    {TRACE_IT(29603);
        tokens firstToken = m_token.tk;

        if (!(m_token.IsIdentifier() || m_token.IsReservedWord()))
        {TRACE_IT(29604);
            Error(ERRsyntax);
        }

        IdentPtr identifierName = m_token.GetIdentifier(m_phtbl);
        IdentPtr identifierAs = identifierName;

        m_pscan->Scan();

        if (m_token.tk == tkID)
        {TRACE_IT(29605);
            // We have the pattern "IdentifierName as"
            if (wellKnownPropertyPids.as != m_token.GetIdentifier(m_phtbl))
            {TRACE_IT(29606);
                Error(ERRsyntax);
            }

            m_pscan->Scan();

            // If we are parsing an import statement, the token after 'as' must be a BindingIdentifier.
            if (!isExportClause)
            {
                ChkCurTokNoScan(tkID, ERRsyntax);
            }

            if (!(m_token.IsIdentifier() || m_token.IsReservedWord()))
            {TRACE_IT(29607);
                Error(ERRsyntax);
            }

            identifierAs = m_token.GetIdentifier(m_phtbl);

            // Scan to the next token.
            m_pscan->Scan();
        }
        else if (!isExportClause && firstToken != tkID)
        {TRACE_IT(29608);
            // If we are parsing an import statement and this ImportSpecifier clause did not have
            // 'as ImportedBinding' at the end of it, identifierName must be a BindingIdentifier.
            Error(ERRsyntax);
        }

        if (m_token.tk == tkComma)
        {TRACE_IT(29609);
            // Consume a trailing comma
            m_pscan->Scan();
        }

        if (buildAST)
        {TRACE_IT(29610);
            // The name we will use 'as' this import/export is a binding identifier in import statements.
            if (!isExportClause)
            {TRACE_IT(29611);
                CreateModuleImportDeclNode(identifierAs);
                AddModuleImportOrExportEntry(importOrExportEntryList, identifierName, identifierAs, nullptr, nullptr);
            }
            else
            {TRACE_IT(29612);
                identifierName->SetIsModuleExport();
                AddModuleImportOrExportEntry(importOrExportEntryList, nullptr, identifierName, identifierAs, nullptr);
            }
        }
    }

    // Final token in a named import or export clause must be a '}'
    ChkCurTokNoScan(tkRCurly, ERRsyntax);
}

IdentPtrList* Parser::GetRequestedModulesList()
{TRACE_IT(29613);
    return m_currentNodeProg->sxModule.requestedModules;
}

ModuleImportOrExportEntryList* Parser::GetModuleImportEntryList()
{TRACE_IT(29614);
    return m_currentNodeProg->sxModule.importEntries;
}

ModuleImportOrExportEntryList* Parser::GetModuleLocalExportEntryList()
{TRACE_IT(29615);
    return m_currentNodeProg->sxModule.localExportEntries;
}

ModuleImportOrExportEntryList* Parser::GetModuleIndirectExportEntryList()
{TRACE_IT(29616);
    return m_currentNodeProg->sxModule.indirectExportEntries;
}

ModuleImportOrExportEntryList* Parser::GetModuleStarExportEntryList()
{TRACE_IT(29617);
    return m_currentNodeProg->sxModule.starExportEntries;
}

IdentPtrList* Parser::EnsureRequestedModulesList()
{TRACE_IT(29618);
    if (m_currentNodeProg->sxModule.requestedModules == nullptr)
    {TRACE_IT(29619);
        m_currentNodeProg->sxModule.requestedModules = Anew(&m_nodeAllocator, IdentPtrList, &m_nodeAllocator);
    }
    return m_currentNodeProg->sxModule.requestedModules;
}

ModuleImportOrExportEntryList* Parser::EnsureModuleImportEntryList()
{TRACE_IT(29620);
    if (m_currentNodeProg->sxModule.importEntries == nullptr)
    {TRACE_IT(29621);
        m_currentNodeProg->sxModule.importEntries = Anew(&m_nodeAllocator, ModuleImportOrExportEntryList, &m_nodeAllocator);
    }
    return m_currentNodeProg->sxModule.importEntries;
}

ModuleImportOrExportEntryList* Parser::EnsureModuleLocalExportEntryList()
{TRACE_IT(29622);
    if (m_currentNodeProg->sxModule.localExportEntries == nullptr)
    {TRACE_IT(29623);
        m_currentNodeProg->sxModule.localExportEntries = Anew(&m_nodeAllocator, ModuleImportOrExportEntryList, &m_nodeAllocator);
    }
    return m_currentNodeProg->sxModule.localExportEntries;
}

ModuleImportOrExportEntryList* Parser::EnsureModuleIndirectExportEntryList()
{TRACE_IT(29624);
    if (m_currentNodeProg->sxModule.indirectExportEntries == nullptr)
    {TRACE_IT(29625);
        m_currentNodeProg->sxModule.indirectExportEntries = Anew(&m_nodeAllocator, ModuleImportOrExportEntryList, &m_nodeAllocator);
    }
    return m_currentNodeProg->sxModule.indirectExportEntries;
}

ModuleImportOrExportEntryList* Parser::EnsureModuleStarExportEntryList()
{TRACE_IT(29626);
    if (m_currentNodeProg->sxModule.starExportEntries == nullptr)
    {TRACE_IT(29627);
        m_currentNodeProg->sxModule.starExportEntries = Anew(&m_nodeAllocator, ModuleImportOrExportEntryList, &m_nodeAllocator);
    }
    return m_currentNodeProg->sxModule.starExportEntries;
}

void Parser::AddModuleSpecifier(IdentPtr moduleRequest)
{TRACE_IT(29628);
    IdentPtrList* requestedModulesList = EnsureRequestedModulesList();

    if (!requestedModulesList->Has(moduleRequest))
    {TRACE_IT(29629);
        requestedModulesList->Prepend(moduleRequest);
    }
}

ModuleImportOrExportEntry* Parser::AddModuleImportOrExportEntry(ModuleImportOrExportEntryList* importOrExportEntryList, ModuleImportOrExportEntry* importOrExportEntry)
{TRACE_IT(29630);
    if (importOrExportEntry->exportName != nullptr)
    {
        CheckForDuplicateExportEntry(importOrExportEntryList, importOrExportEntry->exportName);
    }

    importOrExportEntryList->Prepend(*importOrExportEntry);

    return importOrExportEntry;
}

ModuleImportOrExportEntry* Parser::AddModuleImportOrExportEntry(ModuleImportOrExportEntryList* importOrExportEntryList, IdentPtr importName, IdentPtr localName, IdentPtr exportName, IdentPtr moduleRequest)
{TRACE_IT(29631);
    ModuleImportOrExportEntry* importOrExportEntry = Anew(&m_nodeAllocator, ModuleImportOrExportEntry);

    importOrExportEntry->importName = importName;
    importOrExportEntry->localName = localName;
    importOrExportEntry->exportName = exportName;
    importOrExportEntry->moduleRequest = moduleRequest;

    return AddModuleImportOrExportEntry(importOrExportEntryList, importOrExportEntry);
}

void Parser::AddModuleLocalExportEntry(ParseNodePtr varDeclNode)
{TRACE_IT(29632);
    Assert(varDeclNode->nop == knopVarDecl || varDeclNode->nop == knopLetDecl || varDeclNode->nop == knopConstDecl);

    IdentPtr localName = varDeclNode->sxVar.pid;
    varDeclNode->sxVar.sym->SetIsModuleExportStorage(true);

    AddModuleImportOrExportEntry(EnsureModuleLocalExportEntryList(), nullptr, localName, localName, nullptr);
}

void Parser::CheckForDuplicateExportEntry(ModuleImportOrExportEntryList* exportEntryList, IdentPtr exportName)
{TRACE_IT(29633);
    ModuleImportOrExportEntry* findResult = exportEntryList->Find([&](ModuleImportOrExportEntry exportEntry)
    {
        if (exportName == exportEntry.exportName)
        {TRACE_IT(29634);
            return true;
        }
        return false;
    });

    if (findResult != nullptr)
    {TRACE_IT(29635);
        Error(ERRsyntax);
    }
}

template<bool buildAST>
void Parser::ParseImportClause(ModuleImportOrExportEntryList* importEntryList, bool parsingAfterComma)
{TRACE_IT(29636);
    bool parsedNamespaceOrNamedImport = false;

    switch (m_token.tk)
    {
    case tkID:
        // This is the default binding identifier.

        // If we already saw a comma in the import clause, this is a syntax error.
        if (parsingAfterComma)
        {TRACE_IT(29637);
            Error(ERRsyntax);
        }

        if (buildAST)
        {TRACE_IT(29638);
            IdentPtr localName = m_token.GetIdentifier(m_phtbl);
            IdentPtr importName = wellKnownPropertyPids._default;

            CreateModuleImportDeclNode(localName);
            AddModuleImportOrExportEntry(importEntryList, importName, localName, nullptr, nullptr);
        }

        break;

    case tkLCurly:
        // This begins a list of named imports.
        ParseNamedImportOrExportClause<buildAST>(importEntryList, false);

        parsedNamespaceOrNamedImport = true;
        break;

    case tkStar:
        // This begins a namespace import clause.
        // "* as ImportedBinding"

        // Token following * must be the identifier 'as'
        m_pscan->Scan();
        if (m_token.tk != tkID || wellKnownPropertyPids.as != m_token.GetIdentifier(m_phtbl))
        {TRACE_IT(29639);
            Error(ERRsyntax);
        }

        // Token following 'as' must be a binding identifier.
        m_pscan->Scan();
        ChkCurTokNoScan(tkID, ERRsyntax);

        if (buildAST)
        {TRACE_IT(29640);
            IdentPtr localName = m_token.GetIdentifier(m_phtbl);
            IdentPtr importName = wellKnownPropertyPids._star;

            CreateModuleImportDeclNode(localName);
            AddModuleImportOrExportEntry(importEntryList, importName, localName, nullptr, nullptr);
        }

        parsedNamespaceOrNamedImport = true;
        break;

    default:
        Error(ERRsyntax);
    }

    m_pscan->Scan();

    if (m_token.tk == tkComma)
    {TRACE_IT(29641);
        // There cannot be more than one comma in a module import clause.
        // There cannot be a namespace import or named imports list on the left of the comma in a module import clause.
        if (parsingAfterComma || parsedNamespaceOrNamedImport)
        {TRACE_IT(29642);
            Error(ERRsyntax);
        }

        m_pscan->Scan();

        ParseImportClause<buildAST>(importEntryList, true);
    }
}

bool Parser::IsImportOrExportStatementValidHere()
{TRACE_IT(29643);
    ParseNodePtr curFunc = GetCurrentFunctionNode();

    // Import must be located in the top scope of the module body.
    return curFunc->nop == knopFncDecl
        && curFunc->sxFnc.IsModule()
        && this->m_currentBlockInfo->pnodeBlock == curFunc->sxFnc.pnodeBodyScope
        && (this->m_grfscr & fscrEvalCode) != fscrEvalCode
        && this->m_tryCatchOrFinallyDepth == 0;
}

template<bool buildAST>
ParseNodePtr Parser::ParseImportDeclaration()
{TRACE_IT(29644);
    Assert(m_scriptContext->GetConfig()->IsES6ModuleEnabled());
    Assert(m_token.tk == tkIMPORT);

    if (!IsImportOrExportStatementValidHere())
    {TRACE_IT(29645);
        Error(ERRInvalidModuleImportOrExport);
    }

    // We just parsed an import token. Next valid token is *, {, string constant, or binding identifier.
    m_pscan->Scan();

    if (m_token.tk == tkStrCon)
    {TRACE_IT(29646);
        // This import declaration has no import clause.
        // "import ModuleSpecifier;"
        if (buildAST)
        {TRACE_IT(29647);
            AddModuleSpecifier(m_token.GetStr());
        }

        // Scan past the module identifier.
        m_pscan->Scan();
    }
    else
    {TRACE_IT(29648);
        ModuleImportOrExportEntryList importEntryList(&m_nodeAllocator);

        // Parse the import clause (default binding can only exist before the comma).
        ParseImportClause<buildAST>(&importEntryList);

        // Token following import clause must be the identifier 'from'
        IdentPtr moduleSpecifier = ParseImportOrExportFromClause<buildAST>(true);

        if (buildAST)
        {TRACE_IT(29649);
            Assert(moduleSpecifier != nullptr);

            AddModuleSpecifier(moduleSpecifier);

            importEntryList.Map([this, moduleSpecifier](ModuleImportOrExportEntry& importEntry) {
                importEntry.moduleRequest = moduleSpecifier;
                AddModuleImportOrExportEntry(EnsureModuleImportEntryList(), &importEntry);
            });
        }

        importEntryList.Clear();
    }

    // Import statement is actually a nop, we hoist all the imported bindings to the top of the module.
    return nullptr;
}

template<bool buildAST>
IdentPtr Parser::ParseImportOrExportFromClause(bool throwIfNotFound)
{TRACE_IT(29650);
    IdentPtr moduleSpecifier = nullptr;

    if (m_token.tk == tkID && wellKnownPropertyPids.from == m_token.GetIdentifier(m_phtbl))
    {TRACE_IT(29651);
        m_pscan->Scan();

        // Token following the 'from' token must be a string constant - the module specifier.
        ChkCurTokNoScan(tkStrCon, ERRsyntax);

        if (buildAST)
        {TRACE_IT(29652);
            moduleSpecifier = m_token.GetStr();
        }

        m_pscan->Scan();
    }
    else if (throwIfNotFound)
    {TRACE_IT(29653);
        Error(ERRsyntax);
    }

    return moduleSpecifier;
}

template<bool buildAST>
ParseNodePtr Parser::ParseDefaultExportClause()
{TRACE_IT(29654);
    Assert(m_token.tk == tkDEFAULT);

    m_pscan->Scan();
    ParseNodePtr pnode = nullptr;
    ushort flags = fFncNoFlgs;

    switch (m_token.tk)
    {
    case tkCLASS:
        {TRACE_IT(29655);
            if (!m_scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled())
            {TRACE_IT(29656);
                goto LDefault;
            }

            // Before we parse the class itself we need to know if the class has an identifier name.
            // If it does, we'll treat this class as an ordinary class declaration which will bind
            // it to that name. Otherwise the class should parse as a nameless class expression and
            // bind only to the export binding.
            BOOL classHasName = false;
            RestorePoint parsedClass;
            m_pscan->Capture(&parsedClass);
            m_pscan->Scan();

            if (m_token.tk == tkID)
            {TRACE_IT(29657);
                classHasName = true;
            }

            m_pscan->SeekTo(parsedClass);
            pnode = ParseClassDecl<buildAST>(classHasName, nullptr, nullptr, nullptr);

            if (buildAST)
            {TRACE_IT(29658);
                AnalysisAssert(pnode != nullptr);
                Assert(pnode->nop == knopClassDecl);

                pnode->sxClass.SetIsDefaultModuleExport(true);
            }

            break;
        }
    case tkID:
        // If we parsed an async token, it could either modify the next token (if it is a
        // function token) or it could be an identifier (let async = 0; export default async;).
        // To handle both cases, when we parse an async token we need to keep the parser state
        // and rewind if the next token is not function.
        if (wellKnownPropertyPids.async == m_token.GetIdentifier(m_phtbl))
        {TRACE_IT(29659);
            RestorePoint parsedAsync;
            m_pscan->Capture(&parsedAsync);
            m_pscan->Scan();
            if (m_token.tk == tkFUNCTION)
            {TRACE_IT(29660);
                // Token after async is function, consume the async token and continue to parse the
                // function as an async function.
                flags |= fFncAsync;
                goto LFunction;
            }
            // Token after async is not function, no idea what the async token is supposed to mean
            // so rewind and let the default case handle it.
            m_pscan->SeekTo(parsedAsync);
        }
        goto LDefault;
        break;
    case tkFUNCTION:
        {TRACE_IT(29661);
LFunction:
            // We just parsed a function token but we need to figure out if the function
            // has an identifier name or not before we call the helper.
            RestorePoint parsedFunction;
            m_pscan->Capture(&parsedFunction);
            m_pscan->Scan();

            if (m_token.tk == tkStar)
            {TRACE_IT(29662);
                // If we saw 'function*' that indicates we are going to parse a generator,
                // but doesn't tell us if the generator has an identifier or not.
                // Skip the '*' token for now as it doesn't matter yet.
                m_pscan->Scan();
            }

            // We say that if the function has an identifier name, it is a 'normal' declaration
            // and should create a binding to that identifier as well as one for our default export.
            if (m_token.tk == tkID)
            {TRACE_IT(29663);
                flags |= fFncDeclaration;
            }
            else
            {TRACE_IT(29664);
                flags |= fFncNoName;
            }

            // Rewind back to the function token and let the helper handle the parsing.
            m_pscan->SeekTo(parsedFunction);
            pnode = ParseFncDecl<buildAST>(flags);

            if (buildAST)
            {TRACE_IT(29665);
                AnalysisAssert(pnode != nullptr);
                Assert(pnode->nop == knopFncDecl);

                pnode->sxFnc.SetIsDefaultModuleExport(true);
            }
            break;
        }
    default:
LDefault:
        {
            ParseNodePtr pnodeExpression = ParseExpr<buildAST>();

            // Consider: Can we detect this syntax error earlier?
            if (pnodeExpression && pnodeExpression->nop == knopComma)
            {TRACE_IT(29666);
                Error(ERRsyntax);
            }

            if (buildAST)
            {TRACE_IT(29667);
                AnalysisAssert(pnodeExpression != nullptr);

                // Mark this node as the default module export. We need to make sure it is put into the correct
                // module export slot when we emit the node.
                pnode = CreateNode(knopExportDefault);
                pnode->sxExportDefault.pnodeExpr = pnodeExpression;
            }
            break;
        }
    }

    IdentPtr exportName = wellKnownPropertyPids._default;
    IdentPtr localName = wellKnownPropertyPids._starDefaultStar;
    AddModuleImportOrExportEntry(EnsureModuleLocalExportEntryList(), nullptr, localName, exportName, nullptr);

    return pnode;
}

template<bool buildAST>
ParseNodePtr Parser::ParseExportDeclaration()
{TRACE_IT(29668);
    Assert(m_scriptContext->GetConfig()->IsES6ModuleEnabled());
    Assert(m_token.tk == tkEXPORT);

    if (!IsImportOrExportStatementValidHere())
    {TRACE_IT(29669);
        Error(ERRInvalidModuleImportOrExport);
    }

    ParseNodePtr pnode = nullptr;
    IdentPtr moduleIdentifier = nullptr;
    tokens declarationType;

    // We just parsed an export token. Next valid tokens are *, {, var, let, const, async, function, class, default.
    m_pscan->Scan();

    switch (m_token.tk)
    {
    case tkStar:
        m_pscan->Scan();

        // A star token in an export declaration must be followed by a from clause which begins with a token 'from'.
        moduleIdentifier = ParseImportOrExportFromClause<buildAST>(true);

        if (buildAST)
        {TRACE_IT(29670);
            Assert(moduleIdentifier != nullptr);

            AddModuleSpecifier(moduleIdentifier);
            IdentPtr importName = wellKnownPropertyPids._star;

            AddModuleImportOrExportEntry(EnsureModuleStarExportEntryList(), importName, nullptr, nullptr, moduleIdentifier);
        }

        break;

    case tkLCurly:
        {TRACE_IT(29671);
            ModuleImportOrExportEntryList exportEntryList(&m_nodeAllocator);

            ParseNamedImportOrExportClause<buildAST>(&exportEntryList, true);

            m_pscan->Scan();

            // Export clause may be followed by a from clause.
            moduleIdentifier = ParseImportOrExportFromClause<buildAST>(false);

            if (buildAST)
            {TRACE_IT(29672);
                if (moduleIdentifier != nullptr)
                {TRACE_IT(29673);
                    AddModuleSpecifier(moduleIdentifier);
                }

                exportEntryList.Map([this, moduleIdentifier](ModuleImportOrExportEntry& exportEntry) {
                    if (moduleIdentifier != nullptr)
                    {TRACE_IT(29674);
                        exportEntry.moduleRequest = moduleIdentifier;

                        // We need to swap localname and importname when this is a re-export.
                        exportEntry.importName = exportEntry.localName;
                        exportEntry.localName = nullptr;

                        AddModuleImportOrExportEntry(EnsureModuleIndirectExportEntryList(), &exportEntry);
                    }
                    else
                    {TRACE_IT(29675);
                        AddModuleImportOrExportEntry(EnsureModuleLocalExportEntryList(), &exportEntry);
                    }
                });

                exportEntryList.Clear();
            }
        }
        break;

    case tkID:
        {TRACE_IT(29676);
            IdentPtr pid = m_token.GetIdentifier(m_phtbl);

            if (wellKnownPropertyPids.let == pid)
            {TRACE_IT(29677);
                declarationType = tkLET;
                goto ParseVarDecl;
            }
            if (wellKnownPropertyPids.async == pid && m_scriptContext->GetConfig()->IsES7AsyncAndAwaitEnabled())
            {TRACE_IT(29678);
                // In module export statements, async token is only valid if it's followed by function.
                // We need to check here because ParseStatement would think 'async = 20' is a var decl.
                RestorePoint parsedAsync;
                m_pscan->Capture(&parsedAsync);
                m_pscan->Scan();
                if (m_token.tk == tkFUNCTION)
                {TRACE_IT(29679);
                    // Token after async is function, rewind to the async token and let ParseStatement handle it.
                    m_pscan->SeekTo(parsedAsync);
                    goto ParseFunctionDecl;
                }
                // Token after async is not function, it's a syntax error.
            }
            goto ErrorToken;
        }
    case tkVAR:
    case tkLET:
    case tkCONST:
        {TRACE_IT(29680);
            declarationType = m_token.tk;

ParseVarDecl:
            m_pscan->Scan();

            pnode = ParseVariableDeclaration<buildAST>(declarationType, m_pscan->IchMinTok());

            if (buildAST)
            {TRACE_IT(29681);
                ParseNodePtr temp = pnode;
                while (temp->nop == knopList)
                {TRACE_IT(29682);
                    ParseNodePtr varDeclNode = temp->sxBin.pnode1;
                    temp = temp->sxBin.pnode2;

                    AddModuleLocalExportEntry(varDeclNode);
                }
                AddModuleLocalExportEntry(temp);
            }
        }
        break;

    case tkFUNCTION:
    case tkCLASS:
        {TRACE_IT(29683);
ParseFunctionDecl:
            pnode = ParseStatement<buildAST>();

            if (buildAST)
            {TRACE_IT(29684);
                IdentPtr localName;
                if (pnode->nop == knopClassDecl)
                {TRACE_IT(29685);
                    pnode->sxClass.pnodeName->sxVar.sym->SetIsModuleExportStorage(true);
                    pnode->sxClass.pnodeDeclName->sxVar.sym->SetIsModuleExportStorage(true);
                    localName = pnode->sxClass.pnodeName->sxVar.pid;
                }
                else
                {TRACE_IT(29686);
                    Assert(pnode->nop == knopFncDecl);

                    pnode->sxFnc.GetFuncSymbol()->SetIsModuleExportStorage(true);
                    localName = pnode->sxFnc.pid;
                }
                Assert(localName != nullptr);

                AddModuleImportOrExportEntry(EnsureModuleLocalExportEntryList(), nullptr, localName, localName, nullptr);
            }
        }
        break;

    case tkDEFAULT:
        {TRACE_IT(29687);
            pnode = ParseDefaultExportClause<buildAST>();
        }
        break;

    default:
        {
ErrorToken:
            Error(ERRsyntax);
        }
    }

    return pnode;
}

/***************************************************************************
Parse an expression term.
***************************************************************************/
template<bool buildAST>
ParseNodePtr Parser::ParseTerm(BOOL fAllowCall,
    LPCOLESTR pNameHint,
    uint32 *pHintLength,
    uint32 *pShortNameOffset,
    _Inout_opt_ IdentToken* pToken /*= nullptr*/,
    bool fUnaryOrParen /*= false*/,
    _Out_opt_ BOOL* pfCanAssign /*= nullptr*/,
    _Inout_opt_ BOOL* pfLikelyPattern /*= nullptr*/,
    _Out_opt_ bool* pfIsDotOrIndex /*= nullptr*/,
    _Inout_opt_ charcount_t *plastRParen /*= nullptr*/)
{TRACE_IT(29688);
    ParseNodePtr pnode = nullptr;
    PidRefStack *savedTopAsyncRef = nullptr;
    charcount_t ichMin = 0;
    size_t iecpMin = 0;
    size_t iuMin;
    IdentToken term;
    BOOL fInNew = FALSE;
    BOOL fCanAssign = TRUE;
    bool isAsyncExpr = false;
    bool isLambdaExpr = false;
    Assert(pToken == nullptr || pToken->tk == tkNone); // Must be empty initially

    if (this->IsBackgroundParser())
    {
        PROBE_STACK_NO_DISPOSE(m_scriptContext, Js::Constants::MinStackParseOneTerm);
    }
    else
    {
        PROBE_STACK(m_scriptContext, Js::Constants::MinStackParseOneTerm);
    }

    switch (m_token.tk)
    {
    case tkID:
    {TRACE_IT(29689);
        PidRefStack *ref = nullptr;
        IdentPtr pid = m_token.GetIdentifier(m_phtbl);
        charcount_t ichLim = m_pscan->IchLimTok();
        size_t iecpLim = m_pscan->IecpLimTok();
        ichMin = m_pscan->IchMinTok();
        iecpMin  = m_pscan->IecpMinTok();

        if (pid == wellKnownPropertyPids.async &&
            m_scriptContext->GetConfig()->IsES7AsyncAndAwaitEnabled())
        {TRACE_IT(29690);
            isAsyncExpr = true;
        }

        bool previousAwaitIsKeyword = m_pscan->SetAwaitIsKeyword(isAsyncExpr);
        m_pscan->Scan();
        m_pscan->SetAwaitIsKeyword(previousAwaitIsKeyword);

        // We search for an Async expression (a function declaration or an async lambda expression)
        if (isAsyncExpr && !m_pscan->FHadNewLine())
        {TRACE_IT(29691);
            if (m_token.tk == tkFUNCTION)
            {TRACE_IT(29692);
                goto LFunction;
            }
            else if (m_token.tk == tkID || m_token.tk == tkAWAIT)
            {TRACE_IT(29693);
                isLambdaExpr = true;
                goto LFunction;
            }
            else if (m_token.tk == tkLParen)
            {TRACE_IT(29694);
                // This is potentially an async arrow function. Save the state of the async references
                // in case it needs to be restored. (Note that the case of a single parameter with no ()'s
                // is detected upstream and need not be handled here.)
                savedTopAsyncRef = pid->GetTopRef();
            }
        }

        // Don't push a reference if this is a single lambda parameter, because we'll reparse with
        // a correct function ID.
        if (m_token.tk != tkDArrow)
        {TRACE_IT(29695);
            ref = this->PushPidRef(pid);
        }

        if (buildAST)
        {TRACE_IT(29696);
            pnode = CreateNameNode(pid);
            pnode->ichMin = ichMin;
            pnode->ichLim = ichLim;
            pnode->sxPid.SetSymRef(ref);
        }
        else
        {TRACE_IT(29697);
            // Remember the identifier start and end in case it turns out to be a statement label.
            term.tk = tkID;
            term.pid = pid; // Record the identifier for detection of eval
            term.ichMin = static_cast<charcount_t>(iecpMin);
            term.ichLim = static_cast<charcount_t>(iecpLim);
        }
        CheckArgumentsUse(pid, GetCurrentFunctionNode());
        break;
    }

    case tkTHIS:
        if (buildAST)
        {TRACE_IT(29698);
            pnode = CreateNodeWithScanner<knopThis>();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkLParen:
    {TRACE_IT(29699);
        ichMin = m_pscan->IchMinTok();
        iuMin = m_pscan->IecpMinTok();
        m_pscan->Scan();
        if (m_token.tk == tkRParen)
        {TRACE_IT(29700);
            // Empty parens can only be legal as an empty parameter list to a lambda declaration.
            // We're in a lambda if the next token is =>.
            fAllowCall = FALSE;
            m_pscan->Scan();

            // If the token after the right paren is not => or if there was a newline between () and => this is a syntax error
            if (!m_doingFastScan && (m_token.tk != tkDArrow || m_pscan->FHadNewLine()))
            {TRACE_IT(29701);
                Error(ERRsyntax);
            }

            if (buildAST)
            {TRACE_IT(29702);
                pnode = CreateNodeWithScanner<knopEmpty>();
            }
            break;
        }

        // Advance the block ID here in case this parenthetical expression turns out to be a lambda parameter list.
        // That way the pid ref stacks will be created in their correct final form, and we can simply fix
        // up function ID's.
        uint saveNextBlockId = m_nextBlockId;
        uint saveCurrBlockId = GetCurrentBlock()->sxBlock.blockId;
        GetCurrentBlock()->sxBlock.blockId = m_nextBlockId++;

        this->m_parenDepth++;
        pnode = ParseExpr<buildAST>(koplNo, &fCanAssign, TRUE, FALSE, nullptr, nullptr /*nameLength*/, nullptr  /*pShortNameOffset*/, &term, true, nullptr, plastRParen);
        this->m_parenDepth--;

        if (buildAST && plastRParen)
        {TRACE_IT(29703);
            *plastRParen = m_pscan->IchLimTok();
        }

        ChkCurTok(tkRParen, ERRnoRparen);

        GetCurrentBlock()->sxBlock.blockId = saveCurrBlockId;
        if (m_token.tk == tkDArrow)
        {TRACE_IT(29704);
            // We're going to rewind and reinterpret the expression as a parameter list.
            // Put back the original next-block-ID so the existing pid ref stacks will be correct.
            m_nextBlockId = saveNextBlockId;
        }

        // Emit a deferred ... error if one was parsed.
        if (m_deferEllipsisError && m_token.tk != tkDArrow)
        {TRACE_IT(29705);
            m_pscan->SeekTo(m_EllipsisErrLoc);
            Error(ERRInvalidSpreadUse);
        }
        else
        {TRACE_IT(29706);
            m_deferEllipsisError = false;
        }
        break;
    }

    case tkIntCon:
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {TRACE_IT(29707);
            Error(ERRES5NoOctal);
        }

        if (buildAST)
        {TRACE_IT(29708);
            pnode = CreateIntNodeWithScanner(m_token.GetLong());
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkFltCon:
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {TRACE_IT(29709);
            Error(ERRES5NoOctal);
        }

        if (buildAST)
        {TRACE_IT(29710);
            pnode = CreateNodeWithScanner<knopFlt>();
            pnode->sxFlt.dbl = m_token.GetDouble();
            pnode->sxFlt.maybeInt = m_token.GetDoubleMayBeInt();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkStrCon:
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {TRACE_IT(29711);
            Error(ERRES5NoOctal);
        }

        if (buildAST)
        {TRACE_IT(29712);
            pnode = CreateStrNodeWithScanner(m_token.GetStr());
        }
        else
        {TRACE_IT(29713);
            // Subtract the string literal length from the total char count for the purpose
            // of deciding whether to defer parsing and byte code generation.
            this->ReduceDeferredScriptLength(m_pscan->IchLimTok() - m_pscan->IchMinTok());
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkTRUE:
        if (buildAST)
        {TRACE_IT(29714);
            pnode = CreateNodeWithScanner<knopTrue>();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkFALSE:
        if (buildAST)
        {TRACE_IT(29715);
            pnode = CreateNodeWithScanner<knopFalse>();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkNULL:
        if (buildAST)
        {TRACE_IT(29716);
            pnode = CreateNodeWithScanner<knopNull>();
        }
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkDiv:
    case tkAsgDiv:
        pnode = ParseRegExp<buildAST>();
        fCanAssign = FALSE;
        m_pscan->Scan();
        break;

    case tkNEW:
    {TRACE_IT(29717);
        ichMin = m_pscan->IchMinTok();
        m_pscan->Scan();

        if (m_token.tk == tkDot && m_scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled())
        {TRACE_IT(29718);
            pnode = ParseMetaProperty<buildAST>(tkNEW, ichMin, &fCanAssign);

            m_pscan->Scan();
        }
        else
        {TRACE_IT(29719);
            ParseNodePtr pnodeExpr = ParseTerm<buildAST>(FALSE, pNameHint, pHintLength, pShortNameOffset);
            if (buildAST)
            {TRACE_IT(29720);
                pnode = CreateCallNode(knopNew, pnodeExpr, nullptr);
                pnode->ichMin = ichMin;
            }
            fInNew = TRUE;
            fCanAssign = FALSE;
        }
        break;
    }

    case tkLBrack:
    {TRACE_IT(29721);
        ichMin = m_pscan->IchMinTok();
        m_pscan->Scan();
        pnode = ParseArrayLiteral<buildAST>();
        if (buildAST)
        {TRACE_IT(29722);
            pnode->ichMin = ichMin;
            pnode->ichLim = m_pscan->IchLimTok();
        }

        if (this->m_arrayDepth == 0)
        {TRACE_IT(29723);
            Assert(m_pscan->IchLimTok() - ichMin > m_funcInArray);
            this->ReduceDeferredScriptLength(m_pscan->IchLimTok() - ichMin - this->m_funcInArray);
            this->m_funcInArray = 0;
            this->m_funcInArrayDepth = 0;
        }
        ChkCurTok(tkRBrack, ERRnoRbrack);
        if (!IsES6DestructuringEnabled())
        {TRACE_IT(29724);
            fCanAssign = FALSE;
        }
        else if (pfLikelyPattern != nullptr && !IsPostFixOperators())
        {TRACE_IT(29725);
            *pfLikelyPattern = TRUE;
        }
        break;
    }

    case tkLCurly:
    {TRACE_IT(29726);
        ichMin = m_pscan->IchMinTok();
        m_pscan->ScanForcingPid();
        ParseNodePtr pnodeMemberList = ParseMemberList<buildAST>(pNameHint, pHintLength);
        if (buildAST)
        {TRACE_IT(29727);
            pnode = CreateUniNode(knopObject, pnodeMemberList);
            pnode->ichMin = ichMin;
            pnode->ichLim = m_pscan->IchLimTok();
        }
        ChkCurTok(tkRCurly, ERRnoRcurly);
        if (!IsES6DestructuringEnabled())
        {TRACE_IT(29728);
            fCanAssign = FALSE;
        }
        else if (pfLikelyPattern != nullptr && !IsPostFixOperators())
        {TRACE_IT(29729);
            *pfLikelyPattern = TRUE;
        }
        break;
    }

    case tkFUNCTION:
    {TRACE_IT(29730);
LFunction :
        if (m_grfscr & fscrDeferredFncExpression)
        {TRACE_IT(29731);
            // The top-level deferred function body was defined by a function expression whose parsing was deferred. We are now
            // parsing it, so unset the flag so that any nested functions are parsed normally. This flag is only applicable the
            // first time we see it.
            //
            // Normally, deferred functions will be parsed in ParseStatement upon encountering the 'function' token. The first
            // token of the source code of the function may not a 'function' token though, so we still need to reset this flag
            // for the first function we parse. This can happen in compat modes, for instance, for a function expression enclosed
            // in parentheses, where the legacy behavior was to include the parentheses in the function's source code.
            m_grfscr &= ~fscrDeferredFncExpression;
        }
        ushort flags = fFncNoFlgs;
        if (isLambdaExpr)
        {TRACE_IT(29732);
            flags |= fFncLambda;
        }
        if (isAsyncExpr)
        {TRACE_IT(29733);
            flags |= fFncAsync;
        }
        pnode = ParseFncDecl<buildAST>(flags, pNameHint, false, true, fUnaryOrParen);
        if (isAsyncExpr)
        {TRACE_IT(29734);
            pnode->sxFnc.cbMin = iecpMin;
            pnode->ichMin = ichMin;
        }
        fCanAssign = FALSE;
        break;
    }

    case tkCLASS:
        if (m_scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled())
        {TRACE_IT(29735);
            pnode = ParseClassDecl<buildAST>(FALSE, pNameHint, pHintLength, pShortNameOffset);
        }
        else
        {TRACE_IT(29736);
            goto LUnknown;
        }
        fCanAssign = FALSE;
        break;

    case tkStrTmplBasic:
    case tkStrTmplBegin:
        pnode = ParseStringTemplateDecl<buildAST>(nullptr);
        fCanAssign = FALSE;
        break;

    case tkSUPER:
        if (m_scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled())
        {TRACE_IT(29737);
            pnode = ParseSuper<buildAST>(pnode, !!fAllowCall);
        }
        else
        {TRACE_IT(29738);
            goto LUnknown;
        }
        break;

    case tkCASE:
    {TRACE_IT(29739);
        if (!m_doingFastScan)
        {TRACE_IT(29740);
            goto LUnknown;
        }
        ParseNodePtr pnodeUnused;
        pnode = ParseCase<buildAST>(&pnodeUnused);
        break;
    }

    case tkELSE:
        if (!m_doingFastScan)
        {TRACE_IT(29741);
            goto LUnknown;
        }
        m_pscan->Scan();
        ParseStatement<buildAST>();
        break;

    default:
    LUnknown :
        Error(ERRsyntax);
        break;
    }

    pnode = ParsePostfixOperators<buildAST>(pnode, fAllowCall, fInNew, isAsyncExpr, &fCanAssign, &term, pfIsDotOrIndex);

    if (savedTopAsyncRef != nullptr &&
        this->m_token.tk == tkDArrow)
    {TRACE_IT(29742);
        // This is an async arrow function; we're going to back up and reparse it.
        // Make sure we don't leave behind a bogus reference to the 'async' identifier.
        for (IdentPtr pid = wellKnownPropertyPids.async; pid->GetTopRef() != savedTopAsyncRef;)
        {TRACE_IT(29743);
            Assert(pid->GetTopRef() != nullptr);
            pid->RemovePrevPidRef(nullptr);
        }
    }

    // Pass back identifier if requested
    if (pToken && term.tk == tkID)
    {TRACE_IT(29744);
        *pToken = term;
    }

    if (pfCanAssign)
    {TRACE_IT(29745);
        *pfCanAssign = fCanAssign;
    }

    return pnode;
}

template <bool buildAST>
ParseNodePtr Parser::ParseRegExp()
{TRACE_IT(29746);
    ParseNodePtr pnode = nullptr;

    if (buildAST || m_doingFastScan)
    {TRACE_IT(29747);
        m_pscan->RescanRegExp();

        BOOL saveDeferringAST = this->m_deferringAST;
        if (m_doingFastScan)
        {TRACE_IT(29748);
            this->m_deferringAST = false;
        }
        pnode = CreateNodeWithScanner<knopRegExp>();
        pnode->sxPid.regexPattern = m_token.GetRegex();
        if (m_doingFastScan)
        {TRACE_IT(29749);
            this->m_deferringAST = saveDeferringAST;
            this->AddFastScannedRegExpNode(pnode);
            if (!buildAST)
            {TRACE_IT(29750);
                pnode = nullptr;
            }
        }
#if ENABLE_BACKGROUND_PARSING
        else if (this->IsBackgroundParser())
        {TRACE_IT(29751);
            Assert(pnode->sxPid.regexPattern == nullptr);
            this->AddBackgroundRegExpNode(pnode);
        }
#endif
    }
    else
    {TRACE_IT(29752);
        m_pscan->RescanRegExpNoAST();
    }
    Assert(m_token.tk == tkRegExp);

    return pnode;
}

BOOL Parser::NodeIsEvalName(ParseNodePtr pnode)
{TRACE_IT(29753);
    //WOOB 1107758 Special case of indirect eval binds to local scope in standards mode
    return pnode->nop == knopName && (pnode->sxPid.pid == wellKnownPropertyPids.eval);
}

BOOL Parser::NodeEqualsName(ParseNodePtr pnode, LPCOLESTR sz, uint32 cch)
{TRACE_IT(29754);
    return pnode->nop == knopName &&
        pnode->sxPid.pid->Cch() == cch &&
        !wmemcmp(pnode->sxPid.pid->Psz(), sz, cch);
}

BOOL Parser::NodeIsIdent(ParseNodePtr pnode, IdentPtr pid)
{TRACE_IT(29755);
    for (;;)
    {TRACE_IT(29756);
        switch (pnode->nop)
        {
        case knopName:
            return (pnode->sxPid.pid == pid);

        case knopComma:
            pnode = pnode->sxBin.pnode2;
            break;

        default:
            return FALSE;
        }
    }
}

template<bool buildAST>
ParseNodePtr Parser::ParsePostfixOperators(
    ParseNodePtr pnode,
    BOOL fAllowCall,
    BOOL fInNew,
    BOOL isAsyncExpr,
    BOOL *pfCanAssign,
    _Inout_ IdentToken* pToken,
    _Out_opt_ bool* pfIsDotOrIndex /*= nullptr */)
{TRACE_IT(29757);
    uint16 count = 0;
    bool callOfConstants = false;
    if (pfIsDotOrIndex)
    {TRACE_IT(29758);
        *pfIsDotOrIndex = false;
    }

    for (;;)
    {TRACE_IT(29759);
        uint16 spreadArgCount = 0;
        switch (m_token.tk)
        {
        case tkLParen:
            {TRACE_IT(29760);
                if (fInNew)
                {TRACE_IT(29761);
                    ParseNodePtr pnodeArgs = ParseArgList<buildAST>(&callOfConstants, &spreadArgCount, &count);
                    if (buildAST)
                    {TRACE_IT(29762);
                        Assert(pnode->nop == knopNew);
                        Assert(pnode->sxCall.pnodeArgs == nullptr);
                        pnode->sxCall.pnodeArgs = pnodeArgs;
                        pnode->sxCall.callOfConstants = callOfConstants;
                        pnode->sxCall.isApplyCall = false;
                        pnode->sxCall.isEvalCall = false;
                        pnode->sxCall.argCount = count;
                        pnode->sxCall.spreadArgCount = spreadArgCount;
                        pnode->ichLim = m_pscan->IchLimTok();
                    }
                    else
                    {TRACE_IT(29763);
                        pnode = nullptr;
                        pToken->tk = tkNone; // This is no longer an identifier
                    }
                    fInNew = FALSE;
                    ChkCurTok(tkRParen, ERRnoRparen);
                }
                else
                {TRACE_IT(29764);
                    bool fCallIsEval = false;
                    if (!fAllowCall)
                    {TRACE_IT(29765);
                        return pnode;
                    }

                    uint saveNextBlockId = m_nextBlockId;
                    uint saveCurrBlockId = GetCurrentBlock()->sxBlock.blockId;

                    if (isAsyncExpr)
                    {TRACE_IT(29766);
                        // Advance the block ID here in case this parenthetical expression turns out to be a lambda parameter list.
                        // That way the pid ref stacks will be created in their correct final form, and we can simply fix
                        // up function ID's.
                        GetCurrentBlock()->sxBlock.blockId = m_nextBlockId++;
                    }

                    ParseNodePtr pnodeArgs = ParseArgList<buildAST>(&callOfConstants, &spreadArgCount, &count);
                    // We used to un-defer a deferred function body here if it was called as part of the expression that declared it.
                    // We now detect this case up front in ParseFncDecl, which is cheaper and simpler.
                    if (buildAST)
                    {TRACE_IT(29767);
                        pnode = CreateCallNode(knopCall, pnode, pnodeArgs);
                        Assert(pnode);

                        // Detect call to "eval" and record it on the function.
                        // Note: we used to leave it up to the byte code generator to detect eval calls
                        // at global scope, but now it relies on the flag the parser sets, so set it here.

                        if (count > 0 && this->NodeIsEvalName(pnode->sxCall.pnodeTarget))
                        {TRACE_IT(29768);
                            this->MarkEvalCaller();
                            fCallIsEval = true;
                        }

                        pnode->sxCall.callOfConstants = callOfConstants;
                        pnode->sxCall.spreadArgCount = spreadArgCount;
                        pnode->sxCall.isApplyCall = false;
                        pnode->sxCall.isEvalCall = fCallIsEval;
                        pnode->sxCall.argCount = count;
                        pnode->ichLim = m_pscan->IchLimTok();
                    }
                    else
                    {TRACE_IT(29769);
                        pnode = nullptr;
                        if (pToken->tk == tkID && pToken->pid == wellKnownPropertyPids.eval && count > 0) // Detect eval
                        {TRACE_IT(29770);
                            this->MarkEvalCaller();
                        }
                        pToken->tk = tkNone; // This is no longer an identifier
                    }

                    ChkCurTok(tkRParen, ERRnoRparen);

                    if (isAsyncExpr)
                    {TRACE_IT(29771);
                        GetCurrentBlock()->sxBlock.blockId = saveCurrBlockId;
                        if (m_token.tk == tkDArrow)
                        {TRACE_IT(29772);
                            // We're going to rewind and reinterpret the expression as a parameter list.
                            // Put back the original next-block-ID so the existing pid ref stacks will be correct.
                            m_nextBlockId = saveNextBlockId;
                        }
                    }
                }
                if (pfCanAssign)
                {TRACE_IT(29773);
                    *pfCanAssign = FALSE;
                }
                if (pfIsDotOrIndex)
                {TRACE_IT(29774);
                    *pfIsDotOrIndex = false;
                }
                break;
            }
        case tkLBrack:
            {TRACE_IT(29775);
                m_pscan->Scan();
                ParseNodePtr pnodeExpr = ParseExpr<buildAST>();
                if (buildAST)
                {TRACE_IT(29776);
                    pnode = CreateBinNode(knopIndex, pnode, pnodeExpr);
                    pnode->ichLim = m_pscan->IchLimTok();
                }
                else
                {TRACE_IT(29777);
                    pnode = nullptr;
                    pToken->tk = tkNone; // This is no longer an identifier
                }
                ChkCurTok(tkRBrack, ERRnoRbrack);
                if (pfCanAssign)
                {TRACE_IT(29778);
                    *pfCanAssign = TRUE;
                }
                if (pfIsDotOrIndex)
                {TRACE_IT(29779);
                    *pfIsDotOrIndex = true;
                }

                if (!buildAST)
                {TRACE_IT(29780);
                    break;
                }

                bool shouldConvertToDot = false;
                if (pnode->sxBin.pnode2->nop == knopStr)
                {TRACE_IT(29781);
                    // if the string is empty or contains escape character, we will not convert them to dot node
                    shouldConvertToDot = pnode->sxBin.pnode2->sxPid.pid->Cch() > 0 && !m_pscan->IsEscapeOnLastTkStrCon();
                }

                if (shouldConvertToDot)
                {TRACE_IT(29782);
                    LPCOLESTR str = pnode->sxBin.pnode2->sxPid.pid->Psz();
                    // See if we can convert o["p"] into o.p and o["0"] into o[0] since they're equivalent and the latter forms
                    // are faster
                    uint32 uintValue;
                    if(Js::JavascriptOperators::TryConvertToUInt32(
                           str,
                           pnode->sxBin.pnode2->sxPid.pid->Cch(),
                           &uintValue) &&
                       !Js::TaggedInt::IsOverflow(uintValue)) // the optimization is not very useful if the number can't be represented as a TaggedInt
                    {TRACE_IT(29783);
                        // No need to verify that uintValue != JavascriptArray::InvalidIndex since all nonnegative TaggedInts are valid indexes
                        auto intNode = CreateIntNodeWithScanner(uintValue); // implicit conversion from uint32 to int32
                        pnode->sxBin.pnode2 = intNode;
                    }
                    // Field optimization (see GlobOpt::KillLiveElems) checks for value being a Number,
                    // and since NaN/Infinity is a number it won't kill o.NaN/o.Infinity which would cause a problem
                    // if we decide to hoist o.NaN/o.Infinity.
                    // We need to keep o["NaN"] and o["+/-Infinity"] as array element access (we don't hoist that but we may hoist field access),
                    // so no matter if it's killed by o[x] inside a loop, we make sure that we never hoist these.
                    // We need to follow same logic for strings that convert to a floating point number.
                    else
                    {TRACE_IT(29784);
                        bool doConvertToProperty = false;    // Convert a["x"] -> a.x.
                        if (!Parser::IsNaNOrInfinityLiteral<true>(str))
                        {TRACE_IT(29785);
                            const OLECHAR* terminalChar;
                            double dbl = Js::NumberUtilities::StrToDbl(str, &terminalChar, m_scriptContext);
                            bool convertsToFloat = !Js::NumberUtilities::IsNan(dbl);
                            doConvertToProperty = !convertsToFloat;
                        }

                        if (doConvertToProperty)
                        {TRACE_IT(29786);
                            pnode->sxBin.pnode2->nop = knopName;
                            pnode->nop = knopDot;
                            pnode->grfpn |= PNodeFlags::fpnIndexOperator;
                        }
                    }
                }
            }
            break;

        case tkDot:
            {TRACE_IT(29787);
            ParseNodePtr name = nullptr;
            OpCode opCode = knopDot;

            m_pscan->Scan();
            if (!m_token.IsIdentifier())
            {TRACE_IT(29788);
                //allow reserved words in ES5 mode
                if (!(m_token.IsReservedWord()))
                {TRACE_IT(29789);
                    IdentifierExpectedError(m_token);
                }
            }
            // Note: see comment above about field optimization WRT NaN/Infinity/-Infinity.
            // Convert a.Nan, a.Infinity into a["NaN"], a["Infinity"].
            // We don't care about -Infinity case here because x.-Infinity is invalid in JavaScript.
            // Both NaN and Infinity are identifiers.
            else if (buildAST && Parser::IsNaNOrInfinityLiteral<false>(m_token.GetIdentifier(m_phtbl)->Psz()))
            {TRACE_IT(29790);
                opCode = knopIndex;
            }

            if (buildAST)
            {TRACE_IT(29791);
                if (opCode == knopDot)
                {TRACE_IT(29792);
                    name = CreateNameNode(m_token.GetIdentifier(m_phtbl));
                }
                else
                {TRACE_IT(29793);
                    Assert(opCode == knopIndex);
                    name = CreateStrNodeWithScanner(m_token.GetIdentifier(m_phtbl));
                }
                pnode = CreateBinNode(opCode, pnode, name);
            }
            else
            {TRACE_IT(29794);
                pnode = nullptr;
                pToken->tk = tkNone;
            }

            if (pfCanAssign)
            {TRACE_IT(29795);
                *pfCanAssign = TRUE;
            }
            if (pfIsDotOrIndex)
            {TRACE_IT(29796);
                *pfIsDotOrIndex = true;
            }
            m_pscan->Scan();

            break;
            }

        case tkStrTmplBasic:
        case tkStrTmplBegin:
            {TRACE_IT(29797);
                ParseNode* templateNode = ParseStringTemplateDecl<buildAST>(pnode);

                if (!buildAST)
                {TRACE_IT(29798);
                    pToken->tk = tkNone; // This is no longer an identifier
                }

                pnode = templateNode;
                if (pfCanAssign)
                {TRACE_IT(29799);
                    *pfCanAssign = FALSE;
                }
                if (pfIsDotOrIndex)
                {TRACE_IT(29800);
                    *pfIsDotOrIndex = false;
                }
                break;
            }
        default:
            return pnode;
        }
    }
}

/***************************************************************************
Look for an existing label with the given name.
***************************************************************************/
ParseNodePtr Parser::PnodeLabel(IdentPtr pid, ParseNodePtr pnodeLabels)
{TRACE_IT(29801);
    AssertMem(pid);
    AssertNodeMemN(pnodeLabels);

    StmtNest *pstmt;
    ParseNodePtr pnodeT;

    // Look in the statement stack.
    for (pstmt = m_pstmtCur; nullptr != pstmt; pstmt = pstmt->pstmtOuter)
    {TRACE_IT(29802);
        AssertNodeMem(pstmt->pnodeStmt);
        AssertNodeMemN(pstmt->pnodeLab);

        for (pnodeT = pstmt->pnodeLab; nullptr != pnodeT;
            pnodeT = pnodeT->sxLabel.pnodeNext)
        {TRACE_IT(29803);
            Assert(knopLabel == pnodeT->nop);
            if (pid == pnodeT->sxLabel.pid)
                return pnodeT;
        }
    }

    // Also look in the pnodeLabels list.
    for (pnodeT = pnodeLabels; nullptr != pnodeT;
        pnodeT = pnodeT->sxLabel.pnodeNext)
    {TRACE_IT(29804);
        Assert(knopLabel == pnodeT->nop);
        if (pid == pnodeT->sxLabel.pid)
            return pnodeT;
    }

    return nullptr;
}

// Currently only ints and floats are treated as constants in function call
// TODO: Check if we need for other constants as well
BOOL Parser::IsConstantInFunctionCall(ParseNodePtr pnode)
{TRACE_IT(29805);
    if (pnode->nop == knopInt && !Js::TaggedInt::IsOverflow(pnode->sxInt.lw))
    {TRACE_IT(29806);
        return TRUE;
    }

    if (pnode->nop == knopFlt)
    {TRACE_IT(29807);
        return TRUE;
    }

    return FALSE;
}

/***************************************************************************
Parse a list of arguments.
***************************************************************************/
template<bool buildAST>
ParseNodePtr Parser::ParseArgList( bool *pCallOfConstants, uint16 *pSpreadArgCount, uint16 * pCount)
{TRACE_IT(29808);
    ParseNodePtr pnodeArg;
    ParseNodePtr pnodeList = nullptr;
    ParseNodePtr *lastNodeRef = nullptr;

    // Check for an empty list
    Assert(m_token.tk == tkLParen);

    if (m_pscan->Scan() == tkRParen)
    {TRACE_IT(29809);
        return nullptr;
    }

    *pCallOfConstants = true;
    *pSpreadArgCount = 0;

    int count=0;
    while (true)
    {TRACE_IT(29810);
        // the count of arguments has to fit in an unsigned short
        if (count > 0xffffU)
            Error(ERRnoMemory);
        // Allow spread in argument lists.
        IdentToken token;
        pnodeArg = ParseExpr<buildAST>(koplCma, nullptr, TRUE, /* fAllowEllipsis */TRUE, NULL, nullptr, nullptr, &token);
        ++count;
        this->MarkEscapingRef(pnodeArg, &token);

        if (buildAST)
        {TRACE_IT(29811);
            this->CheckArguments(pnodeArg);

            if (*pCallOfConstants && !IsConstantInFunctionCall(pnodeArg))
            {TRACE_IT(29812);
                *pCallOfConstants = false;
            }

            if (pnodeArg->nop == knopEllipsis)
            {TRACE_IT(29813);
                (*pSpreadArgCount)++;
            }

            AddToNodeListEscapedUse(&pnodeList, &lastNodeRef, pnodeArg);
        }
        if (m_token.tk != tkComma)
        {TRACE_IT(29814);
            break;
        }
        m_pscan->Scan();

        if (m_token.tk == tkRParen && m_scriptContext->GetConfig()->IsES7TrailingCommaEnabled())
        {TRACE_IT(29815);
            break;
        }
    }

    if (pSpreadArgCount!=nullptr && (*pSpreadArgCount) > 0){
        CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(SpreadFeature, m_scriptContext);
    }

    *pCount = static_cast<uint16>(count);
    if (buildAST)
    {TRACE_IT(29816);
        AssertMem(lastNodeRef);
        AssertNodeMem(*lastNodeRef);
        pnodeList->ichLim = (*lastNodeRef)->ichLim;
    }

    return pnodeList;
}

// Currently only ints are treated as constants in ArrayLiterals
BOOL Parser::IsConstantInArrayLiteral(ParseNodePtr pnode)
{TRACE_IT(29817);
    if (pnode->nop == knopInt && !Js::TaggedInt::IsOverflow(pnode->sxInt.lw))
    {TRACE_IT(29818);
        return TRUE;
    }
    return FALSE;
}

template<bool buildAST>
ParseNodePtr Parser::ParseArrayLiteral()
{TRACE_IT(29819);
    ParseNodePtr pnode = nullptr;
    bool arrayOfTaggedInts = false;
    bool arrayOfInts = false;
    bool arrayOfNumbers = false;
    bool hasMissingValues = false;
    uint count = 0;
    uint spreadCount = 0;

    ParseNodePtr pnode1 = ParseArrayList<buildAST>(&arrayOfTaggedInts, &arrayOfInts, &arrayOfNumbers, &hasMissingValues, &count, &spreadCount);

    if (buildAST)
    {TRACE_IT(29820);
        pnode = CreateNodeWithScanner<knopArray>();
        pnode->sxArrLit.pnode1 = pnode1;
        pnode->sxArrLit.arrayOfTaggedInts = arrayOfTaggedInts;
        pnode->sxArrLit.arrayOfInts = arrayOfInts;
        pnode->sxArrLit.arrayOfNumbers = arrayOfNumbers;
        pnode->sxArrLit.hasMissingValues = hasMissingValues;
        pnode->sxArrLit.count = count;
        pnode->sxArrLit.spreadCount = spreadCount;

        if (pnode->sxArrLit.pnode1)
        {TRACE_IT(29821);
            this->CheckArguments(pnode->sxArrLit.pnode1);
        }
    }

    return pnode;
}

/***************************************************************************
Create an ArrayLiteral node
Parse a list of array elements. [ a, b, , c, ]
***************************************************************************/
template<bool buildAST>
ParseNodePtr Parser::ParseArrayList(bool *pArrayOfTaggedInts, bool *pArrayOfInts, bool *pArrayOfNumbers, bool *pHasMissingValues, uint *count, uint *spreadCount)
{TRACE_IT(29822);
    ParseNodePtr pnodeArg = nullptr;
    ParseNodePtr pnodeList = nullptr;
    ParseNodePtr *lastNodeRef = nullptr;

    *count = 0;

    // Check for an empty list
    if (tkRBrack == m_token.tk)
    {TRACE_IT(29823);
        return nullptr;
    }

    this->m_arrayDepth++;
    bool arrayOfTaggedInts = buildAST;
    bool arrayOfInts = buildAST;
    bool arrayOfNumbers = buildAST;
    bool arrayOfVarInts = false;
    bool hasMissingValues = false;

    for (;;)
    {TRACE_IT(29824);
        (*count)++;
        if (tkComma == m_token.tk || tkRBrack == m_token.tk)
        {TRACE_IT(29825);
            hasMissingValues = true;
            arrayOfTaggedInts = false;
            arrayOfInts = false;
            arrayOfNumbers = false;
            if (buildAST)
            {TRACE_IT(29826);
                pnodeArg = CreateNodeWithScanner<knopEmpty>();
            }
        }
        else
        {TRACE_IT(29827);
            // Allow Spread in array literals.
            pnodeArg = ParseExpr<buildAST>(koplCma, nullptr, TRUE, /* fAllowEllipsis */ TRUE);
            if (buildAST)
            {TRACE_IT(29828);
                if (pnodeArg->nop == knopEllipsis)
                {TRACE_IT(29829);
                    (*spreadCount)++;
                }
                this->CheckArguments(pnodeArg);
            }
        }

#if DEBUG
        if(m_grfscr & fscrEnforceJSON && !IsJSONValid(pnodeArg))
        {TRACE_IT(29830);
            Error(ERRsyntax);
        }
#endif

        if (buildAST)
        {TRACE_IT(29831);
            if (arrayOfNumbers)
            {TRACE_IT(29832);
                if (pnodeArg->nop != knopInt)
                {TRACE_IT(29833);
                    arrayOfTaggedInts = false;
                    if (pnodeArg->nop != knopFlt)
                    {TRACE_IT(29834);
                        // Not an array of constants.
                        arrayOfInts = false;
                        arrayOfNumbers = false;
                    }
                    else if (arrayOfInts && Js::JavascriptNumber::IsInt32OrUInt32(pnodeArg->sxFlt.dbl) && (!Js::JavascriptNumber::IsInt32(pnodeArg->sxFlt.dbl) || pnodeArg->sxFlt.dbl == -2147483648.0))
                    {TRACE_IT(29835);
                        // We've seen nothing but ints, and this is a uint32 but not an int32.
                        // Unless we see an actual float at some point, we want an array of vars
                        // so we can work with tagged ints.
                        arrayOfVarInts = true;
                    }
                    else
                    {TRACE_IT(29836);
                        // Not an int array, but it may still be a float array.
                        arrayOfInts = false;
                    }
                }
                else
                {TRACE_IT(29837);
                    if (Js::SparseArraySegment<int32>::IsMissingItem((int32*)&pnodeArg->sxInt.lw))
                    {TRACE_IT(29838);
                        arrayOfInts = false;
                    }
                    if (Js::TaggedInt::IsOverflow(pnodeArg->sxInt.lw))
                    {TRACE_IT(29839);
                        arrayOfTaggedInts = false;
                    }
                }
            }
            AddToNodeListEscapedUse(&pnodeList, &lastNodeRef, pnodeArg);
        }

        if (tkComma != m_token.tk)
        {TRACE_IT(29840);
            break;
        }
        m_pscan->Scan();

        if (tkRBrack == m_token.tk)
        {TRACE_IT(29841);
            break;
        }
    }

    if (spreadCount != nullptr && *spreadCount > 0){
        CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(SpreadFeature, m_scriptContext);
    }

    if (buildAST)
    {TRACE_IT(29842);
        AssertMem(lastNodeRef);
        AssertNodeMem(*lastNodeRef);
        pnodeList->ichLim = (*lastNodeRef)->ichLim;

        if (arrayOfVarInts && arrayOfInts)
        {TRACE_IT(29843);
            arrayOfInts = false;
            arrayOfNumbers = false;
        }
        *pArrayOfTaggedInts = arrayOfTaggedInts;
        *pArrayOfInts = arrayOfInts;
        *pArrayOfNumbers = arrayOfNumbers;
        *pHasMissingValues = hasMissingValues;
    }
    this->m_arrayDepth--;
    return pnodeList;
}

Parser::MemberNameToTypeMap* Parser::CreateMemberNameMap(ArenaAllocator* pAllocator)
{TRACE_IT(29844);
    Assert(pAllocator);
    return Anew(pAllocator, MemberNameToTypeMap, pAllocator, 5);
}

template<bool buildAST> void Parser::ParseComputedName(ParseNodePtr* ppnodeName, LPCOLESTR* ppNameHint, LPCOLESTR* ppFullNameHint, uint32 *pNameLength, uint32 *pShortNameOffset)
{TRACE_IT(29845);
    m_pscan->Scan();
    ParseNodePtr pnodeNameExpr = ParseExpr<buildAST>(koplCma, nullptr, TRUE, FALSE, *ppNameHint, pNameLength, pShortNameOffset);
    if (buildAST)
    {TRACE_IT(29846);
        *ppnodeName = CreateNodeT<knopComputedName>(pnodeNameExpr->ichMin, pnodeNameExpr->ichLim);
        (*ppnodeName)->sxUni.pnode1 = pnodeNameExpr;
    }

    if (ppFullNameHint && buildAST && CONFIG_FLAG(UseFullName))
    {TRACE_IT(29847);
        *ppFullNameHint = FormatPropertyString(*ppNameHint, pnodeNameExpr, pNameLength, pShortNameOffset);
    }

    ChkCurTokNoScan(tkRBrack, ERRnoRbrack);
}

/***************************************************************************
    Parse a list of object set/get members, e.g.:
    { get foo(){ ... }, set bar(arg) { ... } }
***************************************************************************/
template<bool buildAST>
ParseNodePtr Parser::ParseMemberGetSet(OpCode nop, LPCOLESTR* ppNameHint)
{TRACE_IT(29848);
    ParseNodePtr pnodeName = nullptr;
    Assert(nop == knopGetMember || nop == knopSetMember);
    AssertMem(ppNameHint);
    IdentPtr pid = nullptr;
    bool isComputedName = false;

    *ppNameHint=nullptr;

    switch(m_token.tk)
    {
    default:
        if (!m_token.IsReservedWord())
        {TRACE_IT(29849);
            Error(ERRnoMemberIdent);
        }
        // fall through
    case tkID:
        pid = m_token.GetIdentifier(m_phtbl);
        *ppNameHint = pid->Psz();
        if (buildAST)
        {TRACE_IT(29850);
            pnodeName = CreateStrNodeWithScanner(pid);
        }
        break;
    case tkStrCon:
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {TRACE_IT(29851);
            Error(ERRES5NoOctal);
        }
        pid = m_token.GetStr();
        *ppNameHint = pid->Psz();
        if (buildAST)
        {TRACE_IT(29852);
            pnodeName = CreateStrNodeWithScanner(pid);
        }
        break;

    case tkIntCon:
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {TRACE_IT(29853);
            Error(ERRES5NoOctal);
        }

        pid = m_pscan->PidFromLong(m_token.GetLong());
        if (buildAST)
        {TRACE_IT(29854);
            pnodeName = CreateStrNodeWithScanner(pid);
        }
        break;

    case tkFltCon:
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {TRACE_IT(29855);
            Error(ERRES5NoOctal);
        }

        pid = m_pscan->PidFromDbl(m_token.GetDouble());
        if (buildAST)
        {TRACE_IT(29856);
            pnodeName = CreateStrNodeWithScanner(pid);
        }
        break;

    case tkLBrack:
        // Computed property name: get|set [expr] () {  }
        if (!m_scriptContext->GetConfig()->IsES6ObjectLiteralsEnabled())
        {TRACE_IT(29857);
            Error(ERRnoMemberIdent);
        }
        LPCOLESTR emptyHint = nullptr;
        uint32 offset = 0;
        ParseComputedName<buildAST>(&pnodeName, &emptyHint, ppNameHint, &offset);

        isComputedName = true;
        break;
    }

    MemberType memberType;
    ushort flags = fFncMethod | fFncNoName;
    if (nop == knopGetMember)
    {TRACE_IT(29858);
        memberType = MemberTypeGetter;
        flags |= fFncNoArg;
    }
    else
    {TRACE_IT(29859);
        Assert(nop == knopSetMember);
        memberType = MemberTypeSetter;
        flags |= fFncOneArg;
    }

    this->m_parsingSuperRestrictionState = ParsingSuperRestrictionState_SuperPropertyAllowed;
    ParseNodePtr pnodeFnc = ParseFncDecl<buildAST>(flags, *ppNameHint,
        /*needsPIDOnRCurlyScan*/ false, /*resetParsingSuperRestrictionState*/ false);

    if (buildAST)
    {TRACE_IT(29860);
        pnodeFnc->sxFnc.SetIsAccessor();
        return CreateBinNode(nop, pnodeName, pnodeFnc);
    }
    else
    {TRACE_IT(29861);
        return nullptr;
    }
}

/***************************************************************************
Parse a list of object members. e.g. { x:foo, 'y me':bar }
***************************************************************************/
template<bool buildAST>
ParseNodePtr Parser::ParseMemberList(LPCOLESTR pNameHint, uint32* pNameHintLength, tokens declarationType)
{TRACE_IT(29862);
    ParseNodePtr pnodeArg = nullptr;
    ParseNodePtr pnodeName = nullptr;
    ParseNodePtr pnodeList = nullptr;
    ParseNodePtr *lastNodeRef = nullptr;
    LPCOLESTR pFullNameHint = nullptr;       // A calculated full name
    uint32 fullNameHintLength = pNameHintLength ? *pNameHintLength : 0;
    uint32 shortNameOffset = 0;
    bool isProtoDeclared = false;

    // we get declaration tkLCurly - when the possible object pattern found under the expression.
    bool isObjectPattern = (declarationType == tkVAR || declarationType == tkLET || declarationType == tkCONST || declarationType == tkLCurly) && IsES6DestructuringEnabled();

    // Check for an empty list
    if (tkRCurly == m_token.tk)
    {TRACE_IT(29863);
        return nullptr;
    }

    ArenaAllocator tempAllocator(_u("MemberNames"), m_nodeAllocator.GetPageAllocator(), Parser::OutOfMemory);

    bool hasDeferredInitError = false;

    for (;;)
    {TRACE_IT(29864);
        bool isComputedName = false;
#if DEBUG
        if((m_grfscr & fscrEnforceJSON) && (tkStrCon != m_token.tk || !(m_pscan->IsDoubleQuoteOnLastTkStrCon())))
        {TRACE_IT(29865);
            Error(ERRsyntax);
        }
#endif
        bool isAsyncMethod = false;
        charcount_t ichMin = 0;
        size_t iecpMin = 0;
        if (m_token.tk == tkID && m_token.GetIdentifier(m_phtbl) == wellKnownPropertyPids.async && m_scriptContext->GetConfig()->IsES7AsyncAndAwaitEnabled())
        {TRACE_IT(29866);
            RestorePoint parsedAsync;
            m_pscan->Capture(&parsedAsync);
            ichMin = m_pscan->IchMinTok();
            iecpMin = m_pscan->IecpMinTok();

            m_pscan->ScanForcingPid();
            if (m_token.tk == tkLParen || m_token.tk == tkColon || m_token.tk == tkRCurly || m_pscan->FHadNewLine())
            {TRACE_IT(29867);
                m_pscan->SeekTo(parsedAsync);
            }
            else
            {TRACE_IT(29868);
                isAsyncMethod = true;
            }
        }

        bool isGenerator = m_scriptContext->GetConfig()->IsES6GeneratorsEnabled() &&
                           m_token.tk == tkStar;
        ushort fncDeclFlags = fFncNoName | fFncMethod;
        if (isGenerator)
        {TRACE_IT(29869);
            if (isAsyncMethod)
            {TRACE_IT(29870);
                Error(ERRsyntax);
            }
            m_pscan->ScanForcingPid();
            fncDeclFlags |= fFncGenerator;
        }

        IdentPtr pidHint = nullptr;              // A name scoped to current expression
        Token tkHint = m_token;
        charcount_t idHintIchMin = static_cast<charcount_t>(m_pscan->IecpMinTok());
        charcount_t idHintIchLim = static_cast< charcount_t >(m_pscan->IecpLimTok());
        bool wrapInBrackets = false;
        switch (m_token.tk)
        {
        default:
            if (!m_token.IsReservedWord())
            {TRACE_IT(29871);
                Error(ERRnoMemberIdent);
            }
            // allow reserved words
            wrapInBrackets = true;
            // fall-through
        case tkID:
            pidHint = m_token.GetIdentifier(m_phtbl);
            if (buildAST)
            {TRACE_IT(29872);
                pnodeName = CreateStrNodeWithScanner(pidHint);
            }
            break;

        case tkStrCon:
            if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
            {TRACE_IT(29873);
                Error(ERRES5NoOctal);
            }
            wrapInBrackets = true;
            pidHint = m_token.GetStr();
            if (buildAST)
            {TRACE_IT(29874);
                pnodeName = CreateStrNodeWithScanner(pidHint);
            }
            break;

        case tkIntCon:
            // Object initializers with numeric labels allowed in JS6
            if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
            {TRACE_IT(29875);
                Error(ERRES5NoOctal);
            }

            pidHint = m_pscan->PidFromLong(m_token.GetLong());
            if (buildAST)
            {TRACE_IT(29876);
                pnodeName = CreateStrNodeWithScanner(pidHint);
            }
            break;

        case tkFltCon:
            if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
            {TRACE_IT(29877);
                Error(ERRES5NoOctal);
            }

            pidHint = m_pscan->PidFromDbl(m_token.GetDouble());
            if (buildAST)
            {TRACE_IT(29878);
                pnodeName = CreateStrNodeWithScanner(pidHint);
            }
            wrapInBrackets = true;
            break;

        case tkLBrack:
            // Computed property name: [expr] : value
            if (!m_scriptContext->GetConfig()->IsES6ObjectLiteralsEnabled())
            {TRACE_IT(29879);
                Error(ERRnoMemberIdent);
            }

            ParseComputedName<buildAST>(&pnodeName, &pNameHint, &pFullNameHint, &fullNameHintLength, &shortNameOffset);

            isComputedName = true;
            break;
        }

        if (pFullNameHint == nullptr)
        {TRACE_IT(29880);
            if (CONFIG_FLAG(UseFullName))
            {TRACE_IT(29881);
                pFullNameHint = AppendNameHints(pNameHint, pidHint, &fullNameHintLength, &shortNameOffset, false, wrapInBrackets);
            }
            else
            {TRACE_IT(29882);
                pFullNameHint = pidHint? pidHint->Psz() : nullptr;
                fullNameHintLength = pidHint ? pidHint->Cch() : 0;
                shortNameOffset = 0;
            }
        }

        RestorePoint atPid;
        m_pscan->Capture(&atPid);

        m_pscan->ScanForcingPid();

        if (isGenerator && m_token.tk != tkLParen)
        {TRACE_IT(29883);
            Error(ERRnoLparen);
        }

        if (tkColon == m_token.tk)
        {TRACE_IT(29884);
            // It is a syntax error is the production of the form __proto__ : <> occurs more than once. From B.3.1 in spec.
            // Note that previous scan is important because only after that we can determine we have a variable.
            if (!isComputedName && pidHint == wellKnownPropertyPids.__proto__)
            {TRACE_IT(29885);
                if (isProtoDeclared)
                {TRACE_IT(29886);
                    Error(ERRsyntax);
                }
                else
                {TRACE_IT(29887);
                    isProtoDeclared = true;
                }
            }

            m_pscan->Scan();
            ParseNodePtr pnodeExpr = nullptr;
            if (isObjectPattern)
            {TRACE_IT(29888);
                if (m_token.tk == tkEllipsis)
                {TRACE_IT(29889);
                    Error(ERRUnexpectedEllipsis);
                }
                pnodeExpr = ParseDestructuredVarDecl<buildAST>(declarationType, declarationType != tkLCurly, nullptr/* *hasSeenRest*/, false /*topLevel*/, false /*allowEmptyExpression*/);

                if (m_token.tk != tkComma && m_token.tk != tkRCurly)
                {TRACE_IT(29890);
                    if (m_token.IsOperator())
                    {TRACE_IT(29891);
                        Error(ERRDestructNoOper);
                    }
                    Error(ERRsyntax);
                }
            }
            else
            {TRACE_IT(29892);
                pnodeExpr = ParseExpr<buildAST>(koplCma, nullptr, TRUE, FALSE, pFullNameHint, &fullNameHintLength, &shortNameOffset);
            }
#if DEBUG
            if((m_grfscr & fscrEnforceJSON) && !IsJSONValid(pnodeExpr))
            {TRACE_IT(29893);
                Error(ERRsyntax);
            }
#endif
            if (buildAST)
            {TRACE_IT(29894);
                pnodeArg = CreateBinNode(isObjectPattern ? knopObjectPatternMember : knopMember, pnodeName, pnodeExpr);
                if (pnodeArg->sxBin.pnode1->nop == knopStr)
                {TRACE_IT(29895);
                    pnodeArg->sxBin.pnode1->sxPid.pid->PromoteAssignmentState();
                }
            }
        }
        else if (m_token.tk == tkLParen && m_scriptContext->GetConfig()->IsES6ObjectLiteralsEnabled())
        {TRACE_IT(29896);
            if (isObjectPattern)
            {TRACE_IT(29897);
                Error(ERRInvalidAssignmentTarget);
            }
            // Shorthand syntax: foo() {} -> foo: function() {}

            // Rewind to the PID and parse a function expression.
            m_pscan->SeekTo(atPid);
            this->m_parsingSuperRestrictionState = ParsingSuperRestrictionState_SuperPropertyAllowed;
            ParseNodePtr pnodeFunc = ParseFncDecl<buildAST>(fncDeclFlags | (isAsyncMethod ? fFncAsync : fFncNoFlgs), pFullNameHint,
                /*needsPIDOnRCurlyScan*/ false, /*resetParsingSuperRestrictionState*/ false);

            if (isAsyncMethod)
            {TRACE_IT(29898);
                pnodeFunc->sxFnc.cbMin = iecpMin;
                pnodeFunc->ichMin = ichMin;
            }
            if (buildAST)
            {TRACE_IT(29899);
                pnodeArg = CreateBinNode(knopMember, pnodeName, pnodeFunc);
            }
        }
        else if (nullptr != pidHint) //Its either tkID/tkStrCon/tkFloatCon/tkIntCon
        {TRACE_IT(29900);
            Assert(pidHint->Psz() != nullptr);

            if ((pidHint == wellKnownPropertyPids.get || pidHint == wellKnownPropertyPids.set) &&
                // get/set are only pseudo keywords when they are identifiers (i.e. not strings)
                tkHint.tk == tkID && NextTokenIsPropertyNameStart())
            {TRACE_IT(29901);
                if (isObjectPattern)
                {TRACE_IT(29902);
                    Error(ERRInvalidAssignmentTarget);
                }

                LPCOLESTR pNameGetOrSet = nullptr;
                OpCode op = pidHint == wellKnownPropertyPids.get ? knopGetMember : knopSetMember;

                pnodeArg = ParseMemberGetSet<buildAST>(op, &pNameGetOrSet);

                if (CONFIG_FLAG(UseFullName) && buildAST && pnodeArg->sxBin.pnode2->nop == knopFncDecl)
                {TRACE_IT(29903);
                    if (m_scriptContext->GetConfig()->IsES6FunctionNameEnabled())
                    {TRACE_IT(29904);
                        // displays as "get object.funcname" or "set object.funcname"
                        uint32 getOrSetOffset = 0;
                        LPCOLESTR intermediateHint = AppendNameHints(pNameHint, pNameGetOrSet, &fullNameHintLength, &shortNameOffset);
                        pFullNameHint = AppendNameHints(pidHint, intermediateHint, &fullNameHintLength, &getOrSetOffset, true);
                        shortNameOffset += getOrSetOffset;
                    }
                    else
                    {TRACE_IT(29905);
                        // displays as "object.funcname.get" or "object.funcname.set"
                        LPCOLESTR intermediateHint = AppendNameHints(pNameGetOrSet, pidHint, &fullNameHintLength, &shortNameOffset);
                        pFullNameHint = AppendNameHints(pNameHint, intermediateHint, &fullNameHintLength, &shortNameOffset);
                    }
                }
            }
            else if ((m_token.tk == tkRCurly || m_token.tk == tkComma || m_token.tk == tkAsg) && m_scriptContext->GetConfig()->IsES6ObjectLiteralsEnabled())
            {TRACE_IT(29906);
                // Shorthand {foo} -> {foo:foo} syntax.
                // {foo = <initializer>} supported only when on object pattern rules are being applied
                if (tkHint.tk != tkID)
                {TRACE_IT(29907);
                    Assert(tkHint.IsReservedWord()
                        || tkHint.tk == tkIntCon || tkHint.tk == tkFltCon || tkHint.tk == tkStrCon);
                    // All keywords are banned in non-strict mode.
                    // Future reserved words are banned in strict mode.
                    if (IsStrictMode() || !tkHint.IsFutureReservedWord(true))
                    {TRACE_IT(29908);
                        IdentifierExpectedError(tkHint);
                    }
                }

                if (buildAST)
                {
                    CheckArgumentsUse(pidHint, GetCurrentFunctionNode());
                }

                bool couldBeObjectPattern = !isObjectPattern && m_token.tk == tkAsg;

                if (couldBeObjectPattern)
                {TRACE_IT(29909);
                    declarationType = tkLCurly;
                    isObjectPattern = true;

                    // This may be an error but we are deferring for favouring destructuring.
                    hasDeferredInitError = true;
                }

                ParseNodePtr pnodeIdent = nullptr;
                if (isObjectPattern)
                {TRACE_IT(29910);
                    m_pscan->SeekTo(atPid);
                    pnodeIdent = ParseDestructuredVarDecl<buildAST>(declarationType, declarationType != tkLCurly, nullptr/* *hasSeenRest*/, false /*topLevel*/, false /*allowEmptyExpression*/);

                    if (m_token.tk != tkComma && m_token.tk != tkRCurly)
                    {TRACE_IT(29911);
                        if (m_token.IsOperator())
                        {TRACE_IT(29912);
                            Error(ERRDestructNoOper);
                        }
                        Error(ERRsyntax);
                    }
                }
                else
                {TRACE_IT(29913);
                    // Add a reference to the hinted name so we can bind it properly.
                    PidRefStack *ref = PushPidRef(pidHint);

                    if (buildAST)
                    {TRACE_IT(29914);
                        pnodeIdent = CreateNameNode(pidHint, idHintIchMin, idHintIchLim);
                        pnodeIdent->sxPid.SetSymRef(ref);
                    }
                }

                if (buildAST)
                {TRACE_IT(29915);
                    pnodeArg = CreateBinNode(isObjectPattern && !couldBeObjectPattern ? knopObjectPatternMember : knopMemberShort, pnodeName, pnodeIdent);
                }
            }
            else
            {TRACE_IT(29916);
                Error(ERRnoColon);
            }
        }
        else
        {TRACE_IT(29917);
            Error(ERRnoColon);
        }

        if (buildAST)
        {TRACE_IT(29918);
            Assert(pnodeArg->sxBin.pnode2 != nullptr);
            if (pnodeArg->sxBin.pnode2->nop == knopFncDecl)
            {TRACE_IT(29919);
                Assert(fullNameHintLength >= shortNameOffset);
                pnodeArg->sxBin.pnode2->sxFnc.hint = pFullNameHint;
                pnodeArg->sxBin.pnode2->sxFnc.hintLength =  fullNameHintLength;
                pnodeArg->sxBin.pnode2->sxFnc.hintOffset  = shortNameOffset;
            }
            AddToNodeListEscapedUse(&pnodeList, &lastNodeRef, pnodeArg);
        }
        pidHint = nullptr;
        pFullNameHint = nullptr;
        if (tkComma != m_token.tk)
        {TRACE_IT(29920);
            break;
        }
        m_pscan->ScanForcingPid();
        if (tkRCurly == m_token.tk)
        {TRACE_IT(29921);
            break;
        }
    }

    m_hasDeferredShorthandInitError = m_hasDeferredShorthandInitError || hasDeferredInitError;

    if (buildAST)
    {TRACE_IT(29922);
        AssertMem(lastNodeRef);
        AssertNodeMem(*lastNodeRef);
        pnodeList->ichLim = (*lastNodeRef)->ichLim;
    }

    return pnodeList;
}

BOOL Parser::DeferredParse(Js::LocalFunctionId functionId)
{TRACE_IT(29923);
    if ((m_grfscr & fscrDeferFncParse) != 0)
    {TRACE_IT(29924);
        if (m_stoppedDeferredParse)
        {TRACE_IT(29925);
            return false;
        }
        if (PHASE_OFF_RAW(Js::DeferParsePhase, m_sourceContextInfo->sourceContextId, functionId))
        {TRACE_IT(29926);
            return false;
        }
        if (PHASE_FORCE_RAW(Js::DeferParsePhase, m_sourceContextInfo->sourceContextId, functionId))
        {TRACE_IT(29927);
            return true;
        }
#if ENABLE_PROFILE_INFO
#ifndef DISABLE_DYNAMIC_PROFILE_DEFER_PARSE
        if (m_sourceContextInfo->sourceDynamicProfileManager != nullptr)
        {TRACE_IT(29928);
            Js::ExecutionFlags flags = m_sourceContextInfo->sourceDynamicProfileManager->IsFunctionExecuted(functionId);
            return flags != Js::ExecutionFlags_Executed;
        }
#endif
#endif
        return true;
    }

    return false;
}

//
// Call this in ParseFncDecl only to check (and reset) if ParseFncDecl is re-parsing a deferred
// function body. If a deferred function is called and being re-parsed, it shouldn't be deferred again.
//
BOOL Parser::IsDeferredFnc()
{TRACE_IT(29929);
    if (m_grfscr & fscrDeferredFnc)
    {TRACE_IT(29930);
        m_grfscr &= ~fscrDeferredFnc;
        return true;
    }

    return false;
}

template<bool buildAST>
ParseNodePtr Parser::ParseFncDecl(ushort flags, LPCOLESTR pNameHint, const bool needsPIDOnRCurlyScan, bool resetParsingSuperRestrictionState, bool fUnaryOrParen)
{TRACE_IT(29931);
    AutoParsingSuperRestrictionStateRestorer restorer(this);
    if (resetParsingSuperRestrictionState)
    {TRACE_IT(29932);
        //  ParseFncDecl will always reset m_parsingSuperRestrictionState to super disallowed unless explicitly disabled
        this->m_parsingSuperRestrictionState = ParsingSuperRestrictionState_SuperDisallowed;
    }

    ParseNodePtr pnodeFnc = nullptr;
    ParseNodePtr *ppnodeVarSave = nullptr;
    ParseNodePtr pnodeFncBlockScope = nullptr;
    ParseNodePtr *ppnodeScopeSave = nullptr;
    ParseNodePtr *ppnodeExprScopeSave = nullptr;
    bool funcHasName = false;
    bool fDeclaration = flags & fFncDeclaration;
    bool fModule = (flags & fFncModule) != 0;
    bool fLambda = (flags & fFncLambda) != 0;
    charcount_t ichMin = this->m_pscan->IchMinTok();
    bool wasInDeferredNestedFunc = false;

    uint tryCatchOrFinallyDepthSave = this->m_tryCatchOrFinallyDepth;
    this->m_tryCatchOrFinallyDepth = 0;

    if (this->m_arrayDepth)
    {TRACE_IT(29933);
        this->m_funcInArrayDepth++; // Count function depth within array literal
    }

    // Update the count of functions nested in the current parent.
    Assert(m_pnestedCount || !buildAST);
    uint *pnestedCountSave = m_pnestedCount;
    if (buildAST || m_pnestedCount)
    {TRACE_IT(29934);
        (*m_pnestedCount)++;
    }

    uint scopeCountNoAstSave = m_scopeCountNoAst;
    m_scopeCountNoAst = 0;

    bool noStmtContext = false;

    if (fDeclaration)
    {TRACE_IT(29935);
        noStmtContext = m_pstmtCur->GetNop() != knopBlock;

        if (noStmtContext)
        {TRACE_IT(29936);
            // We have a function declaration like "if (a) function f() {}". We didn't see
            // a block scope on the way in, so we need to pretend we did. Note that this is a syntax error
            // in strict mode.
            if (!this->FncDeclAllowedWithoutContext(flags))
            {TRACE_IT(29937);
                Error(ERRsyntax);
            }
            pnodeFncBlockScope = StartParseBlock<buildAST>(PnodeBlockType::Regular, ScopeType_Block);
            if (buildAST)
            {
                PushFuncBlockScope(pnodeFncBlockScope, &ppnodeScopeSave, &ppnodeExprScopeSave);
            }
        }
    }

    // Create the node.
    pnodeFnc = CreateNode(knopFncDecl);
    pnodeFnc->sxFnc.ClearFlags();
    pnodeFnc->sxFnc.SetDeclaration(fDeclaration);
    pnodeFnc->sxFnc.astSize             = 0;
    pnodeFnc->sxFnc.pnodeName           = nullptr;
    pnodeFnc->sxFnc.pnodeScopes         = nullptr;
    pnodeFnc->sxFnc.pnodeRest           = nullptr;
    pnodeFnc->sxFnc.pid                 = nullptr;
    pnodeFnc->sxFnc.hint                = nullptr;
    pnodeFnc->sxFnc.hintOffset          = 0;
    pnodeFnc->sxFnc.hintLength          = 0;
    pnodeFnc->sxFnc.isNameIdentifierRef = true;
    pnodeFnc->sxFnc.nestedFuncEscapes   = false;
    pnodeFnc->sxFnc.pnodeNext           = nullptr;
    pnodeFnc->sxFnc.pnodeParams         = nullptr;
    pnodeFnc->sxFnc.pnodeVars           = nullptr;
    pnodeFnc->sxFnc.funcInfo            = nullptr;
    pnodeFnc->sxFnc.deferredStub        = nullptr;
    pnodeFnc->sxFnc.nestedCount         = 0;
    pnodeFnc->sxFnc.cbMin = m_pscan->IecpMinTok();
    pnodeFnc->sxFnc.functionId = (*m_nextFunctionId)++;
    pnodeFnc->sxFnc.isBodyAndParamScopeMerged = true;

    // Push new parser state with this new function node

    AppendFunctionToScopeList(fDeclaration, pnodeFnc);

    // Start the argument list.
    ppnodeVarSave = m_ppnodeVar;

    if (buildAST)
    {TRACE_IT(29938);
        pnodeFnc->sxFnc.lineNumber = m_pscan->LineCur();
        pnodeFnc->sxFnc.columnNumber = CalculateFunctionColumnNumber();
        pnodeFnc->sxFnc.SetNested(m_currentNodeFunc != nullptr); // If there is a current function, then we're a nested function.
        pnodeFnc->sxFnc.SetStrictMode(IsStrictMode()); // Inherit current strict mode -- may be overridden by the function itself if it contains a strict mode directive.
        pnodeFnc->sxFnc.firstDefaultArg = 0;

        m_pCurrentAstSize = &pnodeFnc->sxFnc.astSize;
    }
    else // if !buildAST
    {TRACE_IT(29939);
        wasInDeferredNestedFunc = m_inDeferredNestedFunc;
        m_inDeferredNestedFunc = true;
    }

    m_pnestedCount = &pnodeFnc->sxFnc.nestedCount;

    AnalysisAssert(pnodeFnc);
    pnodeFnc->sxFnc.SetIsAsync((flags & fFncAsync) != 0);
    pnodeFnc->sxFnc.SetIsLambda(fLambda);
    pnodeFnc->sxFnc.SetIsMethod((flags & fFncMethod) != 0);
    pnodeFnc->sxFnc.SetIsClassMember((flags & fFncClassMember) != 0);
    pnodeFnc->sxFnc.SetIsModule(fModule);

    bool needScanRCurly = true;
    bool result = ParseFncDeclHelper<buildAST>(pnodeFnc, pNameHint, flags, &funcHasName, fUnaryOrParen, noStmtContext, &needScanRCurly, fModule);
    if (!result)
    {TRACE_IT(29940);
        Assert(!pnodeFncBlockScope);

        return pnodeFnc;
    }

    AnalysisAssert(pnodeFnc);

    *m_ppnodeVar = nullptr;
    m_ppnodeVar = ppnodeVarSave;

    if (m_currentNodeFunc && (pnodeFnc->sxFnc.CallsEval() || pnodeFnc->sxFnc.ChildCallsEval()))
    {TRACE_IT(29941);
        GetCurrentFunctionNode()->sxFnc.SetChildCallsEval(true);
    }

    ParseNodePtr pnodeFncParent = buildAST ? m_currentNodeFunc : m_currentNodeDeferredFunc;

    // Lambdas do not have "arguments" and instead capture their parent's
    // binding of "arguments.  To ensure the arguments object of the enclosing
    // non-lambda function is loaded propagate the UsesArguments flag up to
    // the parent function
    if ((flags & fFncLambda) != 0 && pnodeFnc->sxFnc.UsesArguments())
    {TRACE_IT(29942);
        if (pnodeFncParent != nullptr)
        {TRACE_IT(29943);
            pnodeFncParent->sxFnc.SetUsesArguments();
        }
        else
        {TRACE_IT(29944);
            m_UsesArgumentsAtGlobal = true;
        }
    }

    if (needScanRCurly && !fModule)
    {TRACE_IT(29945);
        // Consume the next token now that we're back in the enclosing function (whose strictness may be
        // different from the function we just finished).
#if DBG
        bool expectedTokenValid = m_token.tk == tkRCurly;
        AssertMsg(expectedTokenValid, "Invalid token expected for RCurly match");
#endif
        // The next token may need to have a PID created in !buildAST mode, as we may be parsing a method with a string name.
        if (needsPIDOnRCurlyScan)
        {TRACE_IT(29946);
            m_pscan->ScanForcingPid();
        }
        else
        {TRACE_IT(29947);
            m_pscan->Scan();
        }
    }

    m_pnestedCount = pnestedCountSave;
    Assert(!buildAST || !wasInDeferredNestedFunc);
    m_inDeferredNestedFunc = wasInDeferredNestedFunc;

    if (this->m_arrayDepth)
    {TRACE_IT(29948);
        this->m_funcInArrayDepth--;
        if (this->m_funcInArrayDepth == 0)
        {TRACE_IT(29949);
            // We disable deferred parsing if array literals dominate.
            // But don't do this if the array literal is dominated by function bodies.
            if (flags & (fFncMethod | fFncClassMember) && m_token.tk != tkSColon)
            {TRACE_IT(29950);
                // Class member methods have optional separators. We need to check whether we are
                // getting the IchLim of the correct token.
                Assert(m_pscan->m_tkPrevious == tkRCurly && needScanRCurly);

                this->m_funcInArray += m_pscan->IchMinTok() - /*tkRCurly*/ 1 - ichMin;
            }
            else
            {TRACE_IT(29951);
                this->m_funcInArray += m_pscan->IchLimTok() - ichMin;
            }
        }
    }

    m_scopeCountNoAst = scopeCountNoAstSave;

    if (buildAST && fDeclaration && !IsStrictMode())
    {TRACE_IT(29952);
        if (pnodeFnc->sxFnc.pnodeName != nullptr && pnodeFnc->sxFnc.pnodeName->nop == knopVarDecl &&
            GetCurrentBlock()->sxBlock.blockType == PnodeBlockType::Regular)
        {TRACE_IT(29953);
            // Add a function-scoped VarDecl with the same name as the function for
            // back compat with pre-ES6 code that declares functions in blocks. The
            // idea is that the last executed declaration wins at the function scope
            // level and we accomplish this by having each block scoped function
            // declaration assign to both the block scoped "let" binding, as well
            // as the function scoped "var" binding.
            bool isRedecl = false;
            ParseNodePtr vardecl = CreateVarDeclNode(pnodeFnc->sxFnc.pnodeName->sxVar.pid, STVariable, false, nullptr, false, &isRedecl);
            vardecl->sxVar.isBlockScopeFncDeclVar = true;
            if (isRedecl)
            {TRACE_IT(29954);
                vardecl->sxVar.sym->SetHasBlockFncVarRedecl();
            }
        }
    }

    if (pnodeFncBlockScope)
    {TRACE_IT(29955);
        Assert(pnodeFncBlockScope->sxBlock.pnodeStmt == nullptr);
        pnodeFncBlockScope->sxBlock.pnodeStmt = pnodeFnc;
        if (buildAST)
        {
            PopFuncBlockScope(ppnodeScopeSave, ppnodeExprScopeSave);
        }
        FinishParseBlock(pnodeFncBlockScope);
        return pnodeFncBlockScope;
    }

    this->m_tryCatchOrFinallyDepth = tryCatchOrFinallyDepthSave;

    return pnodeFnc;
}

bool Parser::FncDeclAllowedWithoutContext(ushort flags)
{TRACE_IT(29956);
    // Statement context required for strict mode, async functions, and generators.
    // Note that generators aren't detected yet when this method is called; they're checked elsewhere.
    return !IsStrictMode() && !(flags & fFncAsync);
}

uint Parser::CalculateFunctionColumnNumber()
{TRACE_IT(29957);
    uint columnNumber;

    if (m_pscan->IchMinTok() >= m_pscan->IchMinLine())
    {TRACE_IT(29958);
        // In scenarios involving defer parse IchMinLine() can be incorrect for the first line after defer parse
        columnNumber = m_pscan->IchMinTok() - m_pscan->IchMinLine();
        if (m_functionBody != nullptr && m_functionBody->GetRelativeLineNumber() == m_pscan->LineCur())
        {TRACE_IT(29959);
            // Adjust the column if it falls on the first line, where the re-parse is happening.
            columnNumber += m_functionBody->GetRelativeColumnNumber();
        }
    }
    else if (m_currentNodeFunc)
    {TRACE_IT(29960);
        // For the first line after defer parse, compute the column relative to the column number
        // of the lexically parent function.
        ULONG offsetFromCurrentFunction = m_pscan->IchMinTok() - m_currentNodeFunc->ichMin;
        columnNumber = m_currentNodeFunc->sxFnc.columnNumber + offsetFromCurrentFunction ;
    }
    else
    {TRACE_IT(29961);
        // if there is no current function, lets give a default of 0.
        columnNumber = 0;
    }

    return columnNumber;
}

void Parser::AppendFunctionToScopeList(bool fDeclaration, ParseNodePtr pnodeFnc)
{TRACE_IT(29962);
    if (!fDeclaration && m_ppnodeExprScope)
    {TRACE_IT(29963);
        // We're tracking function expressions separately from declarations in this scope
        // (e.g., inside a catch scope in standards mode).
        Assert(*m_ppnodeExprScope == nullptr);
        *m_ppnodeExprScope = pnodeFnc;
        m_ppnodeExprScope = &pnodeFnc->sxFnc.pnodeNext;
    }
    else
    {TRACE_IT(29964);
        Assert(*m_ppnodeScope == nullptr);
        *m_ppnodeScope = pnodeFnc;
        m_ppnodeScope = &pnodeFnc->sxFnc.pnodeNext;
    }
}

/***************************************************************************
Parse a function definition.
***************************************************************************/
template<bool buildAST>
bool Parser::ParseFncDeclHelper(ParseNodePtr pnodeFnc, LPCOLESTR pNameHint, ushort flags, bool *pHasName, bool fUnaryOrParen, bool noStmtContext, bool *pNeedScanRCurly, bool skipFormals)
{TRACE_IT(29965);
    ParseNodePtr pnodeFncParent = GetCurrentFunctionNode();
    // is the following correct? When buildAST is false, m_currentNodeDeferredFunc can be nullptr on transition to deferred parse from non-deferred
    ParseNodePtr pnodeFncSave = buildAST ? m_currentNodeFunc : m_currentNodeDeferredFunc;
    ParseNodePtr pnodeFncSaveNonLambda = buildAST ? m_currentNodeNonLambdaFunc : m_currentNodeNonLambdaDeferredFunc;
    int32* pAstSizeSave = m_pCurrentAstSize;

    bool fDeclaration = (flags & fFncDeclaration) != 0;
    bool fLambda = (flags & fFncLambda) != 0;
    bool fAsync = (flags & fFncAsync) != 0;
    bool fModule = (flags & fFncModule) != 0;
    bool fDeferred = false;
    StmtNest *pstmtSave;
    ParseNodePtr *lastNodeRef = nullptr;
    bool fFunctionInBlock = false;
    if (buildAST)
    {TRACE_IT(29966);
        fFunctionInBlock = GetCurrentBlockInfo() != GetCurrentFunctionBlockInfo() &&
            (GetCurrentBlockInfo()->pnodeBlock->sxBlock.scope == nullptr ||
             GetCurrentBlockInfo()->pnodeBlock->sxBlock.scope->GetScopeType() != ScopeType_GlobalEvalBlock);
    }

    // Save the position of the scanner in case we need to inspect the name hint later
    RestorePoint beginNameHint;
    m_pscan->Capture(&beginNameHint);

    ParseNodePtr pnodeFncExprScope = nullptr;
    Scope *fncExprScope = nullptr;
    if (!fDeclaration)
    {TRACE_IT(29967);
        pnodeFncExprScope = StartParseBlock<buildAST>(PnodeBlockType::Function, ScopeType_FuncExpr);
        fncExprScope = pnodeFncExprScope->sxBlock.scope;

        // Function expression: push the new function onto the stack now so that the name (if any) will be
        // local to the new function.

        this->UpdateCurrentNodeFunc<buildAST>(pnodeFnc, fLambda);
    }

    *pHasName = !fLambda && !fModule && this->ParseFncNames<buildAST>(pnodeFnc, pnodeFncSave, flags, &lastNodeRef);

    if (fDeclaration)
    {TRACE_IT(29968);
        // Declaration statement: push the new function now, after parsing the name, so the name is local to the
        // enclosing function.

        this->UpdateCurrentNodeFunc<buildAST>(pnodeFnc, fLambda);
    }

    if (noStmtContext && pnodeFnc->sxFnc.IsGenerator())
    {
        // Generator decl not allowed outside stmt context. (We have to wait until we've parsed the '*' to
        // detect generator.)
        Error(ERRsyntax, pnodeFnc);
    }

    // switch scanner to treat 'yield' as keyword in generator functions
    // or as an identifier in non-generator functions
    bool fPreviousYieldIsKeyword = m_pscan->SetYieldIsKeyword(pnodeFnc && pnodeFnc->sxFnc.IsGenerator());

    bool fPreviousAwaitIsKeyword = m_pscan->SetAwaitIsKeyword(fAsync);

    if (pnodeFnc && pnodeFnc->sxFnc.IsGenerator())
    {
        CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(Generator, m_scriptContext);
    }

    if (fncExprScope && !*pHasName)
    {TRACE_IT(29969);
        FinishParseBlock(pnodeFncExprScope);
        m_nextBlockId--;
        Adelete(&m_nodeAllocator, fncExprScope);
        fncExprScope = nullptr;
        pnodeFncExprScope = nullptr;
    }
    if (pnodeFnc)
    {TRACE_IT(29970);
        pnodeFnc->sxFnc.scope = fncExprScope;
    }

    // Start a new statement stack.
    bool topLevelStmt =
        buildAST &&
        !fFunctionInBlock &&
        (this->m_pstmtCur == nullptr || this->m_pstmtCur->pnodeStmt->nop == knopBlock);

    pstmtSave = m_pstmtCur;
    SetCurrentStatement(nullptr);

    // Function definition is inside the parent function's parameter scope
    bool isEnclosedInParamScope = this->m_currentScope->GetScopeType() == ScopeType_Parameter;

    if (this->m_currentScope->GetScopeType() == ScopeType_FuncExpr || this->m_currentScope->GetScopeType() == ScopeType_Block)
    {TRACE_IT(29971);
        // Or this is a function expression or class enclosed in a parameter scope
        isEnclosedInParamScope = this->m_currentScope->GetEnclosingScope() && this->m_currentScope->GetEnclosingScope()->GetScopeType() == ScopeType_Parameter;
    }

    Assert(!isEnclosedInParamScope || pnodeFncSave->sxFnc.HasNonSimpleParameterList());

    RestorePoint beginFormals;
    m_pscan->Capture(&beginFormals);
    BOOL fWasAlreadyStrictMode = IsStrictMode();
    BOOL oldStrictMode = this->m_fUseStrictMode;

    if (fLambda)
    {
        CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(Lambda, m_scriptContext);
    }

    uint uDeferSave = m_grfscr & fscrDeferFncParse;
    if ((!fDeclaration && m_ppnodeExprScope) ||
        isEnclosedInParamScope ||
        (flags & (fFncNoName | fFncLambda)))
    {TRACE_IT(29972);
        // NOTE: Don't defer if this is a function expression inside a construct that induces
        // a scope nested within the current function (like a with, or a catch in ES5 mode, or
        // any function declared inside a nested lexical block or param scope in ES6 mode).
        // We won't be able to reconstruct the scope chain properly when we come back and
        // try to compile just the function expression.
        // Also shut off deferring on getter/setter or other construct with unusual text bounds
        // (fFncNoName|fFncLambda) as these are usually trivial, and re-parsing is problematic.
        m_grfscr &= ~fscrDeferFncParse;
    }

    bool saveInFIB = this->m_inFIB;
    this->m_inFIB = fFunctionInBlock || this->m_inFIB;

    bool isTopLevelDeferredFunc = false;

    struct AutoFastScanFlag {
        bool savedDoingFastScan;
        AutoFastScanFlag(Parser *parser) : m_parser(parser) {TRACE_IT(29973); savedDoingFastScan = m_parser->m_doingFastScan; }
        ~AutoFastScanFlag() {TRACE_IT(29974); m_parser->m_doingFastScan = savedDoingFastScan; }
        Parser *m_parser;
    } flag(this);

    bool doParallel = false;
    bool parallelJobStarted = false;
    if (buildAST)
    {TRACE_IT(29975);
        bool isLikelyIIFE = !fDeclaration && pnodeFnc && fUnaryOrParen;

        BOOL isDeferredFnc = IsDeferredFnc();
        AnalysisAssert(isDeferredFnc || pnodeFnc);
        // These are the conditions that prohibit upfront deferral *and* redeferral.
        isTopLevelDeferredFunc =
            (!fLambda
             && pnodeFnc
             && DeferredParse(pnodeFnc->sxFnc.functionId)
             && (!pnodeFnc->sxFnc.IsNested() || CONFIG_FLAG(DeferNested))
             && !m_InAsmMode
            // Don't defer a module function wrapper because we need to do export resolution at parse time
             && !fModule
            );

        if (pnodeFnc)
        {TRACE_IT(29976);
            pnodeFnc->sxFnc.SetCanBeDeferred(isTopLevelDeferredFunc && PnFnc::CanBeRedeferred(pnodeFnc->sxFnc.fncFlags));
            pnodeFnc->sxFnc.SetFIBPreventsDeferral(false);
        }

        if (this->m_inFIB)
        {TRACE_IT(29977);
            if (isTopLevelDeferredFunc)
            {TRACE_IT(29978);
                // Block-scoping is the only non-heuristic reason for not deferring this function up front.
                // So on creating the full FunctionBody at byte code gen time, verify that there is no
                // block-scoped content visible to this function so it can remain a redeferral candidate.
                pnodeFnc->sxFnc.SetFIBPreventsDeferral(true);
            }
            isTopLevelDeferredFunc = false;
        }        

        // These are heuristic conditions that prohibit upfront deferral but not redeferral.
        isTopLevelDeferredFunc = isTopLevelDeferredFunc && !isDeferredFnc && 
            (!isLikelyIIFE || !topLevelStmt || PHASE_FORCE_RAW(Js::DeferParsePhase, m_sourceContextInfo->sourceContextId, pnodeFnc->sxFnc.functionId));
;
        if (!fLambda &&
            !isDeferredFnc &&
            !isLikelyIIFE &&
            !this->IsBackgroundParser() &&
            !this->m_doingFastScan &&
            !(pnodeFncSave && m_currDeferredStub) &&
            !(this->m_parseType == ParseType_Deferred && this->m_functionBody && this->m_functionBody->GetScopeInfo() && !isTopLevelDeferredFunc))
        {TRACE_IT(29979);
            doParallel = DoParallelParse(pnodeFnc);
#if ENABLE_BACKGROUND_PARSING
            if (doParallel)
            {TRACE_IT(29980);
                BackgroundParser *bgp = m_scriptContext->GetBackgroundParser();
                Assert(bgp);
                if (bgp->HasFailedBackgroundParseItem())
                {TRACE_IT(29981);
                    Error(ERRsyntax);
                }
                doParallel = bgp->ParseBackgroundItem(this, pnodeFnc, isTopLevelDeferredFunc);
                if (doParallel)
                {TRACE_IT(29982);
                    parallelJobStarted = true;
                    this->m_hasParallelJob = true;
                    this->m_doingFastScan = true;
                    doParallel = FastScanFormalsAndBody();
                    if (doParallel)
                    {TRACE_IT(29983);
                        // Let the foreground thread take care of marking the limit on the function node,
                        // because in some cases this function's caller will want to change that limit,
                        // so we don't want the background thread to try and touch it.
                        pnodeFnc->ichLim = m_pscan->IchLimTok();
                        pnodeFnc->sxFnc.cbLim = m_pscan->IecpLimTok();
                    }
                }
            }
#endif
        }
    }

    if (!doParallel)
    {TRACE_IT(29984);
        // We don't want to, or couldn't, let the main thread scan past this function body, so parse
        // it for real.
        ParseNodePtr pnodeRealFnc = pnodeFnc;
        if (parallelJobStarted)
        {TRACE_IT(29985);
            // We have to deal with a failure to fast-scan the function (due to syntax error? "/"?) when
            // a background thread may already have begun to work on the job. Both threads can't be allowed to
            // operate on the same node.
            pnodeFnc = CreateDummyFuncNode(fDeclaration);
        }

        AnalysisAssert(pnodeFnc);
        ParseNodePtr pnodeBlock = StartParseBlock<buildAST>(PnodeBlockType::Parameter, ScopeType_Parameter);
        AnalysisAssert(pnodeBlock != nullptr);
        pnodeFnc->sxFnc.pnodeScopes = pnodeBlock;
        m_ppnodeVar = &pnodeFnc->sxFnc.pnodeParams;
        pnodeFnc->sxFnc.pnodeVars = nullptr;
        ParseNodePtr* varNodesList = &pnodeFnc->sxFnc.pnodeVars;
        ParseNodePtr argNode = nullptr;

        if (!fModule && !fLambda)
        {TRACE_IT(29986);
            ParseNodePtr *const ppnodeVarSave = m_ppnodeVar;
            m_ppnodeVar = &pnodeFnc->sxFnc.pnodeVars;

            // Create the built-in arguments symbol
            argNode = this->AddArgumentsNodeToVars(pnodeFnc);

            // Save the updated var list
            varNodesList = m_ppnodeVar;
            m_ppnodeVar = ppnodeVarSave;
        }

        ParseNodePtr *ppnodeScopeSave = nullptr;
        ParseNodePtr *ppnodeExprScopeSave = nullptr;

        ppnodeScopeSave = m_ppnodeScope;
        if (pnodeBlock)
        {TRACE_IT(29987);
            // This synthetic block scope will contain all the nested scopes.
            m_ppnodeScope = &pnodeBlock->sxBlock.pnodeScopes;
            pnodeBlock->sxBlock.pnodeStmt = pnodeFnc;
        }

        // Keep nested function declarations and expressions in the same list at function scope.
        // (Indicate this by nulling out the current function expressions list.)
        ppnodeExprScopeSave = m_ppnodeExprScope;
        m_ppnodeExprScope = nullptr;

        if (!skipFormals)
        {TRACE_IT(29988);
            bool fLambdaParamsSave = m_reparsingLambdaParams;
            if (fLambda)
            {TRACE_IT(29989);
                m_reparsingLambdaParams = true;
            }
            this->ParseFncFormals<buildAST>(pnodeFnc, pnodeFncParent, flags);
            m_reparsingLambdaParams = fLambdaParamsSave;
        }

        // Create function body scope
        ParseNodePtr pnodeInnerBlock = StartParseBlock<buildAST>(PnodeBlockType::Function, ScopeType_FunctionBody);
        // Set the parameter block's child to the function body block.
        // The pnodeFnc->sxFnc.pnodeScopes list is constructed in such a way that it includes all the scopes in this list.
        // For example if the param scope has one function and body scope has one function then the list will look like below,
        // param scope block -> function decl from param scope -> body socpe block -> function decl from body scope.
        *m_ppnodeScope = pnodeInnerBlock;
        pnodeFnc->sxFnc.pnodeBodyScope = pnodeInnerBlock;

        // This synthetic block scope will contain all the nested scopes.
        m_ppnodeScope = &pnodeInnerBlock->sxBlock.pnodeScopes;
        pnodeInnerBlock->sxBlock.pnodeStmt = pnodeFnc;

        // DEFER: Begin deferral here (after names are parsed and name nodes created).
        // Create no more AST nodes until we're done.

        // Try to defer this func if all these are true:
        //  0. We are not already in deferred parsing (i.e. buildAST is true)
        //  1. We are not re-parsing a deferred func which is being invoked.
        //  2. Dynamic profile suggests this func can be deferred (and deferred parse is on).
        //  3. This func is top level or defer nested func is on.
        //  4. Optionally, the function is non-nested and not in eval, or the deferral decision was based on cached profile info,
        //     or the function is sufficiently long. (I.e., don't defer little nested functions unless we're
        //     confident they'll never be executed, because un-deferring nested functions is more expensive.)
        //     NOTE: I'm disabling #4 by default, because we've found other ways to reduce the cost of un-deferral,
        //           and we don't want to create function bodies aggressively for little functions.

        // We will also temporarily defer all asm.js functions, except for the asm.js
        // module itself, which we will never defer
        bool strictModeTurnedOn = false;

        if (isTopLevelDeferredFunc &&
            !(this->m_grfscr & fscrEvalCode) &&
            pnodeFnc->sxFnc.IsNested() &&
#ifndef DISABLE_DYNAMIC_PROFILE_DEFER_PARSE
            m_sourceContextInfo->sourceDynamicProfileManager == nullptr &&
#endif
            PHASE_ON_RAW(Js::ScanAheadPhase, m_sourceContextInfo->sourceContextId, pnodeFnc->sxFnc.functionId) &&
            (
                !PHASE_FORCE_RAW(Js::DeferParsePhase, m_sourceContextInfo->sourceContextId, pnodeFnc->sxFnc.functionId) ||
                PHASE_FORCE_RAW(Js::ScanAheadPhase, m_sourceContextInfo->sourceContextId, pnodeFnc->sxFnc.functionId)
            ))
        {TRACE_IT(29990);
            // Try to scan ahead to the end of the function. If we get there before we've scanned a minimum
            // number of tokens, don't bother deferring, because it's too small.
            if (this->ScanAheadToFunctionEnd(CONFIG_FLAG(MinDeferredFuncTokenCount)))
            {TRACE_IT(29991);
                isTopLevelDeferredFunc = false;
            }
        }

        Scope* paramScope = pnodeFnc->sxFnc.pnodeScopes ? pnodeFnc->sxFnc.pnodeScopes->sxBlock.scope : nullptr;
        if (paramScope != nullptr)
        {TRACE_IT(29992);
            if (CONFIG_FLAG(ForceSplitScope))
            {TRACE_IT(29993);
                pnodeFnc->sxFnc.ResetBodyAndParamScopeMerged();
            }
            else if (pnodeFnc->sxFnc.HasNonSimpleParameterList() && pnodeFnc->sxFnc.IsBodyAndParamScopeMerged())
            {TRACE_IT(29994);
                paramScope->ForEachSymbolUntil([this, paramScope, pnodeFnc](Symbol* sym) {
                    if (sym->GetPid()->GetTopRef()->GetFuncScopeId() > pnodeFnc->sxFnc.functionId)
                    {TRACE_IT(29995);
                        // One of the symbol has non local reference. Mark the param scope as we can't merge it with body scope.
                        pnodeFnc->sxFnc.ResetBodyAndParamScopeMerged();
                        return true;
                    }
                    return false;
                });
            }
            if (pnodeFnc->sxFnc.IsBodyAndParamScopeMerged() && !fDeclaration && pnodeFnc->sxFnc.pnodeName != nullptr)
            {TRACE_IT(29996);
                Symbol* funcSym = pnodeFnc->sxFnc.pnodeName->sxVar.sym;
                if (funcSym->GetPid()->GetTopRef()->GetFuncScopeId() > pnodeFnc->sxFnc.functionId)
                {TRACE_IT(29997);
                    // This is a function expression with name captured in the param scope. In non-eval, non-split cases the function
                    // name symbol is added to the body scope to make it accessible in the body. But if there is a function or var
                    // declaration with the same name in the body then adding to the body will fail. So in this case we have to add
                    // the name symbol to the param scope by splitting it.
                    pnodeFnc->sxFnc.ResetBodyAndParamScopeMerged();
                }
            }
        }

        // If the param scope is merged with the body scope we want to use the param scope symbols in the body scope.
        // So add a pid ref for the body using the param scope symbol. Note that in this case the same symbol will occur twice
        // in the same pid ref stack.
        if (paramScope != nullptr && pnodeFnc->sxFnc.IsBodyAndParamScopeMerged())
        {TRACE_IT(29998);
            paramScope->ForEachSymbol([this](Symbol* paramSym)
            {
                PidRefStack* ref = PushPidRef(paramSym->GetPid());
                ref->SetSym(paramSym);
            });
        }

        if (isTopLevelDeferredFunc || (m_InAsmMode && m_deferAsmJs))
        {
            AssertMsg(!fLambda, "Deferring function parsing of a function does not handle lambda syntax");
            fDeferred = true;

            this->ParseTopLevelDeferredFunc(pnodeFnc, pnodeFncSave, pNameHint);
        }
        else
        {TRACE_IT(29999);
            if (m_token.tk == tkRParen) // This might be false due to error recovery or lambda.
            {TRACE_IT(30000);
                m_pscan->Scan();
            }

            if (fLambda)
            {TRACE_IT(30001);
                BOOL hadNewLine = m_pscan->FHadNewLine();

                // it can be the case we do not have a fat arrow here if there is a valid expression on the left hand side
                // of the fat arrow, but that expression does not parse as a parameter list.  E.g.
                //    a.x => { }
                // Therefore check for it and error if not found.
                // LS Mode : since this is a lambda we supposed to get the fat arrow, if not we will skip till we get that fat arrow.
                ChkCurTok(tkDArrow, ERRnoDArrow);

                // Newline character between arrow parameters and fat arrow is a syntax error but we want to check for
                // this after verifying there was a => token. Otherwise we would throw the wrong error.
                if (hadNewLine)
                {TRACE_IT(30002);
                    Error(ERRsyntax);
                }
            }

            AnalysisAssert(pnodeFnc);

            // Shouldn't be any temps in the arg list.
            Assert(*m_ppnodeVar == nullptr);

            // Start the var list.
            m_ppnodeVar = varNodesList;

            if (!pnodeFnc->sxFnc.IsBodyAndParamScopeMerged())
            {TRACE_IT(30003);
                OUTPUT_TRACE_DEBUGONLY(Js::ParsePhase, _u("The param and body scope of the function %s cannot be merged\n"), pnodeFnc->sxFnc.pnodeName ? pnodeFnc->sxFnc.pnodeName->sxVar.pid->Psz() : _u("Anonymous function"));
            }

            // Keep nested function declarations and expressions in the same list at function scope.
            // (Indicate this by nulling out the current function expressions list.)
            m_ppnodeExprScope = nullptr;

            if (buildAST)
            {TRACE_IT(30004);
                DeferredFunctionStub *saveCurrentStub = m_currDeferredStub;
                if (isEnclosedInParamScope)
                {TRACE_IT(30005);
                    // if the enclosed scope is the param scope we would not have created the deferred stub.
                    m_currDeferredStub = nullptr;
                }
                else if (pnodeFncSave && m_currDeferredStub)
                {TRACE_IT(30006);
                    // the Deferred stub will not match for the function which are defined on lambda formals.
                    // Since this is not determined upfront that the current function is a part of outer function or part of lambda formal until we have seen the Arrow token.
                    // Due to that the current function may be fetching stubs from the outer function (outer of the lambda) - rather then the lambda function. The way to fix is to match
                    // the function start with the stub. Because they should match. We need to have previous sibling concept as the lambda formals can have more than one
                    // functions and we want to avoid getting wrong stub.

                    if (pnodeFncSave->sxFnc.nestedCount == 1)
                    {TRACE_IT(30007);
                        m_prevSiblingDeferredStub = nullptr;
                    }

                    if (m_prevSiblingDeferredStub == nullptr)
                    {TRACE_IT(30008);
                        m_prevSiblingDeferredStub = (m_currDeferredStub + (pnodeFncSave->sxFnc.nestedCount - 1));
                    }

                    if (m_prevSiblingDeferredStub->ichMin == pnodeFnc->ichMin)
                    {TRACE_IT(30009);
                        m_currDeferredStub = m_prevSiblingDeferredStub->deferredStubs;
                        m_prevSiblingDeferredStub = nullptr;
                    }
                    else
                    {TRACE_IT(30010);
                        m_currDeferredStub = nullptr;
                    }
                }

                if (m_token.tk != tkLCurly && fLambda)
                {TRACE_IT(30011);
                    ParseExpressionLambdaBody<true>(pnodeFnc);
                    *pNeedScanRCurly = false;
                }
                else
                {TRACE_IT(30012);
                    this->FinishFncDecl(pnodeFnc, pNameHint, lastNodeRef, skipFormals);
                }
                m_currDeferredStub = saveCurrentStub;
            }
            else
            {TRACE_IT(30013);
                this->ParseNestedDeferredFunc(pnodeFnc, fLambda, pNeedScanRCurly, &strictModeTurnedOn);
            }
        }

        if (pnodeInnerBlock)
        {
            FinishParseBlock(pnodeInnerBlock, *pNeedScanRCurly);
        }

        if (!fModule && (m_token.tk == tkLCurly || !fLambda))
        {
            UpdateArgumentsNode(pnodeFnc, argNode);
        }

        // Restore the lists of scopes that contain function expressions.

        Assert(m_ppnodeExprScope == nullptr || *m_ppnodeExprScope == nullptr);
        m_ppnodeExprScope = ppnodeExprScopeSave;

        AssertMem(m_ppnodeScope);
        Assert(nullptr == *m_ppnodeScope);
        m_ppnodeScope = ppnodeScopeSave;

        if (pnodeBlock)
        {
            FinishParseBlock(pnodeBlock, *pNeedScanRCurly);
        }

        if (IsStrictMode() || strictModeTurnedOn)
        {TRACE_IT(30014);
            this->m_fUseStrictMode = TRUE; // Now we know this function is in strict mode

            if (!fWasAlreadyStrictMode)
            {TRACE_IT(30015);
                // If this function turned on strict mode then we didn't check the formal
                // parameters or function name hint for future reserved word usage. So do that now.
                RestorePoint afterFnc;
                m_pscan->Capture(&afterFnc);

                if (*pHasName)
                {TRACE_IT(30016);
                    // Rewind to the function name hint and check if the token is a reserved word.
                    m_pscan->SeekTo(beginNameHint);
                    m_pscan->Scan();
                    if (pnodeFnc->sxFnc.IsGenerator())
                    {TRACE_IT(30017);
                        Assert(m_token.tk == tkStar);
                        Assert(m_scriptContext->GetConfig()->IsES6GeneratorsEnabled());
                        Assert(!(flags & fFncClassMember));
                        m_pscan->Scan();
                    }
                    if (m_token.IsReservedWord())
                    {TRACE_IT(30018);
                        IdentifierExpectedError(m_token);
                    }
                    CheckStrictModeEvalArgumentsUsage(m_token.GetIdentifier(m_phtbl));
                }

                // Fast forward to formal parameter list, check for future reserved words,
                // then restore scanner as it was.
                m_pscan->SeekToForcingPid(beginFormals);
                CheckStrictFormalParameters();
                m_pscan->SeekTo(afterFnc);
            }

            if (buildAST)
            {TRACE_IT(30019);
                if (pnodeFnc->sxFnc.pnodeName != nullptr && knopVarDecl == pnodeFnc->sxFnc.pnodeName->nop)
                {TRACE_IT(30020);
                    CheckStrictModeEvalArgumentsUsage(pnodeFnc->sxFnc.pnodeName->sxVar.pid, pnodeFnc->sxFnc.pnodeName);
                }
            }

            this->m_fUseStrictMode = oldStrictMode;
            CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(StrictModeFunction, m_scriptContext);
        }

        if (fDeferred)
        {TRACE_IT(30021);
            AnalysisAssert(pnodeFnc);
            pnodeFnc->sxFnc.pnodeVars = nullptr;
        }

        if (parallelJobStarted)
        {TRACE_IT(30022);
            pnodeFnc = pnodeRealFnc;
            m_currentNodeFunc = pnodeRealFnc;

            // Let the foreground thread take care of marking the limit on the function node,
            // because in some cases this function's caller will want to change that limit,
            // so we don't want the background thread to try and touch it.
            pnodeFnc->ichLim = m_pscan->IchLimTok();
            pnodeFnc->sxFnc.cbLim = m_pscan->IecpLimTok();
        }
    }

    // after parsing asm.js module, we want to reset asm.js state before continuing
    AnalysisAssert(pnodeFnc);
    if (pnodeFnc->sxFnc.GetAsmjsMode())
    {TRACE_IT(30023);
        m_InAsmMode = false;
    }

    // Restore the statement stack.
    Assert(nullptr == m_pstmtCur);
    SetCurrentStatement(pstmtSave);

    if (pnodeFncExprScope)
    {
        FinishParseFncExprScope(pnodeFnc, pnodeFncExprScope);
    }
    if (!m_stoppedDeferredParse)
    {TRACE_IT(30024);
        m_grfscr |= uDeferSave;
    }
    m_inFIB = saveInFIB;

    m_pscan->SetYieldIsKeyword(fPreviousYieldIsKeyword);
    m_pscan->SetAwaitIsKeyword(fPreviousAwaitIsKeyword);

    // Restore the current function.
    if (buildAST)
    {TRACE_IT(30025);
        Assert(pnodeFnc == m_currentNodeFunc);

        m_currentNodeFunc = pnodeFncSave;
        m_pCurrentAstSize = pAstSizeSave;

        if (!fLambda)
        {TRACE_IT(30026);
            Assert(pnodeFnc == m_currentNodeNonLambdaFunc);
            m_currentNodeNonLambdaFunc = pnodeFncSaveNonLambda;
        }
    }
    else
    {TRACE_IT(30027);
        Assert(pnodeFnc == m_currentNodeDeferredFunc);
        if (!fLambda)
        {TRACE_IT(30028);
            Assert(pnodeFnc == m_currentNodeNonLambdaDeferredFunc);
            m_currentNodeNonLambdaDeferredFunc = pnodeFncSaveNonLambda;
        }
        m_currentNodeDeferredFunc = pnodeFncSave;
    }

    if (m_currentNodeFunc && pnodeFnc->sxFnc.HasWithStmt())
    {TRACE_IT(30029);
        GetCurrentFunctionNode()->sxFnc.SetHasWithStmt(true);
    }

    return true;
}

template<bool buildAST>
void Parser::UpdateCurrentNodeFunc(ParseNodePtr pnodeFnc, bool fLambda)
{TRACE_IT(30030);
    if (buildAST)
    {TRACE_IT(30031);
        // Make this the current function and start its sub-function list.
        m_currentNodeFunc = pnodeFnc;

        Assert(m_currentNodeDeferredFunc == nullptr);

        if (!fLambda)
        {TRACE_IT(30032);
            m_currentNodeNonLambdaFunc = pnodeFnc;
        }
    }
    else // if !buildAST
    {TRACE_IT(30033);
        AnalysisAssert(pnodeFnc);

        if (!fLambda)
        {TRACE_IT(30034);
            m_currentNodeNonLambdaDeferredFunc = pnodeFnc;
        }

        m_currentNodeDeferredFunc = pnodeFnc;
    }
}

void Parser::ParseTopLevelDeferredFunc(ParseNodePtr pnodeFnc, ParseNodePtr pnodeFncParent, LPCOLESTR pNameHint)
{TRACE_IT(30035);
    // Parse a function body that is a transition point from building AST to doing fast syntax check.

    pnodeFnc->sxFnc.pnodeVars = nullptr;
    pnodeFnc->sxFnc.pnodeBody = nullptr;

    this->m_deferringAST = TRUE;

    // Put the scanner into "no hashing" mode.
    BYTE deferFlags = m_pscan->SetDeferredParse(TRUE);

    m_pscan->Scan();

    ChkCurTok(tkLCurly, ERRnoLcurly);

    ParseNodePtr *ppnodeVarSave = m_ppnodeVar;

    m_ppnodeVar = &pnodeFnc->sxFnc.pnodeVars;

    if (pnodeFncParent != nullptr
        && m_currDeferredStub != nullptr
        // We don't create stubs for function bodies in parameter scope.
        && pnodeFnc->sxFnc.pnodeScopes->sxBlock.blockType != PnodeBlockType::Parameter)
    {TRACE_IT(30036);
        // We've already parsed this function body for syntax errors on the initial parse of the script.
        // We have information that allows us to skip it, so do so.

        DeferredFunctionStub *stub = m_currDeferredStub + (pnodeFncParent->sxFnc.nestedCount - 1);
        Assert(pnodeFnc->ichMin == stub->ichMin);
        if (stub->fncFlags & kFunctionCallsEval)
        {TRACE_IT(30037);
            this->MarkEvalCaller();
        }
        if (stub->fncFlags & kFunctionChildCallsEval)
        {TRACE_IT(30038);
            pnodeFnc->sxFnc.SetChildCallsEval(true);
        }
        if (stub->fncFlags & kFunctionHasWithStmt)
        {TRACE_IT(30039);
            pnodeFnc->sxFnc.SetHasWithStmt(true);
        }

        PHASE_PRINT_TRACE1(
            Js::SkipNestedDeferredPhase,
            _u("Skipping nested deferred function %d. %s: %d...%d\n"),
            pnodeFnc->sxFnc.functionId, GetFunctionName(pnodeFnc, pNameHint), pnodeFnc->ichMin, stub->restorePoint.m_ichMinTok);

        m_pscan->SeekTo(stub->restorePoint, m_nextFunctionId);
        pnodeFnc->sxFnc.nestedCount = stub->nestedCount;
        pnodeFnc->sxFnc.deferredStub = stub->deferredStubs;
        if (stub->fncFlags & kFunctionStrictMode)
        {TRACE_IT(30040);
            pnodeFnc->sxFnc.SetStrictMode(true);
        }
    }
    else
    {
        ParseStmtList<false>(nullptr, nullptr, SM_DeferredParse, true /* isSourceElementList */);
    }

    pnodeFnc->ichLim = m_pscan->IchLimTok();
    pnodeFnc->sxFnc.cbLim = m_pscan->IecpLimTok();

    m_ppnodeVar = ppnodeVarSave;

    // Restore the scanner's default hashing mode.
    // Do this before we consume the next token.
    m_pscan->SetDeferredParseFlags(deferFlags);

    ChkCurTokNoScan(tkRCurly, ERRnoRcurly);

#if DBG
    pnodeFnc->sxFnc.deferredParseNextFunctionId = *this->m_nextFunctionId;
#endif
    this->m_deferringAST = FALSE;
}

bool Parser::DoParallelParse(ParseNodePtr pnodeFnc) const
{TRACE_IT(30041);
#if ENABLE_BACKGROUND_PARSING
    if (!PHASE_ON_RAW(Js::ParallelParsePhase, m_sourceContextInfo->sourceContextId, pnodeFnc->sxFnc.functionId))
    {TRACE_IT(30042);
        return false;
    }

    BackgroundParser *bgp = m_scriptContext->GetBackgroundParser();
    return bgp != nullptr;
#else
    return false;
#endif
}

bool Parser::ScanAheadToFunctionEnd(uint count)
{TRACE_IT(30043);
    bool found = false;
    uint curlyDepth = 0;

    RestorePoint funcStart;
    m_pscan->Capture(&funcStart);

    for (uint i = 0; i < count; i++)
    {TRACE_IT(30044);
        switch (m_token.tk)
        {
            case tkStrTmplBegin:
            case tkStrTmplMid:
            case tkStrTmplEnd:
            case tkDiv:
            case tkAsgDiv:
            case tkScanError:
            case tkEOF:
                goto LEnd;

            case tkLCurly:
                UInt32Math::Inc(curlyDepth, Parser::OutOfMemory);
                break;

            case tkRCurly:
                if (curlyDepth == 1)
                {TRACE_IT(30045);
                    found = true;
                    goto LEnd;
                }
                if (curlyDepth == 0)
                {TRACE_IT(30046);
                    goto LEnd;
                }
                curlyDepth--;
                break;
        }

        m_pscan->ScanAhead();
    }

 LEnd:
    m_pscan->SeekTo(funcStart);
    return found;
}

bool Parser::FastScanFormalsAndBody()
{TRACE_IT(30047);
    // The scanner is currently pointing just past the name of a function.
    // The idea here is to find the end of the function body as quickly as possible,
    // by tokenizing and tracking {}'s if possible.
    // String templates require some extra logic but can be handled.

    // The real wrinkle is "/" and "/=", which may indicate either a RegExp literal or a division, depending
    // on the context.
    // To handle this with minimal work, keep track of the last ";" seen at each {} depth. If we see one of the
    // difficult tokens, rewind to the last ";" at the current {} depth and parse statements until we pass the
    // point where we had to rewind. This will process the "/" as required.

    RestorePoint funcStart;
    m_pscan->Capture(&funcStart);

    const int maxRestorePointDepth = 16;
    struct FastScanRestorePoint
    {
        RestorePoint restorePoint;
        uint parenDepth;
        Js::LocalFunctionId functionId;
        int blockId;

        FastScanRestorePoint() : restorePoint(), parenDepth(0) {TRACE_IT(30048);};
    };
    FastScanRestorePoint lastSColonAtCurlyDepth[maxRestorePointDepth];

    charcount_t ichStart = m_pscan->IchMinTok();
    uint blockIdSave = m_nextBlockId;
    uint functionIdSave = *m_nextFunctionId;
    uint curlyDepth = 0;
    uint strTmplDepth = 0;
    for (;;)
    {TRACE_IT(30049);
        switch (m_token.tk)
        {
            case tkStrTmplBegin:
                UInt32Math::Inc(strTmplDepth, Parser::OutOfMemory);
                // Fall through

            case tkStrTmplMid:
            case tkLCurly:
                UInt32Math::Inc(curlyDepth, Parser::OutOfMemory);
                Int32Math::Inc(m_nextBlockId, &m_nextBlockId);
                break;

            case tkStrTmplEnd:
                // We can assert here, because the scanner will only return this token if we've told it we're
                // in a string template.
                Assert(strTmplDepth > 0);
                strTmplDepth--;
                break;

            case tkRCurly:
                if (curlyDepth == 1)
                {TRACE_IT(30050);
                    Assert(strTmplDepth == 0);
                    if (PHASE_TRACE1(Js::ParallelParsePhase))
                    {TRACE_IT(30051);
                        Output::Print(_u("Finished fast seek: %d. %s -- %d...%d\n"),
                                      m_currentNodeFunc->sxFnc.functionId,
                                      GetFunctionName(m_currentNodeFunc, m_currentNodeFunc->sxFnc.hint),
                                      ichStart, m_pscan->IchLimTok());
                    }
                    return true;
                }
                if (curlyDepth < maxRestorePointDepth)
                {TRACE_IT(30052);
                    lastSColonAtCurlyDepth[curlyDepth].restorePoint.m_ichMinTok = (uint)-1;
                }
                curlyDepth--;
                if (strTmplDepth > 0)
                {TRACE_IT(30053);
                    m_pscan->SetScanState(Scanner_t::ScanState::ScanStateStringTemplateMiddleOrEnd);
                }
                break;

            case tkSColon:
                // Track the location of the ";" (if it's outside parens, as we don't, for instance, want
                // to track the ";"'s in a for-loop header. If we find it's important to rewind within a paren
                // expression, we can do something more sophisticated.)
                if (curlyDepth < maxRestorePointDepth && lastSColonAtCurlyDepth[curlyDepth].parenDepth == 0)
                {TRACE_IT(30054);
                    m_pscan->Capture(&lastSColonAtCurlyDepth[curlyDepth].restorePoint);
                    lastSColonAtCurlyDepth[curlyDepth].functionId = *this->m_nextFunctionId;
                    lastSColonAtCurlyDepth[curlyDepth].blockId = m_nextBlockId;
                }
                break;

            case tkLParen:
                if (curlyDepth < maxRestorePointDepth)
                {TRACE_IT(30055);
                    UInt32Math::Inc(lastSColonAtCurlyDepth[curlyDepth].parenDepth);
                }
                break;

            case tkRParen:
                if (curlyDepth < maxRestorePointDepth)
                {TRACE_IT(30056);
                    Assert(lastSColonAtCurlyDepth[curlyDepth].parenDepth != 0);
                    lastSColonAtCurlyDepth[curlyDepth].parenDepth--;
                }
                break;

            case tkID:
            {TRACE_IT(30057);
                charcount_t tokLength = m_pscan->IchLimTok() - m_pscan->IchMinTok();
                // Detect the function and class keywords so we can track function ID's.
                // (In fast mode, the scanner doesn't distinguish keywords and doesn't point the token
                // to a PID.)
                // Detect try/catch/for to increment block count for them.
                switch (tokLength)
                {
                case 3:
                    if (!memcmp(m_pscan->PchMinTok(), "try", 3) || !memcmp(m_pscan->PchMinTok(), "for", 3))
                    {TRACE_IT(30058);
                        Int32Math::Inc(m_nextBlockId, &m_nextBlockId);
                    }
                    break;
                case 5:
                    if (!memcmp(m_pscan->PchMinTok(), "catch", 5))
                    {TRACE_IT(30059);
                        Int32Math::Inc(m_nextBlockId, &m_nextBlockId);
                    }
                    else if (!memcmp(m_pscan->PchMinTok(), "class", 5))
                    {TRACE_IT(30060);
                        Int32Math::Inc(m_nextBlockId, &m_nextBlockId);
                        Int32Math::Inc(*this->m_nextFunctionId, (int*)this->m_nextFunctionId);
                    }
                    break;
                case 8:
                    if (!memcmp(m_pscan->PchMinTok(), "function", 8))
                    {TRACE_IT(30061);
                        // Account for the possible func expr scope or dummy block for missing {}'s around a declaration
                        Int32Math::Inc(m_nextBlockId, &m_nextBlockId);
                        Int32Math::Inc(*this->m_nextFunctionId, (int*)this->m_nextFunctionId);
                    }
                    break;
                }
                break;
            }

            case tkDArrow:
                Int32Math::Inc(m_nextBlockId, &m_nextBlockId);
                Int32Math::Inc(*this->m_nextFunctionId, (int*)this->m_nextFunctionId);
                break;

            case tkDiv:
            case tkAsgDiv:
            {TRACE_IT(30062);
                int opl;
                OpCode nop;
                tokens tkPrev = m_pscan->m_tkPrevious;
                if ((m_pscan->m_phtbl->TokIsBinop(tkPrev, &opl, &nop) && nop != knopNone) ||
                    (m_pscan->m_phtbl->TokIsUnop(tkPrev, &opl, &nop) &&
                     nop != knopNone &&
                     tkPrev != tkInc &&
                     tkPrev != tkDec) ||
                    tkPrev == tkColon ||
                    tkPrev == tkLParen ||
                    tkPrev == tkLBrack ||
                    tkPrev == tkRETURN)
                {TRACE_IT(30063);
                    // Previous token indicates that we're starting an expression here and can't have a
                    // binary operator now.
                    // Assume this is a RegExp.
                    ParseRegExp<false>();
                    break;
                }
                uint tempCurlyDepth = curlyDepth < maxRestorePointDepth ? curlyDepth : maxRestorePointDepth - 1;
                for (; tempCurlyDepth != (uint)-1; tempCurlyDepth--)
                {TRACE_IT(30064);
                    // We don't know whether we've got a RegExp or a divide. Rewind to the last safe ";"
                    // if we can and parse statements until we pass this point.
                    if (lastSColonAtCurlyDepth[tempCurlyDepth].restorePoint.m_ichMinTok != -1)
                    {TRACE_IT(30065);
                        break;
                    }
                }
                if (tempCurlyDepth != (uint)-1)
                {TRACE_IT(30066);
                    ParseNodePtr pnodeFncSave = m_currentNodeFunc;
                    int32 *pastSizeSave = m_pCurrentAstSize;
                    uint *pnestedCountSave = m_pnestedCount;
                    ParseNodePtr *ppnodeScopeSave = m_ppnodeScope;
                    ParseNodePtr *ppnodeExprScopeSave = m_ppnodeExprScope;

                    ParseNodePtr pnodeFnc = CreateDummyFuncNode(true);
                    m_ppnodeScope = &pnodeFnc->sxFnc.pnodeScopes;
                    m_ppnodeExprScope = nullptr;

                    charcount_t ichStop = m_pscan->IchLimTok();
                    curlyDepth = tempCurlyDepth;
                    m_pscan->SeekTo(lastSColonAtCurlyDepth[tempCurlyDepth].restorePoint);
                    m_nextBlockId = lastSColonAtCurlyDepth[tempCurlyDepth].blockId;
                    *this->m_nextFunctionId = lastSColonAtCurlyDepth[tempCurlyDepth].functionId;

                    ParseNodePtr pnodeBlock = StartParseBlock<true>(PnodeBlockType::Function, ScopeType_FunctionBody);

                    m_pscan->Scan();
                    do
                    {TRACE_IT(30067);
                        ParseStatement<false>();
                    }
                    while(m_pscan->IchMinTok() < ichStop);

                    FinishParseBlock(pnodeBlock);

                    m_currentNodeFunc = pnodeFncSave;
                    m_pCurrentAstSize = pastSizeSave;
                    m_pnestedCount = pnestedCountSave;
                    m_ppnodeScope = ppnodeScopeSave;
                    m_ppnodeExprScope = ppnodeExprScopeSave;

                    // We've already consumed the first token of the next statement, so just continue
                    // without a further scan.
                    continue;
                }
            }

                // fall through to rewind to function start
            case tkScanError:
            case tkEOF:
                // Unexpected token.
                if (PHASE_TRACE1(Js::ParallelParsePhase))
                {TRACE_IT(30068);
                    Output::Print(_u("Failed fast seek: %d. %s -- %d...%d\n"),
                                  m_currentNodeFunc->sxFnc.functionId,
                                  GetFunctionName(m_currentNodeFunc, m_currentNodeFunc->sxFnc.hint),
                                  ichStart, m_pscan->IchLimTok());
                }
                m_nextBlockId = blockIdSave;
                *m_nextFunctionId = functionIdSave;
                m_pscan->SeekTo(funcStart);
                return false;
        }

        m_pscan->ScanNoKeywords();
    }
}

ParseNodePtr Parser::CreateDummyFuncNode(bool fDeclaration)
{TRACE_IT(30069);
    // Create a dummy node and make it look like the current function declaration.
    // Do this in situations where we want to parse statements without impacting
    // the state of the "real" AST.

    ParseNodePtr pnodeFnc = CreateNode(knopFncDecl);
    pnodeFnc->sxFnc.ClearFlags();
    pnodeFnc->sxFnc.SetDeclaration(fDeclaration);
    pnodeFnc->sxFnc.astSize             = 0;
    pnodeFnc->sxFnc.pnodeName           = nullptr;
    pnodeFnc->sxFnc.pnodeScopes         = nullptr;
    pnodeFnc->sxFnc.pnodeRest           = nullptr;
    pnodeFnc->sxFnc.pid                 = nullptr;
    pnodeFnc->sxFnc.hint                = nullptr;
    pnodeFnc->sxFnc.hintOffset          = 0;
    pnodeFnc->sxFnc.hintLength          = 0;
    pnodeFnc->sxFnc.isNameIdentifierRef = true;
    pnodeFnc->sxFnc.nestedFuncEscapes   = false;
    pnodeFnc->sxFnc.pnodeNext           = nullptr;
    pnodeFnc->sxFnc.pnodeParams         = nullptr;
    pnodeFnc->sxFnc.pnodeVars           = nullptr;
    pnodeFnc->sxFnc.funcInfo            = nullptr;
    pnodeFnc->sxFnc.deferredStub        = nullptr;
    pnodeFnc->sxFnc.nestedCount         = 0;
    pnodeFnc->sxFnc.SetNested(m_currentNodeFunc != nullptr); // If there is a current function, then we're a nested function.
    pnodeFnc->sxFnc.SetStrictMode(IsStrictMode()); // Inherit current strict mode -- may be overridden by the function itself if it contains a strict mode directive.
    pnodeFnc->sxFnc.firstDefaultArg = 0;
    pnodeFnc->sxFnc.isBodyAndParamScopeMerged = true;

    m_pCurrentAstSize = &pnodeFnc->sxFnc.astSize;
    m_currentNodeFunc = pnodeFnc;
    m_pnestedCount = &pnodeFnc->sxFnc.nestedCount;

    return pnodeFnc;
}

void Parser::ParseNestedDeferredFunc(ParseNodePtr pnodeFnc, bool fLambda, bool *pNeedScanRCurly, bool *pStrictModeTurnedOn)
{TRACE_IT(30070);
    // Parse a function nested inside another deferred function.

    size_t lengthBeforeBody = this->GetSourceLength();

    if (m_token.tk != tkLCurly && fLambda)
    {TRACE_IT(30071);
        ParseExpressionLambdaBody<false>(pnodeFnc);
        *pNeedScanRCurly = false;
    }
    else
    {
        ChkCurTok(tkLCurly, ERRnoLcurly);

        bool* detectStrictModeOn = IsStrictMode() ? nullptr : pStrictModeTurnedOn;
        m_ppnodeVar = &m_currentNodeDeferredFunc->sxFnc.pnodeVars;

        ParseStmtList<false>(nullptr, nullptr, SM_DeferredParse, true /* isSourceElementList */, detectStrictModeOn);

        ChkCurTokNoScan(tkRCurly, ERRnoRcurly);
    }

    pnodeFnc->ichLim = m_pscan->IchLimTok();
    pnodeFnc->sxFnc.cbLim = m_pscan->IecpLimTok();
    if (*pStrictModeTurnedOn)
    {TRACE_IT(30072);
        pnodeFnc->sxFnc.SetStrictMode(true);
    }

    if (!PHASE_OFF1(Js::SkipNestedDeferredPhase))
    {TRACE_IT(30073);
        // Record the end of the function and the function ID increment that happens inside the function.
        // Byte code gen will use this to build stub information to allow us to skip this function when the
        // enclosing function is fully parsed.
        RestorePoint *restorePoint = Anew(&m_nodeAllocator, RestorePoint);
        m_pscan->Capture(restorePoint,
                         *m_nextFunctionId - pnodeFnc->sxFnc.functionId - 1,
                         lengthBeforeBody - this->GetSourceLength());
        pnodeFnc->sxFnc.pRestorePoint = restorePoint;
    }
}

template<bool buildAST>
bool Parser::ParseFncNames(ParseNodePtr pnodeFnc, ParseNodePtr pnodeFncParent, ushort flags, ParseNodePtr **pLastNodeRef)
{TRACE_IT(30074);
    BOOL fDeclaration = flags & fFncDeclaration;
    BOOL fIsAsync = flags & fFncAsync;
    ParseNodePtr pnodeT;
    charcount_t ichMinNames, ichLimNames;

    // Get the names to bind to.
    /*
    * KaushiS [5/15/08]:
    * ECMAScript defines a FunctionExpression as follows:
    *
    * "function" [Identifier] ( [FormalParameterList] ) { FunctionBody }
    *
    * The function name being optional is omitted by most real world
    * code that uses a FunctionExpression to define a function. This however
    * is problematic for tools because there isn't a function name that
    * the runtime can provide.
    *
    * To fix this (primarily for the profiler), I'm adding simple, static
    * name inferencing logic to the parser. When it encounters the following
    * productions
    *
    *   "var" Identifier "=" FunctionExpression
    *   "var" IdentifierA.IdentifierB...Identifier "=" FunctionExpression
    *   Identifier = FunctionExpression
    *   "{" Identifier: FunctionExpression "}"
    *
    * it associates Identifier with the function created by the
    * FunctionExpression. This identifier is *not* the function's name. It
    * is ignored by the runtime and is only an additional piece of information
    * about the function (function name hint) that tools could opt to
    * surface.
    */

    m_pscan->Scan();

    // If generators are enabled then we are in a recent enough version
    // that deferred parsing will create a parse node for pnodeFnc and
    // it is safe to assume it is not null.
    if (flags & fFncGenerator)
    {TRACE_IT(30075);
        Assert(m_scriptContext->GetConfig()->IsES6GeneratorsEnabled());
        pnodeFnc->sxFnc.SetIsGenerator();
    }
    else if (m_scriptContext->GetConfig()->IsES6GeneratorsEnabled() &&
        m_token.tk == tkStar &&
        !(flags & fFncClassMember))
    {TRACE_IT(30076);
        if (!fDeclaration)
        {TRACE_IT(30077);
            bool fPreviousYieldIsKeyword = m_pscan->SetYieldIsKeyword(!fDeclaration);
            m_pscan->Scan();
            m_pscan->SetYieldIsKeyword(fPreviousYieldIsKeyword);
        }
        else
        {TRACE_IT(30078);
            m_pscan->Scan();
        }

        pnodeFnc->sxFnc.SetIsGenerator();
    }

    if (fIsAsync)
    {TRACE_IT(30079);
        if (pnodeFnc->sxFnc.IsGenerator())
        {TRACE_IT(30080);
            Error(ERRsyntax);
        }
        pnodeFnc->sxFnc.SetIsAsync();
    }

    if (pnodeFnc)
    {TRACE_IT(30081);
        pnodeFnc->sxFnc.pnodeName = nullptr;
    }

    if ((m_token.tk != tkID || flags & fFncNoName)
        && (IsStrictMode() || (pnodeFnc && pnodeFnc->sxFnc.IsGenerator()) || m_token.tk != tkYIELD || fDeclaration)) // Function expressions can have the name yield even inside generator functions
    {TRACE_IT(30082);
        if (fDeclaration  ||
            m_token.IsReservedWord())  // For example:  var x = (function break(){});
        {TRACE_IT(30083);
            IdentifierExpectedError(m_token);
        }
        return false;
    }

    ichMinNames = m_pscan->IchMinTok();


    Assert(m_token.tk == tkID || (m_token.tk == tkYIELD && !fDeclaration));

    if (IsStrictMode())
    {TRACE_IT(30084);
        CheckStrictModeEvalArgumentsUsage(m_token.GetIdentifier(m_phtbl));
    }
    Token tokenBase = m_token;
    charcount_t ichMinBase = m_pscan->IchMinTok();
    charcount_t ichLimBase = m_pscan->IchLimTok();

    m_pscan->Scan();

    IdentPtr pidBase = tokenBase.GetIdentifier(m_phtbl);
    pnodeT = CreateDeclNode(knopVarDecl, pidBase, STFunction);
    pnodeT->ichMin = ichMinBase;
    pnodeT->ichLim = ichLimBase;

    if (fDeclaration &&
        pnodeFncParent &&
        pnodeFncParent->sxFnc.pnodeName &&
        pnodeFncParent->sxFnc.pnodeName->nop == knopVarDecl &&
        pnodeFncParent->sxFnc.pnodeName->sxVar.pid == pidBase)
    {TRACE_IT(30085);
        pnodeFncParent->sxFnc.SetNameIsHidden();
    }

    if (buildAST)
    {TRACE_IT(30086);
        AnalysisAssert(pnodeFnc);
        ichLimNames = pnodeT->ichLim;
        AddToNodeList(&pnodeFnc->sxFnc.pnodeName, pLastNodeRef, pnodeT);

        pnodeFnc->sxFnc.pnodeName->ichMin = ichMinNames;
        pnodeFnc->sxFnc.pnodeName->ichLim = ichLimNames;
        if (knopVarDecl == pnodeFnc->sxFnc.pnodeName->nop)
        {TRACE_IT(30087);
            // Only one name (the common case).
            pnodeFnc->sxFnc.pid = pnodeFnc->sxFnc.pnodeName->sxVar.pid;
        }
        else
        {TRACE_IT(30088);
            // Multiple names. Turn the source into an IdentPtr.
            pnodeFnc->sxFnc.pid = m_phtbl->PidHashNameLen(
                m_pscan->PchBase() + ichMinNames, 
                m_pscan->AdjustedLast(),
                ichLimNames - ichMinNames);
        }
    }

    return true;
}

void Parser::ValidateFormals()
{
    ParseFncFormals<false>(nullptr, nullptr, fFncNoFlgs);
    // Eat the tkRParen. The ParseFncDeclHelper caller expects to see it.
    m_pscan->Scan();
}

void Parser::ValidateSourceElementList()
{
    ParseStmtList<false>(nullptr, nullptr, SM_NotUsed, true);
}

void Parser::UpdateOrCheckForDuplicateInFormals(IdentPtr pid, SList<IdentPtr> *formals)
{TRACE_IT(30089);
    bool isStrictMode = IsStrictMode();
    if (isStrictMode)
    {TRACE_IT(30090);
        CheckStrictModeEvalArgumentsUsage(pid);
    }

    if (formals->Has(pid))
    {TRACE_IT(30091);
        if (isStrictMode)
        {TRACE_IT(30092);
            Error(ERRES5ArgSame);
        }
        else
        {TRACE_IT(30093);
            Error(ERRFormalSame);
        }
    }
    else
    {TRACE_IT(30094);
        formals->Prepend(pid);
    }
}

template<bool buildAST>
void Parser::ParseFncFormals(ParseNodePtr pnodeFnc, ParseNodePtr pnodeParentFnc, ushort flags)
{TRACE_IT(30095);
    bool fLambda = (flags & fFncLambda) != 0;
    bool fMethod = (flags & fFncMethod) != 0;
    bool fNoArg = (flags & fFncNoArg) != 0;
    bool fOneArg = (flags & fFncOneArg) != 0;
    bool fAsync = (flags & fFncAsync) != 0;

    bool fPreviousYieldIsKeyword = false;
    bool fPreviousAwaitIsKeyword = false;

    if (fLambda)
    {TRACE_IT(30096);
        fPreviousYieldIsKeyword = m_pscan->SetYieldIsKeyword(pnodeParentFnc != nullptr && pnodeParentFnc->sxFnc.IsGenerator());
        fPreviousAwaitIsKeyword = m_pscan->SetAwaitIsKeyword(fAsync || (pnodeParentFnc != nullptr && pnodeParentFnc->sxFnc.IsAsync()));
    }

    Assert(!fNoArg || !fOneArg); // fNoArg and fOneArg can never be true at the same time.

    // strictFormals corresponds to the StrictFormalParameters grammar production
    // in the ES spec which just means duplicate names are not allowed
    bool fStrictFormals = IsStrictMode() || fLambda || fMethod;

    // When detecting duplicated formals pids are needed so force PID creation (unless the function should take 0 or 1 arg).
    bool forcePid = fStrictFormals && !fNoArg && !fOneArg;
    AutoTempForcePid autoForcePid(m_pscan, forcePid);

    // Lambda's allow single formal specified by a single binding identifier without parentheses, special case it.
    if (fLambda && m_token.tk == tkID)
    {TRACE_IT(30097);
        IdentPtr pid = m_token.GetIdentifier(m_phtbl);

        CreateVarDeclNode(pid, STFormal, false, nullptr, false);
        CheckPidIsValid(pid);

        m_pscan->Scan();

        if (m_token.tk != tkDArrow)
        {
            Error(ERRsyntax, m_pscan->IchMinTok(), m_pscan->IchLimTok());
        }

        if (fLambda)
        {TRACE_IT(30098);
            m_pscan->SetYieldIsKeyword(fPreviousYieldIsKeyword);
            m_pscan->SetAwaitIsKeyword(fPreviousAwaitIsKeyword);
        }

        return;
    }
    else if (fLambda && m_token.tk == tkAWAIT)
    {TRACE_IT(30099);
        // async await => {}
        IdentifierExpectedError(m_token);
    }

    // Otherwise, must have a parameter list within parens.
    ChkCurTok(tkLParen, ERRnoLparen);

    // Now parse the list of arguments, if present
    if (m_token.tk == tkRParen)
    {TRACE_IT(30100);
        if (fOneArg)
        {TRACE_IT(30101);
            Error(ERRSetterMustHaveOneParameter);
        }
    }
    else
    {TRACE_IT(30102);
        if (fNoArg)
        {TRACE_IT(30103);
            Error(ERRGetterMustHaveNoParameters);
        }
        SList<IdentPtr> formals(&m_nodeAllocator);
        ParseNodePtr pnodeT = nullptr;
        bool seenRestParameter = false;
        bool isNonSimpleParameterList = false;
        for (Js::ArgSlot argPos = 0; ; ++argPos)
        {TRACE_IT(30104);
            bool isBindingPattern = false;
            if (m_scriptContext->GetConfig()->IsES6RestEnabled() && m_token.tk == tkEllipsis)
            {TRACE_IT(30105);
                // Possible rest parameter
                m_pscan->Scan();
                seenRestParameter = true;
            }
            if (m_token.tk != tkID)
            {TRACE_IT(30106);
                if (IsES6DestructuringEnabled() && IsPossiblePatternStart())
                {TRACE_IT(30107);
                    // Mark that the function has a non simple parameter list before parsing the pattern since the pattern can have function definitions.
                    this->GetCurrentFunctionNode()->sxFnc.SetHasNonSimpleParameterList();

                    ParseNodePtr *const ppnodeVarSave = m_ppnodeVar;
                    m_ppnodeVar = &pnodeFnc->sxFnc.pnodeVars;

                    ParseNodePtr * ppNodeLex = m_currentBlockInfo->m_ppnodeLex;
                    Assert(ppNodeLex != nullptr);

                    ParseNodePtr paramPattern = nullptr;
                    ParseNodePtr pnodePattern = ParseDestructuredLiteral<buildAST>(tkLET, true /*isDecl*/, false /*topLevel*/);

                    // Instead of passing the STFormal all the way on many methods, it seems it is better to change the symbol type afterward.
                    for (ParseNodePtr lexNode = *ppNodeLex; lexNode != nullptr; lexNode = lexNode->sxVar.pnodeNext)
                    {TRACE_IT(30108);
                        Assert(lexNode->IsVarLetOrConst());
                        UpdateOrCheckForDuplicateInFormals(lexNode->sxVar.pid, &formals);
                        lexNode->sxVar.sym->SetSymbolType(STFormal);
                        if (m_currentNodeFunc != nullptr && lexNode->sxVar.pid == wellKnownPropertyPids.arguments)
                        {TRACE_IT(30109);
                            m_currentNodeFunc->grfpn |= PNodeFlags::fpnArguments_overriddenInParam;
                        }
                    }

                    m_ppnodeVar = ppnodeVarSave;
                    if (buildAST)
                    {TRACE_IT(30110);
                        paramPattern = CreateParamPatternNode(pnodePattern);

                        // Linking the current formal parameter (which is pattern parameter) with other formals.
                        *m_ppnodeVar = paramPattern;
                        paramPattern->sxParamPattern.pnodeNext = nullptr;
                        m_ppnodeVar = &paramPattern->sxParamPattern.pnodeNext;
                    }

                    isBindingPattern = true;
                    isNonSimpleParameterList = true;
                }
                else
                {TRACE_IT(30111);
                    IdentifierExpectedError(m_token);
                }
            }

            if (!isBindingPattern)
            {TRACE_IT(30112);
                IdentPtr pid = m_token.GetIdentifier(m_phtbl);
                LPCOLESTR pNameHint = pid->Psz();
                uint32 nameHintLength = pid->Cch();
                uint32 nameHintOffset = 0;

                if (seenRestParameter)
                {TRACE_IT(30113);
                    this->GetCurrentFunctionNode()->sxFnc.SetHasNonSimpleParameterList();
                    if (flags & fFncOneArg)
                    {TRACE_IT(30114);
                        // The parameter of a setter cannot be a rest parameter.
                        Error(ERRUnexpectedEllipsis);
                    }
                    pnodeT = CreateDeclNode(knopVarDecl, pid, STFormal, false);
                    pnodeT->sxVar.sym->SetIsNonSimpleParameter(true);
                    if (buildAST)
                    {TRACE_IT(30115);
                        // When only validating formals, we won't have a function node.
                        pnodeFnc->sxFnc.pnodeRest = pnodeT;
                        if (!isNonSimpleParameterList)
                        {
                            // This is the first non-simple parameter we've seen. We need to go back
                            // and set the Symbols of all previous parameters.
                            MapFormalsWithoutRest(m_currentNodeFunc, [&](ParseNodePtr pnodeArg) { pnodeArg->sxVar.sym->SetIsNonSimpleParameter(true); });
                        }
                    }

                    isNonSimpleParameterList = true;
                }
                else
                {TRACE_IT(30116);
                    pnodeT = CreateVarDeclNode(pid, STFormal, false, nullptr, false);
                    if (isNonSimpleParameterList)
                    {TRACE_IT(30117);
                        pnodeT->sxVar.sym->SetIsNonSimpleParameter(true);
                    }
                }

                if (buildAST && pid == wellKnownPropertyPids.arguments)
                {TRACE_IT(30118);
                    // This formal parameter overrides the built-in 'arguments' object
                    m_currentNodeFunc->grfpn |= PNodeFlags::fpnArguments_overriddenInParam;
                }

                if (fStrictFormals)
                {
                    UpdateOrCheckForDuplicateInFormals(pid, &formals);
                }

                m_pscan->Scan();

                if (seenRestParameter && m_token.tk != tkRParen && m_token.tk != tkAsg)
                {TRACE_IT(30119);
                    Error(ERRRestLastArg);
                }

                if (m_token.tk == tkAsg && m_scriptContext->GetConfig()->IsES6DefaultArgsEnabled())
                {TRACE_IT(30120);
                    if (seenRestParameter && m_scriptContext->GetConfig()->IsES6RestEnabled())
                    {TRACE_IT(30121);
                        Error(ERRRestWithDefault);
                    }

                    // In defer parse mode we have to flag the function node to indicate that it has default arguments
                    // so that it will be considered for any syntax error scenario.
                    // Also mark it before parsing the expression as it may contain functions.
                    ParseNode* currentFncNode = GetCurrentFunctionNode();
                    if (!currentFncNode->sxFnc.HasDefaultArguments())
                    {TRACE_IT(30122);
                        currentFncNode->sxFnc.SetHasDefaultArguments();
                        currentFncNode->sxFnc.SetHasNonSimpleParameterList();
                        currentFncNode->sxFnc.firstDefaultArg = argPos;
                    }

                    m_pscan->Scan();
                    ParseNodePtr pnodeInit = ParseExpr<buildAST>(koplCma, nullptr, TRUE, FALSE, pNameHint, &nameHintLength, &nameHintOffset);

                    if (buildAST && pnodeInit->nop == knopFncDecl)
                    {TRACE_IT(30123);
                        Assert(nameHintLength >= nameHintOffset);
                        pnodeInit->sxFnc.hint = pNameHint;
                        pnodeInit->sxFnc.hintLength = nameHintLength;
                        pnodeInit->sxFnc.hintOffset = nameHintOffset;
                    }

                    AnalysisAssert(pnodeT);
                    pnodeT->sxVar.sym->SetIsNonSimpleParameter(true);
                    if (!isNonSimpleParameterList)
                    {TRACE_IT(30124);
                        if (buildAST)
                        {
                            // This is the first non-simple parameter we've seen. We need to go back
                            // and set the Symbols of all previous parameters.
                            MapFormalsWithoutRest(m_currentNodeFunc, [&](ParseNodePtr pnodeArg) { pnodeArg->sxVar.sym->SetIsNonSimpleParameter(true); });
                        }

                        // There may be previous parameters that need to be checked for duplicates.
                        isNonSimpleParameterList = true;
                    }

                    if (buildAST)
                    {TRACE_IT(30125);
                        if (!m_currentNodeFunc->sxFnc.HasDefaultArguments())
                        {
                            CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(DefaultArgFunction, m_scriptContext);
                        }
                        pnodeT->sxVar.pnodeInit = pnodeInit;
                        pnodeT->ichLim = m_pscan->IchLimTok();
                    }
                }
            }

            if (isNonSimpleParameterList && m_currentScope->GetHasDuplicateFormals())
            {TRACE_IT(30126);
                Error(ERRFormalSame);
            }

            if (flags & fFncOneArg)
            {TRACE_IT(30127);
                if (m_token.tk != tkRParen)
                {TRACE_IT(30128);
                    Error(ERRSetterMustHaveOneParameter);
                }
                break; //enforce only one arg
            }

            if (m_token.tk != tkComma)
            {TRACE_IT(30129);
                break;
            }

            m_pscan->Scan();

            if (m_token.tk == tkRParen && m_scriptContext->GetConfig()->IsES7TrailingCommaEnabled())
            {TRACE_IT(30130);
                break;
            }
        }

        if (seenRestParameter)
        {
            CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(Rest, m_scriptContext);
        }

        if (m_token.tk != tkRParen)
        {TRACE_IT(30131);
            Error(ERRnoRparen);
        }

        if (this->GetCurrentFunctionNode()->sxFnc.CallsEval() || this->GetCurrentFunctionNode()->sxFnc.ChildCallsEval())
        {TRACE_IT(30132);
            Assert(pnodeFnc->sxFnc.HasNonSimpleParameterList());
            pnodeFnc->sxFnc.ResetBodyAndParamScopeMerged();
        }
    }
    Assert(m_token.tk == tkRParen);

    if (fLambda)
    {TRACE_IT(30133);
        m_pscan->SetYieldIsKeyword(fPreviousYieldIsKeyword);
        m_pscan->SetAwaitIsKeyword(fPreviousAwaitIsKeyword);
    }
}

template<bool buildAST>
ParseNodePtr Parser::GenerateModuleFunctionWrapper()
{TRACE_IT(30134);
    ParseNodePtr pnodeFnc = ParseFncDecl<buildAST>(fFncModule, nullptr, false, true, true);
    ParseNodePtr callNode = CreateCallNode(knopCall, pnodeFnc, nullptr);

    return callNode;
}

template<bool buildAST>
ParseNodePtr Parser::GenerateEmptyConstructor(bool extends)
{TRACE_IT(30135);
    ParseNodePtr pnodeFnc;

    // Create the node.
    pnodeFnc = CreateNode(knopFncDecl);
    pnodeFnc->sxFnc.ClearFlags();
    pnodeFnc->sxFnc.SetNested(NULL != m_currentNodeFunc);
    pnodeFnc->sxFnc.SetStrictMode();
    pnodeFnc->sxFnc.SetDeclaration(TRUE);
    pnodeFnc->sxFnc.SetIsMethod(TRUE);
    pnodeFnc->sxFnc.SetIsClassMember(TRUE);
    pnodeFnc->sxFnc.SetIsClassConstructor(TRUE);
    pnodeFnc->sxFnc.SetIsBaseClassConstructor(!extends);
    pnodeFnc->sxFnc.SetHasNonThisStmt();
    pnodeFnc->sxFnc.SetIsGeneratedDefault(TRUE);

    pnodeFnc->ichLim = m_pscan->IchLimTok();
    pnodeFnc->ichMin = m_pscan->IchMinTok();
    pnodeFnc->sxFnc.cbLim = m_pscan->IecpLimTok();
    pnodeFnc->sxFnc.cbMin = m_pscan->IecpMinTok();
    pnodeFnc->sxFnc.astSize = 0;
    pnodeFnc->sxFnc.lineNumber = m_pscan->LineCur();

    pnodeFnc->sxFnc.functionId          = (*m_nextFunctionId);
    pnodeFnc->sxFnc.pid                 = nullptr;
    pnodeFnc->sxFnc.hint                = nullptr;
    pnodeFnc->sxFnc.hintOffset          = 0;
    pnodeFnc->sxFnc.hintLength          = 0;
    pnodeFnc->sxFnc.isNameIdentifierRef = true;
    pnodeFnc->sxFnc.nestedFuncEscapes   = false;
    pnodeFnc->sxFnc.pnodeName           = nullptr;
    pnodeFnc->sxFnc.pnodeScopes         = nullptr;
    pnodeFnc->sxFnc.pnodeParams         = nullptr;
    pnodeFnc->sxFnc.pnodeVars           = nullptr;
    pnodeFnc->sxFnc.pnodeBody           = nullptr;
    pnodeFnc->sxFnc.nestedCount         = 0;
    pnodeFnc->sxFnc.pnodeNext           = nullptr;
    pnodeFnc->sxFnc.pnodeRest           = nullptr;
    pnodeFnc->sxFnc.deferredStub        = nullptr;
    pnodeFnc->sxFnc.funcInfo            = nullptr;

    // In order to (re-)defer the default constructor, we need to, for instance, track
    // deferred class expression the way we track function expression, since we lose the part of the source
    // that tells us which we have.
    pnodeFnc->sxFnc.canBeDeferred       = false;

    pnodeFnc->sxFnc.isBodyAndParamScopeMerged = true;

#ifdef DBG
    pnodeFnc->sxFnc.deferredParseNextFunctionId = *(this->m_nextFunctionId);
#endif

    AppendFunctionToScopeList(true, pnodeFnc);

    if (m_nextFunctionId)
    {TRACE_IT(30136);
        (*m_nextFunctionId)++;
    }

    // Update the count of functions nested in the current parent.
    if (m_pnestedCount)
    {TRACE_IT(30137);
        (*m_pnestedCount)++;
    }

    if (!buildAST)
    {TRACE_IT(30138);
        return NULL;
    }

    if (m_pscan->IchMinTok() >= m_pscan->IchMinLine())
    {TRACE_IT(30139);
        // In scenarios involving defer parse IchMinLine() can be incorrect for the first line after defer parse
        pnodeFnc->sxFnc.columnNumber = m_pscan->IchMinTok() - m_pscan->IchMinLine();
    }
    else if (m_currentNodeFunc)
    {TRACE_IT(30140);
        // For the first line after defer parse, compute the column relative to the column number
        // of the lexically parent function.
        ULONG offsetFromCurrentFunction = m_pscan->IchMinTok() - m_currentNodeFunc->ichMin;
        pnodeFnc->sxFnc.columnNumber = m_currentNodeFunc->sxFnc.columnNumber + offsetFromCurrentFunction;
    }
    else
    {TRACE_IT(30141);
        // if there is no current function, lets give a default of 0.
        pnodeFnc->sxFnc.columnNumber = 0;
    }

    int32 * pAstSizeSave = m_pCurrentAstSize;
    m_pCurrentAstSize = &(pnodeFnc->sxFnc.astSize);

    // Make this the current function.
    ParseNodePtr pnodeFncSave = m_currentNodeFunc;
    m_currentNodeFunc = pnodeFnc;

    ParseNodePtr argsId = nullptr;
    ParseNodePtr *lastNodeRef = nullptr;
    ParseNodePtr pnodeBlock = StartParseBlock<buildAST>(PnodeBlockType::Parameter, ScopeType_Parameter);

    if (extends)
    {TRACE_IT(30142);
        // constructor(...args) { super(...args); }
        //             ^^^^^^^
        ParseNodePtr *const ppnodeVarSave = m_ppnodeVar;
        m_ppnodeVar = &pnodeFnc->sxFnc.pnodeVars;

        IdentPtr pidargs = m_phtbl->PidHashNameLen(_u("args"), sizeof("args") - 1);
        ParseNodePtr pnodeT = CreateVarDeclNode(pidargs, STFormal);
        pnodeT->sxVar.sym->SetIsNonSimpleParameter(true);
        pnodeFnc->sxFnc.pnodeRest = pnodeT;
        PidRefStack *ref = this->PushPidRef(pidargs);

        argsId = CreateNameNode(pidargs, pnodeFnc->ichMin, pnodeFnc->ichLim);

        argsId->sxPid.symRef = ref->GetSymRef();
        m_ppnodeVar = ppnodeVarSave;
    }

    ParseNodePtr pnodeInnerBlock = StartParseBlock<buildAST>(PnodeBlockType::Function, ScopeType_FunctionBody);
    pnodeBlock->sxBlock.pnodeScopes = pnodeInnerBlock;
    pnodeFnc->sxFnc.pnodeBodyScope = pnodeInnerBlock;
    pnodeFnc->sxFnc.pnodeScopes = pnodeBlock;

    if (extends)
    {TRACE_IT(30143);
        // constructor(...args) { super(...args); }
        //                        ^^^^^^^^^^^^^^^
        Assert(argsId);
        ParseNodePtr spreadArg = CreateUniNode(knopEllipsis, argsId, pnodeFnc->ichMin, pnodeFnc->ichLim);

        ParseNodePtr superRef = CreateNodeWithScanner<knopSuper>();
        pnodeFnc->sxFnc.SetHasSuperReference(TRUE);

        ParseNodePtr callNode = CreateCallNode(knopCall, superRef, spreadArg);
        callNode->sxCall.spreadArgCount = 1;
        AddToNodeList(&pnodeFnc->sxFnc.pnodeBody, &lastNodeRef, callNode);
    }

    AddToNodeList(&pnodeFnc->sxFnc.pnodeBody, &lastNodeRef, CreateNodeWithScanner<knopEndCode>());

    FinishParseBlock(pnodeInnerBlock);
    FinishParseBlock(pnodeBlock);

    m_currentNodeFunc = pnodeFncSave;
    m_pCurrentAstSize = pAstSizeSave;

    return pnodeFnc;
}

template<bool buildAST>
void Parser::ParseExpressionLambdaBody(ParseNodePtr pnodeLambda)
{TRACE_IT(30144);
    ParseNodePtr *lastNodeRef = nullptr;

    // The lambda body is a single expression, the result of which is the return value.
    ParseNodePtr pnodeRet = nullptr;

    if (buildAST)
    {TRACE_IT(30145);
        pnodeRet = CreateNodeWithScanner<knopReturn>();
        pnodeRet->grfpn |= PNodeFlags::fpnSyntheticNode;
        pnodeLambda->sxFnc.pnodeScopes->sxBlock.pnodeStmt = pnodeRet;
    }

    IdentToken token;
    charcount_t lastRParen = 0;
    ParseNodePtr result = ParseExpr<buildAST>(koplAsg, nullptr, TRUE, FALSE, nullptr, nullptr, nullptr, &token, false, nullptr, &lastRParen);

    this->MarkEscapingRef(result, &token);

    if (buildAST)
    {TRACE_IT(30146);
        pnodeRet->sxReturn.pnodeExpr = result;

        pnodeRet->ichMin = pnodeRet->sxReturn.pnodeExpr->ichMin;
        pnodeRet->ichLim = pnodeRet->sxReturn.pnodeExpr->ichLim;

        // Pushing a statement node with PushStmt<>() normally does this initialization
        // but do it here manually since we know there is no outer statement node.
        pnodeRet->sxStmt.grfnop = 0;
        pnodeRet->sxStmt.pnodeOuter = nullptr;

        pnodeLambda->ichLim = max(pnodeRet->ichLim, lastRParen);
        pnodeLambda->sxFnc.cbLim = m_pscan->IecpLimTokPrevious();
        pnodeLambda->sxFnc.pnodeScopes->ichLim = pnodeRet->ichLim;

        pnodeLambda->sxFnc.pnodeBody = nullptr;
        AddToNodeList(&pnodeLambda->sxFnc.pnodeBody, &lastNodeRef, pnodeLambda->sxFnc.pnodeScopes);

        // Append an EndCode node.
        ParseNodePtr end = CreateNodeWithScanner<knopEndCode>(pnodeRet->ichLim);
        end->ichLim = end->ichMin; // make end code zero width at the immediate end of lambda body
        AddToNodeList(&pnodeLambda->sxFnc.pnodeBody, &lastNodeRef, end);

        // Lambda's do not have arguments binding
        pnodeLambda->sxFnc.SetHasReferenceableBuiltInArguments(false);
    }
}

void Parser::CheckStrictFormalParameters()
{TRACE_IT(30147);
    if (m_token.tk == tkID)
    {TRACE_IT(30148);
        // single parameter arrow function case
        IdentPtr pid = m_token.GetIdentifier(m_phtbl);
        CheckStrictModeEvalArgumentsUsage(pid);
        return;
    }

    Assert(m_token.tk == tkLParen);
    m_pscan->ScanForcingPid();

    if (m_token.tk != tkRParen)
    {TRACE_IT(30149);
        SList<IdentPtr> formals(&m_nodeAllocator);
        for (;;)
        {TRACE_IT(30150);
            if (m_token.tk != tkID)
            {TRACE_IT(30151);
                IdentifierExpectedError(m_token);
            }

            IdentPtr pid = m_token.GetIdentifier(m_phtbl);
            CheckStrictModeEvalArgumentsUsage(pid);
            if (formals.Has(pid))
            {
                Error(ERRES5ArgSame, m_pscan->IchMinTok(), m_pscan->IchLimTok());
            }
            else
            {TRACE_IT(30152);
                formals.Prepend(pid);
            }

            m_pscan->Scan();

            if (m_token.tk == tkAsg && m_scriptContext->GetConfig()->IsES6DefaultArgsEnabled())
            {TRACE_IT(30153);
                m_pscan->Scan();
                // We can avoid building the AST since we are just checking the default expression.
                ParseNodePtr pnodeInit = ParseExpr<false>(koplCma);
                Assert(pnodeInit == nullptr);
            }

            if (m_token.tk != tkComma)
            {TRACE_IT(30154);
                break;
            }
            m_pscan->ScanForcingPid();

            if (m_token.tk == tkRParen && m_scriptContext->GetConfig()->IsES7TrailingCommaEnabled())
            {TRACE_IT(30155);
                break;
            }
        }
    }
    Assert(m_token.tk == tkRParen);
}

void Parser::FinishFncNode(ParseNodePtr pnodeFnc)
{TRACE_IT(30156);
    AnalysisAssert(pnodeFnc);

    // Finish the AST for a function that was deferred earlier, but which we decided
    // to finish after the fact.
    // We assume that the name(s) and arg(s) have already got parse nodes, so
    // we just have to do the function body.

    // Save the current next function Id, and resume from the old one.
    Js::LocalFunctionId * nextFunctionIdSave = m_nextFunctionId;
    Js::LocalFunctionId tempNextFunctionId = pnodeFnc->sxFnc.functionId + 1;
    this->m_nextFunctionId = &tempNextFunctionId;

    ParseNodePtr pnodeFncSave = m_currentNodeFunc;
    uint *pnestedCountSave = m_pnestedCount;
    int32* pAstSizeSave = m_pCurrentAstSize;

    m_currentNodeFunc = pnodeFnc;
    m_pCurrentAstSize = & (pnodeFnc->sxFnc.astSize);

    pnodeFnc->sxFnc.nestedCount = 0;
    m_pnestedCount = &pnodeFnc->sxFnc.nestedCount;

    // Cue up the parser to the start of the function body.
    if (pnodeFnc->sxFnc.pnodeName)
    {TRACE_IT(30157);
        // Skip the name(s).
        m_pscan->SetCurrentCharacter(pnodeFnc->sxFnc.pnodeName->ichLim, pnodeFnc->sxFnc.lineNumber);
    }
    else
    {TRACE_IT(30158);
        m_pscan->SetCurrentCharacter(pnodeFnc->ichMin, pnodeFnc->sxFnc.lineNumber);
        if (pnodeFnc->sxFnc.IsAccessor())
        {TRACE_IT(30159);
            // Getter/setter. The node text starts with the name, so eat that.
            m_pscan->ScanNoKeywords();
        }
        else
        {TRACE_IT(30160);
            // Anonymous function. Skip any leading "("'s and "function".
            for (;;)
            {TRACE_IT(30161);
                m_pscan->Scan();
                if (m_token.GetIdentifier(m_phtbl) == wellKnownPropertyPids.async)
                {TRACE_IT(30162);
                    Assert(pnodeFnc->sxFnc.IsAsync());
                    continue;
                }
                if (m_token.tk == tkFUNCTION)
                {TRACE_IT(30163);
                    break;
                }
                Assert(m_token.tk == tkLParen || m_token.tk == tkStar);
            }
        }
    }

    // switch scanner to treat 'yield' as keyword in generator functions
    // or as an identifier in non-generator functions
    bool fPreviousYieldIsKeyword = m_pscan->SetYieldIsKeyword(pnodeFnc && pnodeFnc->sxFnc.IsGenerator());

    bool fPreviousAwaitIsKeyword = m_pscan->SetAwaitIsKeyword(pnodeFnc && pnodeFnc->sxFnc.IsAsync());

    // Skip the arg list.
    m_pscan->ScanNoKeywords();
    if (m_token.tk == tkStar)
    {TRACE_IT(30164);
        Assert(pnodeFnc->sxFnc.IsGenerator());
        m_pscan->ScanNoKeywords();
    }
    Assert(m_token.tk == tkLParen);
    m_pscan->ScanNoKeywords();

    if (m_token.tk != tkRParen)
    {TRACE_IT(30165);
        for (;;)
        {TRACE_IT(30166);
            if (m_token.tk == tkEllipsis)
            {TRACE_IT(30167);
                m_pscan->ScanNoKeywords();
            }

            if (m_token.tk == tkID)
            {TRACE_IT(30168);
                m_pscan->ScanNoKeywords();

                if (m_token.tk == tkAsg)
                {TRACE_IT(30169);
                    // Eat the default expression
                    m_pscan->Scan();
                    ParseExpr<false>(koplCma);
                }
            }
            else if (IsPossiblePatternStart())
            {
                ParseDestructuredLiteralWithScopeSave(tkLET, false/*isDecl*/, false /*topLevel*/);
            }
            else
            {
                AssertMsg(false, "Unexpected identifier prefix while fast-scanning formals");
            }

            if (m_token.tk != tkComma)
            {TRACE_IT(30170);
                break;
            }
            m_pscan->ScanNoKeywords();

            if (m_token.tk == tkRParen && m_scriptContext->GetConfig()->IsES7TrailingCommaEnabled())
            {TRACE_IT(30171);
                break;
            }
        }
    }

    if (m_token.tk == tkRParen) // This might be false due to a lambda => token.
    {TRACE_IT(30172);
        m_pscan->Scan();
    }

    // Finish the function body.
    {TRACE_IT(30173);
        // Note that in IE8- modes, surrounding parentheses are considered part of function body. e.g. "( function x(){} )".
        // We lose that context here since we start from middle of function body. So save and restore source range info.
        ParseNodePtr* lastNodeRef = NULL;
        const charcount_t ichLim = pnodeFnc->ichLim;
        const size_t cbLim = pnodeFnc->sxFnc.cbLim;
        this->FinishFncDecl(pnodeFnc, NULL, lastNodeRef);

#if DBG
        // The pnode extent may not match the original extent.
        // We expect this to happen only when there are trailing ")"'s.
        // Consume them and make sure that's all we've got.
        if (pnodeFnc->ichLim != ichLim)
        {TRACE_IT(30174);
            Assert(pnodeFnc->ichLim < ichLim);
            m_pscan->SetCurrentCharacter(pnodeFnc->ichLim);
            while (m_pscan->IchLimTok() != ichLim)
            {TRACE_IT(30175);
                m_pscan->ScanNoKeywords();
                Assert(m_token.tk == tkRParen);
            }
        }
#endif
        pnodeFnc->ichLim = ichLim;
        pnodeFnc->sxFnc.cbLim = cbLim;
    }

    m_currentNodeFunc = pnodeFncSave;
    m_pCurrentAstSize = pAstSizeSave;
    m_pnestedCount = pnestedCountSave;
    Assert(m_pnestedCount);

    Assert(tempNextFunctionId == pnodeFnc->sxFnc.deferredParseNextFunctionId);
    this->m_nextFunctionId = nextFunctionIdSave;

    m_pscan->SetYieldIsKeyword(fPreviousYieldIsKeyword);
    m_pscan->SetAwaitIsKeyword(fPreviousAwaitIsKeyword);
}

void Parser::FinishFncDecl(ParseNodePtr pnodeFnc, LPCOLESTR pNameHint, ParseNodePtr *lastNodeRef, bool skipCurlyBraces)
{TRACE_IT(30176);
    LPCOLESTR name = NULL;
    JS_ETW(int32 startAstSize = *m_pCurrentAstSize);
    if (IS_JS_ETW(EventEnabledJSCRIPT_PARSE_METHOD_START()) || PHASE_TRACE1(Js::DeferParsePhase))
    {TRACE_IT(30177);
        name = GetFunctionName(pnodeFnc, pNameHint);
        m_functionBody = NULL;  // for nested functions we do not want to get the name of the top deferred function return name;
        JS_ETW(EventWriteJSCRIPT_PARSE_METHOD_START(m_sourceContextInfo->dwHostSourceContext, GetScriptContext(), pnodeFnc->sxFnc.functionId, 0, m_parseType, name));
        OUTPUT_TRACE(Js::DeferParsePhase, _u("Parsing function (%s) : %s (%d)\n"), GetParseType(), name, pnodeFnc->sxFnc.functionId);
    }

    JS_ETW_INTERNAL(EventWriteJSCRIPT_PARSE_FUNC(GetScriptContext(), pnodeFnc->sxFnc.functionId, /*Undefer*/FALSE));


    // Do the work of creating an AST for a function body.
    // This is common to the un-deferred case and the case in which we un-defer late in the game.

    Assert(pnodeFnc->nop == knopFncDecl);

    if (!skipCurlyBraces)
    {
        ChkCurTok(tkLCurly, ERRnoLcurly);
    }

    ParseStmtList<true>(&pnodeFnc->sxFnc.pnodeBody, &lastNodeRef, SM_OnFunctionCode, true /* isSourceElementList */);
    // Append an EndCode node.
    AddToNodeList(&pnodeFnc->sxFnc.pnodeBody, &lastNodeRef, CreateNodeWithScanner<knopEndCode>());

    if (!skipCurlyBraces)
    {
        ChkCurTokNoScan(tkRCurly, ERRnoRcurly);
    }

    pnodeFnc->ichLim = m_pscan->IchLimTok();
    pnodeFnc->sxFnc.cbLim = m_pscan->IecpLimTok();

#ifdef ENABLE_JS_ETW
    int32 astSize = *m_pCurrentAstSize - startAstSize;
    EventWriteJSCRIPT_PARSE_METHOD_STOP(m_sourceContextInfo->dwHostSourceContext, GetScriptContext(), pnodeFnc->sxFnc.functionId, astSize, m_parseType, name);
#endif
}

ParseNodePtr Parser::AddArgumentsNodeToVars(ParseNodePtr pnodeFnc)
{TRACE_IT(30178);
    Assert(!GetCurrentFunctionNode()->sxFnc.IsLambda());

    ParseNodePtr argNode = nullptr;
    argNode = CreateVarDeclNode(wellKnownPropertyPids.arguments, STVariable, true, pnodeFnc);
    Assert(argNode);
    argNode->grfpn |= PNodeFlags::fpnArguments; // Flag this as the built-in arguments node

    return argNode;
}

void Parser::UpdateArgumentsNode(ParseNodePtr pnodeFnc, ParseNodePtr argNode)
{TRACE_IT(30179);
    if ((pnodeFnc->grfpn & PNodeFlags::fpnArguments_overriddenInParam) || pnodeFnc->sxFnc.IsLambda())
    {TRACE_IT(30180);
        // There is a parameter named arguments. So we don't have to create the built-in arguments.
        pnodeFnc->sxFnc.SetHasReferenceableBuiltInArguments(false);
    }
    else if ((pnodeFnc->grfpn & PNodeFlags::fpnArguments_overriddenByDecl) && pnodeFnc->sxFnc.IsBodyAndParamScopeMerged())
    {TRACE_IT(30181);
        // In non-split scope case there is a var or function definition named arguments in the body
        pnodeFnc->sxFnc.SetHasReferenceableBuiltInArguments(false);
    }
    else
    {TRACE_IT(30182);
        pnodeFnc->sxFnc.SetHasReferenceableBuiltInArguments(true);
        Assert(argNode);
    }

    if (argNode != nullptr && !argNode->sxVar.sym->GetIsArguments())
    {TRACE_IT(30183);
        // A duplicate definition has updated the declaration node. Need to reset it back.
        argNode->grfpn |= PNodeFlags::fpnArguments;
        argNode->sxVar.sym->SetDecl(argNode);
    }
}

LPCOLESTR Parser::GetFunctionName(ParseNodePtr pnodeFnc, LPCOLESTR pNameHint)
{TRACE_IT(30184);
    LPCOLESTR name = nullptr;
    if(pnodeFnc->sxFnc.pnodeName != nullptr && knopVarDecl == pnodeFnc->sxFnc.pnodeName->nop)
    {TRACE_IT(30185);
        name = pnodeFnc->sxFnc.pnodeName->sxVar.pid->Psz();
    }
    if(name == nullptr && pNameHint != nullptr)
    {TRACE_IT(30186);
        name = pNameHint;
    }
    if(name == nullptr && m_functionBody != nullptr)
    {TRACE_IT(30187);
        name = m_functionBody->GetExternalDisplayName();
    }
    else if(name == nullptr)
    {TRACE_IT(30188);
        name = Js::Constants::AnonymousFunction;
    }
    return name;
}

IdentPtr Parser::ParseClassPropertyName(IdentPtr * pidHint)
{TRACE_IT(30189);
    if (m_token.tk == tkID || m_token.tk == tkStrCon || m_token.IsReservedWord())
    {TRACE_IT(30190);
        IdentPtr pid;
        if (m_token.tk == tkStrCon)
        {TRACE_IT(30191);
            if (m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
            {TRACE_IT(30192);
                Error(ERRES5NoOctal);
            }

            pid = m_token.GetStr();
        }
        else
        {TRACE_IT(30193);
            pid = m_token.GetIdentifier(m_phtbl);
        }
        *pidHint = pid;
        return pid;
    }
    else if (m_token.tk == tkIntCon)
    {TRACE_IT(30194);
        if (m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {TRACE_IT(30195);
            Error(ERRES5NoOctal);
        }

        return m_pscan->PidFromLong(m_token.GetLong());
    }
    else if (m_token.tk == tkFltCon)
    {TRACE_IT(30196);
        if (m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {TRACE_IT(30197);
            Error(ERRES5NoOctal);
        }

        return m_pscan->PidFromDbl(m_token.GetDouble());
    }

    Error(ERRnoMemberIdent);
}

LPCOLESTR Parser::ConstructFinalHintNode(IdentPtr pClassName, IdentPtr pMemberName, IdentPtr pGetSet, bool isStatic, uint32* nameLength, uint32* pShortNameOffset, bool isComputedName, LPCOLESTR pMemberNameHint)
{TRACE_IT(30198);
    if ((pMemberName == nullptr && !isComputedName) ||
        (pMemberNameHint == nullptr && isComputedName) ||
        !CONFIG_FLAG(UseFullName))
    {TRACE_IT(30199);
        return nullptr;
    }

    LPCOLESTR pFinalName = isComputedName? pMemberNameHint : pMemberName->Psz();
    uint32 fullNameHintLength = 0;
    uint32 shortNameOffset = 0;
    if (!isStatic)
    {TRACE_IT(30200);
        // Add prototype.
        pFinalName = AppendNameHints(wellKnownPropertyPids.prototype, pFinalName, &fullNameHintLength, &shortNameOffset);
    }

    if (pClassName)
    {TRACE_IT(30201);
        uint32 classNameOffset = 0;
        pFinalName = AppendNameHints(pClassName, pFinalName, &fullNameHintLength, &classNameOffset);
        shortNameOffset += classNameOffset;
    }

    if (pGetSet)
    {TRACE_IT(30202);
        if (m_scriptContext->GetConfig()->IsES6FunctionNameEnabled())
        {TRACE_IT(30203);
            // displays as get/set prototype.funcname
            uint32 getSetOffset = 0;
            pFinalName = AppendNameHints(pGetSet, pFinalName, &fullNameHintLength, &getSetOffset, true);
            shortNameOffset += getSetOffset;
        }
        else
        {TRACE_IT(30204);
            pFinalName = AppendNameHints(pFinalName, pGetSet, &fullNameHintLength, &shortNameOffset);
        }

    }
    if (fullNameHintLength > *nameLength)
    {TRACE_IT(30205);
        *nameLength = fullNameHintLength;
    }

    if (shortNameOffset > *pShortNameOffset)
    {TRACE_IT(30206);
        *pShortNameOffset = shortNameOffset;
    }

    return pFinalName;
}

class AutoParsingSuperRestrictionStateRestorer
{
public:
    AutoParsingSuperRestrictionStateRestorer(Parser* parser) : m_parser(parser)
    {TRACE_IT(30207);
        AssertMsg(this->m_parser != nullptr, "This just should not happen");
        this->m_originalParsingSuperRestrictionState = this->m_parser->m_parsingSuperRestrictionState;
    }
    ~AutoParsingSuperRestrictionStateRestorer()
    {TRACE_IT(30208);
        AssertMsg(this->m_parser != nullptr, "This just should not happen");
        this->m_parser->m_parsingSuperRestrictionState = m_originalParsingSuperRestrictionState;
    }
private:
    Parser* m_parser;
    int m_originalParsingSuperRestrictionState;
};

template<bool buildAST>
ParseNodePtr Parser::ParseClassDecl(BOOL isDeclaration, LPCOLESTR pNameHint, uint32 *pHintLength, uint32 *pShortNameOffset)
{TRACE_IT(30209);
    bool hasConstructor = false;
    bool hasExtends = false;
    IdentPtr name = nullptr;
    ParseNodePtr pnodeName = nullptr;
    ParseNodePtr pnodeConstructor = nullptr;
    ParseNodePtr pnodeExtends = nullptr;
    ParseNodePtr pnodeMembers = nullptr;
    ParseNodePtr *lastMemberNodeRef = nullptr;
    ParseNodePtr pnodeStaticMembers = nullptr;
    ParseNodePtr *lastStaticMemberNodeRef = nullptr;
    uint32 nameHintLength = pHintLength ? *pHintLength : 0;
    uint32 nameHintOffset = pShortNameOffset ? *pShortNameOffset : 0;

    ArenaAllocator tempAllocator(_u("ClassMemberNames"), m_nodeAllocator.GetPageAllocator(), Parser::OutOfMemory);

    ParseNodePtr pnodeClass = nullptr;
    if (buildAST)
    {TRACE_IT(30210);
        pnodeClass = CreateNode(knopClassDecl);

        CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(Class, m_scriptContext);
    }

    m_pscan->Scan();
    if (m_token.tk == tkID)
    {TRACE_IT(30211);
        name = m_token.GetIdentifier(m_phtbl);
        m_pscan->Scan();
    }
    else if (isDeclaration)
    {TRACE_IT(30212);
        IdentifierExpectedError(m_token);
    }

    if (isDeclaration && name == wellKnownPropertyPids.arguments && GetCurrentBlockInfo()->pnodeBlock->sxBlock.blockType == Function)
    {TRACE_IT(30213);
        GetCurrentFunctionNode()->grfpn |= PNodeFlags::fpnArguments_overriddenByDecl;
    }

    BOOL strictSave = m_fUseStrictMode;
    m_fUseStrictMode = TRUE;

    ParseNodePtr pnodeDeclName = nullptr;
    if (isDeclaration)
    {TRACE_IT(30214);
        pnodeDeclName = CreateBlockScopedDeclNode(name, knopLetDecl);
    }

    ParseNodePtr *ppnodeScopeSave = nullptr;
    ParseNodePtr *ppnodeExprScopeSave = nullptr;

    ParseNodePtr pnodeBlock = StartParseBlock<buildAST>(PnodeBlockType::Regular, ScopeType_Block);
    if (buildAST)
    {
        PushFuncBlockScope(pnodeBlock, &ppnodeScopeSave, &ppnodeExprScopeSave);
        pnodeClass->sxClass.pnodeBlock = pnodeBlock;
    }

    if (name)
    {TRACE_IT(30215);
        pnodeName = CreateBlockScopedDeclNode(name, knopConstDecl);
    }

    if (m_token.tk == tkEXTENDS)
    {TRACE_IT(30216);
        m_pscan->Scan();
        pnodeExtends = ParseExpr<buildAST>();
        hasExtends = true;
    }

    if (m_token.tk != tkLCurly)
    {TRACE_IT(30217);
        Error(ERRnoLcurly);
    }

    OUTPUT_TRACE_DEBUGONLY(Js::ES6VerboseFlag, _u("Parsing class (%s) : %s\n"), GetParseType(), name ? name->Psz() : _u("anonymous class"));

    RestorePoint beginClass;
    m_pscan->Capture(&beginClass);

    m_pscan->ScanForcingPid();

    IdentPtr pClassNamePid = pnodeName ? pnodeName->sxVar.pid : nullptr;

    for (;;)
    {TRACE_IT(30218);
        if (m_token.tk == tkSColon)
        {TRACE_IT(30219);
            m_pscan->ScanForcingPid();
            continue;
        }
        if (m_token.tk == tkRCurly)
        {TRACE_IT(30220);
            break;
        }

        bool isStatic = m_token.tk == tkSTATIC;
        if (isStatic)
        {TRACE_IT(30221);
            m_pscan->ScanForcingPid();
        }

        ushort fncDeclFlags = fFncNoName | fFncMethod | fFncClassMember;
        charcount_t ichMin = 0;
        size_t iecpMin = 0;
        ParseNodePtr pnodeMemberName = nullptr;
        IdentPtr pidHint = nullptr;
        IdentPtr memberPid = nullptr;
        LPCOLESTR pMemberNameHint = nullptr;
        uint32     memberNameHintLength = 0;
        uint32     memberNameOffset = 0;
        bool isComputedName = false;
        bool isAsyncMethod = false;

        if (m_token.tk == tkID && m_token.GetIdentifier(m_phtbl) == wellKnownPropertyPids.async && m_scriptContext->GetConfig()->IsES7AsyncAndAwaitEnabled())
        {TRACE_IT(30222);
            RestorePoint parsedAsync;
            m_pscan->Capture(&parsedAsync);
            ichMin = m_pscan->IchMinTok();
            iecpMin = m_pscan->IecpMinTok();

            m_pscan->Scan();
            if (m_token.tk == tkLParen || m_pscan->FHadNewLine())
            {TRACE_IT(30223);
                m_pscan->SeekTo(parsedAsync);
            }
            else
            {TRACE_IT(30224);
                isAsyncMethod = true;
            }
        }

        bool isGenerator = m_scriptContext->GetConfig()->IsES6GeneratorsEnabled() &&
                           m_token.tk == tkStar;
        if (isGenerator)
        {TRACE_IT(30225);
            fncDeclFlags |= fFncGenerator;
            m_pscan->ScanForcingPid();
        }


        if (m_token.tk == tkLBrack && m_scriptContext->GetConfig()->IsES6ObjectLiteralsEnabled())
        {TRACE_IT(30226);
            // Computed member name: [expr] () { }
            LPCOLESTR emptyHint = nullptr;
            ParseComputedName<buildAST>(&pnodeMemberName, &emptyHint, &pMemberNameHint, &memberNameHintLength, &memberNameOffset);
            isComputedName = true;
        }
        else // not computed name
        {TRACE_IT(30227);
            memberPid = this->ParseClassPropertyName(&pidHint);
            if (pidHint)
            {TRACE_IT(30228);
                pMemberNameHint = pidHint->Psz();
                memberNameHintLength = pidHint->Cch();
            }
        }

        if (buildAST && memberPid)
        {TRACE_IT(30229);
            pnodeMemberName = CreateStrNodeWithScanner(memberPid);
        }

        if (!isStatic && memberPid == wellKnownPropertyPids.constructor)
        {TRACE_IT(30230);
            if (hasConstructor || isAsyncMethod)
            {TRACE_IT(30231);
                Error(ERRsyntax);
            }
            hasConstructor = true;
            LPCOLESTR pConstructorName = nullptr;
            uint32  constructorNameLength = 0;
            uint32  constructorShortNameHintOffset = 0;
            if (pnodeName && pnodeName->sxVar.pid)
            {TRACE_IT(30232);
                pConstructorName = pnodeName->sxVar.pid->Psz();
                constructorNameLength = pnodeName->sxVar.pid->Cch();
            }
            else
            {TRACE_IT(30233);
                pConstructorName = pNameHint;
                constructorNameLength = nameHintLength;
                constructorShortNameHintOffset = nameHintOffset;
            }

            {
                AutoParsingSuperRestrictionStateRestorer restorer(this);
                this->m_parsingSuperRestrictionState = hasExtends ? ParsingSuperRestrictionState_SuperCallAndPropertyAllowed : ParsingSuperRestrictionState_SuperPropertyAllowed;
                pnodeConstructor = ParseFncDecl<buildAST>(fncDeclFlags, pConstructorName, /* needsPIDOnRCurlyScan */ true, /* resetParsingSuperRestrictionState = */false);
            }

            if (pnodeConstructor->sxFnc.IsGenerator())
            {TRACE_IT(30234);
                Error(ERRConstructorCannotBeGenerator);
            }

            Assert(constructorNameLength >= constructorShortNameHintOffset);
            // The constructor function will get the same name as class.
            pnodeConstructor->sxFnc.hint = pConstructorName;
            pnodeConstructor->sxFnc.hintLength = constructorNameLength;
            pnodeConstructor->sxFnc.hintOffset = constructorShortNameHintOffset;
            pnodeConstructor->sxFnc.pid = pnodeName && pnodeName->sxVar.pid ? pnodeName->sxVar.pid : wellKnownPropertyPids.constructor;
            pnodeConstructor->sxFnc.SetIsClassConstructor(TRUE);
            pnodeConstructor->sxFnc.SetHasNonThisStmt();
            pnodeConstructor->sxFnc.SetIsBaseClassConstructor(pnodeExtends == nullptr);
        }
        else
        {TRACE_IT(30235);
            ParseNodePtr pnodeMember = nullptr;

            bool isMemberNamedGetOrSet = false;
            RestorePoint beginMethodName;
            m_pscan->Capture(&beginMethodName);
            if (memberPid == wellKnownPropertyPids.get || memberPid == wellKnownPropertyPids.set)
            {TRACE_IT(30236);
                m_pscan->ScanForcingPid();
            }
            if (m_token.tk == tkLParen)
            {TRACE_IT(30237);
                m_pscan->SeekTo(beginMethodName);
                isMemberNamedGetOrSet = true;
            }

            if ((memberPid == wellKnownPropertyPids.get || memberPid == wellKnownPropertyPids.set) && !isMemberNamedGetOrSet)
            {TRACE_IT(30238);
                bool isGetter = (memberPid == wellKnownPropertyPids.get);

                if (m_token.tk == tkLBrack && m_scriptContext->GetConfig()->IsES6ObjectLiteralsEnabled())
                {TRACE_IT(30239);
                    // Computed get/set member name: get|set [expr] () { }
                    LPCOLESTR emptyHint = nullptr;
                    ParseComputedName<buildAST>(&pnodeMemberName, &emptyHint, &pMemberNameHint, &memberNameHintLength, &memberNameOffset);
                    isComputedName = true;
                }
                else // not computed name
                {TRACE_IT(30240);
                    memberPid = this->ParseClassPropertyName(&pidHint);
                }

                if ((isStatic ? (memberPid == wellKnownPropertyPids.prototype) : (memberPid == wellKnownPropertyPids.constructor)) || isAsyncMethod)
                {TRACE_IT(30241);
                    Error(ERRsyntax);
                }
                if (buildAST && memberPid && !isComputedName)
                {TRACE_IT(30242);
                    pnodeMemberName = CreateStrNodeWithScanner(memberPid);
                }

                ParseNodePtr pnodeFnc = nullptr;
                {
                    AutoParsingSuperRestrictionStateRestorer restorer(this);
                    this->m_parsingSuperRestrictionState = ParsingSuperRestrictionState_SuperPropertyAllowed;
                    pnodeFnc = ParseFncDecl<buildAST>(fncDeclFlags | (isGetter ? fFncNoArg : fFncOneArg),
                        pidHint ? pidHint->Psz() : nullptr, /* needsPIDOnRCurlyScan */ true,
                        /* resetParsingSuperRestrictionState */false);
                }

                pnodeFnc->sxFnc.SetIsStaticMember(isStatic);

                if (buildAST)
                {TRACE_IT(30243);
                    pnodeFnc->sxFnc.SetIsAccessor();
                    pnodeMember = CreateBinNode(isGetter ? knopGetMember : knopSetMember, pnodeMemberName, pnodeFnc);
                    pMemberNameHint = ConstructFinalHintNode(pClassNamePid, pidHint,
                        isGetter ? wellKnownPropertyPids.get : wellKnownPropertyPids.set, isStatic,
                        &memberNameHintLength, &memberNameOffset, isComputedName, pMemberNameHint);
                }
            }
            else
            {TRACE_IT(30244);
                if (isStatic && (memberPid == wellKnownPropertyPids.prototype))
                {TRACE_IT(30245);
                    Error(ERRsyntax);
                }

                ParseNodePtr pnodeFnc = nullptr;
                {
                    AutoParsingSuperRestrictionStateRestorer restorer(this);
                    this->m_parsingSuperRestrictionState = ParsingSuperRestrictionState_SuperPropertyAllowed;

                    if (isAsyncMethod)
                    {TRACE_IT(30246);
                        fncDeclFlags |= fFncAsync;
                    }
                    pnodeFnc = ParseFncDecl<buildAST>(fncDeclFlags, pidHint ? pidHint->Psz() : nullptr, /* needsPIDOnRCurlyScan */ true, /* resetParsingSuperRestrictionState */false);
                    if (isAsyncMethod)
                    {TRACE_IT(30247);
                        pnodeFnc->sxFnc.cbMin = iecpMin;
                        pnodeFnc->ichMin = ichMin;
                    }
                }
                pnodeFnc->sxFnc.SetIsStaticMember(isStatic);

                if (buildAST)
                {TRACE_IT(30248);
                    pnodeMember = CreateBinNode(knopMember, pnodeMemberName, pnodeFnc);
                    pMemberNameHint = ConstructFinalHintNode(pClassNamePid, pidHint, nullptr /*pgetset*/, isStatic, &memberNameHintLength, &memberNameOffset, isComputedName, pMemberNameHint);
                }
            }

            if (buildAST)
            {TRACE_IT(30249);
                Assert(memberNameHintLength >= memberNameOffset);
                pnodeMember->sxBin.pnode2->sxFnc.hint = pMemberNameHint; // Fully qualified name
                pnodeMember->sxBin.pnode2->sxFnc.hintLength = memberNameHintLength;
                pnodeMember->sxBin.pnode2->sxFnc.hintOffset = memberNameOffset;
                pnodeMember->sxBin.pnode2->sxFnc.pid = memberPid; // Short name

                AddToNodeList(isStatic ? &pnodeStaticMembers : &pnodeMembers, isStatic ? &lastStaticMemberNodeRef : &lastMemberNodeRef, pnodeMember);
            }
        }
    }

    if (buildAST)
    {TRACE_IT(30250);
        pnodeClass->ichLim = m_pscan->IchLimTok();
    }

    if (!hasConstructor)
    {TRACE_IT(30251);
        OUTPUT_TRACE_DEBUGONLY(Js::ES6VerboseFlag, _u("Generating constructor (%s) : %s\n"), GetParseType(), name ? name->Psz() : _u("anonymous class"));

        RestorePoint endClass;
        m_pscan->Capture(&endClass);
        m_pscan->SeekTo(beginClass);

        pnodeConstructor = GenerateEmptyConstructor<buildAST>(pnodeExtends != nullptr);
        if (buildAST)
        {TRACE_IT(30252);
            if (pClassNamePid)
            {TRACE_IT(30253);
                pnodeConstructor->sxFnc.hint = pClassNamePid->Psz();
                pnodeConstructor->sxFnc.hintLength = pClassNamePid->Cch();
                pnodeConstructor->sxFnc.hintOffset = 0;
            }
            else
            {TRACE_IT(30254);
                Assert(nameHintLength >= nameHintOffset);
                pnodeConstructor->sxFnc.hint = pNameHint;
                pnodeConstructor->sxFnc.hintLength = nameHintLength;
                pnodeConstructor->sxFnc.hintOffset = nameHintOffset;
            }
            pnodeConstructor->sxFnc.pid = pClassNamePid;
        }

        m_pscan->SeekTo(endClass);
    }

    if (buildAST)
    {TRACE_IT(30255);
        pnodeConstructor->sxFnc.cbMin = pnodeClass->ichMin;
        pnodeConstructor->sxFnc.cbLim = pnodeClass->ichLim;
        pnodeConstructor->ichMin = pnodeClass->ichMin;
        pnodeConstructor->ichLim = pnodeClass->ichLim;

        PopFuncBlockScope(ppnodeScopeSave, ppnodeExprScopeSave);

        pnodeClass->sxClass.pnodeDeclName = pnodeDeclName;
        pnodeClass->sxClass.pnodeName = pnodeName;
        pnodeClass->sxClass.pnodeConstructor = pnodeConstructor;
        pnodeClass->sxClass.pnodeExtends = pnodeExtends;
        pnodeClass->sxClass.pnodeMembers = pnodeMembers;
        pnodeClass->sxClass.pnodeStaticMembers = pnodeStaticMembers;
        pnodeClass->sxClass.isDefaultModuleExport = false;
    }
    FinishParseBlock(pnodeBlock);

    m_fUseStrictMode = strictSave;

    m_pscan->Scan();

    return pnodeClass;
}

template<bool buildAST>
ParseNodePtr Parser::ParseStringTemplateDecl(ParseNodePtr pnodeTagFnc)
{TRACE_IT(30256);
    ParseNodePtr pnodeStringLiterals = nullptr;
    ParseNodePtr* lastStringLiteralNodeRef = nullptr;
    ParseNodePtr pnodeRawStringLiterals = nullptr;
    ParseNodePtr* lastRawStringLiteralNodeRef = nullptr;
    ParseNodePtr pnodeSubstitutionExpressions = nullptr;
    ParseNodePtr* lastSubstitutionExpressionNodeRef = nullptr;
    ParseNodePtr pnodeTagFncArgs = nullptr;
    ParseNodePtr* lastTagFncArgNodeRef = nullptr;
    ParseNodePtr stringLiteral = nullptr;
    ParseNodePtr stringLiteralRaw = nullptr;
    ParseNodePtr pnodeStringTemplate = nullptr;
    bool templateClosed = false;
    const bool isTagged = pnodeTagFnc != nullptr;
    uint16 stringConstantCount = 0;
    charcount_t ichMin = 0;

    Assert(m_token.tk == tkStrTmplBasic || m_token.tk == tkStrTmplBegin);

    if (buildAST)
    {TRACE_IT(30257);
        pnodeStringTemplate = CreateNode(knopStrTemplate);
        pnodeStringTemplate->sxStrTemplate.countStringLiterals = 0;
        pnodeStringTemplate->sxStrTemplate.isTaggedTemplate = isTagged ? TRUE : FALSE;

        // If this is a tagged string template, we need to start building the arg list for the call
        if (isTagged)
        {TRACE_IT(30258);
            ichMin = pnodeTagFnc->ichMin;
            AddToNodeListEscapedUse(&pnodeTagFncArgs, &lastTagFncArgNodeRef, pnodeStringTemplate);
        }

    }
    CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(StringTemplates, m_scriptContext);

    OUTPUT_TRACE_DEBUGONLY(
        Js::StringTemplateParsePhase,
        _u("Starting to parse a string template (%s)...\n\tis tagged = %s\n"),
        GetParseType(),
        isTagged ? _u("true") : _u("false (Raw and cooked strings will not differ!)"));

    // String template grammar
    // `...`   Simple string template
    // `...${  String template beginning
    // }...${  String template middle
    // }...`   String template end
    while (!templateClosed)
    {TRACE_IT(30259);
        // First, extract the string constant part - we always have one
        if (IsStrictMode() && m_pscan->IsOctOrLeadingZeroOnLastTKNumber())
        {TRACE_IT(30260);
            Error(ERRES5NoOctal);
        }

        // We are not able to pass more than a ushort worth of arguments to the tag
        // so use that as a logical limit on the number of string constant pieces.
        if (stringConstantCount >= USHRT_MAX)
        {TRACE_IT(30261);
            Error(ERRnoMemory);
        }

        // Keep track of the string literal count (must be the same for raw strings)
        // We use this in code gen so we don't need to count the string literals list
        stringConstantCount++;

        // If we are not creating parse nodes, there is no need to create strings
        if (buildAST)
        {TRACE_IT(30262);
            stringLiteral = CreateStrNodeWithScanner(m_token.GetStr());

            AddToNodeList(&pnodeStringLiterals, &lastStringLiteralNodeRef, stringLiteral);

            // We only need to collect a raw string when we are going to pass the string template to a tag
            if (isTagged)
            {TRACE_IT(30263);
                // Make the scanner create a PID for the raw string constant for the preceding scan
                IdentPtr pid = m_pscan->GetSecondaryBufferAsPid();

                stringLiteralRaw = CreateStrNodeWithScanner(pid);

                // Should have gotten a raw string literal above
                AddToNodeList(&pnodeRawStringLiterals, &lastRawStringLiteralNodeRef, stringLiteralRaw);
            }
            else
            {TRACE_IT(30264);
#if DBG
                // Assign the raw string for debug tracing below
                stringLiteralRaw = stringLiteral;
#endif
            }

            OUTPUT_TRACE_DEBUGONLY(
                Js::StringTemplateParsePhase,
                _u("Parsed string constant: \n\tcooked = \"%s\" \n\traw = \"%s\" \n\tdiffer = %d\n"),
                stringLiteral->sxPid.pid->Psz(),
                stringLiteralRaw->sxPid.pid->Psz(),
                stringLiteral->sxPid.pid->Psz() == stringLiteralRaw->sxPid.pid->Psz() ? 0 : 1);
        }

        switch (m_token.tk)
        {
        case tkStrTmplEnd:
        case tkStrTmplBasic:
            // We do not need to parse an expression for either the end or basic string template tokens
            templateClosed = true;
            break;
        case tkStrTmplBegin:
        case tkStrTmplMid:
            {TRACE_IT(30265);
            // In the middle or begin string template token case, we need to parse an expression next
            m_pscan->Scan();

            // Parse the contents of the curly braces as an expression
            ParseNodePtr expression = ParseExpr<buildAST>(0);

            // After parsing expression, scan should leave us with an RCurly token.
            // Use the NoScan version so we do not automatically perform a scan - we need to
            // set the scan state before next scan but we don't want to set that state if
            // the token is not as expected since we'll error in that case.
            ChkCurTokNoScan(tkRCurly, ERRnoRcurly);

            // Notify the scanner that it should scan for a middle or end string template token
            m_pscan->SetScanState(Scanner_t::ScanState::ScanStateStringTemplateMiddleOrEnd);
            m_pscan->Scan();

            if (buildAST)
            {TRACE_IT(30266);
                // If we are going to call the tag function, add this expression into the list of args
                if (isTagged)
                {
                    AddToNodeListEscapedUse(&pnodeTagFncArgs, &lastTagFncArgNodeRef, expression);
                }
                else
                {
                    // Otherwise add it to the substitution expression list
                    // TODO: Store the arguments and substitution expressions in a single list?
                    AddToNodeList(&pnodeSubstitutionExpressions, &lastSubstitutionExpressionNodeRef, expression);
                }
            }

            if (!(m_token.tk == tkStrTmplMid || m_token.tk == tkStrTmplEnd))
            {TRACE_IT(30267);
                // Scan with ScanState ScanStateStringTemplateMiddleOrEnd should only return
                // tkStrTmpMid/End unless it is EOF or tkScanError
                Assert(m_token.tk == tkEOF || m_token.tk == tkScanError);
                Error(ERRsyntax);
            }

            OUTPUT_TRACE_DEBUGONLY(Js::StringTemplateParsePhase, _u("Parsed expression\n"));
            }
            break;
        default:
            Assert(false);
            break;
        }
    }

    if (buildAST)
    {TRACE_IT(30268);
        pnodeStringTemplate->sxStrTemplate.pnodeStringLiterals = pnodeStringLiterals;
        pnodeStringTemplate->sxStrTemplate.pnodeStringRawLiterals = pnodeRawStringLiterals;
        pnodeStringTemplate->sxStrTemplate.pnodeSubstitutionExpressions = pnodeSubstitutionExpressions;
        pnodeStringTemplate->sxStrTemplate.countStringLiterals = stringConstantCount;

        // We should still have the last string literal.
        // Use the char offset of the end of that constant as the end of the string template.
        pnodeStringTemplate->ichLim = stringLiteral->ichLim;

        // If this is a tagged template, we now have the argument list and can construct a call node
        if (isTagged)
        {TRACE_IT(30269);
            // Return the call node here and let the byte code generator Emit the string template automagically
            pnodeStringTemplate = CreateCallNode(knopCall, pnodeTagFnc, pnodeTagFncArgs, ichMin, pnodeStringTemplate->ichLim);

            // We need to set the arg count explicitly
            pnodeStringTemplate->sxCall.argCount = stringConstantCount;
        }
    }

    m_pscan->Scan();

    return pnodeStringTemplate;
}

LPCOLESTR Parser::FormatPropertyString(LPCOLESTR propertyString, ParseNodePtr pNode, uint32 *fullNameHintLength, uint32 *pShortNameOffset)
{TRACE_IT(30270);
    // propertyString could be null, such as 'this.foo' =
    // propertyString could be empty, found in pattern as in (-1)[""][(x = z)]

    OpCode op = pNode->nop;
    LPCOLESTR rightNode = nullptr;
    if (propertyString == nullptr)
    {TRACE_IT(30271);
        propertyString = _u("");
    }

    if (op != knopInt && op != knopFlt && op != knopName && op != knopStr)
    {TRACE_IT(30272);
        rightNode = _u("");
    }
    else if (op == knopStr)
    {TRACE_IT(30273);
        return AppendNameHints(propertyString, pNode->sxPid.pid, fullNameHintLength, pShortNameOffset, false, true/*add brackets*/);
    }
    else if(op == knopFlt)
    {TRACE_IT(30274);
        rightNode = m_pscan->StringFromDbl(pNode->sxFlt.dbl);
    }
    else
    {TRACE_IT(30275);
        rightNode = op == knopInt ? m_pscan->StringFromLong(pNode->sxInt.lw)
            : pNode->sxPid.pid->Psz();
    }

    return AppendNameHints(propertyString, rightNode, fullNameHintLength, pShortNameOffset, false, true/*add brackets*/);
}

LPCOLESTR Parser::ConstructNameHint(ParseNodePtr pNode, uint32* fullNameHintLength, uint32 *pShortNameOffset)
{TRACE_IT(30276);
    Assert(pNode != nullptr);
    Assert(pNode->nop == knopDot || pNode->nop == knopIndex);
    LPCOLESTR leftNode = nullptr;
    if (pNode->sxBin.pnode1->nop == knopDot || pNode->sxBin.pnode1->nop == knopIndex)
    {TRACE_IT(30277);
        leftNode = ConstructNameHint(pNode->sxBin.pnode1, fullNameHintLength, pShortNameOffset);
    }
    else if (pNode->sxBin.pnode1->nop == knopName)
    {TRACE_IT(30278);
        leftNode = pNode->sxBin.pnode1->sxPid.pid->Psz();
        *fullNameHintLength = pNode->sxBin.pnode1->sxPid.pid->Cch();
        *pShortNameOffset = 0;
    }

    if (pNode->nop == knopIndex)
    {TRACE_IT(30279);
        return FormatPropertyString(
            leftNode ? leftNode : Js::Constants::AnonymousFunction, // e.g. f()[0] = function () {}
            pNode->sxBin.pnode2, fullNameHintLength, pShortNameOffset);
    }

    Assert(pNode->sxBin.pnode2->nop == knopDot || pNode->sxBin.pnode2->nop == knopName);

    LPCOLESTR rightNode = nullptr;
    bool wrapWithBrackets = false;
    if (pNode->sxBin.pnode2->nop == knopDot)
    {TRACE_IT(30280);
        rightNode = ConstructNameHint(pNode->sxBin.pnode2, fullNameHintLength, pShortNameOffset);
    }
    else
    {TRACE_IT(30281);
        rightNode = pNode->sxBin.pnode2->sxPid.pid->Psz();
        wrapWithBrackets = PNodeFlags::fpnIndexOperator == (pNode->grfpn & PNodeFlags::fpnIndexOperator);
    }
    Assert(rightNode != nullptr);
    return AppendNameHints(leftNode, rightNode, fullNameHintLength, pShortNameOffset, false, wrapWithBrackets);
}

LPCOLESTR Parser::AppendNameHints(LPCOLESTR leftStr, uint32 leftLen, LPCOLESTR rightStr, uint32 rightLen, uint32 *pNameLength, uint32 *pShortNameOffset, bool ignoreAddDotWithSpace, bool wrapInBrackets)
{TRACE_IT(30282);
    Assert(rightStr != nullptr);
    Assert(leftLen  != 0 || wrapInBrackets);
    Assert(rightLen != 0 || wrapInBrackets);

    bool ignoreDot = rightStr[0] == _u('[') && !wrapInBrackets;//if we wrap in brackets it can be a string literal which can have brackets at the first char
    uint32 totalLength = leftLen + rightLen + ((ignoreDot) ? 1 : 2); // 1 (for dot or [) + 1 (for null termination)

    if (wrapInBrackets)
    {TRACE_IT(30283);
        totalLength++; //1 for ']';
    }
    WCHAR * finalName = AllocateStringOfLength(totalLength);

    if (leftStr != nullptr && leftLen != 0)
    {
        wcscpy_s(finalName, leftLen + 1, leftStr);
    }

    if (ignoreAddDotWithSpace)
    {TRACE_IT(30284);
        finalName[leftLen++] = (OLECHAR)_u(' ');
    }
    // mutually exclusive from ignoreAddDotWithSpace which is used for getters/setters

    else if (wrapInBrackets)
    {TRACE_IT(30285);
        finalName[leftLen++] = (OLECHAR)_u('[');
        finalName[totalLength-2] = (OLECHAR)_u(']');
    }
    else if (!ignoreDot)
    {TRACE_IT(30286);
        finalName[leftLen++] = (OLECHAR)_u('.');
    }
    //ignore case falls through
    js_wmemcpy_s(finalName + leftLen, rightLen, rightStr, rightLen);
    finalName[totalLength-1] = (OLECHAR)_u('\0');

    if (pNameLength != nullptr)
    {TRACE_IT(30287);
        *pNameLength = totalLength - 1;
    }
    if (pShortNameOffset != nullptr)
    {TRACE_IT(30288);
        *pShortNameOffset = leftLen;
    }

    return finalName;
}

WCHAR * Parser::AllocateStringOfLength(ULONG length)
{TRACE_IT(30289);
    Assert(length > 0);
    ULONG totalBytes;
    if (ULongMult(length, sizeof(OLECHAR), &totalBytes) != S_OK)
    {TRACE_IT(30290);
        Error(ERRnoMemory);
    }
    WCHAR* finalName = (WCHAR*)m_phtbl->GetAllocator()->Alloc(totalBytes);
    if (finalName == nullptr)
    {TRACE_IT(30291);
        Error(ERRnoMemory);
    }
    return finalName;
}

LPCOLESTR Parser::AppendNameHints(IdentPtr left, IdentPtr right, uint32 *pNameLength, uint32 *pShortNameOffset, bool ignoreAddDotWithSpace, bool wrapInBrackets)
{TRACE_IT(30292);
    if (pShortNameOffset != nullptr)
    {TRACE_IT(30293);
        *pShortNameOffset = 0;
    }

    if (left == nullptr && !wrapInBrackets)
    {TRACE_IT(30294);
        if (right)
        {TRACE_IT(30295);
            *pNameLength = right->Cch();
            return right->Psz();
        }
        return nullptr;
    }

    uint32 leftLen = 0;
    LPCOLESTR leftStr = _u("");

    if (left != nullptr) // if wrapInBrackets is true
    {TRACE_IT(30296);
        leftStr = left->Psz();
        leftLen = left->Cch();
    }

    if (right == nullptr)
    {TRACE_IT(30297);
        *pNameLength = leftLen;
        return left->Psz();
    }
    uint32 rightLen = right->Cch();

    return AppendNameHints(leftStr, leftLen, right->Psz(), rightLen, pNameLength, pShortNameOffset, ignoreAddDotWithSpace, wrapInBrackets);
}

LPCOLESTR Parser::AppendNameHints(IdentPtr left, LPCOLESTR right, uint32 *pNameLength, uint32 *pShortNameOffset, bool ignoreAddDotWithSpace, bool wrapInBrackets)
{TRACE_IT(30298);
    uint32 rightLen = (right == nullptr) ? 0 : (uint32) wcslen(right);

    if (pShortNameOffset != nullptr)
    {TRACE_IT(30299);
        *pShortNameOffset = 0;
    }

    Assert(rightLen <= ULONG_MAX); // name hints should not exceed ULONG_MAX characters

    if (left == nullptr && !wrapInBrackets)
    {TRACE_IT(30300);
        *pNameLength = rightLen;
        return right;
    }

    LPCOLESTR leftStr = _u("");
    uint32 leftLen = 0;

    if (left != nullptr) // if wrapInBrackets is true
    {TRACE_IT(30301);
        leftStr = left->Psz();
        leftLen = left->Cch();
    }

    if (rightLen == 0 && !wrapInBrackets)
    {TRACE_IT(30302);
        *pNameLength = leftLen;
        return left->Psz();
    }

    return AppendNameHints(leftStr, leftLen, right, rightLen, pNameLength, pShortNameOffset, ignoreAddDotWithSpace, wrapInBrackets);
}

LPCOLESTR Parser::AppendNameHints(LPCOLESTR left, IdentPtr right, uint32 *pNameLength, uint32 *pShortNameOffset, bool ignoreAddDotWithSpace, bool wrapInBrackets)
{TRACE_IT(30303);
    uint32 leftLen = (left == nullptr) ? 0 : (uint32) wcslen(left);

    if (pShortNameOffset != nullptr)
    {TRACE_IT(30304);
        *pShortNameOffset = 0;
    }

    Assert(leftLen <= ULONG_MAX); // name hints should not exceed ULONG_MAX characters

    if (left == nullptr || (leftLen == 0 && !wrapInBrackets))
    {TRACE_IT(30305);
        if (right != nullptr)
        {TRACE_IT(30306);
            *pNameLength = right->Cch();
            return right->Psz();
        }
        return nullptr;
    }

    if (right == nullptr)
    {TRACE_IT(30307);
        *pNameLength = leftLen;
        return left;
    }
    uint32 rightLen = right->Cch();

    return AppendNameHints(left, leftLen, right->Psz(), rightLen, pNameLength, pShortNameOffset, ignoreAddDotWithSpace, wrapInBrackets);
}


LPCOLESTR Parser::AppendNameHints(LPCOLESTR left, LPCOLESTR right, uint32 *pNameLength, uint32 *pShortNameOffset, bool ignoreAddDotWithSpace, bool wrapInBrackets)
{TRACE_IT(30308);
    uint32 leftLen = (left == nullptr) ? 0 : (uint32) wcslen(left);
    uint32 rightLen = (right == nullptr) ? 0 : (uint32) wcslen(right);
    if (pShortNameOffset != nullptr)
    {TRACE_IT(30309);
        *pShortNameOffset = 0;
    }
    Assert(rightLen <= ULONG_MAX && leftLen <= ULONG_MAX); // name hints should not exceed ULONG_MAX characters

    if (leftLen == 0 && !wrapInBrackets)
    {TRACE_IT(30310);
        *pNameLength = right ? rightLen : 0;
        return right;
    }

    if (rightLen == 0 && !wrapInBrackets)
    {TRACE_IT(30311);
        *pNameLength = leftLen;
        return left;
    }

    return AppendNameHints(left, leftLen, right, rightLen, pNameLength, pShortNameOffset, ignoreAddDotWithSpace, wrapInBrackets);
}

/**
 * Emits a spread error if there is no ambiguity, or marks defers the error for
 * when we can determine if it is a rest error or a spread error.
 *
 * The ambiguity arises when we are parsing a lambda parameter list but we have
 * not seen the => token. At this point, we are either in a parenthesized
 * expression or a parameter list, and cannot issue an error until the matching
 * RParen has been scanned.
 *
 * The actual emission of the error happens in ParseExpr, when we first know if
 * the expression is a lambda parameter list or not.
 *
 */
void Parser::DeferOrEmitPotentialSpreadError(ParseNodePtr pnodeT)
{TRACE_IT(30312);
    if (m_parenDepth > 0)
    {TRACE_IT(30313);
        if (m_token.tk == tkRParen)
        {TRACE_IT(30314);
           if (!m_deferEllipsisError)
            {TRACE_IT(30315);
                // Capture only the first error instance.
                m_pscan->Capture(&m_EllipsisErrLoc);
                m_deferEllipsisError = true;
            }
        }
        else
        {TRACE_IT(30316);
            Error(ERRUnexpectedEllipsis);
        }
    }
    else
    {TRACE_IT(30317);
        Error(ERRInvalidSpreadUse);
    }
}

/***************************************************************************
Parse an optional sub expression returning null if there was no expression.
Checks for no expression by looking for a token that can follow an
Expression grammar production.
***************************************************************************/
template<bool buildAST>
bool Parser::ParseOptionalExpr(ParseNodePtr* pnode, bool fUnaryOrParen, int oplMin, BOOL *pfCanAssign, BOOL fAllowIn, BOOL fAllowEllipsis, _Inout_opt_ IdentToken* pToken)
{TRACE_IT(30318);
    *pnode = nullptr;
    if (m_token.tk == tkRCurly ||
        m_token.tk == tkRBrack ||
        m_token.tk == tkRParen ||
        m_token.tk == tkSColon ||
        m_token.tk == tkColon ||
        m_token.tk == tkComma ||
        m_token.tk == tkLimKwd ||
        m_pscan->FHadNewLine())
    {TRACE_IT(30319);
        return false;
    }

    IdentToken token;
    ParseNodePtr pnodeT = ParseExpr<buildAST>(oplMin, pfCanAssign, fAllowIn, fAllowEllipsis, nullptr /*pNameHint*/, nullptr /*pHintLength*/, nullptr /*pShortNameOffset*/, &token, fUnaryOrParen);
    // Detect nested function escapes of the pattern "return function(){...}" or "yield function(){...}".
    // Doing so in the parser allows us to disable stack-nested-functions in common cases where an escape
    // is not detected at byte code gen time because of deferred parsing.
    this->MarkEscapingRef(pnodeT, &token);
    if (pToken)
    {TRACE_IT(30320);
        *pToken = token;
    }
    *pnode = pnodeT;
    return true;
}

/***************************************************************************
Parse a sub expression.
'fAllowIn' indicates if the 'in' operator should be allowed in the initializing
expression ( it is not allowed in the context of the first expression in a  'for' loop).
***************************************************************************/
template<bool buildAST>
ParseNodePtr Parser::ParseExpr(int oplMin,
    BOOL *pfCanAssign,
    BOOL fAllowIn,
    BOOL fAllowEllipsis,
    LPCOLESTR pNameHint,
    uint32 *pHintLength,
    uint32 *pShortNameOffset,
    _Inout_opt_ IdentToken* pToken,
    bool fUnaryOrParen,
    _Inout_opt_ bool* pfLikelyPattern,
    _Inout_opt_ charcount_t *plastRParen)
{TRACE_IT(30321);
    Assert(pToken == nullptr || pToken->tk == tkNone); // Must be empty initially
    int opl;
    OpCode nop;
    charcount_t ichMin;
    ParseNodePtr pnode = nullptr;
    ParseNodePtr pnodeT = nullptr;
    BOOL fCanAssign = TRUE;
    bool assignmentStmt = false;
    bool fIsDotOrIndex = false;
    IdentToken term;
    RestorePoint termStart;
    uint32 hintLength = 0;
    uint32 hintOffset = 0;

    ParserState parserState;

    if (pHintLength != nullptr)
    {TRACE_IT(30322);
        hintLength = *pHintLength;
    }

    if (pShortNameOffset != nullptr)
    {TRACE_IT(30323);
        hintOffset = *pShortNameOffset;
    }

    EnsureStackAvailable();

    // Storing the state here as we need to restore this state back when we need to reparse the grammar under lambda syntax.
    CaptureState(&parserState);

    m_pscan->Capture(&termStart);

    bool deferredErrorFoundOnLeftSide = false;
    bool savedDeferredInitError = m_hasDeferredShorthandInitError;
    m_hasDeferredShorthandInitError = false;

    // Is the current token a unary operator?
    if (m_phtbl->TokIsUnop(m_token.tk, &opl, &nop) && nop != knopNone)
    {TRACE_IT(30324);
        IdentToken operandToken;
        ichMin = m_pscan->IchMinTok();

        if (nop == knopYield)
        {TRACE_IT(30325);
            if (!m_pscan->YieldIsKeyword() || oplMin > opl)
            {TRACE_IT(30326);
                // The case where 'yield' is scanned as a keyword (tkYIELD) but the scanner
                // is not treating yield as a keyword (!m_pscan->YieldIsKeyword()) occurs
                // in strict mode non-generator function contexts.
                //
                // That is, 'yield' is a keyword because of strict mode, but YieldExpression
                // is not a grammar production outside of generator functions.
                //
                // Otherwise it is an error for a yield to appear in the context of a higher level
                // binding operator, be it unary or binary.
                Error(ERRsyntax);
            }
            if (m_currentScope->GetScopeType() == ScopeType_Parameter)
            {TRACE_IT(30327);
                Error(ERRsyntax);
            }
        }
        else if (nop == knopAwait)
        {TRACE_IT(30328);
            if (!m_pscan->AwaitIsKeyword() ||
                m_currentScope->GetScopeType() == ScopeType_Parameter)
            {TRACE_IT(30329);
                // As with the 'yield' keyword, the case where 'await' is scanned as a keyword (tkAWAIT)
                // but the scanner is not treating await as a keyword (!m_pscan->AwaitIsKeyword())
                // occurs in strict mode non-async function contexts.
                //
                // That is, 'await' is a keyword because of strict mode, but AwaitExpression
                // is not a grammar production outside of async functions.
                //
                // Further, await expressions are disallowed within parameter scopes.
                Error(ERRBadAwait);
            }
        }

        m_pscan->Scan();

        if (m_token.tk == tkEllipsis) {TRACE_IT(30330);
            // ... cannot have a unary prefix.
            Error(ERRUnexpectedEllipsis);
        }

        if (nop == knopYield && !m_pscan->FHadNewLine() && m_token.tk == tkStar)
        {TRACE_IT(30331);
            m_pscan->Scan();
            nop = knopYieldStar;
        }

        if (nop == knopYield)
        {
            if (!ParseOptionalExpr<buildAST>(&pnodeT, false, opl, NULL, TRUE, fAllowEllipsis))
            {TRACE_IT(30332);
                nop = knopYieldLeaf;
                if (buildAST)
                {TRACE_IT(30333);
                    pnode = CreateNodeT<knopYieldLeaf>(ichMin, m_pscan->IchLimTok());
                }
            }
        }
        else
        {TRACE_IT(30334);
            // Disallow spread after a unary operator.
            pnodeT = ParseExpr<buildAST>(opl, &fCanAssign, TRUE, FALSE, nullptr /*hint*/, nullptr /*hintLength*/, nullptr /*hintOffset*/, &operandToken, true);
        }

        if (nop != knopYieldLeaf)
        {TRACE_IT(30335);
            if (nop == knopIncPre || nop == knopDecPre)
            {TRACE_IT(30336);
                if (!fCanAssign && PHASE_ON1(Js::EarlyReferenceErrorsPhase))
                {TRACE_IT(30337);
                    Error(JSERR_CantAssignTo);
                }
                TrackAssignment<buildAST>(pnodeT, &operandToken);
                if (buildAST)
                {TRACE_IT(30338);
                    if (IsStrictMode() && pnodeT->nop == knopName)
                    {TRACE_IT(30339);
                        CheckStrictModeEvalArgumentsUsage(pnodeT->sxPid.pid);
                    }
                }
                else
                {TRACE_IT(30340);
                    if (IsStrictMode() && operandToken.tk == tkID)
                    {TRACE_IT(30341);
                        CheckStrictModeEvalArgumentsUsage(operandToken.pid);
                    }
                }
            }
            else if (nop == knopEllipsis)
            {TRACE_IT(30342);
                if (!fAllowEllipsis)
                {TRACE_IT(30343);
                    DeferOrEmitPotentialSpreadError(pnodeT);
                }
            }
            else if (m_token.tk == tkExpo)
            {TRACE_IT(30344);
                //Unary operator on the left hand-side of ** is unexpected, except ++, -- or ...
                Error(ERRInvalidUseofExponentiationOperator);
            }

            if (buildAST)
            {TRACE_IT(30345);
                //Do not do the folding for Asm in case of KnopPos as we need this to determine the type
                if (nop == knopPos && (pnodeT->nop == knopInt || pnodeT->nop == knopFlt) && !this->m_InAsmMode)
                {TRACE_IT(30346);
                    // Fold away a unary '+' on a number.
                    pnode = pnodeT;
                }
                else if (nop == knopNeg &&
                    ((pnodeT->nop == knopInt && pnodeT->sxInt.lw != 0) ||
                    (pnodeT->nop == knopFlt && (pnodeT->sxFlt.dbl != 0 || this->m_InAsmMode))))
                {TRACE_IT(30347);
                    // Fold a unary '-' on a number into the value of the number itself.
                    pnode = pnodeT;
                    if (pnode->nop == knopInt)
                    {TRACE_IT(30348);
                        pnode->sxInt.lw = -pnode->sxInt.lw;
                    }
                    else
                    {TRACE_IT(30349);
                        pnode->sxFlt.dbl = -pnode->sxFlt.dbl;
                    }
                }
                else
                {TRACE_IT(30350);
                    pnode = CreateUniNode(nop, pnodeT);
                    this->CheckArguments(pnode->sxUni.pnode1);
                }
                pnode->ichMin = ichMin;
            }

            if (nop == knopDelete)
            {TRACE_IT(30351);
                if (IsStrictMode())
                {TRACE_IT(30352);
                    if ((buildAST && pnode->sxUni.pnode1->nop == knopName) ||
                        (!buildAST && operandToken.tk == tkID))
                    {TRACE_IT(30353);
                        Error(ERRInvalidDelete);
                    }
                }

                if (buildAST)
                {TRACE_IT(30354);
                    ParseNodePtr pnode1 = pnode->sxUni.pnode1;
                    if (m_currentNodeFunc)
                    {TRACE_IT(30355);
                        if (pnode1->nop == knopDot || pnode1->nop == knopIndex)
                        {TRACE_IT(30356);
                            // If we delete an arguments property, use the conservative,
                            // heap-allocated arguments object.
                            this->CheckArguments(pnode1->sxBin.pnode1);
                        }
                    }
                }
            }
        }

        fCanAssign = FALSE;
    }
    else
    {TRACE_IT(30357);
        ichMin = m_pscan->IchMinTok();
        BOOL fLikelyPattern = FALSE;
        pnode = ParseTerm<buildAST>(TRUE, pNameHint, &hintLength, &hintOffset, &term, fUnaryOrParen, &fCanAssign, IsES6DestructuringEnabled() ? &fLikelyPattern : nullptr, &fIsDotOrIndex, plastRParen);
        if (pfLikelyPattern != nullptr)
        {TRACE_IT(30358);
            *pfLikelyPattern = !!fLikelyPattern;
        }

        if (m_token.tk == tkDArrow)
        {TRACE_IT(30359);
            m_hasDeferredShorthandInitError = false;
        }

        if (m_token.tk == tkAsg && oplMin <= koplAsg && fLikelyPattern)
        {TRACE_IT(30360);
            m_pscan->SeekTo(termStart);

            // As we are reparsing from the beginning of the destructured literal we need to reset the Block IDs as well to make sure the Block IDs
            // on the pidref stack match.
            int saveNextBlockId = m_nextBlockId;
            m_nextBlockId = parserState.m_nextBlockId;

            ParseDestructuredLiteralWithScopeSave(tkLCurly, false/*isDecl*/, false /*topLevel*/, DIC_ShouldNotParseInitializer);

            // Restore the Block ID at the end of the reparsing so it matches the one at the end of the first pass. We need to do this 
            // because we don't parse initializers during reparse and there may be additional blocks (e.g. a class declaration)
            // in the initializers that will cause the next Block ID at the end of the reparsing to be different.
            m_nextBlockId = saveNextBlockId;

            if (buildAST)
            {TRACE_IT(30361);
                pnode = ConvertToPattern(pnode);
            }

            // The left-hand side is found to be destructuring pattern - so the shorthand can have initializer.
            m_hasDeferredShorthandInitError = false;
        }

        if (buildAST)
        {TRACE_IT(30362);
            pNameHint = NULL;
            if (pnode->nop == knopName)
            {TRACE_IT(30363);
                pNameHint = pnode->sxPid.pid->Psz();
                hintLength = pnode->sxPid.pid->Cch();
                hintOffset = 0;
            }
            else if (pnode->nop == knopDot || pnode->nop == knopIndex)
            {TRACE_IT(30364);
                if (CONFIG_FLAG(UseFullName))
                {TRACE_IT(30365);
                    pNameHint = ConstructNameHint(pnode, &hintLength, &hintOffset);
                }
                else
                {TRACE_IT(30366);
                    ParseNodePtr pnodeName = pnode;
                    while (pnodeName->nop == knopDot)
                    {TRACE_IT(30367);
                        pnodeName = pnodeName->sxBin.pnode2;
                    }

                    if (pnodeName->nop == knopName)
                    {TRACE_IT(30368);
                        pNameHint = pnodeName->sxPid.pid->Psz();
                        hintLength = pnodeName->sxPid.pid->Cch();
                        hintOffset = 0;
                    }
                }
            }
        }

        // Check for postfix unary operators.
        if (!m_pscan->FHadNewLine() &&
            (tkInc == m_token.tk || tkDec == m_token.tk))
        {TRACE_IT(30369);
            if (!fCanAssign && PHASE_ON1(Js::EarlyReferenceErrorsPhase))
            {TRACE_IT(30370);
                Error(JSERR_CantAssignTo);
            }
            TrackAssignment<buildAST>(pnode, &term);
            fCanAssign = FALSE;
            if (buildAST)
            {TRACE_IT(30371);
                if (IsStrictMode() && pnode->nop == knopName)
                {TRACE_IT(30372);
                    CheckStrictModeEvalArgumentsUsage(pnode->sxPid.pid);
                }
                this->CheckArguments(pnode);
                pnode = CreateUniNode(tkInc == m_token.tk ? knopIncPost : knopDecPost, pnode);
                pnode->ichLim = m_pscan->IchLimTok();
            }
            else
            {TRACE_IT(30373);
                if (IsStrictMode() && term.tk == tkID)
                {TRACE_IT(30374);
                    CheckStrictModeEvalArgumentsUsage(term.pid);
                }
                // This expression is not an identifier
                term.tk = tkNone;
            }
            m_pscan->Scan();
        }
    }

    deferredErrorFoundOnLeftSide = m_hasDeferredShorthandInitError;

    // Process a sequence of operators and operands.
    for (;;)
    {TRACE_IT(30375);
        if (!m_phtbl->TokIsBinop(m_token.tk, &opl, &nop) || nop == knopNone)
        {TRACE_IT(30376);
            break;
        }
        if ( ! fAllowIn && nop == knopIn )
        {TRACE_IT(30377);
            break;
        }
        Assert(opl != koplNo);

        if (opl == koplAsg)
        {TRACE_IT(30378);
            if (m_token.tk != tkDArrow)
            {TRACE_IT(30379);
                // Assignment operator. These are the only right associative
                // binary operators. We also need to special case the left
                // operand - it should only be a LeftHandSideExpression.
                Assert(ParseNode::Grfnop(nop) & fnopAsg || nop == knopFncDecl);
                TrackAssignment<buildAST>(pnode, &term);
                if (buildAST)
                {TRACE_IT(30380);
                    if (IsStrictMode() && pnode->nop == knopName)
                    {TRACE_IT(30381);
                        CheckStrictModeEvalArgumentsUsage(pnode->sxPid.pid);
                    }

                    // Assignment stmt of the form "this.<id> = <expr>"
                    if (nop == knopAsg && pnode->nop == knopDot && pnode->sxBin.pnode1->nop == knopThis && pnode->sxBin.pnode2->nop == knopName)
                    {TRACE_IT(30382);
                        if (pnode->sxBin.pnode2->sxPid.pid != wellKnownPropertyPids.__proto__)
                        {TRACE_IT(30383);
                            assignmentStmt = true;
                        }
                    }
                }
                else
                {TRACE_IT(30384);
                    if (IsStrictMode() && term.tk == tkID)
                    {TRACE_IT(30385);
                        CheckStrictModeEvalArgumentsUsage(term.pid);
                    }
                }
            }

            if (opl < oplMin)
            {TRACE_IT(30386);
                break;
            }
            if (m_token.tk != tkDArrow && !fCanAssign && PHASE_ON1(Js::EarlyReferenceErrorsPhase))
            {TRACE_IT(30387);
                Error(JSERR_CantAssignTo);
                // No recovery necessary since this is a semantic, not structural, error.
            }
        }
        else if (opl == koplExpo)
        {TRACE_IT(30388);
            // ** operator is right associative
            if (opl < oplMin)
            {TRACE_IT(30389);
                break;
            }

        }
        else if (opl <= oplMin)
        {TRACE_IT(30390);
            break;
        }

        // This expression is not an identifier
        term.tk = tkNone;

        // Precedence is high enough. Consume the operator token.
        m_pscan->Scan();
        fCanAssign = FALSE;

        // Special case the "?:" operator
        if (nop == knopQmark)
        {TRACE_IT(30391);
            pnodeT = ParseExpr<buildAST>(koplAsg, NULL, fAllowIn);
            ChkCurTok(tkColon, ERRnoColon);
            ParseNodePtr pnodeT2 = ParseExpr<buildAST>(koplAsg, NULL, fAllowIn);
            if (buildAST)
            {TRACE_IT(30392);
                pnode = CreateTriNode(nop, pnode, pnodeT, pnodeT2);
                this->CheckArguments(pnode->sxTri.pnode2);
                this->CheckArguments(pnode->sxTri.pnode3);
            }
        }
        else if (nop == knopFncDecl)
        {TRACE_IT(30393);
            ushort flags = fFncLambda;
            size_t iecpMin = 0;
            bool isAsyncMethod = false;

            RestoreStateFrom(&parserState);

            m_pscan->SeekTo(termStart);
            if (m_token.tk == tkID && m_token.GetIdentifier(m_phtbl) == wellKnownPropertyPids.async && m_scriptContext->GetConfig()->IsES7AsyncAndAwaitEnabled())
            {TRACE_IT(30394);
                ichMin = m_pscan->IchMinTok();
                iecpMin = m_pscan->IecpMinTok();

                m_pscan->Scan();
                if ((m_token.tk == tkID || m_token.tk == tkLParen) && !m_pscan->FHadNewLine())
                {TRACE_IT(30395);
                    flags |= fFncAsync;
                    isAsyncMethod = true;
                }
                else
                {TRACE_IT(30396);
                    m_pscan->SeekTo(termStart);
                }
            }
            pnode = ParseFncDecl<buildAST>(flags, nullptr, /* needsPIDOnRCurlyScan = */false, /* resetParsingSuperRestrictionState = */false);
            if (isAsyncMethod)
            {TRACE_IT(30397);
                pnode->sxFnc.cbMin = iecpMin;
                pnode->ichMin = ichMin;
            }
        }
        else
        {TRACE_IT(30398);
            // Parse the operand, make a new node, and look for more
            IdentToken token;
            pnodeT = ParseExpr<buildAST>(opl, NULL, fAllowIn, FALSE, pNameHint, &hintLength, &hintOffset, &token);

            // Detect nested function escapes of the pattern "o.f = function(){...}" or "o[s] = function(){...}".
            // Doing so in the parser allows us to disable stack-nested-functions in common cases where an escape
            // is not detected at byte code gen time because of deferred parsing.
            if (fIsDotOrIndex && nop == knopAsg)
            {TRACE_IT(30399);
                this->MarkEscapingRef(pnodeT, &token);
            }

            if (buildAST)
            {TRACE_IT(30400);
                pnode = CreateBinNode(nop, pnode, pnodeT);
                Assert(pnode->sxBin.pnode2 != NULL);
                if (pnode->sxBin.pnode2->nop == knopFncDecl)
                {TRACE_IT(30401);
                    Assert(hintLength >= hintOffset);
                    pnode->sxBin.pnode2->sxFnc.hint = pNameHint;
                    pnode->sxBin.pnode2->sxFnc.hintLength = hintLength;
                    pnode->sxBin.pnode2->sxFnc.hintOffset = hintOffset;

                    if (pnode->sxBin.pnode1->nop == knopDot)
                    {TRACE_IT(30402);
                        pnode->sxBin.pnode2->sxFnc.isNameIdentifierRef  = false;
                    }
                    else if (pnode->sxBin.pnode1->nop == knopName)
                    {TRACE_IT(30403);
                        PidRefStack *pidRef = pnode->sxBin.pnode1->sxPid.pid->GetTopRef();
                        pidRef->isFuncAssignment = true;
                    }
                }
                if (pnode->sxBin.pnode2->nop == knopClassDecl && pnode->sxBin.pnode1->nop == knopDot)
                {TRACE_IT(30404);
                    Assert(pnode->sxBin.pnode2->sxClass.pnodeConstructor);
                    pnode->sxBin.pnode2->sxClass.pnodeConstructor->sxFnc.isNameIdentifierRef  = false;
                }
            }
            pNameHint = NULL;
        }
    }

    if (buildAST)
    {TRACE_IT(30405);
        if (!assignmentStmt)
        {TRACE_IT(30406);
            // Don't set the flag for following nodes
            switch (pnode->nop)
            {
            case knopName:
            case knopInt:
            case knopFlt:
            case knopStr:
            case knopRegExp:
            case knopNull:
            case knopFalse:
            case knopTrue:
                break;
            default:
                if (m_currentNodeFunc)
                {TRACE_IT(30407);
                    m_currentNodeFunc->sxFnc.SetHasNonThisStmt();
                }
                else if (m_currentNodeProg)
                {TRACE_IT(30408);
                    m_currentNodeProg->sxFnc.SetHasNonThisStmt();
                }
            }
        }
    }

    if (m_hasDeferredShorthandInitError && !deferredErrorFoundOnLeftSide)
    {TRACE_IT(30409);
        // Raise error only if it is found not on the right side of the expression.
        // such as  <expr> = {x = 1}
        Error(ERRnoColon);
    }

    m_hasDeferredShorthandInitError = m_hasDeferredShorthandInitError || savedDeferredInitError;

    if (NULL != pfCanAssign)
    {TRACE_IT(30410);
        *pfCanAssign = fCanAssign;
    }

    // Pass back identifier if requested
    if (pToken && term.tk == tkID)
    {TRACE_IT(30411);
        *pToken = term;
    }

    //Track "obj.a" assignment patterns here - Promote the Assignment state for the property's PID.
    // This includes =, += etc.
    if (pnode != NULL)
    {TRACE_IT(30412);
        uint nodeType = ParseNode::Grfnop(pnode->nop);
        if (nodeType & fnopAsg)
        {TRACE_IT(30413);
            if (nodeType & fnopBin)
            {TRACE_IT(30414);
                ParseNodePtr lhs = pnode->sxBin.pnode1;

                Assert(lhs);
                if (lhs->nop == knopDot)
                {TRACE_IT(30415);
                    ParseNodePtr propertyNode = lhs->sxBin.pnode2;
                    if (propertyNode->nop == knopName)
                    {TRACE_IT(30416);
                        propertyNode->sxPid.pid->PromoteAssignmentState();
                    }
                }
            }
            else if (nodeType & fnopUni)
            {TRACE_IT(30417);
                // cases like obj.a++, ++obj.a
                ParseNodePtr lhs = pnode->sxUni.pnode1;
                if (lhs->nop == knopDot)
                {TRACE_IT(30418);
                    ParseNodePtr propertyNode = lhs->sxBin.pnode2;
                    if (propertyNode->nop == knopName)
                    {TRACE_IT(30419);
                        propertyNode->sxPid.pid->PromoteAssignmentState();
                    }
                }
            }
        }
    }
    return pnode;
}

template<bool buildAST>
void Parser::TrackAssignment(ParseNodePtr pnodeT, IdentToken* pToken)
{TRACE_IT(30420);
    if (buildAST)
    {TRACE_IT(30421);
        Assert(pnodeT != nullptr);
        if (pnodeT->nop == knopName)
        {TRACE_IT(30422);
            PidRefStack *ref = pnodeT->sxPid.pid->GetTopRef();
            Assert(ref);
            ref->isAsg = true;
        }
    }
    else
    {TRACE_IT(30423);
        Assert(pToken != nullptr);
        if (pToken->tk == tkID)
        {TRACE_IT(30424);
            PidRefStack *ref = pToken->pid->GetTopRef();
            Assert(ref);
            ref->isAsg = true;
        }
    }
}

void PnPid::SetSymRef(PidRefStack *ref)
{TRACE_IT(30425);
    Assert(symRef == nullptr);
    this->symRef = ref->GetSymRef();
}

Js::PropertyId PnPid::PropertyIdFromNameNode() const
{TRACE_IT(30426);
    Js::PropertyId propertyId;
    Symbol *sym = this->sym;
    if (sym)
    {TRACE_IT(30427);
        propertyId = sym->GetPosition();
    }
    else
    {TRACE_IT(30428);
        propertyId = this->pid->GetPropertyId();
    }
    return propertyId;
}

PidRefStack* Parser::PushPidRef(IdentPtr pid)
{TRACE_IT(30429);
    if (PHASE_ON1(Js::ParallelParsePhase))
    {TRACE_IT(30430);
        // NOTE: the phase check is here to protect perf. See OSG 1020424.
        // In some LS AST-rewrite cases we lose a lot of perf searching the PID ref stack rather
        // than just pushing on the top. This hasn't shown up as a perf issue in non-LS benchmarks.
        return pid->FindOrAddPidRef(&m_nodeAllocator, GetCurrentBlock()->sxBlock.blockId, GetCurrentFunctionNode()->sxFnc.functionId);
    }

    Assert(GetCurrentBlock() != nullptr);
    AssertMsg(pid != nullptr, "PID should be created");
    PidRefStack *ref = pid->GetTopRef();
    int blockId = GetCurrentBlock()->sxBlock.blockId;
    int funcId = GetCurrentFunctionNode()->sxFnc.functionId;
    if (!ref || (ref->GetScopeId() < blockId))
    {TRACE_IT(30431);
        ref = Anew(&m_nodeAllocator, PidRefStack);
        if (ref == nullptr)
        {TRACE_IT(30432);
            Error(ERRnoMemory);
        }
        pid->PushPidRef(blockId, funcId, ref);
    }
    else if (m_reparsingLambdaParams)
    {TRACE_IT(30433);
        // If we're reparsing params, then we may have pid refs left behind from the first pass. Make sure we're
        // working with the right ref at this point.
        ref = this->FindOrAddPidRef(pid, blockId, funcId);
        // Fix up the function ID if we're reparsing lambda parameters.
        ref->funcId = funcId;
    }

    return ref;
}

PidRefStack* Parser::FindOrAddPidRef(IdentPtr pid, int scopeId, Js::LocalFunctionId funcId)
{TRACE_IT(30434);
    PidRefStack *ref = pid->FindOrAddPidRef(&m_nodeAllocator, scopeId, funcId);
    if (ref == NULL)
    {TRACE_IT(30435);
        Error(ERRnoMemory);
    }
    return ref;
}

void Parser::RemovePrevPidRef(IdentPtr pid, PidRefStack *ref)
{TRACE_IT(30436);
    PidRefStack *prevRef = pid->RemovePrevPidRef(ref);
    Assert(prevRef);
    if (prevRef->GetSym() == nullptr)
    {
        AllocatorDelete(ArenaAllocator, &m_nodeAllocator, prevRef);
    }
}

void Parser::SetPidRefsInScopeDynamic(IdentPtr pid, int blockId)
{TRACE_IT(30437);
    PidRefStack *ref = pid->GetTopRef();
    while (ref && ref->GetScopeId() >= blockId)
    {TRACE_IT(30438);
        ref->SetDynamicBinding();
        ref = ref->prev;
    }
}

ParseNode* Parser::GetFunctionBlock()
{TRACE_IT(30439);
    Assert(m_currentBlockInfo != nullptr);
    return m_currentBlockInfo->pBlockInfoFunction->pnodeBlock;
}


ParseNode* Parser::GetCurrentBlock()
{TRACE_IT(30440);
    return m_currentBlockInfo != nullptr ? m_currentBlockInfo->pnodeBlock : nullptr;
}

BlockInfoStack* Parser::GetCurrentBlockInfo()
{TRACE_IT(30441);
    return m_currentBlockInfo;
}

BlockInfoStack* Parser::GetCurrentFunctionBlockInfo()
{TRACE_IT(30442);
    return m_currentBlockInfo->pBlockInfoFunction;
}

/***************************************************************************
Parse a variable declaration.
'fAllowIn' indicates if the 'in' operator should be allowed in the initializing
expression ( it is not allowed in the context of the first expression in a  'for' loop).
***************************************************************************/
template<bool buildAST>
ParseNodePtr Parser::ParseVariableDeclaration(
    tokens declarationType, charcount_t ichMin,
    BOOL fAllowIn/* = TRUE*/,
    BOOL* pfForInOk/* = nullptr*/,
    BOOL singleDefOnly/* = FALSE*/,
    BOOL allowInit/* = TRUE*/,
    BOOL isTopVarParse/* = TRUE*/,
    BOOL isFor/* = FALSE*/,
    BOOL* nativeForOk /*= nullptr*/)
{TRACE_IT(30443);
    ParseNodePtr pnodeThis = nullptr;
    ParseNodePtr pnodeInit;
    ParseNodePtr pnodeList = nullptr;
    ParseNodePtr *lastNodeRef = nullptr;
    LPCOLESTR pNameHint = nullptr;
    uint32     nameHintLength = 0;
    uint32     nameHintOffset = 0;
    Assert(declarationType == tkVAR || declarationType == tkCONST || declarationType == tkLET);

    for (;;)
    {TRACE_IT(30444);
        if (IsES6DestructuringEnabled() && IsPossiblePatternStart())
        {TRACE_IT(30445);
            pnodeThis = ParseDestructuredLiteral<buildAST>(declarationType, true, !!isTopVarParse, DIC_None, !!fAllowIn, pfForInOk, nativeForOk);
            if (pnodeThis != nullptr)
            {TRACE_IT(30446);
                pnodeThis->ichMin = ichMin;
            }
        }
        else
        {TRACE_IT(30447);
            if (m_token.tk != tkID)
            {TRACE_IT(30448);
                IdentifierExpectedError(m_token);
            }

            IdentPtr pid = m_token.GetIdentifier(m_phtbl);
            Assert(pid);
            pNameHint = pid->Psz();
            nameHintLength = pid->Cch();
            nameHintOffset = 0;

            if (pid == wellKnownPropertyPids.let && (declarationType == tkCONST || declarationType == tkLET))
            {
                Error(ERRLetIDInLexicalDecl, pnodeThis);
            }

            if (declarationType == tkVAR)
            {TRACE_IT(30449);
                pnodeThis = CreateVarDeclNode(pid, STVariable);
            }
            else if (declarationType == tkCONST)
            {TRACE_IT(30450);
                pnodeThis = CreateBlockScopedDeclNode(pid, knopConstDecl);
                CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(Const, m_scriptContext);
            }
            else
            {TRACE_IT(30451);
                pnodeThis = CreateBlockScopedDeclNode(pid, knopLetDecl);
                CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(Let, m_scriptContext);
            }

            if (pid == wellKnownPropertyPids.arguments && m_currentNodeFunc)
            {TRACE_IT(30452);
                // This var declaration may change the way an 'arguments' identifier in the function is resolved
                if (declarationType == tkVAR)
                {TRACE_IT(30453);
                    m_currentNodeFunc->grfpn |= PNodeFlags::fpnArguments_varDeclaration;
                }
                else
                {TRACE_IT(30454);
                    if (GetCurrentBlockInfo()->pnodeBlock->sxBlock.blockType == Function)
                    {TRACE_IT(30455);
                        // Only override arguments if we are at the function block level.
                        m_currentNodeFunc->grfpn |= PNodeFlags::fpnArguments_overriddenByDecl;
                    }
                }
            }

            if (pnodeThis)
            {TRACE_IT(30456);
                pnodeThis->ichMin = ichMin;
            }

            m_pscan->Scan();

            if (m_token.tk == tkAsg)
            {TRACE_IT(30457);
                if (!allowInit)
                {TRACE_IT(30458);
                    Error(ERRUnexpectedDefault);
                }
                if (pfForInOk && (declarationType == tkLET || declarationType == tkCONST || IsStrictMode()))
                {TRACE_IT(30459);
                    *pfForInOk = FALSE;
                }

                m_pscan->Scan();
                pnodeInit = ParseExpr<buildAST>(koplCma, nullptr, fAllowIn, FALSE, pNameHint, &nameHintLength, &nameHintOffset);
                if (buildAST)
                {TRACE_IT(30460);
                    AnalysisAssert(pnodeThis);
                    pnodeThis->sxVar.pnodeInit = pnodeInit;
                    pnodeThis->ichLim = pnodeInit->ichLim;

                    if (pnodeInit->nop == knopFncDecl)
                    {TRACE_IT(30461);
                        Assert(nameHintLength >= nameHintOffset);
                        pnodeInit->sxFnc.hint = pNameHint;
                        pnodeInit->sxFnc.hintLength = nameHintLength;
                        pnodeInit->sxFnc.hintOffset = nameHintOffset;
                        pnodeThis->sxVar.pid->GetTopRef()->isFuncAssignment = true;
                    }
                    else
                    {TRACE_IT(30462);
                        this->CheckArguments(pnodeInit);
                    }
                    pNameHint = nullptr;
                }

                //Track var a =, let a= , const a =
                // This is for FixedFields Constant Heuristics
                if (pnodeThis && pnodeThis->sxVar.pnodeInit != nullptr)
                {TRACE_IT(30463);
                    pnodeThis->sxVar.sym->PromoteAssignmentState();
                    if (m_currentNodeFunc && pnodeThis->sxVar.sym->GetIsFormal())
                    {TRACE_IT(30464);
                        m_currentNodeFunc->sxFnc.SetHasAnyWriteToFormals(true);
                    }
                }
            }
            else if (declarationType == tkCONST /*pnodeThis->nop == knopConstDecl*/
                     && !singleDefOnly
                     && !(isFor && TokIsForInOrForOf()))
            {TRACE_IT(30465);
                Error(ERRUninitializedConst);
            }
        }

        if (singleDefOnly)
        {TRACE_IT(30466);
            return pnodeThis;
        }

        if (buildAST)
        {
            AddToNodeListEscapedUse(&pnodeList, &lastNodeRef, pnodeThis);
        }

        if (m_token.tk != tkComma)
        {TRACE_IT(30467);
            return pnodeList;
        }

        if (pfForInOk)
        {TRACE_IT(30468);
            // don't allow "for (var a, b in c)"
            *pfForInOk = FALSE;
        }
        m_pscan->Scan();
        ichMin = m_pscan->IchMinTok();
    }
}

/***************************************************************************
Parse try-catch-finally statement
***************************************************************************/

// The try-catch-finally tree nests the try-catch within a try-finally.
// This matches the new runtime implementation.
template<bool buildAST>
ParseNodePtr Parser::ParseTryCatchFinally()
{TRACE_IT(30469);
    this->m_tryCatchOrFinallyDepth++;

    ParseNodePtr pnodeT = ParseTry<buildAST>();
    ParseNodePtr pnodeTC = nullptr;
    StmtNest stmt;
    bool hasCatch = false;

    if (tkCATCH == m_token.tk)
    {TRACE_IT(30470);
        hasCatch = true;
        if (buildAST)
        {TRACE_IT(30471);
            pnodeTC = CreateNodeWithScanner<knopTryCatch>();
            pnodeT->sxStmt.pnodeOuter = pnodeTC;
            pnodeTC->sxTryCatch.pnodeTry = pnodeT;
        }
        PushStmt<buildAST>(&stmt, pnodeTC, knopTryCatch, nullptr, nullptr);

        ParseNodePtr pnodeCatch = ParseCatch<buildAST>();
        if (buildAST)
        {TRACE_IT(30472);
            pnodeTC->sxTryCatch.pnodeCatch = pnodeCatch;
        }
        PopStmt(&stmt);
    }
    if (tkFINALLY != m_token.tk)
    {TRACE_IT(30473);
        if (!hasCatch)
        {TRACE_IT(30474);
            Error(ERRnoCatch);
        }
        Assert(!buildAST || pnodeTC);
        return pnodeTC;
    }

    ParseNodePtr pnodeTF = nullptr;
    if (buildAST)
    {TRACE_IT(30475);
        pnodeTF = CreateNode(knopTryFinally);
    }
    PushStmt<buildAST>(&stmt, pnodeTF, knopTryFinally, nullptr, nullptr);
    ParseNodePtr pnodeFinally = ParseFinally<buildAST>();
    if (buildAST)
    {TRACE_IT(30476);
        if (!hasCatch)
        {TRACE_IT(30477);
            pnodeTF->sxTryFinally.pnodeTry = pnodeT;
            pnodeT->sxStmt.pnodeOuter = pnodeTF;
        }
        else
        {TRACE_IT(30478);
            pnodeTF->sxTryFinally.pnodeTry = CreateNode(knopTry);
            pnodeTF->sxTryFinally.pnodeTry->sxStmt.pnodeOuter = pnodeTF;
            pnodeTF->sxTryFinally.pnodeTry->sxTry.pnodeBody = pnodeTC;
            pnodeTC->sxStmt.pnodeOuter = pnodeTF->sxTryFinally.pnodeTry;
        }
        pnodeTF->sxTryFinally.pnodeFinally = pnodeFinally;
    }
    PopStmt(&stmt);
    this->m_tryCatchOrFinallyDepth--;
    return pnodeTF;
}

template<bool buildAST>
ParseNodePtr Parser::ParseTry()
{TRACE_IT(30479);
    ParseNodePtr pnode = nullptr;
    StmtNest stmt;
    Assert(tkTRY == m_token.tk);
    if (buildAST)
    {TRACE_IT(30480);
        pnode = CreateNode(knopTry);
    }
    m_pscan->Scan();
    if (tkLCurly != m_token.tk)
    {TRACE_IT(30481);
        Error(ERRnoLcurly);
    }

    PushStmt<buildAST>(&stmt, pnode, knopTry, nullptr, nullptr);
    ParseNodePtr pnodeBody = ParseStatement<buildAST>();
    if (buildAST)
    {TRACE_IT(30482);
        pnode->sxTry.pnodeBody = pnodeBody;
        if (pnode->sxTry.pnodeBody)
            pnode->ichLim = pnode->sxTry.pnodeBody->ichLim;
    }
    PopStmt(&stmt);
    return pnode;
}

template<bool buildAST>
ParseNodePtr Parser::ParseFinally()
{TRACE_IT(30483);
    ParseNodePtr pnode = nullptr;
    StmtNest stmt;
    Assert(tkFINALLY == m_token.tk);
    if (buildAST)
    {TRACE_IT(30484);
        pnode = CreateNode(knopFinally);
    }
    m_pscan->Scan();
    if (tkLCurly != m_token.tk)
    {TRACE_IT(30485);
        Error(ERRnoLcurly);
    }

    PushStmt<buildAST>(&stmt, pnode, knopFinally, nullptr, nullptr);
    ParseNodePtr pnodeBody = ParseStatement<buildAST>();
    if (buildAST)
    {TRACE_IT(30486);
        pnode->sxFinally.pnodeBody = pnodeBody;
        if (!pnode->sxFinally.pnodeBody)
            // Will only occur due to error correction.
            pnode->sxFinally.pnodeBody = CreateNodeWithScanner<knopEmpty>();
        else
            pnode->ichLim = pnode->sxFinally.pnodeBody->ichLim;
    }
    PopStmt(&stmt);

    return pnode;
}

template<bool buildAST>
ParseNodePtr Parser::ParseCatch()
{TRACE_IT(30487);
    ParseNodePtr rootNode = nullptr;
    ParseNodePtr* ppnode = &rootNode;
    ParseNodePtr *ppnodeExprScopeSave = nullptr;
    ParseNodePtr pnode = nullptr;
    ParseNodePtr pnodeCatchScope = nullptr;
    StmtNest stmt;
    IdentPtr pidCatch = nullptr;
    //while (tkCATCH == m_token.tk)
    if (tkCATCH == m_token.tk)
    {TRACE_IT(30488);
        charcount_t ichMin;
        if (buildAST)
        {TRACE_IT(30489);
            ichMin = m_pscan->IchMinTok();
        }
        m_pscan->Scan(); //catch
        ChkCurTok(tkLParen, ERRnoLparen); //catch(

        bool isPattern = false;
        if (tkID != m_token.tk)
        {TRACE_IT(30490);
            isPattern = IsES6DestructuringEnabled() && IsPossiblePatternStart();
            if (!isPattern)
            {TRACE_IT(30491);
                IdentifierExpectedError(m_token);
            }
        }

        if (buildAST)
        {TRACE_IT(30492);
            pnode = CreateNodeWithScanner<knopCatch>(ichMin);
            PushStmt<buildAST>(&stmt, pnode, knopCatch, nullptr, nullptr);
            *ppnode = pnode;
            ppnode = &pnode->sxCatch.pnodeNext;
            *ppnode = nullptr;
        }

        pnodeCatchScope = StartParseBlock<buildAST>(PnodeBlockType::Regular, isPattern ? ScopeType_CatchParamPattern : ScopeType_Catch);

        if (buildAST)
        {TRACE_IT(30493);
            // Add this catch to the current scope list.

            if (m_ppnodeExprScope)
            {TRACE_IT(30494);
                Assert(*m_ppnodeExprScope == nullptr);
                *m_ppnodeExprScope = pnode;
                m_ppnodeExprScope = &pnode->sxCatch.pnodeNext;
            }
            else
            {TRACE_IT(30495);
                Assert(m_ppnodeScope);
                Assert(*m_ppnodeScope == nullptr);
                *m_ppnodeScope = pnode;
                m_ppnodeScope = &pnode->sxCatch.pnodeNext;
            }

            // Keep a list of function expressions (not declarations) at this scope.

            ppnodeExprScopeSave = m_ppnodeExprScope;
            m_ppnodeExprScope = &pnode->sxCatch.pnodeScopes;
            pnode->sxCatch.pnodeScopes = nullptr;
        }

        if (isPattern)
        {TRACE_IT(30496);
            ParseNodePtr pnodePattern = ParseDestructuredLiteral<buildAST>(tkLET, true /*isDecl*/, true /*topLevel*/, DIC_ForceErrorOnInitializer);
            if (buildAST)
            {TRACE_IT(30497);
                pnode->sxCatch.pnodeParam = CreateParamPatternNode(pnodePattern);
                Scope *scope = pnodeCatchScope->sxBlock.scope;
                pnode->sxCatch.scope = scope;
            }
        }
        else
        {TRACE_IT(30498);
            if (IsStrictMode())
            {TRACE_IT(30499);
                IdentPtr pid = m_token.GetIdentifier(m_phtbl);
                if (pid == wellKnownPropertyPids.eval)
                {TRACE_IT(30500);
                    Error(ERREvalUsage);
                }
                else if (pid == wellKnownPropertyPids.arguments)
                {TRACE_IT(30501);
                    Error(ERRArgsUsage);
                }
            }

            pidCatch = m_token.GetIdentifier(m_phtbl);
            PidRefStack *ref = this->PushPidRef(pidCatch);

            ParseNodePtr pnodeParam = CreateNameNode(pidCatch);
            pnodeParam->sxPid.symRef = ref->GetSymRef();

            const char16 *name = reinterpret_cast<const char16*>(pidCatch->Psz());
            int nameLength = pidCatch->Cch();
            SymbolName const symName(name, nameLength);
            Symbol *sym = Anew(&m_nodeAllocator, Symbol, symName, pnodeParam, STVariable);
            sym->SetPid(pidCatch);
            if (sym == nullptr)
            {TRACE_IT(30502);
                Error(ERRnoMemory);
            }
            Assert(ref->GetSym() == nullptr);
            ref->SetSym(sym);

            Scope *scope = pnodeCatchScope->sxBlock.scope;
            scope->AddNewSymbol(sym);

            if (buildAST)
            {TRACE_IT(30503);
                pnode->sxCatch.pnodeParam = pnodeParam;
                pnode->sxCatch.scope = scope;
            }

            m_pscan->Scan();
        }

        charcount_t ichLim;
        if (buildAST)
        {TRACE_IT(30504);
            ichLim = m_pscan->IchLimTok();
        }
        ChkCurTok(tkRParen, ERRnoRparen); //catch(id[:expr])

        if (tkLCurly != m_token.tk)
        {TRACE_IT(30505);
            Error(ERRnoLcurly);
        }

        ParseNodePtr pnodeBody = ParseStatement<buildAST>();  //catch(id[:expr]) {block}
        if (buildAST)
        {TRACE_IT(30506);
            pnode->sxCatch.pnodeBody = pnodeBody;
            pnode->ichLim = ichLim;
        }

        if (pnodeCatchScope != nullptr)
        {TRACE_IT(30507);
            FinishParseBlock(pnodeCatchScope);
        }

        if (buildAST)
        {TRACE_IT(30508);
            PopStmt(&stmt);

            // Restore the lists of function expression scopes.

            AssertMem(m_ppnodeExprScope);
            Assert(*m_ppnodeExprScope == nullptr);
            m_ppnodeExprScope = ppnodeExprScopeSave;
        }
    }
    return rootNode;
}

template<bool buildAST>
ParseNodePtr Parser::ParseCase(ParseNodePtr *ppnodeBody)
{TRACE_IT(30509);
    ParseNodePtr pnodeT = nullptr;

    charcount_t ichMinT = m_pscan->IchMinTok();
    m_pscan->Scan();
    ParseNodePtr pnodeExpr = ParseExpr<buildAST>();
    charcount_t ichLim = m_pscan->IchLimTok();

    ChkCurTok(tkColon, ERRnoColon);

    if (buildAST)
    {TRACE_IT(30510);
        pnodeT = CreateNodeWithScanner<knopCase>(ichMinT);
        pnodeT->sxCase.pnodeExpr = pnodeExpr;
        pnodeT->ichLim = ichLim;
    }
    ParseStmtList<buildAST>(ppnodeBody);

    return pnodeT;
}

/***************************************************************************
Parse a single statement. Digest a trailing semicolon.
***************************************************************************/
template<bool buildAST>
ParseNodePtr Parser::ParseStatement()
{TRACE_IT(30511);
    ParseNodePtr *ppnodeT;
    ParseNodePtr pnodeT;
    ParseNodePtr pnode = nullptr;
    LabelId* pLabelIdList = nullptr;
    charcount_t ichMin = 0;
    size_t iecpMin = 0;
    StmtNest stmt;
    StmtNest *pstmt;
    BOOL fForInOrOfOkay;
    BOOL fCanAssign;
    IdentPtr pid;
    uint fnop;
    ParseNodePtr pnodeLabel = nullptr;
    bool expressionStmt = false;
    bool isAsyncMethod = false;
    tokens tok;
#if EXCEPTION_RECOVERY
    ParseNodePtr pParentTryCatch = nullptr;
    ParseNodePtr pTryBlock = nullptr;
    ParseNodePtr pTry = nullptr;
    ParseNodePtr pParentTryCatchBlock = nullptr;

    StmtNest stmtTryCatchBlock;
    StmtNest stmtTryCatch;
    StmtNest stmtTry;
    StmtNest stmtTryBlock;
#endif

    if (buildAST)
    {TRACE_IT(30512);
#if EXCEPTION_RECOVERY
        if(Js::Configuration::Global.flags.SwallowExceptions)
        {TRACE_IT(30513);
            // If we're swallowing exceptions, surround this statement with a try/catch block:
            //
            //   Before: x.y = 3;
            //   After:  try { x.y = 3; } catch(__ehobj) { }
            //
            // This is done to force the runtime to recover from exceptions at the most granular
            // possible point.  Recovering from EH dramatically improves coverage of testing via
            // fault injection.


            // create and push the try-catch node
            pParentTryCatchBlock = CreateBlockNode();
            PushStmt<buildAST>(&stmtTryCatchBlock, pParentTryCatchBlock, knopBlock, nullptr, nullptr);
            pParentTryCatch = CreateNodeWithScanner<knopTryCatch>();
            PushStmt<buildAST>(&stmtTryCatch, pParentTryCatch, knopTryCatch, nullptr, nullptr);

            // create and push a try node
            pTry = CreateNodeWithScanner<knopTry>();
            PushStmt<buildAST>(&stmtTry, pTry, knopTry, nullptr, nullptr);
            pTryBlock = CreateBlockNode();
            PushStmt<buildAST>(&stmtTryBlock, pTryBlock, knopBlock, nullptr, nullptr);
            // these nodes will be closed after the statement is parsed.
        }
#endif // EXCEPTION_RECOVERY
    }

    EnsureStackAvailable();

LRestart:
    tok = m_token.tk;

    switch (tok)
    {
    case tkEOF:
        if (buildAST)
        {TRACE_IT(30514);
            pnode = nullptr;
        }
        break;

    case tkFUNCTION:
    {TRACE_IT(30515);
LFunctionStatement:
        if (m_grfscr & fscrDeferredFncExpression)
        {TRACE_IT(30516);
            // The top-level deferred function body was defined by a function expression whose parsing was deferred. We are now
            // parsing it, so unset the flag so that any nested functions are parsed normally. This flag is only applicable the
            // first time we see it.
            m_grfscr &= ~fscrDeferredFncExpression;
            pnode = ParseFncDecl<buildAST>(isAsyncMethod ? fFncAsync : fFncNoFlgs, nullptr);
        }
        else
        {TRACE_IT(30517);
            pnode = ParseFncDecl<buildAST>(fFncDeclaration | (isAsyncMethod ? fFncAsync : fFncNoFlgs), nullptr);
        }
        if (isAsyncMethod)
        {TRACE_IT(30518);
            pnode->sxFnc.cbMin = iecpMin;
            pnode->ichMin = ichMin;
        }
        break;
    }

    case tkCLASS:
        if (m_scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled())
        {TRACE_IT(30519);
            pnode = ParseClassDecl<buildAST>(TRUE, nullptr, nullptr, nullptr);
        }
        else
        {TRACE_IT(30520);
            goto LDefaultToken;
        }
        break;

    case tkID:
        if (m_token.GetIdentifier(m_phtbl) == wellKnownPropertyPids.let)
        {TRACE_IT(30521);
            // We see "let" at the start of a statement. This could either be a declaration or an identifier
            // reference. The next token determines which.
            RestorePoint parsedLet;
            m_pscan->Capture(&parsedLet);
            ichMin = m_pscan->IchMinTok();

            m_pscan->Scan();
            if (this->NextTokenConfirmsLetDecl())
            {TRACE_IT(30522);
                pnode = ParseVariableDeclaration<buildAST>(tkLET, ichMin);
                goto LNeedTerminator;
            }
            m_pscan->SeekTo(parsedLet);
        }
        else if (m_token.GetIdentifier(m_phtbl) == wellKnownPropertyPids.async && m_scriptContext->GetConfig()->IsES7AsyncAndAwaitEnabled())
        {TRACE_IT(30523);
            RestorePoint parsedAsync;
            m_pscan->Capture(&parsedAsync);
            ichMin = m_pscan->IchMinTok();
            iecpMin = m_pscan->IecpMinTok();

            m_pscan->Scan();
            if (m_token.tk == tkFUNCTION && !m_pscan->FHadNewLine())
            {TRACE_IT(30524);
                isAsyncMethod = true;
                goto LFunctionStatement;
            }
            m_pscan->SeekTo(parsedAsync);
        }
        goto LDefaultToken;

    case tkCONST:
    case tkLET:
        ichMin = m_pscan->IchMinTok();

        m_pscan->Scan();
        pnode = ParseVariableDeclaration<buildAST>(tok, ichMin);
        goto LNeedTerminator;

    case tkVAR:
        ichMin = m_pscan->IchMinTok();

        m_pscan->Scan();
        pnode = ParseVariableDeclaration<buildAST>(tok, ichMin);
        goto LNeedTerminator;

    case tkFOR:
    {TRACE_IT(30525);
        ParseNodePtr pnodeBlock = nullptr;
        ParseNodePtr *ppnodeScopeSave = nullptr;
        ParseNodePtr *ppnodeExprScopeSave = nullptr;

        ichMin = m_pscan->IchMinTok();
        ChkNxtTok(tkLParen, ERRnoLparen);
        pnodeBlock = StartParseBlock<buildAST>(PnodeBlockType::Regular, ScopeType_Block);
        if (buildAST)
        {
            PushFuncBlockScope(pnodeBlock, &ppnodeScopeSave, &ppnodeExprScopeSave);
        }

        RestorePoint startExprOrIdentifier;
        fForInOrOfOkay = TRUE;
        fCanAssign = TRUE;
        tok = m_token.tk;
        BOOL nativeForOkay = TRUE;

        switch (tok)
        {
        case tkID:
            if (m_token.GetIdentifier(m_phtbl) == wellKnownPropertyPids.let)
            {TRACE_IT(30526);
                // We see "let" in the init part of a for loop. This could either be a declaration or an identifier
                // reference. The next token determines which.
                RestorePoint parsedLet;
                m_pscan->Capture(&parsedLet);
                auto ichMinInner = m_pscan->IchMinTok();

                m_pscan->Scan();
                if (IsPossiblePatternStart())
                {TRACE_IT(30527);
                    m_pscan->Capture(&startExprOrIdentifier);
                }
                if (this->NextTokenConfirmsLetDecl() && m_token.tk != tkIN)
                {TRACE_IT(30528);
                    pnodeT = ParseVariableDeclaration<buildAST>(tkLET, ichMinInner
                                                                , /*fAllowIn = */FALSE
                                                                , /*pfForInOk = */&fForInOrOfOkay
                                                                , /*singleDefOnly*/FALSE
                                                                , /*allowInit*/TRUE
                                                                , /*isTopVarParse*/TRUE
                                                                , /*isFor*/TRUE
                                                                , &nativeForOkay);
                    break;
                }
                m_pscan->SeekTo(parsedLet);
            }
            goto LDefaultTokenFor;
        case tkLET:
        case tkCONST:
        case tkVAR:
            {TRACE_IT(30529);
                auto ichMinInner = m_pscan->IchMinTok();

                m_pscan->Scan();
                if (IsPossiblePatternStart())
                {TRACE_IT(30530);
                    m_pscan->Capture(&startExprOrIdentifier);
                }
                pnodeT = ParseVariableDeclaration<buildAST>(tok, ichMinInner
                                                            , /*fAllowIn = */FALSE
                                                            , /*pfForInOk = */&fForInOrOfOkay
                                                            , /*singleDefOnly*/FALSE
                                                            , /*allowInit*/TRUE
                                                            , /*isTopVarParse*/TRUE
                                                            , /*isFor*/TRUE
                                                            , &nativeForOkay);
            }
            break;
        case tkSColon:
            pnodeT = nullptr;
            fForInOrOfOkay = FALSE;
            break;
        default:
            {TRACE_IT(30531);
LDefaultTokenFor:
                RestorePoint exprStart;
                tokens beforeToken = tok;
                m_pscan->Capture(&exprStart);
                if (IsPossiblePatternStart())
                {TRACE_IT(30532);
                    m_pscan->Capture(&startExprOrIdentifier);
                }
                bool fLikelyPattern = false;
                if (IsES6DestructuringEnabled() && (beforeToken == tkLBrack || beforeToken == tkLCurly))
                {TRACE_IT(30533);
                    pnodeT = ParseExpr<buildAST>(koplNo,
                        &fCanAssign,
                        /*fAllowIn = */FALSE,
                        /*fAllowEllipsis*/FALSE,
                        /*pHint*/nullptr,
                        /*pHintLength*/nullptr,
                        /*pShortNameOffset*/nullptr,
                        /*pToken*/nullptr,
                        /**fUnaryOrParen*/false,
                        &fLikelyPattern);
                }
                else
                {TRACE_IT(30534);
                    pnodeT = ParseExpr<buildAST>(koplNo, &fCanAssign, /*fAllowIn = */FALSE);
                }

                // We would veryfiy the grammar as destructuring grammar only when  for..in/of case. As in the native for loop case the above ParseExpr call
                // has already converted them appropriately.
                if (fLikelyPattern && TokIsForInOrForOf())
                {TRACE_IT(30535);
                    m_pscan->SeekTo(exprStart);
                    ParseDestructuredLiteralWithScopeSave(tkNone, false/*isDecl*/, false /*topLevel*/, DIC_None, false /*allowIn*/);

                    if (buildAST)
                    {TRACE_IT(30536);
                        pnodeT = ConvertToPattern(pnodeT);
                    }
                }
                if (buildAST)
                {TRACE_IT(30537);
                    Assert(pnodeT);
                    pnodeT->isUsed = false;
                }
            }
            break;
        }

        if (TokIsForInOrForOf())
        {TRACE_IT(30538);
            bool isForOf = (m_token.tk != tkIN);
            Assert(!isForOf || (m_token.tk == tkID && m_token.GetIdentifier(m_phtbl) == wellKnownPropertyPids.of));

            if ((buildAST && nullptr == pnodeT) || !fForInOrOfOkay)
            {TRACE_IT(30539);
                if (isForOf)
                {TRACE_IT(30540);
                    Error(ERRForOfNoInitAllowed);
                }
                else
                {TRACE_IT(30541);
                    Error(ERRForInNoInitAllowed);
                }
            }
            if (!fCanAssign && PHASE_ON1(Js::EarlyReferenceErrorsPhase))
            {TRACE_IT(30542);
                Error(JSERR_CantAssignTo);
            }

            m_pscan->Scan();
            ParseNodePtr pnodeObj = ParseExpr<buildAST>(isForOf ? koplCma : koplNo);
            charcount_t ichLim = m_pscan->IchLimTok();
            ChkCurTok(tkRParen, ERRnoRparen);

            if (buildAST)
            {TRACE_IT(30543);
                if (isForOf)
                {TRACE_IT(30544);
                    pnode = CreateNodeWithScanner<knopForOf>(ichMin);
                }
                else
                {TRACE_IT(30545);
                    pnode = CreateNodeWithScanner<knopForIn>(ichMin);
                }
                pnode->sxForInOrForOf.pnodeBlock = pnodeBlock;
                pnode->sxForInOrForOf.pnodeLval = pnodeT;
                pnode->sxForInOrForOf.pnodeObj = pnodeObj;
                pnode->ichLim = ichLim;

                TrackAssignment<true>(pnodeT, nullptr);
            }
            PushStmt<buildAST>(&stmt, pnode, isForOf ? knopForOf : knopForIn, pnodeLabel, pLabelIdList);
            ParseNodePtr pnodeBody = ParseStatement<buildAST>();

            if (buildAST)
            {TRACE_IT(30546);
                pnode->sxForInOrForOf.pnodeBody = pnodeBody;
            }
            PopStmt(&stmt);
        }
        else
        {TRACE_IT(30547);
            if (!nativeForOkay)
            {TRACE_IT(30548);
                Error(ERRDestructInit);
            }

            ChkCurTok(tkSColon, ERRnoSemic);
            ParseNodePtr pnodeCond = nullptr;
            if (m_token.tk != tkSColon)
            {TRACE_IT(30549);
                pnodeCond = ParseExpr<buildAST>();
                if (m_token.tk != tkSColon)
                {TRACE_IT(30550);
                    Error(ERRnoSemic);
                }
            }

            tokens tk;
            tk = m_pscan->Scan();

            ParseNodePtr pnodeIncr = nullptr;
            if (tk != tkRParen)
            {TRACE_IT(30551);
                pnodeIncr = ParseExpr<buildAST>();
                if(pnodeIncr)
                {TRACE_IT(30552);
                    pnodeIncr->isUsed = false;
                }
            }

            charcount_t ichLim = m_pscan->IchLimTok();

            ChkCurTok(tkRParen, ERRnoRparen);

            if (buildAST)
            {TRACE_IT(30553);
                pnode = CreateNodeWithScanner<knopFor>(ichMin);
                pnode->sxFor.pnodeBlock = pnodeBlock;
                pnode->sxFor.pnodeInverted= nullptr;
                pnode->sxFor.pnodeInit = pnodeT;
                pnode->sxFor.pnodeCond = pnodeCond;
                pnode->sxFor.pnodeIncr = pnodeIncr;
                pnode->ichLim = ichLim;
            }
            PushStmt<buildAST>(&stmt, pnode, knopFor, pnodeLabel, pLabelIdList);
            ParseNodePtr pnodeBody = ParseStatement<buildAST>();
            if (buildAST)
            {TRACE_IT(30554);
                pnode->sxFor.pnodeBody = pnodeBody;
            }
            PopStmt(&stmt);
        }

        if (buildAST)
        {
            PopFuncBlockScope(ppnodeScopeSave, ppnodeExprScopeSave);
        }

        FinishParseBlock(pnodeBlock);

        break;
    }

    case tkSWITCH:
    {TRACE_IT(30555);
        BOOL fSeenDefault = FALSE;
        ParseNodePtr pnodeBlock = nullptr;
        ParseNodePtr *ppnodeScopeSave = nullptr;
        ParseNodePtr *ppnodeExprScopeSave = nullptr;

        ichMin = m_pscan->IchMinTok();
        ChkNxtTok(tkLParen, ERRnoLparen);
        ParseNodePtr pnodeVal = ParseExpr<buildAST>();
        charcount_t ichLim = m_pscan->IchLimTok();

        ChkCurTok(tkRParen, ERRnoRparen);
        ChkCurTok(tkLCurly, ERRnoLcurly);

        if (buildAST)
        {TRACE_IT(30556);
            pnode = CreateNodeWithScanner<knopSwitch>(ichMin);
        }
        PushStmt<buildAST>(&stmt, pnode, knopSwitch, pnodeLabel, pLabelIdList);
        pnodeBlock = StartParseBlock<buildAST>(PnodeBlockType::Regular, ScopeType_Block, nullptr, pLabelIdList);

        if (buildAST)
        {TRACE_IT(30557);
            pnode->sxSwitch.pnodeVal = pnodeVal;
            pnode->sxSwitch.pnodeBlock = pnodeBlock;
            pnode->ichLim = ichLim;
            PushFuncBlockScope(pnode->sxSwitch.pnodeBlock, &ppnodeScopeSave, &ppnodeExprScopeSave);

            pnode->sxSwitch.pnodeDefault = nullptr;
            ppnodeT = &pnode->sxSwitch.pnodeCases;
        }

        for (;;)
        {TRACE_IT(30558);
            ParseNodePtr pnodeBody = nullptr;
            switch (m_token.tk)
            {
            default:
                goto LEndSwitch;
            case tkCASE:
            {TRACE_IT(30559);
                pnodeT = this->ParseCase<buildAST>(&pnodeBody);
                break;
            }
            case tkDEFAULT:
                if (fSeenDefault)
                {TRACE_IT(30560);
                    Error(ERRdupDefault);
                    // No recovery necessary since this is a semantic, not structural, error
                }
                fSeenDefault = TRUE;
                charcount_t ichMinT = m_pscan->IchMinTok();
                m_pscan->Scan();
                charcount_t ichMinInner = m_pscan->IchLimTok();
                ChkCurTok(tkColon, ERRnoColon);
                if (buildAST)
                {TRACE_IT(30561);
                    pnodeT = CreateNodeWithScanner<knopCase>(ichMinT);
                    pnode->sxSwitch.pnodeDefault = pnodeT;
                    pnodeT->ichLim = ichMinInner;
                    pnodeT->sxCase.pnodeExpr = nullptr;
                }
                ParseStmtList<buildAST>(&pnodeBody);
                break;
            }
            if (buildAST)
            {TRACE_IT(30562);
                if (pnodeBody)
                {TRACE_IT(30563);
                    // Create a block node to contain the statement list for this case.
                    // This helps us insert byte code to return the right value from
                    // global/eval code.
                    pnodeT->sxCase.pnodeBody = CreateBlockNode(pnodeT->ichMin, pnodeT->ichLim);
                    pnodeT->sxCase.pnodeBody->grfpn |= PNodeFlags::fpnSyntheticNode; // block is not a user specifier block
                    pnodeT->sxCase.pnodeBody->sxBlock.pnodeStmt = pnodeBody;
                }
                else
                {TRACE_IT(30564);
                    pnodeT->sxCase.pnodeBody = nullptr;
                }
                *ppnodeT = pnodeT;
                ppnodeT = &pnodeT->sxCase.pnodeNext;
            }
        }
LEndSwitch:
        ChkCurTok(tkRCurly, ERRnoRcurly);
        if (buildAST)
        {TRACE_IT(30565);
            *ppnodeT = nullptr;
            PopFuncBlockScope(ppnodeScopeSave, ppnodeExprScopeSave);
            FinishParseBlock(pnode->sxSwitch.pnodeBlock);
        }
        else
        {TRACE_IT(30566);
            FinishParseBlock(pnodeBlock);
        }
        PopStmt(&stmt);

        break;
    }

    case tkWHILE:
    {TRACE_IT(30567);
        ichMin = m_pscan->IchMinTok();
        ChkNxtTok(tkLParen, ERRnoLparen);
        ParseNodePtr pnodeCond = ParseExpr<buildAST>();
        charcount_t ichLim = m_pscan->IchLimTok();
        ChkCurTok(tkRParen, ERRnoRparen);

        if (buildAST)
        {TRACE_IT(30568);
            pnode = CreateNodeWithScanner<knopWhile>(ichMin);
            pnode->sxWhile.pnodeCond = pnodeCond;
            pnode->ichLim = ichLim;
        }
        PushStmt<buildAST>(&stmt, pnode, knopWhile, pnodeLabel, pLabelIdList);
        ParseNodePtr pnodeBody = ParseStatement<buildAST>();
        PopStmt(&stmt);

        if (buildAST)
        {TRACE_IT(30569);
            pnode->sxWhile.pnodeBody = pnodeBody;
        }
        break;
    }

    case tkDO:
    {TRACE_IT(30570);
        if (buildAST)
        {TRACE_IT(30571);
            pnode = CreateNodeWithScanner<knopDoWhile>();
        }
        PushStmt<buildAST>(&stmt, pnode, knopDoWhile, pnodeLabel, pLabelIdList);
        m_pscan->Scan();
        ParseNodePtr pnodeBody = ParseStatement<buildAST>();
        PopStmt(&stmt);
        charcount_t ichMinT = m_pscan->IchMinTok();

        ChkCurTok(tkWHILE, ERRnoWhile);
        ChkCurTok(tkLParen, ERRnoLparen);

        ParseNodePtr pnodeCond = ParseExpr<buildAST>();
        charcount_t ichLim = m_pscan->IchLimTok();
        ChkCurTok(tkRParen, ERRnoRparen);

        if (buildAST)
        {TRACE_IT(30572);
            pnode->sxWhile.pnodeBody = pnodeBody;
            pnode->sxWhile.pnodeCond = pnodeCond;
            pnode->ichLim = ichLim;
            pnode->ichMin = ichMinT;
        }

        // REVIEW: Allow do...while statements to be embedded in other compound statements like if..else, or do..while?
        //      goto LNeedTerminator;

        // For now just eat the trailing semicolon if present.
        if (m_token.tk == tkSColon)
        {TRACE_IT(30573);
            if (pnode)
            {TRACE_IT(30574);
                pnode->grfpn |= PNodeFlags::fpnExplicitSemicolon;
            }
            m_pscan->Scan();
        }
        else if (pnode)
        {TRACE_IT(30575);
            pnode->grfpn |= PNodeFlags::fpnAutomaticSemicolon;
        }

        break;
    }

    case tkIF:
    {TRACE_IT(30576);
        ichMin = m_pscan->IchMinTok();
        ChkNxtTok(tkLParen, ERRnoLparen);
        ParseNodePtr pnodeCond = ParseExpr<buildAST>();
        if (buildAST)
        {TRACE_IT(30577);
            pnode = CreateNodeWithScanner<knopIf>(ichMin);
            pnode->ichLim = m_pscan->IchLimTok();
            pnode->sxIf.pnodeCond = pnodeCond;
        }
        ChkCurTok(tkRParen, ERRnoRparen);

        PushStmt<buildAST>(&stmt, pnode, knopIf, pnodeLabel, pLabelIdList);
        ParseNodePtr pnodeTrue = ParseStatement<buildAST>();
        ParseNodePtr pnodeFalse = nullptr;
        if (m_token.tk == tkELSE)
        {TRACE_IT(30578);
            m_pscan->Scan();
            pnodeFalse = ParseStatement<buildAST>();
        }
        if (buildAST)
        {TRACE_IT(30579);
            pnode->sxIf.pnodeTrue = pnodeTrue;
            pnode->sxIf.pnodeFalse = pnodeFalse;
        }
        PopStmt(&stmt);
        break;
    }

    case tkTRY:
    {TRACE_IT(30580);
        if (buildAST)
        {TRACE_IT(30581);
            pnode = CreateBlockNode();
            pnode->grfpn |= PNodeFlags::fpnSyntheticNode; // block is not a user specifier block
        }
        PushStmt<buildAST>(&stmt, pnode, knopBlock, pnodeLabel, pLabelIdList);
        ParseNodePtr pnodeStmt = ParseTryCatchFinally<buildAST>();
        if (buildAST)
        {TRACE_IT(30582);
            pnode->sxBlock.pnodeStmt = pnodeStmt;
        }
        PopStmt(&stmt);
        break;
    }

    case tkWITH:
    {TRACE_IT(30583);
        if ( IsStrictMode() )
        {TRACE_IT(30584);
            Error(ERRES5NoWith);
        }
        if (m_currentNodeFunc)
        {TRACE_IT(30585);
            GetCurrentFunctionNode()->sxFnc.SetHasWithStmt(); // Used by DeferNested
        }

        ichMin = m_pscan->IchMinTok();
        ChkNxtTok(tkLParen, ERRnoLparen);
        ParseNodePtr pnodeObj = ParseExpr<buildAST>();
        if (!buildAST)
        {TRACE_IT(30586);
            m_scopeCountNoAst++;
        }
        charcount_t ichLim = m_pscan->IchLimTok();
        ChkCurTok(tkRParen, ERRnoRparen);

        if (buildAST)
        {TRACE_IT(30587);
            pnode = CreateNodeWithScanner<knopWith>(ichMin);
        }
        PushStmt<buildAST>(&stmt, pnode, knopWith, pnodeLabel, pLabelIdList);

        ParseNodePtr *ppnodeExprScopeSave = nullptr;
        if (buildAST)
        {TRACE_IT(30588);
            pnode->sxWith.pnodeObj = pnodeObj;
            this->CheckArguments(pnode->sxWith.pnodeObj);

            if (m_ppnodeExprScope)
            {TRACE_IT(30589);
                Assert(*m_ppnodeExprScope == nullptr);
                *m_ppnodeExprScope = pnode;
                m_ppnodeExprScope = &pnode->sxWith.pnodeNext;
            }
            else
            {TRACE_IT(30590);
                Assert(m_ppnodeScope);
                Assert(*m_ppnodeScope == nullptr);
                *m_ppnodeScope = pnode;
                m_ppnodeScope = &pnode->sxWith.pnodeNext;
            }
            pnode->sxWith.pnodeNext = nullptr;
            pnode->sxWith.scope = nullptr;

            ppnodeExprScopeSave = m_ppnodeExprScope;
            m_ppnodeExprScope = &pnode->sxWith.pnodeScopes;
            pnode->sxWith.pnodeScopes = nullptr;

            pnode->ichLim = ichLim;
        }

        PushBlockInfo(CreateBlockNode());
        PushDynamicBlock();

        ParseNodePtr pnodeBody = ParseStatement<buildAST>();
        if (buildAST)
        {TRACE_IT(30591);
            pnode->sxWith.pnodeBody = pnodeBody;
            m_ppnodeExprScope = ppnodeExprScopeSave;
        }
        else
        {TRACE_IT(30592);
            m_scopeCountNoAst--;
        }

        // The dynamic block is not stored in the actual parse tree and so will not
        // be visited by the byte code generator.  Grab the callsEval flag off it and
        // pass on to outer block in case of:
        // with (...) eval(...); // i.e. blockless form of with
        bool callsEval = GetCurrentBlock()->sxBlock.GetCallsEval();
        PopBlockInfo();
        if (callsEval)
        {TRACE_IT(30593);
            // be careful not to overwrite an existing true with false
            GetCurrentBlock()->sxBlock.SetCallsEval(true);
        }

        PopStmt(&stmt);
        break;
    }

    case tkLCurly:
        pnode = ParseBlock<buildAST>(pnodeLabel, pLabelIdList);
        break;

    case tkSColon:
        pnode = nullptr;
        m_pscan->Scan();
        break;

    case tkBREAK:
        if (buildAST)
        {TRACE_IT(30594);
            pnode = CreateNodeWithScanner<knopBreak>();
        }
        fnop = fnopBreak;
        goto LGetJumpStatement;

    case tkCONTINUE:
        if (buildAST)
        {TRACE_IT(30595);
            pnode = CreateNode(knopContinue);
        }
        fnop = fnopContinue;

LGetJumpStatement:
        m_pscan->ScanForcingPid();
        if (tkID == m_token.tk && !m_pscan->FHadNewLine())
        {TRACE_IT(30596);
            // Labeled break or continue.
            pid = m_token.GetIdentifier(m_phtbl);
            AssertMem(pid);
            if (buildAST)
            {TRACE_IT(30597);
                pnode->sxJump.hasExplicitTarget=true;
                pnode->ichLim = m_pscan->IchLimTok();

                m_pscan->Scan();
                PushStmt<buildAST>(&stmt, pnode, pnode->nop, pnodeLabel, nullptr);
                Assert(pnode->sxStmt.grfnop == 0);
                for (pstmt = m_pstmtCur; nullptr != pstmt; pstmt = pstmt->pstmtOuter)
                {TRACE_IT(30598);
                    AssertNodeMem(pstmt->pnodeStmt);
                    AssertNodeMemN(pstmt->pnodeLab);
                    for (pnodeT = pstmt->pnodeLab; nullptr != pnodeT;
                         pnodeT = pnodeT->sxLabel.pnodeNext)
                    {TRACE_IT(30599);
                        Assert(knopLabel == pnodeT->nop);
                        if (pid == pnodeT->sxLabel.pid)
                        {TRACE_IT(30600);
                            // Found the label. Make sure we can use it. We can
                            // break out of any statement, but we can only
                            // continue loops.
                            if (fnop == fnopContinue &&
                                !(pstmt->pnodeStmt->Grfnop() & fnop))
                            {TRACE_IT(30601);
                                Error(ERRbadContinue);
                            }
                            else
                            {TRACE_IT(30602);
                                pstmt->pnodeStmt->sxStmt.grfnop |= fnop;
                                pnode->sxJump.pnodeTarget = pstmt->pnodeStmt;
                            }
                            PopStmt(&stmt);
                            goto LNeedTerminator;
                        }
                    }
                    pnode->sxStmt.grfnop |=
                        (pstmt->pnodeStmt->Grfnop() & fnopCleanup);
                }
            }
            else
            {TRACE_IT(30603);
                m_pscan->Scan();
                for (pstmt = m_pstmtCur; pstmt; pstmt = pstmt->pstmtOuter)
                {TRACE_IT(30604);
                    LabelId* pLabelId;
                    for (pLabelId = pstmt->pLabelId; pLabelId; pLabelId = pLabelId->next)
                    {TRACE_IT(30605);

                        if (pid == pLabelId->pid)
                        {TRACE_IT(30606);
                            // Found the label. Make sure we can use it. We can
                            // break out of any statement, but we can only
                            // continue loops.
                            if (fnop == fnopContinue &&
                                !(ParseNode::Grfnop(pstmt->op) & fnop))
                            {TRACE_IT(30607);
                                Error(ERRbadContinue);
                            }
                            goto LNeedTerminator;
                        }
                    }
                }
            }
            Error(ERRnoLabel);
        }
        else
        {TRACE_IT(30608);
            // If we're doing a fast scan, we're not tracking labels, so we can't accurately do this analysis.
            // Let the thread that's doing the full parse detect the error, if there is one.
            if (!this->m_doingFastScan)
            {TRACE_IT(30609);
                // Unlabeled break or continue.
                if (buildAST)
                {TRACE_IT(30610);
                    pnode->sxJump.hasExplicitTarget=false;
                    PushStmt<buildAST>(&stmt, pnode, pnode->nop, pnodeLabel, nullptr);
                    Assert(pnode->sxStmt.grfnop == 0);
                }

                for (pstmt = m_pstmtCur; nullptr != pstmt; pstmt = pstmt->pstmtOuter)
                {TRACE_IT(30611);
                    if (buildAST)
                    {TRACE_IT(30612);
                        AnalysisAssert(pstmt->pnodeStmt);
                        if (pstmt->pnodeStmt->Grfnop() & fnop)
                        {TRACE_IT(30613);
                            pstmt->pnodeStmt->sxStmt.grfnop |= fnop;
                            pnode->sxJump.pnodeTarget = pstmt->pnodeStmt;
                            PopStmt(&stmt);
                            goto LNeedTerminator;
                        }
                        pnode->sxStmt.grfnop |=
                            (pstmt->pnodeStmt->Grfnop() & fnopCleanup);
                    }
                    else
                    {TRACE_IT(30614);
                        if (ParseNode::Grfnop(pstmt->GetNop()) & fnop)
                        {TRACE_IT(30615);
                            if (!pstmt->isDeferred)
                            {TRACE_IT(30616);
                                AnalysisAssert(pstmt->pnodeStmt);
                                pstmt->pnodeStmt->sxStmt.grfnop |= fnop;
                            }
                            goto LNeedTerminator;
                        }
                    }
                }
                Error(fnop == fnopBreak ? ERRbadBreak : ERRbadContinue);
            }
            goto LNeedTerminator;
        }

    case tkRETURN:
    {TRACE_IT(30617);
        if (buildAST)
        {TRACE_IT(30618);
            if (nullptr == m_currentNodeFunc)
            {TRACE_IT(30619);
                Error(ERRbadReturn);
            }
            pnode = CreateNodeWithScanner<knopReturn>();
        }
        m_pscan->Scan();
        ParseNodePtr pnodeExpr = nullptr;
        ParseOptionalExpr<buildAST>(&pnodeExpr, true);
        if (buildAST)
        {TRACE_IT(30620);
            pnode->sxReturn.pnodeExpr = pnodeExpr;
            if (pnodeExpr)
            {TRACE_IT(30621);
                this->CheckArguments(pnode->sxReturn.pnodeExpr);
                pnode->ichLim = pnode->sxReturn.pnodeExpr->ichLim;
            }
            // See if return should call finally
            PushStmt<buildAST>(&stmt, pnode, knopReturn, pnodeLabel, nullptr);
            Assert(pnode->sxStmt.grfnop == 0);
            for (pstmt = m_pstmtCur; nullptr != pstmt; pstmt = pstmt->pstmtOuter)
            {TRACE_IT(30622);
                AssertNodeMem(pstmt->pnodeStmt);
                AssertNodeMemN(pstmt->pnodeLab);
                if (pstmt->pnodeStmt->Grfnop() & fnopCleanup)
                {TRACE_IT(30623);
                    pnode->sxStmt.grfnop |= fnopCleanup;
                    break;
                }
            }
            PopStmt(&stmt);
        }
        goto LNeedTerminator;
    }

    case tkTHROW:
    {TRACE_IT(30624);
        if (buildAST)
        {TRACE_IT(30625);
            pnode = CreateUniNode(knopThrow, nullptr);
        }
        m_pscan->Scan();
        ParseNodePtr pnode1 = nullptr;
        if (m_token.tk != tkSColon &&
            m_token.tk != tkRCurly &&
            !m_pscan->FHadNewLine())
        {TRACE_IT(30626);
            pnode1 = ParseExpr<buildAST>();
        }
        else
        {TRACE_IT(30627);
            Error(ERRdanglingThrow);
        }

        if (buildAST)
        {TRACE_IT(30628);
            pnode->sxUni.pnode1 = pnode1;
            if (pnode1)
            {TRACE_IT(30629);
                this->CheckArguments(pnode->sxUni.pnode1);
                pnode->ichLim = pnode->sxUni.pnode1->ichLim;
            }
        }
        goto LNeedTerminator;
    }

    case tkDEBUGGER:
        if (buildAST)
        {TRACE_IT(30630);
            pnode = CreateNodeWithScanner<knopDebugger>();
        }
        m_pscan->Scan();
        goto LNeedTerminator;

    case tkIMPORT:
        if (!(m_grfscr & fscrIsModuleCode))
        {TRACE_IT(30631);
            goto LDefaultToken;
        }

        pnode = ParseImportDeclaration<buildAST>();

        goto LNeedTerminator;

    case tkEXPORT:
        if (!(m_grfscr & fscrIsModuleCode))
        {TRACE_IT(30632);
            goto LDefaultToken;
        }

        pnode = ParseExportDeclaration<buildAST>();

        goto LNeedTerminator;

LDefaultToken:
    default:
    {
        // First check for a label via lookahead. If not found,
        // rewind and reparse as expression statement.
        if (m_token.tk == tkLParen || m_token.tk == tkID)
        {TRACE_IT(30633);
            RestorePoint idStart;
            m_pscan->Capture(&idStart);

            // Support legacy behavior of allowing parentheses around label identifiers.
            // Require balanced parentheses for correcting parsing.  Note unbalanced cases
            // take care of themselves correctly by resulting in rewind and parsing as
            // an expression statement.
            // REVIEW[ianhall]: Can this legacy functionality be removed? Chrome does not support this parsing behavior.
            uint parenCount = 0;
            while (m_token.tk == tkLParen)
            {TRACE_IT(30634);
                parenCount += 1;
                m_pscan->Scan();
            }

            if (m_token.tk == tkID)
            {TRACE_IT(30635);
                IdentToken tokInner;
                tokInner.tk = tkID;
                tokInner.ichMin = m_pscan->IchMinTok();
                tokInner.ichLim = m_pscan->IchLimTok();
                tokInner.pid = m_token.GetIdentifier(m_phtbl);

                m_pscan->Scan();

                while (parenCount > 0 && m_token.tk == tkRParen)
                {TRACE_IT(30636);
                    parenCount -= 1;
                    m_pscan->Scan();
                }

                if (parenCount == 0 && m_token.tk == tkColon)
                {TRACE_IT(30637);
                    // We have a label.
                    // TODO[ianhall]: Refactor to eliminate separate code paths for buildAST and !buildAST
                    if (buildAST)
                    {TRACE_IT(30638);
                        // See if the label is already defined.
                        if (nullptr != PnodeLabel(tokInner.pid, pnodeLabel))
                        {TRACE_IT(30639);
                            Error(ERRbadLabel);
                        }
                        pnodeT = CreateNodeWithScanner<knopLabel>();
                        pnodeT->sxLabel.pid = tokInner.pid;
                        pnodeT->sxLabel.pnodeNext = pnodeLabel;
                        pnodeLabel = pnodeT;
                    }
                    else
                    {
                        // See if the label is already defined.
                        if (PnodeLabelNoAST(&tokInner, pLabelIdList))
                        {TRACE_IT(30640);
                            Error(ERRbadLabel);
                        }
                        LabelId* pLabelId = CreateLabelId(&tokInner);
                        pLabelId->next = pLabelIdList;
                        pLabelIdList = pLabelId;
                    }
                    m_pscan->Scan();
                    goto LRestart;
                }
            }

            // No label, rewind back to the tkID and parse an expression
            m_pscan->SeekTo(idStart);
        }

        // Must be an expression statement.
        pnode = ParseExpr<buildAST>();

        if (m_hasDeferredShorthandInitError)
        {TRACE_IT(30641);
            Error(ERRnoColon);
        }

        if (buildAST)
        {TRACE_IT(30642);
            expressionStmt = true;

            AnalysisAssert(pnode);
            pnode->isUsed = false;
        }
    }

LNeedTerminator:
        // Need a semicolon, new-line, } or end-of-file.
        // We digest a semicolon if it's there.
        switch (m_token.tk)
        {
        case tkSColon:
            m_pscan->Scan();
            if (pnode!= nullptr) pnode->grfpn |= PNodeFlags::fpnExplicitSemicolon;
            break;
        case tkEOF:
        case tkRCurly:
            if (pnode!= nullptr) pnode->grfpn |= PNodeFlags::fpnAutomaticSemicolon;
            break;
        default:
            if (!m_pscan->FHadNewLine())
            {TRACE_IT(30643);
                Error(ERRnoSemic);
            }
            else
            {TRACE_IT(30644);
                if (pnode!= nullptr) pnode->grfpn |= PNodeFlags::fpnAutomaticSemicolon;
            }
            break;
        }
        break;
    }

    if (m_hasDeferredShorthandInitError)
    {TRACE_IT(30645);
        Error(ERRnoColon);
    }

    if (buildAST)
    {TRACE_IT(30646);
        // All non expression statements excluded from the "this.x" optimization
        // Another check while parsing expressions
        if (!expressionStmt)
        {TRACE_IT(30647);
            if (m_currentNodeFunc)
            {TRACE_IT(30648);
                m_currentNodeFunc->sxFnc.SetHasNonThisStmt();
            }
            else if (m_currentNodeProg)
            {TRACE_IT(30649);
                m_currentNodeProg->sxFnc.SetHasNonThisStmt();
            }
        }

#if EXCEPTION_RECOVERY
        // close the try/catch block
        if(Js::Configuration::Global.flags.SwallowExceptions)
        {TRACE_IT(30650);
            // pop the try block and fill in the body
            PopStmt(&stmtTryBlock);
            pTryBlock->sxBlock.pnodeStmt = pnode;
            PopStmt(&stmtTry);
            if(pnode != nullptr)
            {TRACE_IT(30651);
                pTry->ichLim = pnode->ichLim;
            }
            pTry->sxTry.pnodeBody = pTryBlock;


            // create a catch block with an empty body
            StmtNest stmtCatch;
            ParseNodePtr pCatch;
            pCatch = CreateNodeWithScanner<knopCatch>();
            PushStmt<buildAST>(&stmtCatch, pCatch, knopCatch, nullptr, nullptr);
            pCatch->sxCatch.pnodeBody = nullptr;
            if(pnode != nullptr)
            {TRACE_IT(30652);
                pCatch->ichLim = pnode->ichLim;
            }
            pCatch->sxCatch.grfnop = 0;
            pCatch->sxCatch.pnodeNext = nullptr;

            // create a fake name for the catch var.
            const WCHAR *uniqueNameStr = _u("__ehobj");
            IdentPtr uniqueName = m_phtbl->PidHashNameLen(uniqueNameStr, static_cast<int32>(wcslen(uniqueNameStr)));

            pCatch->sxCatch.pnodeParam = CreateNameNode(uniqueName);

            // Add this catch to the current list. We don't bother adjusting the catch and function expression
            // lists here because the catch is just an empty statement.

            if (m_ppnodeExprScope)
            {TRACE_IT(30653);
                Assert(*m_ppnodeExprScope == nullptr);
                *m_ppnodeExprScope = pCatch;
                m_ppnodeExprScope = &pCatch->sxCatch.pnodeNext;
            }
            else
            {TRACE_IT(30654);
                Assert(m_ppnodeScope);
                Assert(*m_ppnodeScope == nullptr);
                *m_ppnodeScope = pCatch;
                m_ppnodeScope = &pCatch->sxCatch.pnodeNext;
            }

            pCatch->sxCatch.pnodeScopes = nullptr;

            PopStmt(&stmtCatch);

            // fill in and pop the try-catch
            pParentTryCatch->sxTryCatch.pnodeTry = pTry;
            pParentTryCatch->sxTryCatch.pnodeCatch = pCatch;
            PopStmt(&stmtTryCatch);
            PopStmt(&stmtTryCatchBlock);

            // replace the node that's being returned
            pParentTryCatchBlock->sxBlock.pnodeStmt = pParentTryCatch;
            pnode = pParentTryCatchBlock;
        }
#endif // EXCEPTION_RECOVERY

    }

    return pnode;
}

BOOL
Parser::TokIsForInOrForOf()
{TRACE_IT(30655);
    return m_token.tk == tkIN ||
        (m_token.tk == tkID &&
         m_token.GetIdentifier(m_phtbl) == wellKnownPropertyPids.of);
}

/***************************************************************************
Parse a sequence of statements.
***************************************************************************/
template<bool buildAST>
void Parser::ParseStmtList(ParseNodePtr *ppnodeList, ParseNodePtr **pppnodeLast, StrictModeEnvironment smEnvironment, const bool isSourceElementList, bool* strictModeOn)
{TRACE_IT(30656);
    BOOL doneDirectives = !isSourceElementList; // directives may only exist in a SourceElementList, not a StatementList
    BOOL seenDirectiveContainingOctal = false; // Have we seen an octal directive before a use strict directive?

    BOOL old_UseStrictMode = m_fUseStrictMode;

    ParseNodePtr pnodeStmt;
    ParseNodePtr *lastNodeRef = nullptr;

    if (buildAST)
    {TRACE_IT(30657);
        AssertMem(ppnodeList);
        AssertMemN(pppnodeLast);
        *ppnodeList = nullptr;
    }

    if(CONFIG_FLAG(ForceStrictMode))
    {TRACE_IT(30658);
        m_fUseStrictMode = TRUE;
    }

    for (;;)
    {TRACE_IT(30659);
        switch (m_token.tk)
        {
        case tkCASE:
        case tkDEFAULT:
        case tkRCurly:
        case tkEOF:
            if (buildAST && nullptr != pppnodeLast)
            {TRACE_IT(30660);
                *pppnodeLast = lastNodeRef;
            }
            if (!buildAST)
            {TRACE_IT(30661);
                m_fUseStrictMode = old_UseStrictMode;
            }
            return;
        }

        if (doneDirectives == FALSE)
        {TRACE_IT(30662);
            bool isOctalInString = false;
            bool isUseStrictDirective = false;
            bool isUseAsmDirective = false;
            if (smEnvironment != SM_NotUsed && CheckForDirective(&isUseStrictDirective, &isUseAsmDirective, &isOctalInString))
            {TRACE_IT(30663);
                // Ignore "use asm" statement when not building the AST
                isUseAsmDirective &= buildAST;

                if (isUseStrictDirective)
                {TRACE_IT(30664);
                    // Functions with non-simple parameter list cannot be made strict mode
                    if (GetCurrentFunctionNode()->sxFnc.HasNonSimpleParameterList())
                    {TRACE_IT(30665);
                        Error(ERRNonSimpleParamListInStrictMode);
                    }

                    if (seenDirectiveContainingOctal)
                    {TRACE_IT(30666);
                        // Directives seen before a "use strict" cannot contain an octal.
                        Error(ERRES5NoOctal);
                    }
                    if (!buildAST)
                    {TRACE_IT(30667);
                        // Turning on strict mode in deferred code.
                        m_fUseStrictMode = TRUE;
                        if (!m_inDeferredNestedFunc)
                        {TRACE_IT(30668);
                            // Top-level deferred function, so there's a parse node
                            Assert(m_currentNodeFunc != nullptr);
                            m_currentNodeFunc->sxFnc.SetStrictMode();
                        }
                        else if (strictModeOn)
                        {TRACE_IT(30669);
                            // This turns on strict mode in a deferred function, we need to go back
                            // and re-check duplicated formals.
                            *strictModeOn = true;
                        }
                    }
                    else
                    {TRACE_IT(30670);
                        if (smEnvironment == SM_OnGlobalCode)
                        {TRACE_IT(30671);
                            // Turning on strict mode at the top level
                            m_fUseStrictMode = TRUE;
                        }
                        else
                        {TRACE_IT(30672);
                            // i.e. smEnvironment == SM_OnFunctionCode
                            Assert(m_currentNodeFunc != nullptr);
                            m_currentNodeFunc->sxFnc.SetStrictMode();
                        }
                    }
                }
                else if (isUseAsmDirective)
                {TRACE_IT(30673);
                    if (smEnvironment != SM_OnGlobalCode) //Top level use asm doesn't mean anything.
                    {TRACE_IT(30674);
                        // i.e. smEnvironment == SM_OnFunctionCode
                        Assert(m_currentNodeFunc != nullptr);
                        m_currentNodeFunc->sxFnc.SetAsmjsMode();
                        m_currentNodeFunc->sxFnc.SetCanBeDeferred(false);
                        m_InAsmMode = true;

                        CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(AsmJSFunction, m_scriptContext);
                    }
                }
                else if (isOctalInString)
                {TRACE_IT(30675);
                    seenDirectiveContainingOctal = TRUE;
                }
            }
            else
            {TRACE_IT(30676);
                // The first time we see anything other than a directive we can have no more directives.
                doneDirectives = TRUE;
            }
        }

        if (nullptr != (pnodeStmt = ParseStatement<buildAST>()))
        {TRACE_IT(30677);
            if (buildAST)
            {
                AddToNodeList(ppnodeList, &lastNodeRef, pnodeStmt);
            }
        }
    }
}

template <class Fn>
void Parser::VisitFunctionsInScope(ParseNodePtr pnodeScopeList, Fn fn)
{TRACE_IT(30678);
    ParseNodePtr pnodeScope;
    for (pnodeScope = pnodeScopeList; pnodeScope;)
    {TRACE_IT(30679);
        switch (pnodeScope->nop)
        {
        case knopBlock:
            VisitFunctionsInScope(pnodeScope->sxBlock.pnodeScopes, fn);
            pnodeScope = pnodeScope->sxBlock.pnodeNext;
            break;

        case knopFncDecl:
            fn(pnodeScope);
            pnodeScope = pnodeScope->sxFnc.pnodeNext;
            break;

        case knopCatch:
            VisitFunctionsInScope(pnodeScope->sxCatch.pnodeScopes, fn);
            pnodeScope = pnodeScope->sxCatch.pnodeNext;
            break;

        case knopWith:
            VisitFunctionsInScope(pnodeScope->sxWith.pnodeScopes, fn);
            pnodeScope = pnodeScope->sxWith.pnodeNext;
            break;

        default:
            AssertMsg(false, "Unexpected node with scope list");
            return;
        }
    }
}

// Scripts above this size (minus string literals and comments) will have parsing of
// function bodies deferred.
ULONG Parser::GetDeferralThreshold(bool isProfileLoaded)
{TRACE_IT(30680);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (CONFIG_FLAG(ForceDeferParse) ||
        PHASE_FORCE1(Js::DeferParsePhase) ||
        Js::Configuration::Global.flags.IsEnabled(Js::ForceUndoDeferFlag))
    {TRACE_IT(30681);
        return 0;
    }
    else if (Js::Configuration::Global.flags.IsEnabled(Js::DeferParseFlag))
    {TRACE_IT(30682);
        return Js::Configuration::Global.flags.DeferParse;
    }
    else
#endif
    {TRACE_IT(30683);
        if (isProfileLoaded)
        {TRACE_IT(30684);
            return DEFAULT_CONFIG_ProfileBasedDeferParseThreshold;
        }
        return DEFAULT_CONFIG_DeferParseThreshold;
    }
}

void Parser::FinishDeferredFunction(ParseNodePtr pnodeScopeList)
{
    VisitFunctionsInScope(pnodeScopeList,
        [this](ParseNodePtr pnodeFnc)
    {
        Assert(pnodeFnc->nop == knopFncDecl);

        // Non-simple params (such as default) require a good amount of logic to put vars on appriopriate scopes. ParseFncDecl handles it
        // properly (both on defer and non-defer case). This is to avoid write duplicated logic here as well. Function with non-simple-param
        // will remain deferred untill they are called.
        if (pnodeFnc->sxFnc.pnodeBody == nullptr && !pnodeFnc->sxFnc.HasNonSimpleParameterList())
        {TRACE_IT(30685);
            // Go back and generate an AST for this function.
            JS_ETW_INTERNAL(EventWriteJSCRIPT_PARSE_FUNC(this->GetScriptContext(), pnodeFnc->sxFnc.functionId, /*Undefer*/TRUE));

            ParseNodePtr pnodeFncSave = this->m_currentNodeFunc;
            this->m_currentNodeFunc = pnodeFnc;

            ParseNodePtr pnodeFncExprBlock = nullptr;
            if (pnodeFnc->sxFnc.pnodeName &&
                !pnodeFnc->sxFnc.IsDeclaration())
            {TRACE_IT(30686);
                // Set up the named function expression symbol so references inside the function can be bound.
                ParseNodePtr pnodeName = pnodeFnc->sxFnc.pnodeName;
                Assert(pnodeName->nop == knopVarDecl);
                Assert(pnodeName->sxVar.pnodeNext == nullptr);

                pnodeFncExprBlock = this->StartParseBlock<true>(PnodeBlockType::Function, ScopeType_FuncExpr);
                PidRefStack *ref = this->PushPidRef(pnodeName->sxVar.pid);
                pnodeName->sxVar.symRef = ref->GetSymRef();
                ref->SetSym(pnodeName->sxVar.sym);

                Scope *fncExprScope = pnodeFncExprBlock->sxBlock.scope;
                fncExprScope->AddNewSymbol(pnodeName->sxVar.sym);
                pnodeFnc->sxFnc.scope = fncExprScope;
            }

            ParseNodePtr pnodeBlock = this->StartParseBlock<true>(PnodeBlockType::Parameter, ScopeType_Parameter);
            pnodeFnc->sxFnc.pnodeScopes = pnodeBlock;
            m_ppnodeScope = &pnodeBlock->sxBlock.pnodeScopes;
            pnodeBlock->sxBlock.pnodeStmt = pnodeFnc;

            ParseNodePtr* varNodesList = &pnodeFnc->sxFnc.pnodeVars;
            ParseNodePtr argNode = nullptr;
            if (!pnodeFnc->sxFnc.IsModule() && !pnodeFnc->sxFnc.IsLambda() && !(pnodeFnc->grfpn & PNodeFlags::fpnArguments_overriddenInParam))
            {TRACE_IT(30687);
                ParseNodePtr *const ppnodeVarSave = m_ppnodeVar;
                m_ppnodeVar = &pnodeFnc->sxFnc.pnodeVars;

                argNode = this->AddArgumentsNodeToVars(pnodeFnc);

                varNodesList = m_ppnodeVar;
                m_ppnodeVar = ppnodeVarSave;
            }

            // Add the args to the scope, since we won't re-parse those.
            Scope *scope = pnodeBlock->sxBlock.scope;
            auto addArgsToScope = [&](ParseNodePtr pnodeArg) {TRACE_IT(30688);
                if (pnodeArg->IsVarLetOrConst())
                {TRACE_IT(30689);
                    PidRefStack *ref = this->PushPidRef(pnodeArg->sxVar.pid);
                    pnodeArg->sxVar.symRef = ref->GetSymRef();
                    if (ref->GetSym() != nullptr)
                    {TRACE_IT(30690);
                        // Duplicate parameter in a configuration that allows them.
                        // The symbol is already in the scope, just point it to the right declaration.
                        Assert(ref->GetSym() == pnodeArg->sxVar.sym);
                        ref->GetSym()->SetDecl(pnodeArg);
                    }
                    else
                    {TRACE_IT(30691);
                        ref->SetSym(pnodeArg->sxVar.sym);
                        scope->AddNewSymbol(pnodeArg->sxVar.sym);
                    }
                }
            };
            MapFormals(pnodeFnc, addArgsToScope);
            MapFormalsFromPattern(pnodeFnc, addArgsToScope);

            ParseNodePtr pnodeInnerBlock = this->StartParseBlock<true>(PnodeBlockType::Function, ScopeType_FunctionBody);
            pnodeFnc->sxFnc.pnodeBodyScope = pnodeInnerBlock;

            // Set the parameter block's child to the function body block.
            *m_ppnodeScope = pnodeInnerBlock;

            ParseNodePtr *ppnodeScopeSave = nullptr;
            ParseNodePtr *ppnodeExprScopeSave = nullptr;

            ppnodeScopeSave = m_ppnodeScope;

            // This synthetic block scope will contain all the nested scopes.
            m_ppnodeScope = &pnodeInnerBlock->sxBlock.pnodeScopes;
            pnodeInnerBlock->sxBlock.pnodeStmt = pnodeFnc;

            // Keep nested function declarations and expressions in the same list at function scope.
            // (Indicate this by nulling out the current function expressions list.)
            ppnodeExprScopeSave = m_ppnodeExprScope;
            m_ppnodeExprScope = nullptr;

            // Shouldn't be any temps in the arg list.
            Assert(*m_ppnodeVar == nullptr);

            // Start the var list.
            m_ppnodeVar = varNodesList;

            if (scope != nullptr)
            {TRACE_IT(30692);
                Assert(pnodeFnc->sxFnc.IsBodyAndParamScopeMerged());
                scope->ForEachSymbol([this](Symbol* paramSym)
                {
                    PidRefStack* ref = PushPidRef(paramSym->GetPid());
                    ref->SetSym(paramSym);
                });
            }

            Assert(m_currentNodeNonLambdaFunc == nullptr);
            m_currentNodeNonLambdaFunc = pnodeFnc;

            this->FinishFncNode(pnodeFnc);

            Assert(pnodeFnc == m_currentNodeNonLambdaFunc);
            m_currentNodeNonLambdaFunc = nullptr;

            m_ppnodeExprScope = ppnodeExprScopeSave;

            AssertMem(m_ppnodeScope);
            Assert(nullptr == *m_ppnodeScope);
            m_ppnodeScope = ppnodeScopeSave;

            this->FinishParseBlock(pnodeInnerBlock);

            if (!pnodeFnc->sxFnc.IsModule() && (m_token.tk == tkLCurly || !pnodeFnc->sxFnc.IsLambda()))
            {
                UpdateArgumentsNode(pnodeFnc, argNode);
            }

            this->FinishParseBlock(pnodeBlock);
            if (pnodeFncExprBlock)
            {TRACE_IT(30693);
                this->FinishParseBlock(pnodeFncExprBlock);
            }

            this->m_currentNodeFunc = pnodeFncSave;
        }
    });
}

void Parser::InitPids()
{TRACE_IT(30694);
    AssertMemN(m_phtbl);
    wellKnownPropertyPids.arguments = m_phtbl->PidHashNameLen(g_ssym_arguments.sz, g_ssym_arguments.cch);
    wellKnownPropertyPids.async = m_phtbl->PidHashNameLen(g_ssym_async.sz, g_ssym_async.cch);
    wellKnownPropertyPids.eval = m_phtbl->PidHashNameLen(g_ssym_eval.sz, g_ssym_eval.cch);
    wellKnownPropertyPids.get = m_phtbl->PidHashNameLen(g_ssym_get.sz, g_ssym_get.cch);
    wellKnownPropertyPids.set = m_phtbl->PidHashNameLen(g_ssym_set.sz, g_ssym_set.cch);
    wellKnownPropertyPids.let = m_phtbl->PidHashNameLen(g_ssym_let.sz, g_ssym_let.cch);
    wellKnownPropertyPids.constructor = m_phtbl->PidHashNameLen(g_ssym_constructor.sz, g_ssym_constructor.cch);
    wellKnownPropertyPids.prototype = m_phtbl->PidHashNameLen(g_ssym_prototype.sz, g_ssym_prototype.cch);
    wellKnownPropertyPids.__proto__ = m_phtbl->PidHashNameLen(_u("__proto__"), sizeof("__proto__") - 1);
    wellKnownPropertyPids.of = m_phtbl->PidHashNameLen(_u("of"), sizeof("of") - 1);
    wellKnownPropertyPids.target = m_phtbl->PidHashNameLen(_u("target"), sizeof("target") - 1);
    wellKnownPropertyPids.as = m_phtbl->PidHashNameLen(_u("as"), sizeof("as") - 1);
    wellKnownPropertyPids.from = m_phtbl->PidHashNameLen(_u("from"), sizeof("from") - 1);
    wellKnownPropertyPids._default = m_phtbl->PidHashNameLen(_u("default"), sizeof("default") - 1);
    wellKnownPropertyPids._starDefaultStar = m_phtbl->PidHashNameLen(_u("*default*"), sizeof("*default*") - 1);
    wellKnownPropertyPids._star = m_phtbl->PidHashNameLen(_u("*"), sizeof("*") - 1);
}

void Parser::RestoreScopeInfo(Js::ParseableFunctionInfo* functionBody)
{TRACE_IT(30695);
    if (!functionBody)
    {TRACE_IT(30696);
        return;
    }

    Js::ScopeInfo* scopeInfo = functionBody->GetScopeInfo();
    if (!scopeInfo)
    {TRACE_IT(30697);
        return;
    }

    if (this->IsBackgroundParser())
    {
        PROBE_STACK_NO_DISPOSE(m_scriptContext, Js::Constants::MinStackByteCodeVisitor);
    }
    else
    {
        PROBE_STACK(m_scriptContext, Js::Constants::MinStackByteCodeVisitor);
    }

    RestoreScopeInfo(scopeInfo->GetParent()); // Recursively restore outer func scope info

    Js::ScopeInfo* funcExprScopeInfo = scopeInfo->GetFuncExprScopeInfo();
    if (funcExprScopeInfo)
    {TRACE_IT(30698);
        funcExprScopeInfo->SetScopeId(m_nextBlockId);
        ParseNodePtr pnodeFncExprScope = StartParseBlockWithCapacity<true>(PnodeBlockType::Function, ScopeType_FuncExpr, funcExprScopeInfo->GetSymbolCount());
        Scope *scope = pnodeFncExprScope->sxBlock.scope;
        funcExprScopeInfo->GetScopeInfo(this, nullptr, nullptr, scope);
    }

    Js::ScopeInfo* paramScopeInfo = scopeInfo->GetParamScopeInfo();
    if (paramScopeInfo)
    {TRACE_IT(30699);
        paramScopeInfo->SetScopeId(m_nextBlockId);
        ParseNodePtr pnodeFncExprScope = StartParseBlockWithCapacity<true>(PnodeBlockType::Parameter, ScopeType_Parameter, paramScopeInfo->GetSymbolCount());
        Scope *scope = pnodeFncExprScope->sxBlock.scope;
        paramScopeInfo->GetScopeInfo(this, nullptr, nullptr, scope);
    }

    scopeInfo->SetScopeId(m_nextBlockId);
    ParseNodePtr pnodeFncScope = nullptr;
    if (scopeInfo->IsGlobalEval())
    {TRACE_IT(30700);
        pnodeFncScope = StartParseBlockWithCapacity<true>(PnodeBlockType::Regular, ScopeType_GlobalEvalBlock, scopeInfo->GetSymbolCount());
    }
    else
    {TRACE_IT(30701);
        pnodeFncScope = StartParseBlockWithCapacity<true>(PnodeBlockType::Function, ScopeType_FunctionBody, scopeInfo->GetSymbolCount());
    }
    Scope *scope = pnodeFncScope->sxBlock.scope;
    scopeInfo->GetScopeInfo(this, nullptr, nullptr, scope);
}

void Parser::FinishScopeInfo(Js::ParseableFunctionInfo *functionBody)
{TRACE_IT(30702);
    if (!functionBody)
    {TRACE_IT(30703);
        return;
    }

    Js::ScopeInfo* scopeInfo = functionBody->GetScopeInfo();
    if (!scopeInfo)
    {TRACE_IT(30704);
        return;
    }

    if (this->IsBackgroundParser())
    {
        PROBE_STACK_NO_DISPOSE(m_scriptContext, Js::Constants::MinStackByteCodeVisitor);
    }
    else
    {
        PROBE_STACK(m_scriptContext, Js::Constants::MinStackByteCodeVisitor);
    }

    int scopeId = scopeInfo->GetScopeId();

    scopeInfo->GetScope()->ForEachSymbol([this, scopeId](Symbol *sym)
    {
        this->BindPidRefsInScope(sym->GetPid(), sym, scopeId);
    });
    PopScope(scopeInfo->GetScope());
    PopStmt(&m_currentBlockInfo->pstmt);
    PopBlockInfo();

    Js::ScopeInfo *paramScopeInfo = scopeInfo->GetParamScopeInfo();
    if (paramScopeInfo)
    {TRACE_IT(30705);
        scopeId = paramScopeInfo->GetScopeId();
        paramScopeInfo->GetScope()->ForEachSymbol([this, scopeId](Symbol *sym)
        {
            this->BindPidRefsInScope(sym->GetPid(), sym, scopeId);
        });
        PopScope(paramScopeInfo->GetScope());
        PopStmt(&m_currentBlockInfo->pstmt);
        PopBlockInfo();
    }

    Js::ScopeInfo *funcExprScopeInfo = scopeInfo->GetFuncExprScopeInfo();
    if (funcExprScopeInfo)
    {TRACE_IT(30706);
        scopeId = funcExprScopeInfo->GetScopeId();
        funcExprScopeInfo->GetScope()->ForEachSymbol([this, scopeId](Symbol *sym)
        {
            this->BindPidRefsInScope(sym->GetPid(), sym, scopeId);
        });
        PopScope(funcExprScopeInfo->GetScope());
        PopStmt(&m_currentBlockInfo->pstmt);
        PopBlockInfo();
    }

    FinishScopeInfo(scopeInfo->GetParent());
}

/***************************************************************************
Parse the code.
***************************************************************************/
ParseNodePtr Parser::Parse(LPCUTF8 pszSrc, size_t offset, size_t length, charcount_t charOffset, ULONG grfscr, ULONG lineNumber, Js::LocalFunctionId * nextFunctionId, CompileScriptException *pse)
{TRACE_IT(30707);
    ParseNodePtr pnodeProg;
    ParseNodePtr *lastNodeRef = nullptr;

    m_nextBlockId = 0;

    // Scanner should run in Running mode and not syntax coloring mode
    grfscr &= ~fscrSyntaxColor;

    if (this->m_scriptContext->IsScriptContextInDebugMode()
#ifdef ENABLE_PREJIT
         || Js::Configuration::Global.flags.Prejit
#endif
         || ((grfscr & fscrNoDeferParse) != 0)
        )
    {TRACE_IT(30708);
        // Don't do deferred parsing if debugger is attached or feature is disabled
        // by command-line switch.
        grfscr &= ~fscrDeferFncParse;
    }
    else if (!(grfscr & fscrGlobalCode) &&
             (
                 PHASE_OFF1(Js::Phase::DeferEventHandlersPhase) ||
                 this->m_scriptContext->IsScriptContextInSourceRundownOrDebugMode()
             )
        )
    {TRACE_IT(30709);
        // Don't defer event handlers in debug/rundown mode, because we need to register the document,
        // so we need to create a full FunctionBody for the script body.
        grfscr &= ~fscrDeferFncParse;
    }

    bool isDeferred = (grfscr & fscrDeferredFnc) != 0;
    bool isModuleSource = (grfscr & fscrIsModuleCode) != 0;

    m_grfscr = grfscr;
    m_length = length;
    m_originalLength = length;
    m_nextFunctionId = nextFunctionId;

    if(m_parseType != ParseType_Deferred)
    {TRACE_IT(30710);
        JS_ETW(EventWriteJSCRIPT_PARSE_METHOD_START(m_sourceContextInfo->dwHostSourceContext, GetScriptContext(), *m_nextFunctionId, 0, m_parseType, Js::Constants::GlobalFunction));
        OUTPUT_TRACE(Js::DeferParsePhase, _u("Parsing function (%s) : %s (%d)\n"), GetParseType(), Js::Constants::GlobalFunction, *m_nextFunctionId);
    }

    // Give the scanner the source and get the first token
    m_pscan->SetText(pszSrc, offset, length, charOffset, grfscr, lineNumber);
    m_pscan->Scan();

    // Make the main 'knopProg' node
    int32 initSize = 0;
    m_pCurrentAstSize = &initSize;
    pnodeProg = CreateProgNodeWithScanner(isModuleSource);
    pnodeProg->grfpn = PNodeFlags::fpnNone;
    pnodeProg->sxFnc.pid = nullptr;
    pnodeProg->sxFnc.pnodeName = nullptr;
    pnodeProg->sxFnc.pnodeRest = nullptr;
    pnodeProg->sxFnc.ClearFlags();
    pnodeProg->sxFnc.SetNested(FALSE);
    pnodeProg->sxFnc.astSize = 0;
    pnodeProg->sxFnc.cbMin = m_pscan->IecpMinTok();
    pnodeProg->sxFnc.lineNumber = lineNumber;
    pnodeProg->sxFnc.columnNumber = 0;
    pnodeProg->sxFnc.isBodyAndParamScopeMerged = true;

    if (!isDeferred || (isDeferred && grfscr & fscrGlobalCode))
    {TRACE_IT(30711);
        // In the deferred case, if the global function is deferred parse (which is in no-refresh case),
        // we will re-use the same function body, so start with the correct functionId.
        pnodeProg->sxFnc.functionId = (*m_nextFunctionId)++;
    }
    else
    {TRACE_IT(30712);
        pnodeProg->sxFnc.functionId = Js::Constants::NoFunctionId;
    }

    if (isModuleSource)
    {TRACE_IT(30713);
        Assert(m_scriptContext->GetConfig()->IsES6ModuleEnabled());

        pnodeProg->sxModule.localExportEntries = nullptr;
        pnodeProg->sxModule.indirectExportEntries = nullptr;
        pnodeProg->sxModule.starExportEntries = nullptr;
        pnodeProg->sxModule.importEntries = nullptr;
        pnodeProg->sxModule.requestedModules = nullptr;
    }

    m_pCurrentAstSize = & (pnodeProg->sxFnc.astSize);

    pnodeProg->sxFnc.hint = nullptr;
    pnodeProg->sxFnc.hintLength = 0;
    pnodeProg->sxFnc.hintOffset = 0;
    pnodeProg->sxFnc.isNameIdentifierRef = true;
    pnodeProg->sxFnc.nestedFuncEscapes = false;

    // initialize parsing variables
    pnodeProg->sxFnc.pnodeNext = nullptr;

    m_currentNodeFunc = nullptr;
    m_currentNodeDeferredFunc = nullptr;
    m_currentNodeProg = pnodeProg;
    m_cactIdentToNodeLookup = 1;

    pnodeProg->sxFnc.nestedCount = 0;
    m_pnestedCount = &pnodeProg->sxFnc.nestedCount;
    m_inDeferredNestedFunc = false;

    pnodeProg->sxFnc.pnodeParams = nullptr;
    pnodeProg->sxFnc.pnodeVars = nullptr;
    pnodeProg->sxFnc.pnodeRest = nullptr;
    m_ppnodeVar = &pnodeProg->sxFnc.pnodeVars;
    SetCurrentStatement(nullptr);
    AssertMsg(m_pstmtCur == nullptr, "Statement stack should be empty when we start parse global code");

    // Create block for const's and let's
    ParseNodePtr pnodeGlobalBlock = StartParseBlock<true>(PnodeBlockType::Global, ScopeType_Global);
    pnodeProg->sxProg.scope = pnodeGlobalBlock->sxBlock.scope;
    ParseNodePtr pnodeGlobalEvalBlock = nullptr;

    // Don't track function expressions separately from declarations at global scope.
    m_ppnodeExprScope = nullptr;

    // This synthetic block scope will contain all the nested scopes.
    pnodeProg->sxFnc.pnodeBodyScope = nullptr;
    pnodeProg->sxFnc.pnodeScopes = pnodeGlobalBlock;
    m_ppnodeScope = &pnodeGlobalBlock->sxBlock.pnodeScopes;

    if ((this->m_grfscr & fscrEvalCode) &&
        !(this->m_functionBody && this->m_functionBody->GetScopeInfo()))
    {TRACE_IT(30714);
        pnodeGlobalEvalBlock = StartParseBlock<true>(PnodeBlockType::Regular, ScopeType_GlobalEvalBlock);
        pnodeProg->sxFnc.pnodeScopes = pnodeGlobalEvalBlock;
        m_ppnodeScope = &pnodeGlobalEvalBlock->sxBlock.pnodeScopes;
    }

    Js::ScopeInfo *scopeInfo = nullptr;
    if (m_parseType == ParseType_Deferred && m_functionBody)
    {TRACE_IT(30715);
        // this->m_functionBody can be cleared during parsing, but we need access to the scope info later.
        scopeInfo = m_functionBody->GetScopeInfo();
        if (scopeInfo)
        {TRACE_IT(30716);
            // Create an enclosing function context.
            m_currentNodeFunc = CreateNode(knopFncDecl);
            m_currentNodeFunc->sxFnc.pnodeName = nullptr;
            m_currentNodeFunc->sxFnc.functionId = m_functionBody->GetLocalFunctionId();
            m_currentNodeFunc->sxFnc.nestedCount = m_functionBody->GetNestedCount();
            m_currentNodeFunc->sxFnc.SetStrictMode(!!this->m_fUseStrictMode);

            this->RestoreScopeInfo(scopeInfo->GetParent());
        }
    }

    // It's possible for the module global to be defer-parsed in debug scenarios.
    if (isModuleSource && (!isDeferred || (isDeferred && grfscr & fscrGlobalCode)))
    {TRACE_IT(30717);
        ParseNodePtr moduleFunction = GenerateModuleFunctionWrapper<true>();
        pnodeProg->sxFnc.pnodeBody = nullptr;
        AddToNodeList(&pnodeProg->sxFnc.pnodeBody, &lastNodeRef, moduleFunction);
    }
    else
    {TRACE_IT(30718);
        // Process a sequence of statements/declarations
        ParseStmtList<true>(
            &pnodeProg->sxFnc.pnodeBody,
            &lastNodeRef,
            SM_OnGlobalCode,
            !(m_grfscr & fscrDeferredFncExpression) /* isSourceElementList */);
    }

    if (m_parseType == ParseType_Deferred)
    {TRACE_IT(30719);
        if (scopeInfo)
        {TRACE_IT(30720);
            this->FinishScopeInfo(scopeInfo->GetParent());
        }
    }

    pnodeProg->sxProg.m_UsesArgumentsAtGlobal = m_UsesArgumentsAtGlobal;

    if (IsStrictMode())
    {TRACE_IT(30721);
        pnodeProg->sxFnc.SetStrictMode();
    }

#if DEBUG
    if(m_grfscr & fscrEnforceJSON && !IsJSONValid(pnodeProg->sxFnc.pnodeBody))
    {TRACE_IT(30722);
        Error(ERRsyntax);
    }
#endif

    if (tkEOF != m_token.tk)
        Error(ERRsyntax);

    // Append an EndCode node.
    AddToNodeList(&pnodeProg->sxFnc.pnodeBody, &lastNodeRef,
        CreateNodeWithScanner<knopEndCode>());
    AssertMem(lastNodeRef);
    AssertNodeMem(*lastNodeRef);
    Assert((*lastNodeRef)->nop == knopEndCode);
    (*lastNodeRef)->ichMin = 0;
    (*lastNodeRef)->ichLim = 0;

    // Get the extent of the code.
    pnodeProg->ichLim = m_pscan->IchLimTok();
    pnodeProg->sxFnc.cbLim = m_pscan->IecpLimTok();

    // Terminate the local list
    *m_ppnodeVar = nullptr;

    Assert(nullptr == *m_ppnodeScope);
    Assert(nullptr == pnodeProg->sxFnc.pnodeNext);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.IsEnabled(Js::ForceUndoDeferFlag))
    {TRACE_IT(30723);
        m_stoppedDeferredParse = true;
    }
#endif

    if (m_stoppedDeferredParse)
    {TRACE_IT(30724);
        if (this->m_hasParallelJob)
        {TRACE_IT(30725);
#if ENABLE_BACKGROUND_PARSING
            BackgroundParser *bgp = static_cast<BackgroundParser*>(m_scriptContext->GetBackgroundParser());
            Assert(bgp);
            this->WaitForBackgroundJobs(bgp, pse);
#endif
        }

        // Finally, see if there are any function bodies we now want to generate because we
        // decided to stop deferring.
        FinishDeferredFunction(pnodeProg->sxFnc.pnodeScopes);
    }

    if (pnodeGlobalEvalBlock)
    {TRACE_IT(30726);
        FinishParseBlock(pnodeGlobalEvalBlock);
    }
    // Append block as body of pnodeProg
    FinishParseBlock(pnodeGlobalBlock);

    m_scriptContext->AddSourceSize(m_length);

    if (m_parseType != ParseType_Deferred)
    {TRACE_IT(30727);
        JS_ETW(EventWriteJSCRIPT_PARSE_METHOD_STOP(m_sourceContextInfo->dwHostSourceContext, GetScriptContext(), pnodeProg->sxFnc.functionId, *m_pCurrentAstSize, false, Js::Constants::GlobalFunction));
    }
    return pnodeProg;
}


bool Parser::CheckForDirective(bool* pIsUseStrict, bool *pIsUseAsm, bool* pIsOctalInString)
{TRACE_IT(30728);
    // A directive is a string constant followed by a statement terminating token
    if (m_token.tk != tkStrCon)
        return false;

    // Careful, need to check for octal before calling m_pscan->Scan()
    // because Scan() clears the "had octal" flag on the scanner and
    // m_pscan->Restore() does not restore this flag.
    if (pIsOctalInString != nullptr)
    {TRACE_IT(30729);
        *pIsOctalInString = m_pscan->IsOctOrLeadingZeroOnLastTKNumber();
    }

    Ident* pidDirective = m_token.GetStr();
    RestorePoint start;
    m_pscan->Capture(&start);
    m_pscan->Scan();

    bool isDirective = true;

    switch (m_token.tk)
    {
    case tkSColon:
    case tkEOF:
    case tkLCurly:
    case tkRCurly:
        break;
    default:
        if (!m_pscan->FHadNewLine())
        {TRACE_IT(30730);
            isDirective = false;
        }
        break;
    }

    if (isDirective)
    {TRACE_IT(30731);
        if (pIsUseStrict != nullptr)
        {TRACE_IT(30732);
            *pIsUseStrict = CheckStrictModeStrPid(pidDirective);
        }
        if (pIsUseAsm != nullptr)
        {TRACE_IT(30733);
            *pIsUseAsm = CheckAsmjsModeStrPid(pidDirective);
        }
    }

    m_pscan->SeekTo(start);
    return isDirective;
}

bool Parser::CheckStrictModeStrPid(IdentPtr pid)
{TRACE_IT(30734);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.NoStrictMode)
        return false;
#endif

    return pid != nullptr &&
        pid->Cch() == 10 &&
        !m_pscan->IsEscapeOnLastTkStrCon() &&
        wcsncmp(pid->Psz(), _u("use strict"), 10) == 0;
}

bool Parser::CheckAsmjsModeStrPid(IdentPtr pid)
{TRACE_IT(30735);
#ifdef ASMJS_PLAT
    if (!CONFIG_FLAG_RELEASE(Asmjs))
    {TRACE_IT(30736);
        return false;
    }

    bool isAsmCandidate = (pid != nullptr &&
        AutoSystemInfo::Data.SSE2Available() &&
        pid->Cch() == 7 &&
        !m_pscan->IsEscapeOnLastTkStrCon() &&
        wcsncmp(pid->Psz(), _u("use asm"), 10) == 0);

    if (isAsmCandidate && m_scriptContext->IsScriptContextInDebugMode())
    {TRACE_IT(30737);
        // We would like to report this to debugger - they may choose to disable debugging.
        // TODO : localization of the string?
        m_scriptContext->RaiseMessageToDebugger(DEIT_ASMJS_IN_DEBUGGING, _u("AsmJs initialization error - AsmJs disabled due to script debugger"), !m_sourceContextInfo->IsDynamic() ? m_sourceContextInfo->url : nullptr);
        return false;
    }

    return isAsmCandidate && !(m_grfscr & fscrNoAsmJs);
#else
    return false;
#endif
}

HRESULT Parser::ParseUtf8Source(__out ParseNodePtr* parseTree, LPCUTF8 pSrc, size_t length, ULONG grfsrc, CompileScriptException *pse,
    Js::LocalFunctionId * nextFunctionId, SourceContextInfo * sourceContextInfo)
{TRACE_IT(30738);
    m_functionBody = nullptr;
    m_parseType = ParseType_Upfront;
    return ParseSourceInternal( parseTree, pSrc, 0, length, 0, true, grfsrc, pse, nextFunctionId, 0, sourceContextInfo);
}

HRESULT Parser::ParseCesu8Source(__out ParseNodePtr* parseTree, LPCUTF8 pSrc, size_t length, ULONG grfsrc, CompileScriptException *pse,
    Js::LocalFunctionId * nextFunctionId, SourceContextInfo * sourceContextInfo)
{TRACE_IT(30739);
    m_functionBody = nullptr;
    m_parseType = ParseType_Upfront;
    return ParseSourceInternal( parseTree, pSrc, 0, length, 0, false, grfsrc, pse, nextFunctionId, 0, sourceContextInfo);
}

void Parser::PrepareScanner(bool fromExternal)
{TRACE_IT(30740);
    // NOTE: HashTbl and Scanner are currently allocated from the CRT heap. If we want to allocate them from the
    // parser arena, then we also need to change the way the HashTbl allocates PID's from its underlying
    // allocator (which also currently uses the CRT heap). This is not trivial, because we still need to support
    // heap allocation for the colorizer interface.

    // create the hash table and init PID members
    if (nullptr == (m_phtbl = HashTbl::Create(HASH_TABLE_SIZE, &m_err)))
        Error(ERRnoMemory);
    InitPids();

    // create the scanner
    if (nullptr == (m_pscan = Scanner_t::Create(this, m_phtbl, &m_token, &m_err, m_scriptContext)))
        Error(ERRnoMemory);

    if (fromExternal)
        m_pscan->FromExternalSource();
}

#if ENABLE_BACKGROUND_PARSING
void Parser::PrepareForBackgroundParse()
{TRACE_IT(30741);
    m_pscan->PrepareForBackgroundParse(m_scriptContext);
}

void Parser::AddBackgroundParseItem(BackgroundParseItem *const item)
{TRACE_IT(30742);
    if (currBackgroundParseItem == nullptr)
    {TRACE_IT(30743);
        backgroundParseItems = item;
    }
    else
    {TRACE_IT(30744);
        currBackgroundParseItem->SetNext(item);
    }
    currBackgroundParseItem = item;
}
#endif

void Parser::AddFastScannedRegExpNode(ParseNodePtr const pnode)
{TRACE_IT(30745);
    Assert(!IsBackgroundParser());
    Assert(m_doingFastScan);

    if (fastScannedRegExpNodes == nullptr)
    {TRACE_IT(30746);
        fastScannedRegExpNodes = Anew(&m_nodeAllocator, NodeDList, &m_nodeAllocator);
    }
    fastScannedRegExpNodes->Append(pnode);
}

#if ENABLE_BACKGROUND_PARSING
void Parser::AddBackgroundRegExpNode(ParseNodePtr const pnode)
{TRACE_IT(30747);
    Assert(IsBackgroundParser());
    Assert(currBackgroundParseItem != nullptr);

    currBackgroundParseItem->AddRegExpNode(pnode, &m_nodeAllocator);
}
#endif

HRESULT Parser::ParseFunctionInBackground(ParseNodePtr pnodeFnc, ParseContext *parseContext, bool topLevelDeferred, CompileScriptException *pse)
{TRACE_IT(30748);
    m_functionBody = nullptr;
    m_parseType = ParseType_Upfront;
    HRESULT hr = S_OK;
    SmartFPUControl smartFpuControl;
    uint nextFunctionId = pnodeFnc->sxFnc.functionId + 1;

    this->RestoreContext(parseContext);
    DebugOnly( m_err.fInited = TRUE; )
    m_nextFunctionId = &nextFunctionId;
    m_deferringAST = topLevelDeferred;
    m_inDeferredNestedFunc = false;
    m_scopeCountNoAst = 0;

    SetCurrentStatement(nullptr);

    pnodeFnc->sxFnc.pnodeVars = nullptr;
    pnodeFnc->sxFnc.pnodeParams = nullptr;
    pnodeFnc->sxFnc.pnodeBody = nullptr;
    pnodeFnc->sxFnc.nestedCount = 0;

    ParseNodePtr pnodeParentFnc = GetCurrentFunctionNode();
    m_currentNodeFunc = pnodeFnc;
    m_currentNodeDeferredFunc = nullptr;
    m_ppnodeScope = nullptr;
    m_ppnodeExprScope = nullptr;

    m_pnestedCount = &pnodeFnc->sxFnc.nestedCount;
    m_pCurrentAstSize = &pnodeFnc->sxFnc.astSize;

    ParseNodePtr pnodeBlock = StartParseBlock<true>(PnodeBlockType::Function, ScopeType_FunctionBody);
    pnodeFnc->sxFnc.pnodeScopes = pnodeBlock;
    m_ppnodeScope = &pnodeBlock->sxBlock.pnodeScopes;

    uint uDeferSave = m_grfscr & fscrDeferFncParse;

    try
    {TRACE_IT(30749);
        m_pscan->Scan();

        m_ppnodeVar = &pnodeFnc->sxFnc.pnodeParams;
        this->ParseFncFormals<true>(pnodeFnc, pnodeParentFnc, fFncNoFlgs);

        if (m_token.tk == tkRParen)
        {TRACE_IT(30750);
            m_pscan->Scan();
        }

        ChkCurTok(tkLCurly, ERRnoLcurly);

        m_ppnodeVar = &pnodeFnc->sxFnc.pnodeVars;

        // Put the scanner into "no hashing" mode.
        BYTE deferFlags = m_pscan->SetDeferredParse(topLevelDeferred);

        // Process a sequence of statements/declarations
        if (topLevelDeferred)
        {
            ParseStmtList<false>(nullptr, nullptr, SM_DeferredParse, true);
        }
        else
        {TRACE_IT(30751);
            ParseNodePtr *lastNodeRef = nullptr;
            ParseStmtList<true>(&pnodeFnc->sxFnc.pnodeBody, &lastNodeRef, SM_OnFunctionCode, true);
            AddArgumentsNodeToVars(pnodeFnc);
            // Append an EndCode node.
            AddToNodeList(&pnodeFnc->sxFnc.pnodeBody, &lastNodeRef, CreateNodeWithScanner<knopEndCode>());
        }

        // Restore the scanner's default hashing mode.
        m_pscan->SetDeferredParseFlags(deferFlags);

#if DBG
        pnodeFnc->sxFnc.deferredParseNextFunctionId = *this->m_nextFunctionId;
#endif
        this->m_deferringAST = FALSE;

        // Append block as body of pnodeProg
        FinishParseBlock(pnodeBlock);
    }
    catch(ParseExceptionObject& e)
    {TRACE_IT(30752);
        m_err.m_hr = e.GetError();
        hr = pse->ProcessError( m_pscan, m_err.m_hr, nullptr);
    }

    if (IsStrictMode())
    {TRACE_IT(30753);
        pnodeFnc->sxFnc.SetStrictMode();
    }

    if (topLevelDeferred)
    {TRACE_IT(30754);
        pnodeFnc->sxFnc.pnodeVars = nullptr;
    }

    m_grfscr |= uDeferSave;

    Assert(nullptr == *m_ppnodeScope);

    return hr;
}

HRESULT Parser::ParseSourceWithOffset(__out ParseNodePtr* parseTree, LPCUTF8 pSrc, size_t offset, size_t cbLength, charcount_t cchOffset,
        bool isCesu8, ULONG grfscr, CompileScriptException *pse, Js::LocalFunctionId * nextFunctionId, ULONG lineNumber, SourceContextInfo * sourceContextInfo,
        Js::ParseableFunctionInfo* functionInfo)
{TRACE_IT(30755);
    m_functionBody = functionInfo;
    if (m_functionBody)
    {TRACE_IT(30756);
        m_currDeferredStub = m_functionBody->GetDeferredStubs();
        m_InAsmMode = grfscr & fscrNoAsmJs ? false : m_functionBody->GetIsAsmjsMode();
    }
    m_deferAsmJs = !m_InAsmMode;
    m_parseType = ParseType_Deferred;
    return ParseSourceInternal( parseTree, pSrc, offset, cbLength, cchOffset, !isCesu8, grfscr, pse, nextFunctionId, lineNumber, sourceContextInfo);
}

bool Parser::IsStrictMode() const
{TRACE_IT(30757);
    return (m_fUseStrictMode ||
           (m_currentNodeFunc != nullptr && m_currentNodeFunc->sxFnc.GetStrictMode()));
}

BOOL Parser::ExpectingExternalSource()
{TRACE_IT(30758);
    return m_fExpectExternalSource;
}

Symbol *PnFnc::GetFuncSymbol()
{TRACE_IT(30759);
    if (pnodeName &&
        pnodeName->nop == knopVarDecl)
    {TRACE_IT(30760);
        return pnodeName->sxVar.sym;
    }
    return nullptr;
}

void PnFnc::SetFuncSymbol(Symbol *sym)
{TRACE_IT(30761);
    Assert(pnodeName &&
           pnodeName->nop == knopVarDecl);
    pnodeName->sxVar.sym = sym;
}

ParseNodePtr PnFnc::GetParamScope() const
{TRACE_IT(30762);
    if (this->pnodeScopes == nullptr)
    {TRACE_IT(30763);
        return nullptr;
    }
    Assert(this->pnodeScopes->nop == knopBlock &&
           this->pnodeScopes->sxBlock.pnodeNext == nullptr);
    return this->pnodeScopes->sxBlock.pnodeScopes;
}

ParseNodePtr PnFnc::GetBodyScope() const
{TRACE_IT(30764);
    if (this->pnodeBodyScope == nullptr)
    {TRACE_IT(30765);
        return nullptr;
    }
    Assert(this->pnodeBodyScope->nop == knopBlock &&
           this->pnodeBodyScope->sxBlock.pnodeNext == nullptr);
    return this->pnodeBodyScope->sxBlock.pnodeScopes;
}

// Create node versions with explicit token limits
ParseNodePtr Parser::CreateNode(OpCode nop, charcount_t ichMin, charcount_t ichLim)
{TRACE_IT(30766);
    Assert(!this->m_deferringAST);
    Assert(nop >= 0 && nop < knopLim);
    ParseNodePtr pnode;
    __analysis_assume(nop < knopLim);
    int cb = nop >= 0 && nop < knopLim ? g_mpnopcbNode[nop] : kcbPnNone;

    pnode = (ParseNodePtr)m_nodeAllocator.Alloc(cb);
    Assert(pnode);

    Assert(m_pCurrentAstSize != NULL);
    *m_pCurrentAstSize += cb;

    InitNode(nop,pnode);

    pnode->ichMin = ichMin;
    pnode->ichLim = ichLim;

    return pnode;
}

ParseNodePtr Parser::CreateNameNode(IdentPtr pid,charcount_t ichMin,charcount_t ichLim) {TRACE_IT(30767);
  ParseNodePtr pnode = CreateNodeT<knopName>(ichMin,ichLim);
  pnode->sxPid.pid = pid;
  pnode->sxPid.sym=NULL;
  pnode->sxPid.symRef=NULL;
  return pnode;
}

ParseNodePtr Parser::CreateUniNode(OpCode nop, ParseNodePtr pnode1, charcount_t ichMin,charcount_t ichLim)
{TRACE_IT(30768);
    Assert(!this->m_deferringAST);
    DebugOnly(VerifyNodeSize(nop, kcbPnUni));

    ParseNodePtr pnode = (ParseNodePtr)m_nodeAllocator.Alloc(kcbPnUni);

    Assert(m_pCurrentAstSize != NULL);
    *m_pCurrentAstSize += kcbPnUni;

    InitNode(nop, pnode);

    pnode->sxUni.pnode1 = pnode1;

    pnode->ichMin = ichMin;
    pnode->ichLim = ichLim;

    return pnode;
}

ParseNodePtr Parser::CreateBinNode(OpCode nop, ParseNodePtr pnode1,
                                   ParseNodePtr pnode2,charcount_t ichMin,charcount_t ichLim)
{TRACE_IT(30769);
    Assert(!this->m_deferringAST);
    ParseNodePtr pnode = StaticCreateBinNode(nop, pnode1, pnode2, &m_nodeAllocator);

    Assert(m_pCurrentAstSize != NULL);
    *m_pCurrentAstSize += kcbPnBin;

    pnode->ichMin = ichMin;
    pnode->ichLim = ichLim;

    return pnode;
}

ParseNodePtr Parser::CreateTriNode(OpCode nop, ParseNodePtr pnode1,
                                   ParseNodePtr pnode2, ParseNodePtr pnode3,
                                   charcount_t ichMin,charcount_t ichLim)
{TRACE_IT(30770);
    Assert(!this->m_deferringAST);
    DebugOnly(VerifyNodeSize(nop, kcbPnTri));
    ParseNodePtr pnode = (ParseNodePtr)m_nodeAllocator.Alloc(kcbPnTri);

    Assert(m_pCurrentAstSize != NULL);
    *m_pCurrentAstSize += kcbPnTri;

    InitNode(nop, pnode);

    pnode->sxTri.pnodeNext = NULL;
    pnode->sxTri.pnode1 = pnode1;
    pnode->sxTri.pnode2 = pnode2;
    pnode->sxTri.pnode3 = pnode3;

    pnode->ichMin = ichMin;
    pnode->ichLim = ichLim;

    return pnode;
}

bool PnBlock::HasBlockScopedContent() const
{TRACE_IT(30771);
    // A block has its own content if a let, const, or function is declared there.

    if (this->pnodeLexVars != nullptr || this->blockType == Parameter)
    {TRACE_IT(30772);
        return true;
    }

    // The enclosing scopes can contain functions and other things, so walk the list
    // looking specifically for functions.

    for (ParseNodePtr pnode = this->pnodeScopes; pnode;)
    {TRACE_IT(30773);
        switch (pnode->nop) {

        case knopFncDecl:
            return true;

        case knopBlock:
            pnode = pnode->sxBlock.pnodeNext;
            break;

        case knopCatch:
            pnode = pnode->sxCatch.pnodeNext;
            break;

        case knopWith:
            pnode = pnode->sxWith.pnodeNext;
            break;

        default:
            Assert(UNREACHED);
            return true;
        }
    }

    return false;
}

class ByteCodeGenerator;

// Copy AST; this works mostly on expressions for now
ParseNode* Parser::CopyPnode(ParseNode *pnode) {TRACE_IT(30774);
    if (pnode==NULL)
        return NULL;
    switch (pnode->nop) {
        //PTNODE(knopName       , "name"        ,None    ,Pid  ,fnopLeaf)
    case knopName: {TRACE_IT(30775);
      ParseNode* nameNode=CreateNameNode(pnode->sxPid.pid,pnode->ichMin,pnode->ichLim);
      nameNode->sxPid.sym=pnode->sxPid.sym;
      return nameNode;
    }
      //PTNODE(knopInt        , "int const"    ,None    ,Int  ,fnopLeaf|fnopConst)
  case knopInt:
    return pnode;
      //PTNODE(knopFlt        , "flt const"    ,None    ,Flt  ,fnopLeaf|fnopConst)
  case knopFlt:
    return pnode;
      //PTNODE(knopStr        , "str const"    ,None    ,Pid  ,fnopLeaf|fnopConst)
  case knopStr:
    return pnode;
      //PTNODE(knopRegExp     , "reg expr"    ,None    ,Pid  ,fnopLeaf|fnopConst)
  case knopRegExp:
    return pnode;
    break;
      //PTNODE(knopThis       , "this"        ,None    ,None ,fnopLeaf)
  case knopThis:
    return CreateNodeT<knopThis>(pnode->ichMin,pnode->ichLim);
      //PTNODE(knopNull       , "null"        ,Null    ,None ,fnopLeaf)
  case knopNull:
    return pnode;
      //PTNODE(knopFalse      , "false"        ,False   ,None ,fnopLeaf)
  case knopFalse:
    {TRACE_IT(30776);
      ParseNode* ret = CreateNodeT<knopFalse>(pnode->ichMin, pnode->ichLim);
      ret->location = pnode->location;
      return ret;
    }
      //PTNODE(knopTrue       , "true"        ,True    ,None ,fnopLeaf)
  case knopTrue:
    {TRACE_IT(30777);
        ParseNode* ret = CreateNodeT<knopTrue>(pnode->ichMin, pnode->ichLim);
        ret->location = pnode->location;
        return ret;
    }
      //PTNODE(knopEmpty      , "empty"        ,Empty   ,None ,fnopLeaf)
  case knopEmpty:
    return CreateNodeT<knopEmpty>(pnode->ichMin,pnode->ichLim);
      // Unary operators.
      //PTNODE(knopNot        , "~"            ,BitNot  ,Uni  ,fnopUni)
      //PTNODE(knopNeg        , "unary -"    ,Neg     ,Uni  ,fnopUni)
      //PTNODE(knopPos        , "unary +"    ,Pos     ,Uni  ,fnopUni)
      //PTNODE(knopLogNot     , "!"            ,LogNot  ,Uni  ,fnopUni)
      //PTNODE(knopEllipsis     , "..."       ,Spread  ,Uni    , fnopUni)
      //PTNODE(knopDecPost    , "-- post"    ,Dec     ,Uni  ,fnopUni|fnopAsg)
      //PTNODE(knopIncPre     , "++ pre"    ,Inc     ,Uni  ,fnopUni|fnopAsg)
      //PTNODE(knopDecPre     , "-- pre"    ,Dec     ,Uni  ,fnopUni|fnopAsg)
      //PTNODE(knopTypeof     , "typeof"    ,None    ,Uni  ,fnopUni)
      //PTNODE(knopVoid       , "void"        ,Void    ,Uni  ,fnopUni)
      //PTNODE(knopDelete     , "delete"    ,None    ,Uni  ,fnopUni)
  case knopNot:
  case knopNeg:
  case knopPos:
  case knopLogNot:
  case knopEllipsis:
  case knopIncPost:
  case knopDecPost:
  case knopIncPre:
  case knopDecPre:
  case knopTypeof:
  case knopVoid:
  case knopDelete:
    return CreateUniNode(pnode->nop,CopyPnode(pnode->sxUni.pnode1),pnode->ichMin,pnode->ichLim);
      //PTNODE(knopArray      , "arr cnst"    ,None    ,Uni  ,fnopUni)
      //PTNODE(knopObject     , "obj cnst"    ,None    ,Uni  ,fnopUni)
  case knopArray:
  case knopObject:
    // TODO: need to copy arr
    Assert(false);
    break;
      // Binary operators
      //PTNODE(knopAdd        , "+"            ,Add     ,Bin  ,fnopBin)
      //PTNODE(knopSub        , "-"            ,Sub     ,Bin  ,fnopBin)
      //PTNODE(knopMul        , "*"            ,Mul     ,Bin  ,fnopBin)
      //PTNODE(knopExpo       , "**"           ,Expo     ,Bin  ,fnopBin)
      //PTNODE(knopDiv        , "/"            ,Div     ,Bin  ,fnopBin)
      //PTNODE(knopMod        , "%"            ,Mod     ,Bin  ,fnopBin)
      //PTNODE(knopOr         , "|"            ,BitOr   ,Bin  ,fnopBin)
      //PTNODE(knopXor        , "^"            ,BitXor  ,Bin  ,fnopBin)
      //PTNODE(knopAnd        , "&"            ,BitAnd  ,Bin  ,fnopBin)
      //PTNODE(knopEq         , "=="        ,EQ      ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopNe         , "!="        ,NE      ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopLt         , "<"            ,LT      ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopLe         , "<="        ,LE      ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopGe         , ">="        ,GE      ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopGt         , ">"            ,GT      ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopEqv        , "==="        ,Eqv     ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopIn         , "in"        ,In      ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopInstOf     , "instanceof",InstOf  ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopNEqv       , "!=="        ,NEqv    ,Bin  ,fnopBin|fnopRel)
      //PTNODE(knopComma      , ","            ,None    ,Bin  ,fnopBin)
      //PTNODE(knopLogOr      , "||"        ,None    ,Bin  ,fnopBin)
      //PTNODE(knopLogAnd     , "&&"        ,None    ,Bin  ,fnopBin)
      //PTNODE(knopLsh        , "<<"        ,Lsh     ,Bin  ,fnopBin)
      //PTNODE(knopRsh        , ">>"        ,Rsh     ,Bin  ,fnopBin)
      //PTNODE(knopRs2        , ">>>"        ,Rs2     ,Bin  ,fnopBin)
  case knopAdd:
  case knopSub:
  case knopMul:
  case knopExpo:
  case knopDiv:
  case knopMod:
  case knopOr:
  case knopXor:
  case knopAnd:
  case knopEq:
  case knopNe:
  case knopLt:
  case knopLe:
  case knopGe:
  case knopGt:
  case knopEqv:
  case knopIn:
  case knopInstOf:
  case knopNEqv:
  case knopComma:
  case knopLogOr:
  case knopLogAnd:
  case knopLsh:
  case knopRsh:
  case knopRs2:
      //PTNODE(knopAsg        , "="            ,None    ,Bin  ,fnopBin|fnopAsg)
  case knopAsg:
      //PTNODE(knopDot        , "."            ,None    ,Bin  ,fnopBin)
  case knopDot:
      //PTNODE(knopAsgAdd     , "+="        ,Add     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgAdd:
      //PTNODE(knopAsgSub     , "-="        ,Sub     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgSub:
      //PTNODE(knopAsgMul     , "*="        ,Mul     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgMul:
      //PTNODE(knopAsgDiv     , "/="        ,Div     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgExpo:
      //PTNODE(knopAsgExpo    , "**="       ,Expo    ,Bin  ,fnopBin|fnopAsg)
  case knopAsgDiv:
      //PTNODE(knopAsgMod     , "%="        ,Mod     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgMod:
      //PTNODE(knopAsgAnd     , "&="        ,BitAnd  ,Bin  ,fnopBin|fnopAsg)
  case knopAsgAnd:
      //PTNODE(knopAsgXor     , "^="        ,BitXor  ,Bin  ,fnopBin|fnopAsg)
  case knopAsgXor:
      //PTNODE(knopAsgOr      , "|="        ,BitOr   ,Bin  ,fnopBin|fnopAsg)
  case knopAsgOr:
      //PTNODE(knopAsgLsh     , "<<="        ,Lsh     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgLsh:
      //PTNODE(knopAsgRsh     , ">>="        ,Rsh     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgRsh:
      //PTNODE(knopAsgRs2     , ">>>="        ,Rs2     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgRs2:
      //PTNODE(knopMember     , ":"            ,None    ,Bin  ,fnopBin)
  case knopMember:
  case knopMemberShort:
      //PTNODE(knopIndex      , "[]"        ,None    ,Bin  ,fnopBin)
      //PTNODE(knopList       , "<list>"    ,None    ,Bin  ,fnopNone)

  case knopIndex:
  case knopList:
    return CreateBinNode(pnode->nop,CopyPnode(pnode->sxBin.pnode1),
                         CopyPnode(pnode->sxBin.pnode2),pnode->ichMin,pnode->ichLim);

      //PTNODE(knopCall       , "()"        ,None    ,Bin  ,fnopBin)
      //PTNODE(knopNew        , "new"        ,None    ,Bin  ,fnopBin)
  case knopNew:
  case knopCall:
    return CreateCallNode(pnode->nop,CopyPnode(pnode->sxCall.pnodeTarget),
                         CopyPnode(pnode->sxCall.pnodeArgs),pnode->ichMin,pnode->ichLim);
      //PTNODE(knopQmark      , "?"            ,None    ,Tri  ,fnopBin)
  case knopQmark:
    return CreateTriNode(pnode->nop,CopyPnode(pnode->sxTri.pnode1),
                         CopyPnode(pnode->sxTri.pnode2),CopyPnode(pnode->sxTri.pnode3),
                         pnode->ichMin,pnode->ichLim);
      // General nodes.
      //PTNODE(knopVarDecl    , "varDcl"    ,None    ,Var  ,fnopNone)
    case knopVarDecl: {TRACE_IT(30778);
      ParseNode* copyNode=CreateNodeT<knopVarDecl>(pnode->ichMin,pnode->ichLim);
      copyNode->sxVar.pnodeInit=CopyPnode(pnode->sxVar.pnodeInit);
      copyNode->sxVar.sym=pnode->sxVar.sym;
      // TODO: mult-decl
      Assert(pnode->sxVar.pnodeNext==NULL);
      copyNode->sxVar.pnodeNext=NULL;
      return copyNode;
    }
      //PTNODE(knopFncDecl    , "fncDcl"    ,None    ,Fnc  ,fnopLeaf)
      //PTNODE(knopProg       , "program"    ,None    ,Fnc  ,fnopNone)
  case knopFncDecl:
  case knopProg:
    Assert(false);
    break;
      //PTNODE(knopEndCode    , "<endcode>"    ,None    ,None ,fnopNone)
  case knopEndCode:
    break;
      //PTNODE(knopDebugger   , "debugger"    ,None    ,None ,fnopNone)
  case knopDebugger:
    break;
      //PTNODE(knopFor        , "for"        ,None    ,For  ,fnopBreak|fnopContinue)
    case knopFor: {TRACE_IT(30779);
      ParseNode* copyNode=CreateNodeT<knopFor>(pnode->ichMin,pnode->ichLim);
      copyNode->sxFor.pnodeInverted=NULL;
      copyNode->sxFor.pnodeInit=CopyPnode(pnode->sxFor.pnodeInit);
      copyNode->sxFor.pnodeCond=CopyPnode(pnode->sxFor.pnodeCond);
      copyNode->sxFor.pnodeIncr=CopyPnode(pnode->sxFor.pnodeIncr);
      copyNode->sxFor.pnodeBody=CopyPnode(pnode->sxFor.pnodeBody);
      return copyNode;
    }
      //PTNODE(knopIf         , "if"        ,None    ,If   ,fnopNone)
  case knopIf:
    Assert(false);
    break;
      //PTNODE(knopWhile      , "while"        ,None    ,While,fnopBreak|fnopContinue)
  case knopWhile:
    Assert(false);
    break;
      //PTNODE(knopDoWhile    , "do-while"    ,None    ,While,fnopBreak|fnopContinue)
  case knopDoWhile:
    Assert(false);
    break;
      //PTNODE(knopForIn      , "for in"    ,None    ,ForIn,fnopBreak|fnopContinue|fnopCleanup)
  case knopForIn:
    Assert(false);
    break;
  case knopForOf:
    Assert(false);
    break;
      //PTNODE(knopReturn     , "return"    ,None    ,Uni  ,fnopNone)
  case knopReturn: {TRACE_IT(30780);
    ParseNode* copyNode=CreateNodeT<knopReturn>(pnode->ichMin,pnode->ichLim);
    copyNode->sxReturn.pnodeExpr=CopyPnode(pnode->sxReturn.pnodeExpr);
    return copyNode;
  }
      //PTNODE(knopBlock      , "{}"        ,None    ,Block,fnopNone)
  case knopBlock: {TRACE_IT(30781);
    ParseNode* copyNode=CreateBlockNode(pnode->ichMin,pnode->ichLim,pnode->sxBlock.blockType);
    if (pnode->grfpn & PNodeFlags::fpnSyntheticNode) {TRACE_IT(30782);
        // fpnSyntheticNode is sometimes set on PnodeBlockType::Regular blocks which
        // CreateBlockNode() will not automatically set for us, so set it here if it's
        // specified on the source node.
        copyNode->grfpn |= PNodeFlags::fpnSyntheticNode;
    }
    copyNode->sxBlock.pnodeStmt=CopyPnode(pnode->sxBlock.pnodeStmt);
    return copyNode;
  }
      //PTNODE(knopWith       , "with"        ,None    ,With ,fnopCleanup)
  case knopWith:
    Assert(false);
    break;
      //PTNODE(knopBreak      , "break"        ,None    ,Jump ,fnopNone)
  case knopBreak:
    Assert(false);
    break;
      //PTNODE(knopContinue   , "continue"    ,None    ,Jump ,fnopNone)
  case knopContinue:
    Assert(false);
    break;
      //PTNODE(knopLabel      , "label"        ,None    ,Label,fnopNone)
  case knopLabel:
    Assert(false);
    break;
      //PTNODE(knopSwitch     , "switch"    ,None    ,Switch,fnopBreak)
  case knopSwitch:
    Assert(false);
    break;
      //PTNODE(knopCase       , "case"        ,None    ,Case ,fnopNone)
  case knopCase:
    Assert(false);
    break;
      //PTNODE(knopTryFinally,"try-finally",None,TryFinally,fnopCleanup)
  case knopTryFinally:
    Assert(false);
    break;
  case knopFinally:
    Assert(false);
    break;
      //PTNODE(knopCatch      , "catch"     ,None    ,Catch,fnopNone)
  case knopCatch:
    Assert(false);
    break;
      //PTNODE(knopTryCatch      , "try-catch" ,None    ,TryCatch  ,fnopCleanup)
  case knopTryCatch:
    Assert(false);
    break;
      //PTNODE(knopTry        , "try"       ,None    ,Try  ,fnopCleanup)
  case knopTry:
    Assert(false);
    break;
      //PTNODE(knopThrow      , "throw"     ,None    ,Uni  ,fnopNone)
  case knopThrow:
    Assert(false);
    break;
  default:
    Assert(false);
    break;
    }
    return NULL;
}

// Returns true when str is string for Nan, Infinity or -Infinity.
// Does not check for double number value being in NaN/Infinity range.
// static
template<bool CheckForNegativeInfinity>
inline bool Parser::IsNaNOrInfinityLiteral(LPCOLESTR str)
{TRACE_IT(30783);
    // Note: wcscmp crashes when one of the parameters is NULL.
    return str &&
           (wcscmp(_u("NaN"), str) == 0 ||
           wcscmp(_u("Infinity"), str) == 0 ||
               (CheckForNegativeInfinity && wcscmp(_u("-Infinity"), str) == 0));
}

template <bool buildAST>
ParseNodePtr Parser::ParseSuper(ParseNodePtr pnode, bool fAllowCall)
{TRACE_IT(30784);
    ParseNodePtr currentNodeFunc = GetCurrentFunctionNode();

    if (buildAST) {TRACE_IT(30785);
        pnode = CreateNodeWithScanner<knopSuper>();
    }

    m_pscan->ScanForcingPid();

    switch (m_token.tk)
    {
    case tkDot:     // super.prop
    case tkLBrack:  // super[foo]
    case tkLParen:  // super(args)
        break;

    default:
        Error(ERRInvalidSuper);
        break;
    }

    if (!fAllowCall && (m_token.tk == tkLParen))
    {TRACE_IT(30786);
        Error(ERRInvalidSuper); // new super() is not allowed
    }
    else if (this->m_parsingSuperRestrictionState == ParsingSuperRestrictionState_SuperCallAndPropertyAllowed)
    {TRACE_IT(30787);
        // Any super access is good within a class constructor
    }
    else if (this->m_parsingSuperRestrictionState == ParsingSuperRestrictionState_SuperPropertyAllowed)
    {TRACE_IT(30788);
        if (m_token.tk == tkLParen)
        {TRACE_IT(30789);
            if ((this->m_grfscr & fscrEval) == fscrNil)
            {TRACE_IT(30790);
                // Cannot call super within a class member
                Error(ERRInvalidSuper);
            }
            else
            {TRACE_IT(30791);
                Js::JavascriptFunction * caller = nullptr;
                if (Js::JavascriptStackWalker::GetCaller(&caller, m_scriptContext))
                {TRACE_IT(30792);
                    Js::FunctionBody * callerBody = caller->GetFunctionBody();
                    Assert(callerBody);
                    if (!callerBody->GetFunctionInfo()->GetAllowDirectSuper())
                    {TRACE_IT(30793);
                        Error(ERRInvalidSuper);
                    }
                }
            }
        }
    }
    else
    {TRACE_IT(30794);
        // Anything else is an error
        Error(ERRInvalidSuper);
    }

    currentNodeFunc->sxFnc.SetHasSuperReference(TRUE);
    CHAKRATEL_LANGSTATS_INC_LANGFEATURECOUNT(Super, m_scriptContext);
    return pnode;
}

void Parser::AppendToList(ParseNodePtr *node, ParseNodePtr nodeToAppend)
{TRACE_IT(30795);
    Assert(nodeToAppend);
    ParseNodePtr* lastPtr = node;
    while ((*lastPtr) && (*lastPtr)->nop == knopList)
    {TRACE_IT(30796);
        lastPtr = &(*lastPtr)->sxBin.pnode2;
    }
    auto last = (*lastPtr);
    if (last)
    {TRACE_IT(30797);
        *lastPtr = CreateBinNode(knopList, last, nodeToAppend, last->ichMin, nodeToAppend->ichLim);
    }
    else
    {TRACE_IT(30798);
        *lastPtr = nodeToAppend;
    }
}

ParseNodePtr Parser::ConvertArrayToArrayPattern(ParseNodePtr pnode)
{TRACE_IT(30799);
    Assert(pnode->nop == knopArray);
    pnode->nop = knopArrayPattern;

    ForEachItemRefInList(&pnode->sxArrLit.pnode1, [&](ParseNodePtr *itemRef) {
        ParseNodePtr item = *itemRef;
        if (item->nop == knopEllipsis)
        {TRACE_IT(30800);
            itemRef = &item->sxUni.pnode1;
            item = *itemRef;
            if (!(item->nop == knopName
                  || item->nop == knopDot
                  || item->nop == knopIndex
                  || item->nop == knopArray
                  || item->nop == knopObject))
            {TRACE_IT(30801);
                Error(ERRInvalidAssignmentTarget);
            }
        }
        else if (item->nop == knopAsg)
        {TRACE_IT(30802);
            itemRef = &item->sxBin.pnode1;
            item = *itemRef;
        }

        if (item->nop == knopArray)
        {TRACE_IT(30803);
            ConvertArrayToArrayPattern(item);
        }
        else if (item->nop == knopObject)
        {TRACE_IT(30804);
            *itemRef = ConvertObjectToObjectPattern(item);
        }
        else if (item->nop == knopName)
        {
            TrackAssignment<true>(item, nullptr);
        }
    });

    return pnode;
}

ParseNodePtr Parser::CreateParamPatternNode(ParseNodePtr pnode1)
{TRACE_IT(30805);
    ParseNodePtr paramPatternNode = CreateNode(knopParamPattern, pnode1->ichMin, pnode1->ichLim);
    paramPatternNode->sxParamPattern.pnode1 = pnode1;
    paramPatternNode->sxParamPattern.pnodeNext = nullptr;
    paramPatternNode->sxParamPattern.location = Js::Constants::NoRegister;
    return paramPatternNode;
}

ParseNodePtr Parser::ConvertObjectToObjectPattern(ParseNodePtr pnodeMemberList)
{TRACE_IT(30806);
    charcount_t ichMin = m_pscan->IchMinTok();
    charcount_t ichLim = m_pscan->IchLimTok();
    ParseNodePtr pnodeMemberNodeList = nullptr;
    if (pnodeMemberList != nullptr && pnodeMemberList->nop == knopObject)
    {TRACE_IT(30807);
        ichMin = pnodeMemberList->ichMin;
        ichLim = pnodeMemberList->ichLim;
        pnodeMemberList = pnodeMemberList->sxUni.pnode1;
    }

    ForEachItemInList(pnodeMemberList, [&](ParseNodePtr item) {
        ParseNodePtr memberNode = ConvertMemberToMemberPattern(item);
        AppendToList(&pnodeMemberNodeList, memberNode);
    });

    return CreateUniNode(knopObjectPattern, pnodeMemberNodeList, ichMin, ichLim);
}

ParseNodePtr Parser::GetRightSideNodeFromPattern(ParseNodePtr pnode)
{TRACE_IT(30808);
    Assert(pnode != nullptr);
    ParseNodePtr rightNode = nullptr;
    OpCode op = pnode->nop;
    if (op == knopObject)
    {TRACE_IT(30809);
        rightNode = ConvertObjectToObjectPattern(pnode);
    }
    else if (op == knopArray)
    {TRACE_IT(30810);
        rightNode = ConvertArrayToArrayPattern(pnode);
    }
    else
    {TRACE_IT(30811);
        rightNode = pnode;
        if (op == knopName)
        {
            TrackAssignment<true>(pnode, nullptr);
        }
    }

    return rightNode;
}

ParseNodePtr Parser::ConvertMemberToMemberPattern(ParseNodePtr pnodeMember)
{TRACE_IT(30812);
    if (pnodeMember->nop == knopObjectPatternMember)
    {TRACE_IT(30813);
        return pnodeMember;
    }

    Assert(pnodeMember->nop == knopMember || pnodeMember->nop == knopMemberShort);

    ParseNodePtr rightNode = GetRightSideNodeFromPattern(pnodeMember->sxBin.pnode2);
    ParseNodePtr resultNode = CreateBinNode(knopObjectPatternMember, pnodeMember->sxBin.pnode1, rightNode);
    resultNode->ichMin = pnodeMember->ichMin;
    resultNode->ichLim = pnodeMember->ichLim;
    return resultNode;
}

ParseNodePtr Parser::ConvertToPattern(ParseNodePtr pnode)
{TRACE_IT(30814);
    if (pnode != nullptr)
    {TRACE_IT(30815);
        if (pnode->nop == knopArray)
        {TRACE_IT(30816);
            ConvertArrayToArrayPattern(pnode);
        }
        else if (pnode->nop == knopObject)
        {TRACE_IT(30817);
            pnode = ConvertObjectToObjectPattern(pnode);
        }
    }
    return pnode;
}

// This essentially be called for verifying the structure of the current tree with satisfying the destructuring grammar.
void Parser::ParseDestructuredLiteralWithScopeSave(tokens declarationType,
    bool isDecl,
    bool topLevel,
    DestructuringInitializerContext initializerContext/* = DIC_None*/,
    bool allowIn /*= true*/)
{TRACE_IT(30818);
    // We are going to parse the text again to validate the current grammar as Destructuring. Saving some scopes and
    // AST related information before the validation parsing and later they will be restored.

    ParseNodePtr pnodeFncSave = m_currentNodeFunc;
    ParseNodePtr pnodeDeferredFncSave = m_currentNodeDeferredFunc;
    if (m_currentNodeDeferredFunc == nullptr)
    {TRACE_IT(30819);
        m_currentNodeDeferredFunc = m_currentNodeFunc;
    }
    int32 *pAstSizeSave = m_pCurrentAstSize;
    uint *pNestedCountSave = m_pnestedCount;
    ParseNodePtr *ppnodeScopeSave = m_ppnodeScope;
    ParseNodePtr *ppnodeExprScopeSave = m_ppnodeExprScope;

    ParseNodePtr newTempScope = nullptr;
    m_ppnodeScope = &newTempScope;

    int32 newTempAstSize = 0;
    m_pCurrentAstSize = &newTempAstSize;

    uint newTempNestedCount = 0;
    m_pnestedCount = &newTempNestedCount;

    m_ppnodeExprScope = nullptr;

    charcount_t funcInArraySave = m_funcInArray;
    uint funcInArrayDepthSave = m_funcInArrayDepth;

    // we need to reset this as we are going to parse the grammar again.
    m_hasDeferredShorthandInitError = false;

    ParseDestructuredLiteral<false>(declarationType, isDecl, topLevel, initializerContext, allowIn);

    m_currentNodeFunc = pnodeFncSave;
    m_currentNodeDeferredFunc = pnodeDeferredFncSave;
    m_pCurrentAstSize = pAstSizeSave;
    m_pnestedCount = pNestedCountSave;
    m_ppnodeScope = ppnodeScopeSave;
    m_ppnodeExprScope = ppnodeExprScopeSave;
    m_funcInArray = funcInArraySave;
    m_funcInArrayDepth = funcInArrayDepthSave;
}

template <bool buildAST>
ParseNodePtr Parser::ParseDestructuredLiteral(tokens declarationType,
    bool isDecl,
    bool topLevel/* = true*/,
    DestructuringInitializerContext initializerContext/* = DIC_None*/,
    bool allowIn/* = true*/,
    BOOL *forInOfOkay/* = nullptr*/,
    BOOL *nativeForOkay/* = nullptr*/)
{TRACE_IT(30820);
    ParseNodePtr pnode = nullptr;
    Assert(IsPossiblePatternStart());
    if (m_token.tk == tkLCurly)
    {TRACE_IT(30821);
        pnode = ParseDestructuredObjectLiteral<buildAST>(declarationType, isDecl, topLevel);
    }
    else
    {TRACE_IT(30822);
        pnode = ParseDestructuredArrayLiteral<buildAST>(declarationType, isDecl, topLevel);
    }

    return ParseDestructuredInitializer<buildAST>(pnode, isDecl, topLevel, initializerContext, allowIn, forInOfOkay, nativeForOkay);
}

template <bool buildAST>
ParseNodePtr Parser::ParseDestructuredInitializer(ParseNodePtr lhsNode,
    bool isDecl,
    bool topLevel,
    DestructuringInitializerContext initializerContext,
    bool allowIn,
    BOOL *forInOfOkay,
    BOOL *nativeForOkay)
{TRACE_IT(30823);
    m_pscan->Scan();
    if (topLevel && nativeForOkay == nullptr)
    {TRACE_IT(30824);
        if (initializerContext != DIC_ForceErrorOnInitializer && m_token.tk != tkAsg)
        {TRACE_IT(30825);
            // e.g. var {x};
            Error(ERRDestructInit);
        }
        else if (initializerContext == DIC_ForceErrorOnInitializer && m_token.tk == tkAsg)
        {TRACE_IT(30826);
            // e.g. catch([x] = [0])
            Error(ERRDestructNotInit);
        }
    }

    if (m_token.tk != tkAsg || initializerContext == DIC_ShouldNotParseInitializer)
    {TRACE_IT(30827);
        if (topLevel && nativeForOkay != nullptr)
        {TRACE_IT(30828);
            // Native loop should have destructuring initializer
            *nativeForOkay = FALSE;
        }

        return lhsNode;
    }

    if (forInOfOkay)
    {TRACE_IT(30829);
        *forInOfOkay = FALSE;
    }

    m_pscan->Scan();


    bool alreadyHasInitError = m_hasDeferredShorthandInitError;

    ParseNodePtr pnodeDefault = ParseExpr<buildAST>(koplCma, nullptr, allowIn);

    if (m_hasDeferredShorthandInitError && !alreadyHasInitError)
    {TRACE_IT(30830);
        Error(ERRnoColon);
    }

    ParseNodePtr pnodeDestructAsg = nullptr;
    if (buildAST)
    {TRACE_IT(30831);
        Assert(lhsNode != nullptr);

        pnodeDestructAsg = CreateNodeWithScanner<knopAsg>();
        pnodeDestructAsg->sxBin.pnode1 = lhsNode;
        pnodeDestructAsg->sxBin.pnode2 = pnodeDefault;
        pnodeDestructAsg->ichMin = lhsNode->ichMin;
        pnodeDestructAsg->ichLim = pnodeDefault->ichLim;
    }
    return pnodeDestructAsg;
}

template <bool buildAST>
ParseNodePtr Parser::ParseDestructuredObjectLiteral(tokens declarationType, bool isDecl, bool topLevel/* = true*/)
{TRACE_IT(30832);
    Assert(m_token.tk == tkLCurly);
    charcount_t ichMin = m_pscan->IchMinTok();
    m_pscan->Scan();

    if (!isDecl)
    {TRACE_IT(30833);
        declarationType = tkLCurly;
    }
    ParseNodePtr pnodeMemberList = ParseMemberList<buildAST>(nullptr/*pNameHint*/, nullptr/*pHintLength*/, declarationType);
    Assert(m_token.tk == tkRCurly);

    ParseNodePtr objectPatternNode = nullptr;
    if (buildAST)
    {TRACE_IT(30834);
        charcount_t ichLim = m_pscan->IchLimTok();
        objectPatternNode = CreateUniNode(knopObjectPattern, pnodeMemberList, ichMin, ichLim);
    }
    return objectPatternNode;
}

template <bool buildAST>
ParseNodePtr Parser::ParseDestructuredVarDecl(tokens declarationType, bool isDecl, bool *hasSeenRest, bool topLevel/* = true*/, bool allowEmptyExpression/* = true*/)
{TRACE_IT(30835);
    ParseNodePtr pnodeElem = nullptr;
    int parenCount = 0;
    bool seenRest = false;

    // Save the Block ID prior to the increments, so we can restore it back.
    int originalCurrentBlockId = GetCurrentBlock()->sxBlock.blockId;

    // Eat the left parentheses only when its not a declaration. This will make sure we throw syntax errors early.
    if (!isDecl)
    {TRACE_IT(30836);
        while (m_token.tk == tkLParen)
        {TRACE_IT(30837);
            m_pscan->Scan();
            ++parenCount;

            // Match the block increment we do upon entering parenthetical expressions
            // so that the block ID's will match on reparsing of parameters.
            GetCurrentBlock()->sxBlock.blockId = m_nextBlockId++;
        }
    }

    if (m_token.tk == tkEllipsis)
    {TRACE_IT(30838);
        // As per ES 2015 : Rest can have left-hand-side-expression when on assignment expression, but under declaration only binding identifier is allowed
        // But spec is going to change for this one to allow LHS-expression both on expression and declaration - so making that happen early.

        seenRest = true;
        m_pscan->Scan();

        // Eat the left parentheses only when its not a declaration. This will make sure we throw syntax errors early.
        if (!isDecl)
        {TRACE_IT(30839);
            while (m_token.tk == tkLParen)
            {TRACE_IT(30840);
                m_pscan->Scan();
                ++parenCount;

                // Match the block increment we do upon entering parenthetical expressions
                // so that the block ID's will match on reparsing of parameters.
                GetCurrentBlock()->sxBlock.blockId = m_nextBlockId++;
            }
        }

        if (m_token.tk != tkID && m_token.tk != tkTHIS && m_token.tk != tkSUPER && m_token.tk != tkLCurly && m_token.tk != tkLBrack)
        {TRACE_IT(30841);
            if (isDecl)
            {TRACE_IT(30842);
                Error(ERRnoIdent);
            }
            else
            {TRACE_IT(30843);
                Error(ERRInvalidAssignmentTarget);
            }
        }
    }

    if (IsPossiblePatternStart())
    {TRACE_IT(30844);
        // Go recursively
        pnodeElem = ParseDestructuredLiteral<buildAST>(declarationType, isDecl, false /*topLevel*/, seenRest ? DIC_ShouldNotParseInitializer : DIC_None);
        if (!isDecl)
        {TRACE_IT(30845);
            BOOL fCanAssign;
            IdentToken token;
            // Look for postfix operator
            pnodeElem = ParsePostfixOperators<buildAST>(pnodeElem, TRUE, FALSE, FALSE, &fCanAssign, &token);
        }
    }
    else if (m_token.tk == tkSUPER || m_token.tk == tkID || m_token.tk == tkTHIS)
    {TRACE_IT(30846);
        if (isDecl)
        {TRACE_IT(30847);
            charcount_t ichMin = m_pscan->IchMinTok();
            pnodeElem = ParseVariableDeclaration<buildAST>(declarationType, ichMin
                ,/* fAllowIn */false, /* pfForInOk */nullptr, /* singleDefOnly */true, /* allowInit */!seenRest, false /*topLevelParse*/);

        }
        else
        {TRACE_IT(30848);
            BOOL fCanAssign;
            IdentToken token;
            // We aren't declaring anything, so scan the ID reference manually.
            pnodeElem = ParseTerm<buildAST>(/* fAllowCall */ m_token.tk != tkSUPER, nullptr /*pNameHint*/, nullptr /*pHintLength*/, nullptr /*pShortNameOffset*/, &token, false,
                                                             &fCanAssign);

            // In this destructuring case we can force error here as we cannot assign.

            if (!fCanAssign)
            {TRACE_IT(30849);
                Error(ERRInvalidAssignmentTarget);
            }

            if (buildAST)
            {TRACE_IT(30850);
                if (IsStrictMode() && pnodeElem != nullptr && pnodeElem->nop == knopName)
                {TRACE_IT(30851);
                    CheckStrictModeEvalArgumentsUsage(pnodeElem->sxPid.pid);
                }
            }
            else
            {TRACE_IT(30852);
                if (IsStrictMode() && token.tk == tkID)
                {TRACE_IT(30853);
                    CheckStrictModeEvalArgumentsUsage(token.pid);
                }
                token.tk = tkNone;
            }
        }
    }
    else if (!((m_token.tk == tkComma || m_token.tk == tkRBrack || m_token.tk == tkRCurly) && allowEmptyExpression))
    {TRACE_IT(30854);
        if (m_token.IsOperator())
        {TRACE_IT(30855);
            Error(ERRDestructNoOper);
        }
        Error(ERRDestructIDRef);
    }

    // Swallow RParens before a default expression, if any.
    // We eat the left parentheses only when its not a declaration. This will make sure we throw syntax errors early. We need to do the same for right parentheses.
    if (!isDecl)
    {TRACE_IT(30856);
        while (m_token.tk == tkRParen)
        {TRACE_IT(30857);
            m_pscan->Scan();
            --parenCount;
        }
    }

    if (hasSeenRest != nullptr)
    {TRACE_IT(30858);
        *hasSeenRest = seenRest;
    }

    if (m_token.tk == tkAsg)
    {TRACE_IT(30859);
        // Parse the initializer.
        if (seenRest)
        {TRACE_IT(30860);
            Error(ERRRestWithDefault);
        }
        m_pscan->Scan();

        bool alreadyHasInitError = m_hasDeferredShorthandInitError;
        ParseNodePtr pnodeInit = ParseExpr<buildAST>(koplCma);

        if (m_hasDeferredShorthandInitError && !alreadyHasInitError)
        {TRACE_IT(30861);
            Error(ERRnoColon);
        }

        if (buildAST)
        {TRACE_IT(30862);
            pnodeElem = CreateBinNode(knopAsg, pnodeElem, pnodeInit);
        }
    }

    if (buildAST && seenRest)
    {TRACE_IT(30863);
        ParseNodePtr pnodeRest = CreateNodeWithScanner<knopEllipsis>();
        pnodeRest->sxUni.pnode1 = pnodeElem;
        pnodeElem = pnodeRest;
    }

    // We eat the left parentheses only when its not a declaration. This will make sure we throw syntax errors early. We need to do the same for right parentheses.
    if (!isDecl)
    {TRACE_IT(30864);
        while (m_token.tk == tkRParen)
        {TRACE_IT(30865);
            m_pscan->Scan();
            --parenCount;
        }

        // Restore the Block ID of the current block after the parsing of destructured variable declarations and initializers.
        GetCurrentBlock()->sxBlock.blockId = originalCurrentBlockId;
    }

    if (!(m_token.tk == tkComma || m_token.tk == tkRBrack || m_token.tk == tkRCurly))
    {TRACE_IT(30866);
        if (m_token.IsOperator())
        {TRACE_IT(30867);
            Error(ERRDestructNoOper);
        }
        Error(ERRsyntax);
    }

    if (parenCount != 0)
    {TRACE_IT(30868);
        Error(ERRnoRparen);
    }
    return pnodeElem;
}

template <bool buildAST>
ParseNodePtr Parser::ParseDestructuredArrayLiteral(tokens declarationType, bool isDecl, bool topLevel)
{TRACE_IT(30869);
    Assert(m_token.tk == tkLBrack);
    charcount_t ichMin = m_pscan->IchMinTok();

    m_pscan->Scan();

    ParseNodePtr pnodeDestructArr = nullptr;
    ParseNodePtr pnodeList = nullptr;
    ParseNodePtr *lastNodeRef = nullptr;
    uint count = 0;
    bool hasMissingValues = false;
    bool seenRest = false;

    if (m_token.tk != tkRBrack)
    {TRACE_IT(30870);
        while (true)
        {TRACE_IT(30871);
            ParseNodePtr pnodeElem = ParseDestructuredVarDecl<buildAST>(declarationType, isDecl, &seenRest, topLevel);
            if (buildAST)
            {TRACE_IT(30872);
                if (pnodeElem == nullptr && buildAST)
                {TRACE_IT(30873);
                    pnodeElem = CreateNodeWithScanner<knopEmpty>();
                    hasMissingValues = true;
                }
                AddToNodeListEscapedUse(&pnodeList, &lastNodeRef, pnodeElem);
            }
            count++;

            if (m_token.tk == tkRBrack)
            {TRACE_IT(30874);
                break;
            }

            if (m_token.tk != tkComma)
            {TRACE_IT(30875);
                Error(ERRDestructNoOper);
            }

            if (seenRest) // Rest must be in the last position.
            {TRACE_IT(30876);
                Error(ERRDestructRestLast);
            }

            m_pscan->Scan();

            // break if we have the trailing comma as well, eg. [a,]
            if (m_token.tk == tkRBrack)
            {TRACE_IT(30877);
                break;
            }
        }
    }

    if (buildAST)
    {TRACE_IT(30878);
        pnodeDestructArr = CreateNodeWithScanner<knopArrayPattern>();
        pnodeDestructArr->sxArrLit.pnode1 = pnodeList;
        pnodeDestructArr->sxArrLit.arrayOfTaggedInts = false;
        pnodeDestructArr->sxArrLit.arrayOfInts = false;
        pnodeDestructArr->sxArrLit.arrayOfNumbers = false;
        pnodeDestructArr->sxArrLit.hasMissingValues = hasMissingValues;
        pnodeDestructArr->sxArrLit.count = count;
        pnodeDestructArr->sxArrLit.spreadCount = seenRest ? 1 : 0;
        pnodeDestructArr->ichMin = ichMin;
        pnodeDestructArr->ichLim = m_pscan->IchLimTok();

        if (pnodeDestructArr->sxArrLit.pnode1)
        {TRACE_IT(30879);
            this->CheckArguments(pnodeDestructArr->sxArrLit.pnode1);
        }
    }

    return pnodeDestructArr;
}

void Parser::CaptureContext(ParseContext *parseContext) const
{TRACE_IT(30880);
    parseContext->pszSrc = m_pscan->PchBase();
    parseContext->length = this->m_originalLength;
    parseContext->characterOffset = m_pscan->IchMinTok();
    parseContext->offset = parseContext->characterOffset + m_pscan->m_cMultiUnits;
    parseContext->grfscr = this->m_grfscr;
    parseContext->lineNumber = m_pscan->LineCur();

    parseContext->pnodeProg = this->m_currentNodeProg;
    parseContext->fromExternal = m_pscan->IsFromExternalSource();
    parseContext->strictMode = this->IsStrictMode();
    parseContext->sourceContextInfo = this->m_sourceContextInfo;
    parseContext->currentBlockInfo = this->m_currentBlockInfo;
    parseContext->nextBlockId = this->m_nextBlockId;
}

void Parser::RestoreContext(ParseContext *const parseContext)
{TRACE_IT(30881);
    m_sourceContextInfo = parseContext->sourceContextInfo;
    m_currentBlockInfo = parseContext->currentBlockInfo;
    m_nextBlockId = parseContext->nextBlockId;
    m_grfscr = parseContext->grfscr;
    m_length = parseContext->length;
    m_pscan->SetText(parseContext->pszSrc, parseContext->offset, parseContext->length, parseContext->characterOffset, parseContext->grfscr, parseContext->lineNumber);
    m_currentNodeProg = parseContext->pnodeProg;
    m_fUseStrictMode = parseContext->strictMode;
}

class ByteCodeGenerator;
#if DBG_DUMP

#define INDENT_SIZE 2

void PrintPnodeListWIndent(ParseNode *pnode,int indentAmt);
void PrintFormalsWIndent(ParseNode *pnode, int indentAmt);


void Indent(int indentAmt) {TRACE_IT(30882);
    for (int i=0;i<indentAmt;i++) {TRACE_IT(30883);
        Output::Print(_u(" "));
    }
}

void PrintBlockType(PnodeBlockType type)
{TRACE_IT(30884);
    switch (type)
    {
    case Global:
        Output::Print(_u("(Global)"));
        break;
    case Function:
        Output::Print(_u("(Function)"));
        break;
    case Regular:
        Output::Print(_u("(Regular)"));
        break;
    case Parameter:
        Output::Print(_u("(Parameter)"));
        break;
    default:
        Output::Print(_u("(unknown blocktype)"));
        break;
    }
}

void PrintScopesWIndent(ParseNode *pnode,int indentAmt) {TRACE_IT(30885);
    ParseNode *scope = nullptr;
    bool firstOnly = false;
    switch(pnode->nop)
    {
    case knopProg:
    case knopFncDecl: scope = pnode->sxFnc.pnodeScopes; break;
    case knopBlock: scope = pnode->sxBlock.pnodeScopes; break;
    case knopCatch: scope = pnode->sxCatch.pnodeScopes; break;
    case knopWith: scope = pnode->sxWith.pnodeScopes; break;
    case knopSwitch: scope = pnode->sxSwitch.pnodeBlock; firstOnly = true; break;
    case knopFor: scope = pnode->sxFor.pnodeBlock; firstOnly = true; break;
    case knopForIn: scope = pnode->sxForInOrForOf.pnodeBlock; firstOnly = true; break;
    case knopForOf: scope = pnode->sxForInOrForOf.pnodeBlock; firstOnly = true; break;
    }
    if (scope) {TRACE_IT(30886);
        Output::Print(_u("[%4d, %4d): "), scope->ichMin, scope->ichLim);
        Indent(indentAmt);
        Output::Print(_u("Scopes: "));
        ParseNode *next = nullptr;
        ParseNode *syntheticBlock = nullptr;
        while (scope) {TRACE_IT(30887);
            switch (scope->nop) {
            case knopFncDecl: Output::Print(_u("knopFncDecl")); next = scope->sxFnc.pnodeNext; break;
            case knopBlock: Output::Print(_u("knopBlock")); PrintBlockType(scope->sxBlock.blockType); next = scope->sxBlock.pnodeNext; break;
            case knopCatch: Output::Print(_u("knopCatch")); next = scope->sxCatch.pnodeNext; break;
            case knopWith: Output::Print(_u("knopWith")); next = scope->sxWith.pnodeNext; break;
            default: Output::Print(_u("unknown")); break;
            }
            if (firstOnly) {TRACE_IT(30888);
                next = nullptr;
                syntheticBlock = scope;
            }
            if (scope->grfpn & fpnSyntheticNode) {TRACE_IT(30889);
                Output::Print(_u(" synthetic"));
                if (scope->nop == knopBlock)
                    syntheticBlock = scope;
            }
            Output::Print(_u(" (%d-%d)"), scope->ichMin, scope->ichLim);
            if (next) Output::Print(_u(", "));
            scope = next;
        }
        Output::Print(_u("\n"));
        if (syntheticBlock || firstOnly) {
            PrintScopesWIndent(syntheticBlock, indentAmt + INDENT_SIZE);
        }
    }
}

void PrintPnodeWIndent(ParseNode *pnode,int indentAmt) {TRACE_IT(30890);
    if (pnode==NULL)
        return;

    Output::Print(_u("[%4d, %4d): "), pnode->ichMin, pnode->ichLim);
    switch (pnode->nop) {
        //PTNODE(knopName       , "name"        ,None    ,Pid  ,fnopLeaf)
  case knopName:
      Indent(indentAmt);
      if (pnode->sxPid.pid!=NULL) {TRACE_IT(30891);
        Output::Print(_u("id: %s\n"),pnode->sxPid.pid->Psz());
      }
      else {TRACE_IT(30892);
        Output::Print(_u("name node\n"));
      }
      break;
      //PTNODE(knopInt        , "int const"    ,None    ,Int  ,fnopLeaf|fnopConst)
  case knopInt:
      Indent(indentAmt);
      Output::Print(_u("%d\n"),pnode->sxInt.lw);
      break;
      //PTNODE(knopFlt        , "flt const"    ,None    ,Flt  ,fnopLeaf|fnopConst)
  case knopFlt:
      Indent(indentAmt);
      Output::Print(_u("%lf\n"),pnode->sxFlt.dbl);
      break;
      //PTNODE(knopStr        , "str const"    ,None    ,Pid  ,fnopLeaf|fnopConst)
  case knopStr:
      Indent(indentAmt);
      Output::Print(_u("\"%s\"\n"),pnode->sxPid.pid->Psz());
      break;
      //PTNODE(knopRegExp     , "reg expr"    ,None    ,Pid  ,fnopLeaf|fnopConst)
  case knopRegExp:
      Indent(indentAmt);
      Output::Print(_u("/%x/\n"),pnode->sxPid.regexPattern);
      break;
      //PTNODE(knopThis       , "this"        ,None    ,None ,fnopLeaf)
  case knopThis:
      Indent(indentAmt);
      Output::Print(_u("this\n"));
      break;
      //PTNODE(knopSuper      , "super"       ,None    ,None ,fnopLeaf)
  case knopSuper:
      Indent(indentAmt);
      Output::Print(_u("super\n"));
      break;
      //PTNODE(knopNewTarget  , "new.target"  ,None    ,None ,fnopLeaf)
  case knopNewTarget:
      Indent(indentAmt);
      Output::Print(_u("new.target\n"));
      break;
      //PTNODE(knopNull       , "null"        ,Null    ,None ,fnopLeaf)
  case knopNull:
      Indent(indentAmt);
      Output::Print(_u("null\n"));
      break;
      //PTNODE(knopFalse      , "false"        ,False   ,None ,fnopLeaf)
  case knopFalse:
      Indent(indentAmt);
      Output::Print(_u("false\n"));
      break;
      //PTNODE(knopTrue       , "true"        ,True    ,None ,fnopLeaf)
  case knopTrue:
      Indent(indentAmt);
      Output::Print(_u("true\n"));
      break;
      //PTNODE(knopEmpty      , "empty"        ,Empty   ,None ,fnopLeaf)
  case knopEmpty:
      Indent(indentAmt);
      Output::Print(_u("empty\n"));
      break;
      // Unary operators.
      //PTNODE(knopNot        , "~"            ,BitNot  ,Uni  ,fnopUni)
  case knopNot:
      Indent(indentAmt);
      Output::Print(_u("~\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopNeg        , "unary -"    ,Neg     ,Uni  ,fnopUni)
  case knopNeg:
      Indent(indentAmt);
      Output::Print(_u("U-\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopPos        , "unary +"    ,Pos     ,Uni  ,fnopUni)
  case knopPos:
      Indent(indentAmt);
      Output::Print(_u("U+\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopLogNot     , "!"            ,LogNot  ,Uni  ,fnopUni)
  case knopLogNot:
      Indent(indentAmt);
      Output::Print(_u("!\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopEllipsis     , "..."       ,Spread  ,Uni    , fnopUni)
  case knopEllipsis:
      Indent(indentAmt);
      Output::Print(_u("...<expr>\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopIncPost    , "++ post"    ,Inc     ,Uni  ,fnopUni|fnopAsg)
  case knopIncPost:
      Indent(indentAmt);
      Output::Print(_u("<expr>++\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopDecPost    , "-- post"    ,Dec     ,Uni  ,fnopUni|fnopAsg)
  case knopDecPost:
      Indent(indentAmt);
      Output::Print(_u("<expr>--\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopIncPre     , "++ pre"    ,Inc     ,Uni  ,fnopUni|fnopAsg)
  case knopIncPre:
      Indent(indentAmt);
      Output::Print(_u("++<expr>\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopDecPre     , "-- pre"    ,Dec     ,Uni  ,fnopUni|fnopAsg)
  case knopDecPre:
      Indent(indentAmt);
      Output::Print(_u("--<expr>\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopTypeof     , "typeof"    ,None    ,Uni  ,fnopUni)
  case knopTypeof:
      Indent(indentAmt);
      Output::Print(_u("typeof\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopVoid       , "void"        ,Void    ,Uni  ,fnopUni)
  case knopVoid:
      Indent(indentAmt);
      Output::Print(_u("void\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopDelete     , "delete"    ,None    ,Uni  ,fnopUni)
  case knopDelete:
      Indent(indentAmt);
      Output::Print(_u("delete\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopArray      , "arr cnst"    ,None    ,Uni  ,fnopUni)

  case knopArrayPattern:
      Indent(indentAmt);
      Output::Print(_u("Array Pattern\n"));
      PrintPnodeListWIndent(pnode->sxUni.pnode1, indentAmt + INDENT_SIZE);
      break;

  case knopObjectPattern:
      Indent(indentAmt);
      Output::Print(_u("Object Pattern\n"));
      PrintPnodeListWIndent(pnode->sxUni.pnode1, indentAmt + INDENT_SIZE);
      break;

  case knopArray:
      Indent(indentAmt);
      Output::Print(_u("Array Literal\n"));
      PrintPnodeListWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopObject     , "obj cnst"    ,None    ,Uni  ,fnopUni)
  case knopObject:
      Indent(indentAmt);
      Output::Print(_u("Object Literal\n"));
      PrintPnodeListWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      // Binary and Ternary Operators
      //PTNODE(knopAdd        , "+"            ,Add     ,Bin  ,fnopBin)
  case knopAdd:
      Indent(indentAmt);
      Output::Print(_u("+\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopSub        , "-"            ,Sub     ,Bin  ,fnopBin)
  case knopSub:
      Indent(indentAmt);
      Output::Print(_u("-\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopMul        , "*"            ,Mul     ,Bin  ,fnopBin)
  case knopMul:
      Indent(indentAmt);
      Output::Print(_u("*\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopDiv        , "/"            ,Div     ,Bin  ,fnopBin)
  case knopExpo:
      Indent(indentAmt);
      Output::Print(_u("**\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1, indentAmt + INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2, indentAmt + INDENT_SIZE);
      break;
      //PTNODE(knopExpo        , "**"            ,Expo     ,Bin  ,fnopBin)

  case knopDiv:
      Indent(indentAmt);
      Output::Print(_u("/\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopMod        , "%"            ,Mod     ,Bin  ,fnopBin)
  case knopMod:
      Indent(indentAmt);
      Output::Print(_u("%\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopOr         , "|"            ,BitOr   ,Bin  ,fnopBin)
  case knopOr:
      Indent(indentAmt);
      Output::Print(_u("|\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopXor        , "^"            ,BitXor  ,Bin  ,fnopBin)
  case knopXor:
      Indent(indentAmt);
      Output::Print(_u("^\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAnd        , "&"            ,BitAnd  ,Bin  ,fnopBin)
  case knopAnd:
      Indent(indentAmt);
      Output::Print(_u("&\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopEq         , "=="        ,EQ      ,Bin  ,fnopBin|fnopRel)
  case knopEq:
      Indent(indentAmt);
      Output::Print(_u("==\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopNe         , "!="        ,NE      ,Bin  ,fnopBin|fnopRel)
  case knopNe:
      Indent(indentAmt);
      Output::Print(_u("!=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopLt         , "<"            ,LT      ,Bin  ,fnopBin|fnopRel)
  case knopLt:
      Indent(indentAmt);
      Output::Print(_u("<\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopLe         , "<="        ,LE      ,Bin  ,fnopBin|fnopRel)
  case knopLe:
      Indent(indentAmt);
      Output::Print(_u("<=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopGe         , ">="        ,GE      ,Bin  ,fnopBin|fnopRel)
  case knopGe:
      Indent(indentAmt);
      Output::Print(_u(">=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopGt         , ">"            ,GT      ,Bin  ,fnopBin|fnopRel)
  case knopGt:
      Indent(indentAmt);
      Output::Print(_u(">\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopCall       , "()"        ,None    ,Bin  ,fnopBin)
  case knopCall:
      Indent(indentAmt);
      Output::Print(_u("Call\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeListWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopDot        , "."            ,None    ,Bin  ,fnopBin)
  case knopDot:
      Indent(indentAmt);
      Output::Print(_u(".\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsg        , "="            ,None    ,Bin  ,fnopBin|fnopAsg)
  case knopAsg:
      Indent(indentAmt);
      Output::Print(_u("=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopInstOf     , "instanceof",InstOf  ,Bin  ,fnopBin|fnopRel)
  case knopInstOf:
      Indent(indentAmt);
      Output::Print(_u("instanceof\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopIn         , "in"        ,In      ,Bin  ,fnopBin|fnopRel)
  case knopIn:
      Indent(indentAmt);
      Output::Print(_u("in\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopEqv        , "==="        ,Eqv     ,Bin  ,fnopBin|fnopRel)
  case knopEqv:
      Indent(indentAmt);
      Output::Print(_u("===\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopNEqv       , "!=="        ,NEqv    ,Bin  ,fnopBin|fnopRel)
  case knopNEqv:
      Indent(indentAmt);
      Output::Print(_u("!==\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopComma      , ","            ,None    ,Bin  ,fnopBin)
  case knopComma:
      Indent(indentAmt);
      Output::Print(_u(",\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopLogOr      , "||"        ,None    ,Bin  ,fnopBin)
  case knopLogOr:
      Indent(indentAmt);
      Output::Print(_u("||\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopLogAnd     , "&&"        ,None    ,Bin  ,fnopBin)
  case knopLogAnd:
      Indent(indentAmt);
      Output::Print(_u("&&\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopLsh        , "<<"        ,Lsh     ,Bin  ,fnopBin)
  case knopLsh:
      Indent(indentAmt);
      Output::Print(_u("<<\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopRsh        , ">>"        ,Rsh     ,Bin  ,fnopBin)
  case knopRsh:
      Indent(indentAmt);
      Output::Print(_u(">>\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopRs2        , ">>>"        ,Rs2     ,Bin  ,fnopBin)
  case knopRs2:
      Indent(indentAmt);
      Output::Print(_u(">>>\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopNew        , "new"        ,None    ,Bin  ,fnopBin)
  case knopNew:
      Indent(indentAmt);
      Output::Print(_u("new\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeListWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopIndex      , "[]"        ,None    ,Bin  ,fnopBin)
  case knopIndex:
      Indent(indentAmt);
      Output::Print(_u("[]\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeListWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopQmark      , "?"            ,None    ,Tri  ,fnopBin)
  case knopQmark:
      Indent(indentAmt);
      Output::Print(_u("?:\n"));
      PrintPnodeWIndent(pnode->sxTri.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxTri.pnode2,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxTri.pnode3,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgAdd     , "+="        ,Add     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgAdd:
      Indent(indentAmt);
      Output::Print(_u("+=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgSub     , "-="        ,Sub     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgSub:
      Indent(indentAmt);
      Output::Print(_u("-=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgMul     , "*="        ,Mul     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgMul:
      Indent(indentAmt);
      Output::Print(_u("*=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgDiv     , "/="        ,Div     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgExpo:
      Indent(indentAmt);
      Output::Print(_u("**=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1, indentAmt + INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2, indentAmt + INDENT_SIZE);
      break;
      //PTNODE(knopAsgExpo     , "**="       ,Expo     ,Bin  ,fnopBin|fnopAsg)

  case knopAsgDiv:
      Indent(indentAmt);
      Output::Print(_u("/=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgMod     , "%="        ,Mod     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgMod:
      Indent(indentAmt);
      Output::Print(_u("%=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgAnd     , "&="        ,BitAnd  ,Bin  ,fnopBin|fnopAsg)
  case knopAsgAnd:
      Indent(indentAmt);
      Output::Print(_u("&=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgXor     , "^="        ,BitXor  ,Bin  ,fnopBin|fnopAsg)
  case knopAsgXor:
      Indent(indentAmt);
      Output::Print(_u("^=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgOr      , "|="        ,BitOr   ,Bin  ,fnopBin|fnopAsg)
  case knopAsgOr:
      Indent(indentAmt);
      Output::Print(_u("|=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgLsh     , "<<="        ,Lsh     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgLsh:
      Indent(indentAmt);
      Output::Print(_u("<<=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgRsh     , ">>="        ,Rsh     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgRsh:
      Indent(indentAmt);
      Output::Print(_u(">>=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopAsgRs2     , ">>>="        ,Rs2     ,Bin  ,fnopBin|fnopAsg)
  case knopAsgRs2:
      Indent(indentAmt);
      Output::Print(_u(">>>=\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;

  case knopComputedName:
      Indent(indentAmt);
      Output::Print(_u("ComputedProperty\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1, indentAmt + INDENT_SIZE);
      break;

      //PTNODE(knopMember     , ":"            ,None    ,Bin  ,fnopBin)
  case knopMember:
  case knopMemberShort:
  case knopObjectPatternMember:
      Indent(indentAmt);
      Output::Print(_u(":\n"));
      PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxBin.pnode2,indentAmt+INDENT_SIZE);
      break;
      // General nodes.
      //PTNODE(knopList       , "<list>"    ,None    ,Bin  ,fnopNone)
  case knopList:
      Indent(indentAmt);
      Output::Print(_u("List\n"));
      PrintPnodeListWIndent(pnode,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopVarDecl    , "varDcl"    ,None    ,Var  ,fnopNone)
  case knopVarDecl:
      Indent(indentAmt);
      Output::Print(_u("var %s\n"),pnode->sxVar.pid->Psz());
      if (pnode->sxVar.pnodeInit!=NULL)
          PrintPnodeWIndent(pnode->sxVar.pnodeInit,indentAmt+INDENT_SIZE);
      break;
  case knopConstDecl:
      Indent(indentAmt);
      Output::Print(_u("const %s\n"),pnode->sxVar.pid->Psz());
      if (pnode->sxVar.pnodeInit!=NULL)
          PrintPnodeWIndent(pnode->sxVar.pnodeInit,indentAmt+INDENT_SIZE);
      break;
  case knopLetDecl:
      Indent(indentAmt);
      Output::Print(_u("let %s\n"),pnode->sxVar.pid->Psz());
      if (pnode->sxVar.pnodeInit!=NULL)
          PrintPnodeWIndent(pnode->sxVar.pnodeInit,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopFncDecl    , "fncDcl"    ,None    ,Fnc  ,fnopLeaf)
  case knopFncDecl:
      Indent(indentAmt);
      if (pnode->sxFnc.pid!=NULL)
      {TRACE_IT(30893);
          Output::Print(_u("fn decl %d nested %d name %s (%d-%d)\n"),pnode->sxFnc.IsDeclaration(),pnode->sxFnc.IsNested(),
              pnode->sxFnc.pid->Psz(), pnode->ichMin, pnode->ichLim);
      }
      else
      {TRACE_IT(30894);
          Output::Print(_u("fn decl %d nested %d anonymous (%d-%d)\n"),pnode->sxFnc.IsDeclaration(),pnode->sxFnc.IsNested(),pnode->ichMin,pnode->ichLim);
      }
      PrintScopesWIndent(pnode, indentAmt+INDENT_SIZE);
      PrintFormalsWIndent(pnode->sxFnc.pnodeParams, indentAmt + INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxFnc.pnodeRest, indentAmt + INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxFnc.pnodeBody, indentAmt + INDENT_SIZE);
      if (pnode->sxFnc.pnodeBody == nullptr)
      {TRACE_IT(30895);
          Output::Print(_u("[%4d, %4d): "), pnode->ichMin, pnode->ichLim);
          Indent(indentAmt + INDENT_SIZE);
          Output::Print(_u("<parse deferred body>\n"));
      }
      break;
      //PTNODE(knopProg       , "program"    ,None    ,Fnc  ,fnopNone)
  case knopProg:
      Indent(indentAmt);
      Output::Print(_u("program\n"));
      PrintScopesWIndent(pnode, indentAmt+INDENT_SIZE);
      PrintPnodeListWIndent(pnode->sxFnc.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopEndCode    , "<endcode>"    ,None    ,None ,fnopNone)
  case knopEndCode:
      Indent(indentAmt);
      Output::Print(_u("<endcode>\n"));
      break;
      //PTNODE(knopDebugger   , "debugger"    ,None    ,None ,fnopNone)
  case knopDebugger:
      Indent(indentAmt);
      Output::Print(_u("<debugger>\n"));
      break;
      //PTNODE(knopFor        , "for"        ,None    ,For  ,fnopBreak|fnopContinue)
  case knopFor:
      Indent(indentAmt);
      Output::Print(_u("for\n"));
      PrintScopesWIndent(pnode, indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxFor.pnodeInit,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxFor.pnodeCond,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxFor.pnodeIncr,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxFor.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopIf         , "if"        ,None    ,If   ,fnopNone)
  case knopIf:
      Indent(indentAmt);
      Output::Print(_u("if\n"));
      PrintPnodeWIndent(pnode->sxIf.pnodeCond,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxIf.pnodeTrue,indentAmt+INDENT_SIZE);
      if (pnode->sxIf.pnodeFalse!=NULL)
          PrintPnodeWIndent(pnode->sxIf.pnodeFalse,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopWhile      , "while"        ,None    ,While,fnopBreak|fnopContinue)
  case knopWhile:
      Indent(indentAmt);
      Output::Print(_u("while\n"));
      PrintPnodeWIndent(pnode->sxWhile.pnodeCond,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxWhile.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopDoWhile    , "do-while"    ,None    ,While,fnopBreak|fnopContinue)
  case knopDoWhile:
      Indent(indentAmt);
      Output::Print(_u("do\n"));
      PrintPnodeWIndent(pnode->sxWhile.pnodeCond,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxWhile.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopForIn      , "for in"    ,None    ,ForIn,fnopBreak|fnopContinue|fnopCleanup)
  case knopForIn:
      Indent(indentAmt);
      Output::Print(_u("forIn\n"));
      PrintScopesWIndent(pnode, indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxForInOrForOf.pnodeLval,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxForInOrForOf.pnodeObj,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxForInOrForOf.pnodeBody,indentAmt+INDENT_SIZE);
      break;
  case knopForOf:
      Indent(indentAmt);
      Output::Print(_u("forOf\n"));
      PrintScopesWIndent(pnode, indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxForInOrForOf.pnodeLval,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxForInOrForOf.pnodeObj,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxForInOrForOf.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopReturn     , "return"    ,None    ,Uni  ,fnopNone)
  case knopReturn:
      Indent(indentAmt);
      Output::Print(_u("return\n"));
      if (pnode->sxReturn.pnodeExpr!=NULL)
          PrintPnodeWIndent(pnode->sxReturn.pnodeExpr,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopBlock      , "{}"        ,None    ,Block,fnopNone)
  case knopBlock:
      Indent(indentAmt);
      Output::Print(_u("block "));
      if (pnode->grfpn & fpnSyntheticNode)
          Output::Print(_u("synthetic "));
      PrintBlockType(pnode->sxBlock.blockType);
      Output::Print(_u("(%d-%d)\n"),pnode->ichMin,pnode->ichLim);
      PrintScopesWIndent(pnode, indentAmt+INDENT_SIZE);
      if (pnode->sxBlock.pnodeStmt!=NULL)
          PrintPnodeWIndent(pnode->sxBlock.pnodeStmt,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopWith       , "with"        ,None    ,With ,fnopCleanup)
  case knopWith:
      Indent(indentAmt);
      Output::Print(_u("with (%d-%d)\n"), pnode->ichMin,pnode->ichLim);
      PrintScopesWIndent(pnode, indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxWith.pnodeObj,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxWith.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopBreak      , "break"        ,None    ,Jump ,fnopNone)
  case knopBreak:
      Indent(indentAmt);
      Output::Print(_u("break\n"));
      // TODO: some representation of target
      break;
      //PTNODE(knopContinue   , "continue"    ,None    ,Jump ,fnopNone)
  case knopContinue:
      Indent(indentAmt);
      Output::Print(_u("continue\n"));
      // TODO: some representation of target
      break;
      //PTNODE(knopLabel      , "label"        ,None    ,Label,fnopNone)
  case knopLabel:
      Indent(indentAmt);
      Output::Print(_u("label %s"),pnode->sxLabel.pid->Psz());
      // TODO: print labeled statement
      break;
      //PTNODE(knopSwitch     , "switch"    ,None    ,Switch,fnopBreak)
  case knopSwitch:
      Indent(indentAmt);
      Output::Print(_u("switch\n"));
      PrintScopesWIndent(pnode, indentAmt+INDENT_SIZE);
      for (ParseNode *pnodeT = pnode->sxSwitch.pnodeCases; NULL != pnodeT;pnodeT = pnodeT->sxCase.pnodeNext) {
          PrintPnodeWIndent(pnodeT,indentAmt+2);
      }
      break;
      //PTNODE(knopCase       , "case"        ,None    ,Case ,fnopNone)
  case knopCase:
      Indent(indentAmt);
      Output::Print(_u("case\n"));
      PrintPnodeWIndent(pnode->sxCase.pnodeExpr,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxCase.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopTryFinally,"try-finally",None,TryFinally,fnopCleanup)
  case knopTryFinally:
      PrintPnodeWIndent(pnode->sxTryFinally.pnodeTry,indentAmt);
      PrintPnodeWIndent(pnode->sxTryFinally.pnodeFinally,indentAmt);
      break;
  case knopFinally:
      Indent(indentAmt);
      Output::Print(_u("finally\n"));
      PrintPnodeWIndent(pnode->sxFinally.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopCatch      , "catch"     ,None    ,Catch,fnopNone)
  case knopCatch:
      Indent(indentAmt);
      Output::Print(_u("catch (%d-%d)\n"), pnode->ichMin,pnode->ichLim);
      PrintScopesWIndent(pnode, indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxCatch.pnodeParam,indentAmt+INDENT_SIZE);
//      if (pnode->sxCatch.pnodeGuard!=NULL)
//          PrintPnodeWIndent(pnode->sxCatch.pnodeGuard,indentAmt+INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxCatch.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopTryCatch      , "try-catch" ,None    ,TryCatch  ,fnopCleanup)
  case knopTryCatch:
      PrintPnodeWIndent(pnode->sxTryCatch.pnodeTry,indentAmt);
      PrintPnodeWIndent(pnode->sxTryCatch.pnodeCatch,indentAmt);
      break;
      //PTNODE(knopTry        , "try"       ,None    ,Try  ,fnopCleanup)
  case knopTry:
      Indent(indentAmt);
      Output::Print(_u("try\n"));
      PrintPnodeWIndent(pnode->sxTry.pnodeBody,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopThrow      , "throw"     ,None    ,Uni  ,fnopNone)
  case knopThrow:
      Indent(indentAmt);
      Output::Print(_u("throw\n"));
      PrintPnodeWIndent(pnode->sxUni.pnode1,indentAmt+INDENT_SIZE);
      break;
      //PTNODE(knopClassDecl, "classDecl", None , Class, fnopLeaf)
  case knopClassDecl:
      Indent(indentAmt);
      Output::Print(_u("class %s"), pnode->sxClass.pnodeName->sxVar.pid->Psz());
      if (pnode->sxClass.pnodeExtends != nullptr)
      {TRACE_IT(30896);
          Output::Print(_u(" extends "));
          PrintPnodeWIndent(pnode->sxClass.pnodeExtends, 0);
      }
      else {TRACE_IT(30897);
          Output::Print(_u("\n"));
      }

      PrintPnodeWIndent(pnode->sxClass.pnodeConstructor,   indentAmt + INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxClass.pnodeMembers,       indentAmt + INDENT_SIZE);
      PrintPnodeWIndent(pnode->sxClass.pnodeStaticMembers, indentAmt + INDENT_SIZE);
      break;
  case knopStrTemplate:
      Indent(indentAmt);
      Output::Print(_u("string template\n"));
      PrintPnodeListWIndent(pnode->sxStrTemplate.pnodeSubstitutionExpressions, indentAmt + INDENT_SIZE);
      break;
  case knopYieldStar:
      Indent(indentAmt);
      Output::Print(_u("yield*\n"));
      PrintPnodeListWIndent(pnode->sxUni.pnode1, indentAmt + INDENT_SIZE);
      break;
  case knopYield:
  case knopYieldLeaf:
      Indent(indentAmt);
      Output::Print(_u("yield\n"));
      PrintPnodeListWIndent(pnode->sxUni.pnode1, indentAmt + INDENT_SIZE);
      break;
  case knopAwait:
      Indent(indentAmt);
      Output::Print(_u("await\n"));
      PrintPnodeListWIndent(pnode->sxUni.pnode1, indentAmt + INDENT_SIZE);
      break;
  case knopExportDefault:
      Indent(indentAmt);
      Output::Print(_u("export default\n"));
      PrintPnodeListWIndent(pnode->sxExportDefault.pnodeExpr, indentAmt + INDENT_SIZE);
      break;
  default:
      Output::Print(_u("unhandled pnode op %d\n"),pnode->nop);
      break;
    }
}

void PrintPnodeListWIndent(ParseNode *pnode,int indentAmt) {TRACE_IT(30898);
    if (pnode!=NULL) {TRACE_IT(30899);
        while(pnode->nop==knopList) {TRACE_IT(30900);
            PrintPnodeWIndent(pnode->sxBin.pnode1,indentAmt);
            pnode = pnode->sxBin.pnode2;
        }
        PrintPnodeWIndent(pnode,indentAmt);
    }
}

void PrintFormalsWIndent(ParseNode *pnodeArgs, int indentAmt)
{TRACE_IT(30901);
    for (ParseNode *pnode = pnodeArgs; pnode != nullptr; pnode = pnode->GetFormalNext())
    {TRACE_IT(30902);
        PrintPnodeWIndent(pnode->nop == knopParamPattern ? pnode->sxParamPattern.pnode1 : pnode, indentAmt);
    }
}

void PrintPnode(ParseNode *pnode) {
    PrintPnodeWIndent(pnode,0);
}

void ParseNode::Dump()
{TRACE_IT(30903);
    switch(nop)
    {
    case knopFncDecl:
    case knopProg:
        LPCOLESTR name = Js::Constants::AnonymousFunction;
        if(this->sxFnc.pnodeName)
        {TRACE_IT(30904);
            name = this->sxFnc.pnodeName->sxVar.pid->Psz();
        }

        Output::Print(_u("%s (%d) [%d, %d]:\n"), name, this->sxFnc.functionId, this->sxFnc.lineNumber, this->sxFnc.columnNumber);
        Output::Print(_u("hasArguments: %s callsEval:%s childCallsEval:%s HasReferenceableBuiltInArguments:%s ArgumentsObjectEscapes:%s HasWith:%s HasThis:%s HasOnlyThis:%s \n"),
            IsTrueOrFalse(this->sxFnc.HasHeapArguments()),
            IsTrueOrFalse(this->sxFnc.CallsEval()),
            IsTrueOrFalse(this->sxFnc.ChildCallsEval()),
            IsTrueOrFalse(this->sxFnc.HasReferenceableBuiltInArguments()),
            IsTrueOrFalse(this->sxFnc.GetArgumentsObjectEscapes()),
            IsTrueOrFalse(this->sxFnc.HasWithStmt()),
            IsTrueOrFalse(this->sxFnc.HasThisStmt()),
            IsTrueOrFalse(this->sxFnc.HasOnlyThisStmts()));
        if(this->sxFnc.funcInfo)
        {TRACE_IT(30905);
            this->sxFnc.funcInfo->Dump();
        }
        break;
    }
}
#endif

DeferredFunctionStub * BuildDeferredStubTree(ParseNode *pnodeFnc, Recycler *recycler)
{TRACE_IT(30906);
    Assert(pnodeFnc->nop == knopFncDecl);

    uint nestedCount = pnodeFnc->sxFnc.nestedCount;
    if (nestedCount == 0)
    {TRACE_IT(30907);
        return nullptr;
    }

    if (pnodeFnc->sxFnc.deferredStub)
    {TRACE_IT(30908);
        return pnodeFnc->sxFnc.deferredStub;
    }

    DeferredFunctionStub *deferredStubs = RecyclerNewArray(recycler, DeferredFunctionStub, nestedCount);
    uint i = 0;

    ParseNode *pnodeBlock = pnodeFnc->sxFnc.pnodeBodyScope;
    Assert(pnodeBlock != nullptr
        && pnodeBlock->nop == knopBlock
        && (pnodeBlock->sxBlock.blockType == PnodeBlockType::Function
            || pnodeBlock->sxBlock.blockType == PnodeBlockType::Parameter));

    for (ParseNode *pnodeChild = pnodeBlock->sxBlock.pnodeScopes; pnodeChild != nullptr;)
    {TRACE_IT(30909);

        if (pnodeChild->nop != knopFncDecl)
        {TRACE_IT(30910);
            // We only expect to find a function body block in a parameter scope block.
            Assert(pnodeChild->nop == knopBlock
                && (pnodeBlock->sxBlock.blockType == PnodeBlockType::Parameter
                    || pnodeChild->sxBlock.blockType == PnodeBlockType::Function));
            pnodeChild = pnodeChild->sxBlock.pnodeNext;
            continue;
        }
        AssertOrFailFast(i < nestedCount);

        if (pnodeChild->sxFnc.pnodeBody != nullptr)
        {TRACE_IT(30911);
            // Anomalous case of a non-deferred function nested within a deferred one.
            // Work around by discarding the stub tree.
            return nullptr;
        }

        if (pnodeChild->sxFnc.IsGeneratedDefault())
        {TRACE_IT(30912);
            ++i;
            pnodeChild = pnodeChild->sxFnc.pnodeNext;
            continue;
        }

        AnalysisAssertOrFailFast(i < nestedCount);

        deferredStubs[i].fncFlags = pnodeChild->sxFnc.fncFlags;
        deferredStubs[i].nestedCount = pnodeChild->sxFnc.nestedCount;
        deferredStubs[i].restorePoint = *pnodeChild->sxFnc.pRestorePoint;
        deferredStubs[i].deferredStubs = BuildDeferredStubTree(pnodeChild, recycler);
        deferredStubs[i].ichMin = pnodeChild->ichMin;
        ++i;
        pnodeChild = pnodeChild->sxFnc.pnodeNext;
    }

    return deferredStubs;
}
