//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

#define ASSERT_THREAD() AssertMsg(mainThreadId == GetCurrentThreadContextId(), \
    "Cannot use this member of BackgroundParser from thread other than the creating context's current thread")

#if ENABLE_NATIVE_CODEGEN
BackgroundParser::BackgroundParser(Js::ScriptContext *scriptContext)
    :   JsUtil::WaitableJobManager(scriptContext->GetThreadContext()->GetJobProcessor()),
        scriptContext(scriptContext),
        unprocessedItemsHead(nullptr),
        unprocessedItemsTail(nullptr),
        failedBackgroundParseItem(nullptr),
        pendingBackgroundItems(0)
{TRACE_IT(28553);
    Processor()->AddManager(this);

#if DBG
    this->mainThreadId = GetCurrentThreadContextId();
#endif
}

BackgroundParser::~BackgroundParser()
{TRACE_IT(28554);
    JsUtil::JobProcessor *processor = Processor();
    if (processor->ProcessesInBackground())
    {TRACE_IT(28555);
        static_cast<JsUtil::BackgroundJobProcessor*>(processor)->IterateBackgroundThreads([&](JsUtil::ParallelThreadData *threadData)->bool {
            if (threadData->parser)
            {TRACE_IT(28556);
                threadData->parser->Release();
                threadData->parser = nullptr;
            }
            return false;
        });
    }
    processor->RemoveManager(this);
}

BackgroundParser * BackgroundParser::New(Js::ScriptContext *scriptContext)
{TRACE_IT(28557);
    return HeapNew(BackgroundParser, scriptContext);
}

void BackgroundParser::Delete(BackgroundParser *backgroundParser)
{TRACE_IT(28558);
    HeapDelete(backgroundParser);
}

bool BackgroundParser::Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData)
{TRACE_IT(28559);
    BackgroundParseItem *backgroundItem = static_cast<BackgroundParseItem*>(job);

    if (failedBackgroundParseItem)
    {TRACE_IT(28560);
        if (backgroundItem->GetParseNode()->ichMin > failedBackgroundParseItem->GetParseNode()->ichMin)
        {TRACE_IT(28561);
            return true;
        }
    }

    if (threadData->parser == nullptr || threadData->canDecommit)
    {TRACE_IT(28562);
        if (threadData->parser != nullptr)
        {TRACE_IT(28563);
            // "canDecommit" means the previous parse finished.
            // Don't leave a parser with stale state in the thread data, or we'll mess up the bindings.
            threadData->backgroundPageAllocator.DecommitNow();
            this->OnDecommit(threadData);
        }
        threadData->canDecommit = false;

        // Lazily create a parser instance for this thread from the thread's page allocator.
        // It will stay around until the main thread's current parser instance goes away, which will free
        // the background thread to decommit its pages.
        threadData->parser = Anew(threadData->threadArena, Parser, this->scriptContext, backgroundItem->IsStrictMode(), &threadData->backgroundPageAllocator, true);
        threadData->pse = Anew(threadData->threadArena, CompileScriptException);
        threadData->parser->PrepareScanner(backgroundItem->GetParseContext()->fromExternal);
    }

    Parser *parser = threadData->parser;

    return this->Process(backgroundItem, parser, threadData->pse);
}

bool BackgroundParser::Process(JsUtil::Job *const job, Parser *parser, CompileScriptException *pse)
{TRACE_IT(28564);
    BackgroundParseItem *backgroundItem = static_cast<BackgroundParseItem*>(job);

    Assert(parser->GetCurrBackgroundParseItem() == nullptr);
    parser->SetCurrBackgroundParseItem(backgroundItem);
    backgroundItem->SetParser(parser);

    HRESULT hr = parser->ParseFunctionInBackground(backgroundItem->GetParseNode(), backgroundItem->GetParseContext(), backgroundItem->IsDeferred(), pse);
    backgroundItem->SetMaxBlockId(parser->GetLastBlockId());
    backgroundItem->SetHR(hr);
    if (FAILED(hr))
    {TRACE_IT(28565);
        backgroundItem->SetPSE(pse);
    }
    backgroundItem->SetCompleted(true);
    parser->SetCurrBackgroundParseItem(nullptr);
    return hr == S_OK;
}

void BackgroundParser::JobProcessed(JsUtil::Job *const job, const bool succeeded)
{TRACE_IT(28566);
    // This is called from inside a lock, so we can mess with background parser attributes.
    BackgroundParseItem *backgroundItem = static_cast<BackgroundParseItem*>(job);
    this->RemoveFromUnprocessedItems(backgroundItem);
    --this->pendingBackgroundItems;
    if (!succeeded)
    {TRACE_IT(28567);
        Assert(FAILED(backgroundItem->GetHR()) || failedBackgroundParseItem);

        if (FAILED(backgroundItem->GetHR()))
        {TRACE_IT(28568);
            if (!failedBackgroundParseItem)
            {TRACE_IT(28569);
                failedBackgroundParseItem = backgroundItem;
            }
            else
            {TRACE_IT(28570);
                // If syntax errors are detected on multiple threads, the lexically earlier one should win.
                CompileScriptException *newPse = backgroundItem->GetPSE();
                CompileScriptException *oldPse = failedBackgroundParseItem->GetPSE();

                if (newPse->line < oldPse->line ||
                    (newPse->line == oldPse->line && newPse->ichMinLine < oldPse->ichMinLine))
                {TRACE_IT(28571);
                    failedBackgroundParseItem = backgroundItem;
                }
            }
        }
    }
}

void BackgroundParser::OnDecommit(JsUtil::ParallelThreadData *threadData)
{TRACE_IT(28572);
    if (threadData->parser)
    {TRACE_IT(28573);
        threadData->parser->Release();
        threadData->parser = nullptr;
    }
}

BackgroundParseItem * BackgroundParser::NewBackgroundParseItem(Parser *parser, ParseNode *parseNode, bool isDeferred)
{TRACE_IT(28574);
    BackgroundParseItem *item = Anew(parser->GetAllocator(), BackgroundParseItem, this, parser, parseNode, isDeferred);
    parser->AddBackgroundParseItem(item);
    return item;
}

bool BackgroundParser::ParseBackgroundItem(Parser *parser, ParseNode *parseNode, bool isDeferred)
{TRACE_IT(28575);
    ASSERT_THREAD();

    AutoPtr<BackgroundParseItem> workItemAutoPtr(this->NewBackgroundParseItem(parser, parseNode, isDeferred));
    if ((BackgroundParseItem*) workItemAutoPtr == nullptr)
    {TRACE_IT(28576);
        // OOM, just skip this work item and return.
        // TODO: Raise an OOM parse-time exception.
        return false;
    }

    parser->PrepareForBackgroundParse();

    BackgroundParseItem * backgroundItem = workItemAutoPtr.Detach();
    this->AddToParseQueue(backgroundItem, false, this->Processor()->ProcessesInBackground());

    return true;
}

BackgroundParseItem *BackgroundParser::GetJob(BackgroundParseItem *workitem) const
{TRACE_IT(28577);
    return workitem;
}

bool BackgroundParser::WasAddedToJobProcessor(JsUtil::Job *const job) const
{TRACE_IT(28578);
    ASSERT_THREAD();
    Assert(job);

    return static_cast<BackgroundParseItem*>(job)->IsInParseQueue();
}

void BackgroundParser::BeforeWaitForJob(BackgroundParseItem *const item) const
{TRACE_IT(28579);
}

void BackgroundParser::AfterWaitForJob(BackgroundParseItem *const item) const
{TRACE_IT(28580);
}

void BackgroundParser::AddToParseQueue(BackgroundParseItem *const item, bool prioritize, bool lock)
{TRACE_IT(28581);
    AutoOptionalCriticalSection autoLock(lock ? Processor()->GetCriticalSection() : nullptr);
    ++this->pendingBackgroundItems;
    Processor()->AddJob(item, prioritize);   // This one can throw (really unlikely though), OOM specifically.
    this->AddUnprocessedItem(item);
    item->OnAddToParseQueue();
}

void BackgroundParser::AddUnprocessedItem(BackgroundParseItem *const item)
{TRACE_IT(28582);
    if (this->unprocessedItemsTail == nullptr)
    {TRACE_IT(28583);
        this->unprocessedItemsHead = item;
    }
    else
    {TRACE_IT(28584);
        this->unprocessedItemsTail->SetNextUnprocessedItem(item);
    }
    item->SetPrevUnprocessedItem(this->unprocessedItemsTail);
    this->unprocessedItemsTail = item;
}

void BackgroundParser::RemoveFromUnprocessedItems(BackgroundParseItem *const item)
{TRACE_IT(28585);
    if (this->unprocessedItemsHead == item)
    {TRACE_IT(28586);
        this->unprocessedItemsHead = item->GetNextUnprocessedItem();
    }
    else
    {TRACE_IT(28587);
        item->GetPrevUnprocessedItem()->SetNextUnprocessedItem(item->GetNextUnprocessedItem());
    }
    if (this->unprocessedItemsTail == item)
    {TRACE_IT(28588);
        this->unprocessedItemsTail = item->GetPrevUnprocessedItem();
    }
    else
    {TRACE_IT(28589);
        item->GetNextUnprocessedItem()->SetPrevUnprocessedItem(item->GetPrevUnprocessedItem());
    }
    item->SetNextUnprocessedItem(nullptr);
    item->SetPrevUnprocessedItem(nullptr);
}

BackgroundParseItem *BackgroundParser::GetNextUnprocessedItem() const
{TRACE_IT(28590);
    BackgroundParseItem *item;
    bool background = this->Processor()->ProcessesInBackground();
    for (item = this->unprocessedItemsHead; item; item = item->GetNextUnprocessedItem())
    {TRACE_IT(28591);
        if (!background || !static_cast<JsUtil::BackgroundJobProcessor*>(Processor())->IsBeingProcessed(item))
        {TRACE_IT(28592);
            return item;
        }
    }
    return nullptr;
}

BackgroundParseItem::BackgroundParseItem(JsUtil::JobManager *const manager, Parser *const parser, ParseNode *parseNode, bool defer)
    : JsUtil::Job(manager),
      maxBlockId((uint)-1),
      strictMode(parser->IsStrictMode()),
      parseNode(parseNode),
      parser(nullptr),
      nextItem(nullptr),
      nextUnprocessedItem(nullptr),
      prevUnprocessedItem(nullptr),
      pse(nullptr),
      regExpNodes(nullptr),
      completed(false),
      inParseQueue(false),
      isDeferred(defer)
{TRACE_IT(28593);
    parser->CaptureContext(&parseContext);
}

void BackgroundParseItem::OnAddToParseQueue()
{TRACE_IT(28594);
    this->inParseQueue = true;
}

void BackgroundParseItem::OnRemoveFromParseQueue()
{TRACE_IT(28595);
    this->inParseQueue = false;
}

void BackgroundParseItem::AddRegExpNode(ParseNode *const pnode, ArenaAllocator *alloc)
{TRACE_IT(28596);
    if (regExpNodes == nullptr)
    {TRACE_IT(28597);
        regExpNodes = Anew(alloc, NodeDList, alloc);
    }

    regExpNodes->Append(pnode);
}
#endif
