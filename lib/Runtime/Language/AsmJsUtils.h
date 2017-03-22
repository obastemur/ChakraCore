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
        inline char16* msg() {LOGMEIN("AsmJsUtils.h] 43\n"); return msg_; }
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
        static inline bool IsMinInt(ParseNode *node){LOGMEIN("AsmJsUtils.h] 59\n"); return node && node->nop == knopFlt && node->sxFlt.maybeInt && node->sxFlt.dbl == -2147483648.0; };
        static inline bool IsUnsigned(ParseNode *node)
        {LOGMEIN("AsmJsUtils.h] 61\n");
            return node &&
                node->nop == knopFlt &&
                node->sxFlt.maybeInt &&
                node->sxFlt.dbl > (double)INT_MAX &&
                node->sxFlt.dbl <= (double)UINT_MAX;
        }

        static bool IsDefinition( ParseNode *arg );
        static bool ParseVarOrConstStatement( AsmJSParser &parser, ParseNode **var );
        static inline bool IsNumericLiteral(ParseNode* node) {LOGMEIN("AsmJsUtils.h] 71\n"); return node && (node->nop == knopInt || node->nop == knopFlt); }
        static inline bool IsFroundNumericLiteral(ParseNode* node) {LOGMEIN("AsmJsUtils.h] 72\n"); return node && (IsNumericLiteral(node) || IsNegativeZero(node)); }
        static inline ParseNode* GetUnaryNode( ParseNode* node ){LOGMEIN("AsmJsUtils.h] 73\n");Assert(IsNodeUnary(node));return node->sxUni.pnode1;}
        static inline ParseNode* GetBinaryLeft( ParseNode* node ){LOGMEIN("AsmJsUtils.h] 74\n");Assert(IsNodeBinary(node));return node->sxBin.pnode1;}
        static inline ParseNode* GetBinaryRight( ParseNode* node ){LOGMEIN("AsmJsUtils.h] 75\n");Assert(IsNodeBinary(node));return node->sxBin.pnode2;}
        static inline ParseNode* DotBase( ParseNode *node );
        static inline bool IsDotMember( ParseNode *node );
        static inline PropertyName DotMember( ParseNode *node );
        // Get the VarDecl from the node or nullptr if unable to find
        static ParseNode* GetVarDeclList(ParseNode* node);
        // Goes through the nodes until the end of the list of VarDecl
        static void ReachEndVarDeclList( ParseNode** node );

        // nop utils
        static inline bool IsNodeBinary    (ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 85\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & (fnopBin|fnopBinList)); }
        static inline bool IsNodeUnary     (ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 86\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopUni        ); }
        static inline bool IsNodeConst     (ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 87\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopConst      ); }
        static inline bool IsNodeLeaf      (ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 88\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopLeaf       ); }
        static inline bool IsNodeRelational(ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 89\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopRel        ); }
        static inline bool IsNodeAssignment(ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 90\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopAsg        ); }
        static inline bool IsNodeBreak     (ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 91\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopBreak      ); }
        static inline bool IsNodeContinue  (ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 92\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopContinue   ); }
        static inline bool IsNodeCleanUp   (ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 93\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopCleanup    ); }
        static inline bool IsNodeJump      (ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 94\n"); return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopJump       ); }
        static inline bool IsNodeExpression(ParseNode* pnode){LOGMEIN("AsmJsUtils.h] 95\n"); return pnode &&  !(ParseNode::Grfnop(pnode->nop) & fnopNotExprStmt); }
    };

    bool ParserWrapper::IsNameDeclaration( ParseNode *node )
    {LOGMEIN("AsmJsUtils.h] 99\n");
        return node->nop == knopName || node->nop == knopStr;
    }

    bool ParserWrapper::IsNegativeZero(ParseNode *node)
    {LOGMEIN("AsmJsUtils.h] 104\n");
        return node && ((node->nop == knopFlt && JavascriptNumber::IsNegZero(node->sxFlt.dbl)) ||
            (node->nop == knopNeg && node->sxUni.pnode1->nop == knopInt && node->sxUni.pnode1->sxInt.lw == 0));
    }

    bool ParserWrapper::IsUInt( ParseNode *node )
    {LOGMEIN("AsmJsUtils.h] 110\n");
        return node->nop == knopInt || IsUnsigned(node);
    }

    uint ParserWrapper::GetUInt( ParseNode *node )
    {LOGMEIN("AsmJsUtils.h] 115\n");
        Assert( IsUInt( node ) );
        if( node->nop == knopInt )
        {LOGMEIN("AsmJsUtils.h] 118\n");
            return (uint)node->sxInt.lw;
        }
        Assert( node->nop == knopFlt );
        return (uint)node->sxFlt.dbl;
    }

    bool ParserWrapper::IsDotMember( ParseNode *node )
    {LOGMEIN("AsmJsUtils.h] 126\n");
        return node && (node->nop == knopDot || node->nop == knopIndex);
    }

    PropertyName ParserWrapper::DotMember( ParseNode *node )
    {LOGMEIN("AsmJsUtils.h] 131\n");
        Assert( IsDotMember(node) );
        if( IsNameDeclaration( GetBinaryRight( node ) ) )
        {LOGMEIN("AsmJsUtils.h] 134\n");
            return GetBinaryRight( node )->name();
        }
        return nullptr;
    }

    ParseNode* ParserWrapper::DotBase( ParseNode *node )
    {LOGMEIN("AsmJsUtils.h] 141\n");
        Assert( IsDotMember( node ) );
        return GetBinaryLeft( node );
    }

    ParseNode * ParserWrapper::GetListHead( ParseNode *node )
    {LOGMEIN("AsmJsUtils.h] 147\n");
        Assert( node->nop == knopList );
        return node->sxBin.pnode1;
    }
};
#endif
