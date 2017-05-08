//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
// Portions of this file are copyright 2014 Mozilla Foundation, available under the Apache 2.0 license.
//-------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------
// Copyright 2014 Mozilla Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//-------------------------------------------------------------------------------------------------------

#pragma once

#ifdef ASMJS_PLAT
// Removed code from original location, if the expression is true, check if extra code needed
#define MaybeTodo( expr ) AssertMsg( !(expr), "Unhandled scenario in asm.js" )

namespace Js {
    Var AsmJsChangeHeapBuffer(RecyclableObject * function, CallInfo callInfo, ...);

#pragma warning (suppress: 25057) // Suppress unannotated buffer warning
    void * UnboxAsmJsArguments(ScriptFunction* func, Var * origArgs, char * argDst, CallInfo callInfo);
#if _M_X64
    int GetStackSizeForAsmJsUnboxing(ScriptFunction* func);
    Var BoxAsmJsReturnValue(ScriptFunction* func, int64 intRetVal, double doubleRetVal, float floatRetVal, __m128 simdReturn);
#endif

    class AsmJsCompilationException
    {
        char16 msg_[256];
    public:
        AsmJsCompilationException( const char16* _msg, ... );
        inline char16* msg() {TRACE_IT(47358); return msg_; }
    };

    class ParserWrapper
    {
    public:
        static PropertyName FunctionName( ParseNode *node );
        static PropertyName VariableName( ParseNode *node );
        static ParseNode* FunctionArgsList( ParseNode *node, ArgSlot &numformals );
        static ParseNode* NextVar( ParseNode *node );
        static ParseNode* NextInList( ParseNode *node );
        static inline ParseNode *GetListHead( ParseNode *node );
        static inline bool IsNameDeclaration(ParseNode *node);
        static inline bool IsUInt(ParseNode *node);
        static inline uint GetUInt(ParseNode *node);
        static inline bool IsNegativeZero(ParseNode* node);
        static inline bool IsMinInt(ParseNode *node){TRACE_IT(47359); return node && node->nop == knopFlt && node->sxFlt.maybeInt && node->sxFlt.dbl == -2147483648.0; };
        static inline bool IsUnsigned(ParseNode *node)
        {TRACE_IT(47360);
            return node &&
                node->nop == knopFlt &&
                node->sxFlt.maybeInt &&
                node->sxFlt.dbl > (double)INT_MAX &&
                node->sxFlt.dbl <= (double)UINT_MAX;
        }

        static bool IsDefinition( ParseNode *arg );
        static bool ParseVarOrConstStatement( AsmJSParser &parser, ParseNode **var );
        static inline bool IsNumericLiteral(ParseNode* node) {TRACE_IT(47361); return node && (node->nop == knopInt || node->nop == knopFlt); }
        static inline bool IsFroundNumericLiteral(ParseNode* node) {TRACE_IT(47362); return node && (IsNumericLiteral(node) || IsNegativeZero(node)); }
        static inline ParseNode* GetUnaryNode( ParseNode* node ){TRACE_IT(47363);Assert(IsNodeUnary(node));return node->sxUni.pnode1;}
        static inline ParseNode* GetBinaryLeft( ParseNode* node ){TRACE_IT(47364);Assert(IsNodeBinary(node));return node->sxBin.pnode1;}
        static inline ParseNode* GetBinaryRight( ParseNode* node ){TRACE_IT(47365);Assert(IsNodeBinary(node));return node->sxBin.pnode2;}
        static inline ParseNode* DotBase( ParseNode *node );
        static inline bool IsDotMember( ParseNode *node );
        static inline PropertyName DotMember( ParseNode *node );
        // Get the VarDecl from the node or nullptr if unable to find
        static ParseNode* GetVarDeclList(ParseNode* node);
        // Goes through the nodes until the end of the list of VarDecl
        static void ReachEndVarDeclList( ParseNode** node );

        // nop utils
        static inline bool IsNodeBinary    (ParseNode* pnode){TRACE_IT(47366); return pnode && !!(ParseNode::Grfnop(pnode->nop) & (fnopBin|fnopBinList)); }
        static inline bool IsNodeUnary     (ParseNode* pnode){TRACE_IT(47367); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopUni        ); }
        static inline bool IsNodeConst     (ParseNode* pnode){TRACE_IT(47368); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopConst      ); }
        static inline bool IsNodeLeaf      (ParseNode* pnode){TRACE_IT(47369); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopLeaf       ); }
        static inline bool IsNodeRelational(ParseNode* pnode){TRACE_IT(47370); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopRel        ); }
        static inline bool IsNodeAssignment(ParseNode* pnode){TRACE_IT(47371); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopAsg        ); }
        static inline bool IsNodeBreak     (ParseNode* pnode){TRACE_IT(47372); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopBreak      ); }
        static inline bool IsNodeContinue  (ParseNode* pnode){TRACE_IT(47373); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopContinue   ); }
        static inline bool IsNodeCleanUp   (ParseNode* pnode){TRACE_IT(47374); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopCleanup    ); }
        static inline bool IsNodeJump      (ParseNode* pnode){TRACE_IT(47375); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopJump       ); }
        static inline bool IsNodeExpression(ParseNode* pnode){TRACE_IT(47376); return pnode &&  !(ParseNode::Grfnop(pnode->nop) & fnopNotExprStmt); }
    };

    bool ParserWrapper::IsNameDeclaration( ParseNode *node )
    {TRACE_IT(47377);
        return node->nop == knopName || node->nop == knopStr;
    }

    bool ParserWrapper::IsNegativeZero(ParseNode *node)
    {TRACE_IT(47378);
        return node && ((node->nop == knopFlt && JavascriptNumber::IsNegZero(node->sxFlt.dbl)) ||
            (node->nop == knopNeg && node->sxUni.pnode1->nop == knopInt && node->sxUni.pnode1->sxInt.lw == 0));
    }

    bool ParserWrapper::IsUInt( ParseNode *node )
    {TRACE_IT(47379);
        return node->nop == knopInt || IsUnsigned(node);
    }

    uint ParserWrapper::GetUInt( ParseNode *node )
    {TRACE_IT(47380);
        Assert( IsUInt( node ) );
        if( node->nop == knopInt )
        {TRACE_IT(47381);
            return (uint)node->sxInt.lw;
        }
        Assert( node->nop == knopFlt );
        return (uint)node->sxFlt.dbl;
    }

    bool ParserWrapper::IsDotMember( ParseNode *node )
    {TRACE_IT(47382);
        return node && (node->nop == knopDot || node->nop == knopIndex);
    }

    PropertyName ParserWrapper::DotMember( ParseNode *node )
    {TRACE_IT(47383);
        Assert( IsDotMember(node) );
        if( IsNameDeclaration( GetBinaryRight( node ) ) )
        {TRACE_IT(47384);
            return GetBinaryRight( node )->name();
        }
        return nullptr;
    }

    ParseNode* ParserWrapper::DotBase( ParseNode *node )
    {TRACE_IT(47385);
        Assert( IsDotMember( node ) );
        return GetBinaryLeft( node );
    }

    ParseNode * ParserWrapper::GetListHead( ParseNode *node )
    {TRACE_IT(47386);
        Assert( node->nop == knopList );
        return node->sxBin.pnode1;
    }
};
#endif
