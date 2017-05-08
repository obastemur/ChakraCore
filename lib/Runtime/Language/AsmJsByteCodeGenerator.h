//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef ASMJS_PLAT
namespace Js
{
    enum EBinaryMathOpCodes: int;
    enum EBinaryComparatorOpCodes: int;

    // Information about the expression that has been emitted
    struct EmitExpressionInfo : WAsmJs::EmitInfoBase
    {
        EmitExpressionInfo( RegSlot location_, const AsmJsType& type_ ) :
            WAsmJs::EmitInfoBase( location_ ), type( type_ )
        {TRACE_IT(46175);
        }
        EmitExpressionInfo( const AsmJsType& type_ ) : type( type_ ) {TRACE_IT(46176);}
        EmitExpressionInfo(): type( AsmJsType::Void ) {TRACE_IT(46177);}

        AsmJsType type;
    };

    /// AutoPtr cleanup for asmjs bytecode compilation
    class AsmJsFunctionCompilation
    {
        AsmJSByteCodeGenerator* mGenerator;
    public:
        AsmJsFunctionCompilation( AsmJSByteCodeGenerator* gen ) :
            mGenerator( gen )
        {TRACE_IT(46178);

        }
        ~AsmJsFunctionCompilation()
        {TRACE_IT(46179);
            CleanUp();
        }

        void CleanUp();

        void FinishCompilation()
        {TRACE_IT(46180);
            mGenerator = nullptr;
        }

    };


    class AsmJSByteCodeGenerator
    {
        friend AsmJsFunctionCompilation;
        AsmJsFunc*         mFunction;
        FuncInfo*          mInfo;
        AsmJsModuleCompiler* mCompiler;
        // Reference to non-asmjs bytecode gen. Needed to bind fields for SIMD.js code
        ByteCodeGenerator* mByteCodeGenerator;
        AsmJsByteCodeWriter mWriter;
        int mNestedCallCount;
        bool mIsCallLegal;
    public:
        AsmJSByteCodeGenerator(AsmJsFunc* func, AsmJsModuleCompiler* compiler);
        static void EmitEmptyByteCode(FuncInfo* funcInfo, ByteCodeGenerator* byteCodeGen, ParseNode* funcNode);

        bool EmitOneFunction();
    private:
        ArenaAllocator mAllocator;
        bool BlockHasOwnScope( ParseNode* pnodeBlock );

        void PrintAsmJsCompilationError(__out_ecount(256) char16* msg);
        void DefineLabels();

        void EmitAsmJsFunctionBody();
        void EmitTopLevelStatement( ParseNode *stmt );
        EmitExpressionInfo Emit( ParseNode *pnode );
        EmitExpressionInfo EmitIdentifier( ParseNode * pnode );
        EmitExpressionInfo EmitLdArrayBuffer( ParseNode * pnode );
        enum TypedArrayEmitType{
            LoadTypedArray,
            StoreTypedArray,
        };
        EmitExpressionInfo EmitTypedArrayIndex(ParseNode* indexNode, OpCodeAsmJs &op, uint32 &indexSlot, ArrayBufferView::ViewType viewType, TypedArrayEmitType emitType);
        EmitExpressionInfo EmitAssignment( ParseNode * pnode );
        EmitExpressionInfo EmitReturn( ParseNode * pnode );
        EmitExpressionInfo EmitCall( ParseNode * pnode, AsmJsRetType expectedType = AsmJsRetType::Void );
        EmitExpressionInfo EmitMathBuiltin( ParseNode* pnode, AsmJsMathFunction* mathFunction, AsmJsRetType expectedType );
        EmitExpressionInfo EmitMinMax(ParseNode* pnode, AsmJsMathFunction* mathFunction, AsmJsRetType expectedType);
        EmitExpressionInfo EmitUnaryPos( ParseNode * pnode );
        EmitExpressionInfo EmitUnaryNeg( ParseNode * pnode );
        EmitExpressionInfo EmitUnaryNot( ParseNode * pnode );
        EmitExpressionInfo EmitUnaryLogNot( ParseNode * pnode );
        EmitExpressionInfo EmitBinaryMultiType( ParseNode * pnode, EBinaryMathOpCodes op );
        EmitExpressionInfo EmitBinaryInt( ParseNode * pnode, OpCodeAsmJs op );
        EmitExpressionInfo EmitQMark( ParseNode * pnode );
        EmitExpressionInfo EmitSwitch( ParseNode * pnode );
        EmitExpressionInfo EmitBinaryComparator( ParseNode * pnode, EBinaryComparatorOpCodes op);
        EmitExpressionInfo EmitLoop( ParseNode *loopNode, ParseNode *cond, ParseNode *body, ParseNode *incr, BOOL doWhile = false );
        EmitExpressionInfo EmitIf( ParseNode * pnode );
        EmitExpressionInfo EmitBooleanExpression( ParseNode* pnodeCond, Js::ByteCodeLabel trueLabel, Js::ByteCodeLabel falseLabel );

        EmitExpressionInfo* EmitSimdBuiltinArguments(ParseNode* pnode, AsmJsFunctionDeclaration* func, __out_ecount(pnode->sxCall.argCount) AsmJsType *argsTypes, EmitExpressionInfo *argsInfo);
        bool ValidateSimdFieldAccess(PropertyName field, const AsmJsType& receiverType, OpCodeAsmJs &op);
        EmitExpressionInfo EmitDotExpr(ParseNode* pnode);
        EmitExpressionInfo EmitSimdBuiltin(ParseNode* pnode, AsmJsSIMDFunction* simdFunction, AsmJsRetType expectedType);
        EmitExpressionInfo EmitSimdLoadStoreBuiltin(ParseNode* pnode, AsmJsSIMDFunction* simdFunction, AsmJsRetType expectedType);

        void FinalizeRegisters( FunctionBody* byteCodeFunction );
        template<typename T> byte* SetConstsToTable(byte* byteTable, T zeroValue);
        void LoadAllConstants();
        void StartStatement(ParseNode* pnode);
        void EndStatement(ParseNode* pnode);

        // Emits the bytecode to load from the module
        // dst is the location of the variable in the function
        // index is the location of the target in the module's table
        void LoadModuleInt(RegSlot dst, RegSlot index); // dst points to the IntRegisterSpace
        void LoadModuleFloat(RegSlot dst, RegSlot index); // dst points to the FloatRegisterSpace
        void LoadModuleDouble( RegSlot dst, RegSlot index ); // dst points to the DoubleRegisterSpace

        void LoadModuleFFI( RegSlot dst, RegSlot index ); // dst points to a Var
        void LoadModuleFunction( RegSlot dst, RegSlot index ); // dst points to a Var
        void LoadModuleFunctionTable( RegSlot dst, RegSlot FuncTableIndex, RegSlot FuncIndexLocation ); // dst points to a Var

        // Emits the bytecode to set a variable int the module
        // dst is the location of the variable in the module's table
        // src is the location of the variable in the function
        void SetModuleInt(Js::RegSlot dst, RegSlot src);
        void SetModuleFloat(Js::RegSlot dst, RegSlot src);
        void SetModuleDouble( Js::RegSlot dst, RegSlot src );

        void LoadModuleSimd(RegSlot dst, RegSlot index, AsmJsVarType type);
        void SetModuleSimd(RegSlot dst, RegSlot src, AsmJsVarType type);
        void LoadSimd(RegSlot dst, RegSlot src, AsmJsVarType type);

        bool IsFRound(AsmJsMathFunction* sym);
        bool IsValidSimdFcnRetType(AsmJsSIMDFunction& simdFunction, const AsmJsRetType& expectedType, const AsmJsRetType& retType);
        /// TODO:: Finish removing references to old bytecode generator
        ByteCodeGenerator* GetOldByteCodeGenerator() const
        {TRACE_IT(46181);
            return mByteCodeGenerator;
        }

        bool IsSimdjsEnabled()
        {TRACE_IT(46182);
            return mFunction->GetFuncBody()->GetScriptContext()->GetConfig()->IsSimdjsEnabled();
        }
        // try to reuse a tmp register or acquire a new one
        // also takes care of releasing tmp register
        template<typename T>
        RegSlot GetAndReleaseBinaryLocations( const EmitExpressionInfo* lhs, const EmitExpressionInfo* rhs )
        {TRACE_IT(46183);
            RegSlot tmpRegToUse;
            if( mFunction->IsTmpLocation<T>( lhs ) )
            {TRACE_IT(46184);
                tmpRegToUse = lhs->location;
                mFunction->ReleaseLocation<T>( rhs );
            }
            else if( mFunction->IsTmpLocation<T>( rhs ) )
            {TRACE_IT(46185);
                tmpRegToUse = rhs->location;
                mFunction->ReleaseLocation<T>( lhs );
            }
            else
            {TRACE_IT(46186);
                tmpRegToUse = mFunction->AcquireTmpRegister<T>();
                mFunction->ReleaseLocation<T>( rhs );
                mFunction->ReleaseLocation<T>( lhs );
            }
            return tmpRegToUse;
        }

        template<typename T>
        RegSlot GetAndReleaseUnaryLocations( const EmitExpressionInfo* rhs )
        {TRACE_IT(46187);
            RegSlot tmpRegToUse;
            if( mFunction->IsTmpLocation<T>( rhs ) )
            {TRACE_IT(46188);
                tmpRegToUse = rhs->location;
            }
            else
            {TRACE_IT(46189);
                tmpRegToUse = mFunction->AcquireTmpRegister<T>();
                mFunction->ReleaseLocation<T>( rhs );
            }
            return tmpRegToUse;
        }
    };
}
#endif
