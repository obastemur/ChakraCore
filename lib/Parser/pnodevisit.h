//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// Minimum amount of memory on the stack required by a visitor call.
// Use this value to control when to stop the visitor recursion before a SOE occurs.
#define PNODEVISIRORSIZE 256

template <class Context>
struct VisitorPolicyBase
{
    typedef Context Context;

protected:
    inline bool Preorder(ParseNode *pnode, Context context) {TRACE_IT(33235); return true; }
    inline void Inorder(ParseNode *pnode, Context context) {TRACE_IT(33236); }
    inline void Midorder(ParseNode *pnode, Context context) {TRACE_IT(33237); }
    inline void Postorder(ParseNode *pnode, Context context) {TRACE_IT(33238); }
    inline void InList(ParseNode *pnode, Context context) {TRACE_IT(33239); }

    // This will be useful when you want the reference of your current node.
    inline void PassReference(ParseNode **ppnode, Context context) {TRACE_IT(33240); }
};

template <class Visitor, class VisitorPolicy>
struct VisitorWalkerPolicy : public VisitorPolicy
{
public:
    typedef ParseNode *ResultType;
    typedef struct WalkerContext
    {
        typename VisitorPolicy::Context visitorContext;
        Visitor *visitor;
        WalkerContext(typename VisitorPolicy::Context context, Visitor *visitor): visitorContext(context), visitor(visitor) {TRACE_IT(33241); }
    } *Context;
    inline ParseNode *DefaultResult() {TRACE_IT(33242); return NULL; }
    inline bool ContinueWalk(ParseNode *result) {TRACE_IT(33243); return true; }
    inline ParseNode *WalkNode(ParseNode *pnode, Context context) { Inorder(pnode, context->visitorContext); return pnode; }
    inline ParseNode *WalkListNode(ParseNode *pnode, Context context) { InList(pnode, context->visitorContext); return NULL; }
    inline ParseNode *WalkFirstChild(ParseNode *pnode, Context context) {TRACE_IT(33244); context->visitor->VisitNode(pnode, context); return pnode; }
    inline ParseNode *WalkSecondChild(ParseNode *pnode, Context context) {TRACE_IT(33245); context->visitor->VisitNode(pnode, context); return pnode; }
    inline ParseNode *WalkNthChild(ParseNode *pparentnode, ParseNode *pnode, Context context) { Midorder(pparentnode, context->visitorContext); context->visitor->VisitNode(pnode, context); return pnode; }
    inline void WalkReference(ParseNode **ppnode, Context context) {TRACE_IT(33246); context->visitor->PassReferenceNode(ppnode, context); }
};

template <class VisitorPolicy>
class ParseNodeVisitor : public ParseNodeWalker<VisitorWalkerPolicy<ParseNodeVisitor<VisitorPolicy>, VisitorPolicy> >
{
    typedef VisitorWalkerPolicy<ParseNodeVisitor<VisitorPolicy>, VisitorPolicy> WalkerPolicy;
    typedef typename WalkerPolicy::WalkerContext WalkerContext;

public:
    typedef typename VisitorPolicy::Context VisitorContext;

    void Visit(ParseNode *pnode, VisitorContext context = VisitorContext())
    {TRACE_IT(33247);
        WalkerContext walkerContext(context, this);
        VisitNode(pnode, &walkerContext);
    }

    void VisitNode(ParseNode *pnode, Context context)
    {TRACE_IT(33248);
        if (!ThreadContext::IsCurrentStackAvailable(PNODEVISIRORSIZE))
            return;

        if (!pnode) return;

        if (!Preorder(pnode, context->visitorContext))
            return;

        Walk(pnode, context);

        Postorder(pnode, context->visitorContext);
    }

    void PassReferenceNode(ParseNode **ppnode, Context context)
    {
        PassReference(ppnode, context->visitorContext);
    }
};

