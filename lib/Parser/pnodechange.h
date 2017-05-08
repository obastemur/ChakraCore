//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template <class Context>
class ParseNodeMutatorBase
{
public:
    typedef Context Context;
};

template <typename Mutator, typename TContext = typename Mutator::Context>
struct ParseNodeMutatingVisitor : public Mutator
{
public:
    typedef TContext Context;
    typedef typename Mutator::Context MutatorContext;

    class MutatorWalkerPolicy
    {
    public:
        typedef bool ResultType;
        typedef struct
        {
            MutatorContext mutatorContext;
            ParseNodeMutatingVisitor<Mutator, Context> *mutator;
        } Context;
        inline bool DefaultResult() {TRACE_IT(33107); return true; }
        inline bool ContinueWalk(bool value) {TRACE_IT(33108); return value; }
        inline bool WalkNode(ParseNode *node, Context context) {TRACE_IT(33109); return true; }
        inline bool WalkListNode(ParseNode *node, Context context) {TRACE_IT(33110); return true; }
        inline bool WalkFirstChild(ParseNode *&node, Context context) {TRACE_IT(33111); return context.mutator->Mutate(node, context.mutatorContext); }
        inline bool WalkSecondChild(ParseNode *&node, Context context) {TRACE_IT(33112); return context.mutator->Mutate(node, context.mutatorContext);  }
        inline bool WalkNthChild(ParseNode* pnodeParent, ParseNode *&node, Context context) {TRACE_IT(33113); return context.mutator->Mutate(node, context.mutatorContext); }
    };

    // Warning: This contains an unsafe cast if TContext != Mutator::Context.
    // If you use a non-default type parameter for TContext you must override this method with the safe version.
    // This cast is in place because if TContext != Muator::Context this will not compile even thought it will
    // not be used if it is overridden.
    virtual MutatorContext GetMutatorContext(Context context) {TRACE_IT(33114); return (MutatorContext)context; }

    inline bool Preorder(ParseNode *node, Context context)
    {TRACE_IT(33115);
        MutatorWalkerPolicy::Context mutatorWalkerContext;
        mutatorWalkerContext.mutatorContext = GetMutatorContext(context);
        mutatorWalkerContext.mutator = this;
        ParseNodeWalker<MutatorWalkerPolicy> walker;
        return walker.Walk(node, mutatorWalkerContext);
    }

    inline void Inorder(ParseNode *node, Context context) {TRACE_IT(33116); }
    inline void Midorder(ParseNode *node, Context context) {TRACE_IT(33117); }
    inline void Postorder(ParseNode *node, Context context) {TRACE_IT(33118); }
    inline void InList(ParseNode *pnode, Context context) {TRACE_IT(33119); }
    inline void PassReference(ParseNode **ppnode, Context context) {TRACE_IT(33120); }
};
