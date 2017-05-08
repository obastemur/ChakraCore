//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if ENABLE_NATIVE_CODEGEN
typedef DList<ParseNode*, ArenaAllocator> NodeDList;

struct BackgroundParseItem sealed : public JsUtil::Job
{
    BackgroundParseItem(JsUtil::JobManager *const manager, Parser *const parser, ParseNode *parseNode, bool defer);

    ParseContext *GetParseContext() {TRACE_IT(28598); return &parseContext; }
    ParseNode *GetParseNode() const {TRACE_IT(28599); return parseNode; }
    CompileScriptException *GetPSE() const {TRACE_IT(28600); return pse; }
    HRESULT GetHR() const {TRACE_IT(28601); return hr; }
    bool IsStrictMode() const {TRACE_IT(28602); return strictMode; }
    bool Succeeded() const {TRACE_IT(28603); return hr == S_OK; }
    bool IsInParseQueue() const {TRACE_IT(28604); return inParseQueue; }
    bool IsDeferred() const {TRACE_IT(28605); return isDeferred;}
    void SetHR(HRESULT hr) {TRACE_IT(28606); this->hr = hr; }
    void SetCompleted(bool has) {TRACE_IT(28607); completed = has; }
    void SetPSE(CompileScriptException *pse) {TRACE_IT(28608); this->pse = pse; }

    uint GetMaxBlockId() const {TRACE_IT(28609); return maxBlockId; }
    void SetMaxBlockId(uint blockId) {TRACE_IT(28610); maxBlockId = blockId; }
    Parser *GetParser() const {TRACE_IT(28611); return parser; }
    void SetParser(Parser *p) {TRACE_IT(28612); parser = p; }
    BackgroundParseItem *GetNext() const {TRACE_IT(28613); return nextItem; }
    void SetNext(BackgroundParseItem *item) {TRACE_IT(28614); nextItem = item; }
    BackgroundParseItem *GetNextUnprocessedItem() const {TRACE_IT(28615); return nextUnprocessedItem; }
    void SetNextUnprocessedItem(BackgroundParseItem *item) {TRACE_IT(28616); nextUnprocessedItem = item; }
    BackgroundParseItem *GetPrevUnprocessedItem() const {TRACE_IT(28617); return prevUnprocessedItem; }
    void SetPrevUnprocessedItem(BackgroundParseItem *item) {TRACE_IT(28618); prevUnprocessedItem = item; }
    DList<ParseNode*, ArenaAllocator>* RegExpNodeList() {TRACE_IT(28619); return regExpNodes; }

    void OnAddToParseQueue();
    void OnRemoveFromParseQueue();
    void AddRegExpNode(ParseNode *const pnode, ArenaAllocator *alloc);

private:
    ParseContext parseContext;
    Parser *parser;
    BackgroundParseItem *nextItem;
    BackgroundParseItem *nextUnprocessedItem;
    BackgroundParseItem *prevUnprocessedItem;
    ParseNode *parseNode;
    CompileScriptException *pse;
    NodeDList* regExpNodes;
    HRESULT hr;
    uint maxBlockId;
    bool isDeferred;
    bool strictMode;
    bool inParseQueue;
    bool completed;
};

class BackgroundParser sealed : public JsUtil::WaitableJobManager
{
public:
    BackgroundParser(Js::ScriptContext *scriptContext);
    ~BackgroundParser();

    static BackgroundParser * New(Js::ScriptContext *scriptContext);
    static void Delete(BackgroundParser *backgroundParser);

    volatile uint* GetPendingBackgroundItemsPtr() const {TRACE_IT(28620); return (volatile uint*)&pendingBackgroundItems; }

    virtual bool Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData) override;
    virtual void JobProcessed(JsUtil::Job *const job, const bool succeeded) override;
    virtual void OnDecommit(JsUtil::ParallelThreadData *threadData) override;

    bool Process(JsUtil::Job *const job, Parser *parser, CompileScriptException *pse);
    bool ParseBackgroundItem(Parser *parser, ParseNode *parseNode, bool isDeferred);
    BackgroundParseItem * NewBackgroundParseItem(Parser *parser, ParseNode *parseNode, bool isDeferred);

    BackgroundParseItem *GetJob(BackgroundParseItem *item) const;
    bool WasAddedToJobProcessor(JsUtil::Job *const job) const;
    void BeforeWaitForJob(BackgroundParseItem *const item) const;
    void AfterWaitForJob(BackgroundParseItem *const item) const;

    BackgroundParseItem *GetNextUnprocessedItem() const;
    void AddUnprocessedItem(BackgroundParseItem *const item);
    void RemoveFromUnprocessedItems(BackgroundParseItem *const item);

    void SetFailedBackgroundParseItem(BackgroundParseItem *item) {TRACE_IT(28621); failedBackgroundParseItem = item; }
    BackgroundParseItem *GetFailedBackgroundParseItem() const {TRACE_IT(28622); return failedBackgroundParseItem; }
    bool HasFailedBackgroundParseItem() const {TRACE_IT(28623); return failedBackgroundParseItem != nullptr; }

private:
    void AddToParseQueue(BackgroundParseItem *const item, bool prioritize, bool lock);

private:
    Js::ScriptContext *scriptContext;
    uint pendingBackgroundItems;
    BackgroundParseItem *failedBackgroundParseItem;
    BackgroundParseItem *unprocessedItemsHead;
    BackgroundParseItem *unprocessedItemsTail;

#if DBG
    ThreadContextId mainThreadId;
#endif
};
#endif
