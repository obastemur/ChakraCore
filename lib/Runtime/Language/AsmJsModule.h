//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#ifdef ASMJS_PLAT
#define ASMMATH_BUILTIN_SIZE (32)
#define ASMARRAY_BUILTIN_SIZE (16)
#define ASMSIMD_BUILTIN_SIZE (512)
namespace Js {
    // ModuleCompiler encapsulates the compilation of an entire asm.js module. Over
    // the course of a ModuleCompiler object's lifetime, many FunctionCompiler
    // objects will be created and destroyed in sequence, one for each function in
    // the module.
    //
    // *** asm.js FFI calls ***
    //
    // asm.js allows calling out to non-asm.js via "FFI calls". The asm.js type
    // system does not place any constraints on the FFI call. In particular:
    //  - an FFI call's target is not known or speculated at module-compile time;
    //  - a single external function can be called with different signatures.
    //
    // If performance didn't matter, all FFI calls could simply box their arguments
    // and call js::Invoke. However, we'd like to be able to specialize FFI calls
    // to be more efficient in several cases:
    //
    //  - for calls to JS functions which have been JITed, we'd like to call
    //    directly into JIT code without going through C++.
    //
    //  - for calls to certain builtins, we'd like to be call directly into the C++
    //    code for the builtin without going through the general call path.
    //
    // All of this requires dynamic specialization techniques which must happen
    // after module compilation. To support this, at module-compilation time, each
    // FFI call generates a call signature according to the system ABI, as if the
    // callee was a C++ function taking/returning the same types as the caller was
    // passing/expecting. The callee is loaded from a fixed offset in the global
    // data array which allows the callee to change at runtime. Initially, the
    // callee is stub which boxes its arguments and calls js::Invoke.
    //
    // To do this, we need to generate a callee stub for each pairing of FFI callee
    // and signature. We call this pairing an "exit". For example, this code has
    // two external functions and three exits:
    //
    //  function f(global, imports) {
    //    "use asm";
    //    var foo = imports.foo;
    //    var bar = imports.bar;
    //    function g() {
    //      foo(1);      // Exit #1: (int) -> void
    //      foo(1.5);    // Exit #2: (double) -> void
    //      bar(1)|0;    // Exit #3: (int) -> int
    //      bar(2)|0;    // Exit #3: (int) -> int
    //    }
    //  }
    //
    // The ModuleCompiler maintains a hash table (ExitMap) which allows a call site
    // to add a new exit or reuse an existing one. The key is an ExitDescriptor
    // (which holds the exit pairing) and the value is an index into the
    // Vector<Exit> stored in the AsmJSModule.
    //
    // Rooting note: ModuleCompiler is a stack class that contains un-rooted
    // PropertyName (JSAtom) pointers.  This is safe because it cannot be
    // constructed without a TokenStream reference.  TokenStream is itself a stack
    // class that cannot be constructed without an AutoKeepAtoms being live on the
    // stack, which prevents collection of atoms.
    //
    // ModuleCompiler is marked as rooted in the rooting analysis.  Don't add
    // non-JSAtom pointers, or this will break!
    typedef Js::Tick AsmJsCompileTime;
    namespace AsmJsLookupSource
    {
        enum Source: int
        {
            AsmJsModule, AsmJsFunction
        };
    }

    struct AsmJsModuleMemory
    {
        static const int32   MemoryTableBeginOffset = 0;
        // Memory is allocated in this order
        Field(int32) mArrayBufferOffset
            , mStdLibOffset
            , mDoubleOffset
            , mFuncOffset
            , mFFIOffset
            , mFuncPtrOffset
            , mIntOffset
            , mFloatOffset
            , mSimdOffset // in SIMDValues
            ;
        Field(int32)   mMemorySize;
    };

    struct AsmJsFunctionMemory
    {
        enum
        {
            // Register where module slots are loaded
            ModuleSlotRegister = 0,
            ReturnRegister = 0,

            FunctionRegister = 0,
            CallReturnRegister = 0,
            // These are created from the const table which starts after the FirstRegSlot
            ModuleEnvRegister = FunctionBody::FirstRegSlot,
            ArrayBufferRegister,
            ArraySizeRegister,
            ScriptContextBufferRegister,
            //Var Return register and Module Environment and Array Buffer
            RequiredVarConstants
        };
    };
    namespace AsmJsCompilation
    {
        enum Phases
        {
            Module,
            ByteCode,
            TemplateJIT,

            Phases_COUNT
        };
    };

    class AsmJsModuleCompiler
    {
        struct AsmJsModuleExport
        {
            PropertyId id;
            RegSlot location;
        };
    private:
        typedef JsUtil::BaseDictionary<PropertyId, MathBuiltin, ArenaAllocator> MathNameMap;
        typedef JsUtil::BaseDictionary<PropertyId, TypedArrayBuiltin, ArenaAllocator> ArrayNameMap;
        typedef JsUtil::BaseDictionary<PropertyId, AsmJsSymbol*, ArenaAllocator> ModuleEnvironment;
        typedef JsUtil::List<AsmJsFunc*, ArenaAllocator> ModuleFunctionArray;
        typedef JsUtil::List<AsmJsFunctionTable*, ArenaAllocator> ModuleFunctionTableArray;
        typedef JsUtil::List<AsmJsModuleExport, ArenaAllocator> ModuleExportArray;
        typedef JsUtil::Queue<AsmJsArrayView *, ArenaAllocator> ModuleArrayViewList;
        typedef WAsmJs::RegisterSpace ModuleIntVars;
        typedef WAsmJs::RegisterSpace ModuleDoubleVars;
        typedef WAsmJs::RegisterSpace ModuleFloatVars;
        typedef WAsmJs::RegisterSpace ModuleImportFunctions;

        typedef WAsmJs::RegisterSpace ModuleSIMDVars;
        typedef JsUtil::BaseDictionary<PropertyId, AsmJsSIMDFunction*, ArenaAllocator> SIMDNameMap;

        inline bool LookupStdLibSIMDNameInMap   (PropertyName name, AsmJsSIMDFunction **simdFunc, SIMDNameMap* map) const;
        bool AddStandardLibrarySIMDNameInMap    (PropertyId id, AsmJsSIMDFunction* simdFunc, SIMDNameMap* map);

        // Keep allocator first to free Dictionary before deleting the allocator
        ArenaAllocator                  mAllocator;
        ExclusiveContext *              mCx;
        AsmJSParser &                   mCurrentParserNode;
        PropertyName                    mModuleFunctionName;
        ParseNode *                     mModuleFunctionNode;
        MathNameMap                     mStandardLibraryMathNames;
        ArrayNameMap                    mStandardLibraryArrayNames;
        ModuleEnvironment               mModuleEnvironment;
        PropertyName                    mStdLibArgName, mForeignArgName, mBufferArgName;
        ModuleFunctionArray             mFunctionArray;
        ModuleIntVars                   mIntVarSpace;
        ModuleDoubleVars                mDoubleVarSpace;
        ModuleFloatVars                 mFloatVarSpace;
        ModuleImportFunctions           mImportFunctions;

        // Maps functions names to func symbols. Three maps since names are not unique across SIMD types (e.g. SIMD.{float32x4|int32x4}.add)
        // Also used to find if an operation is supported on a SIMD type.
        SIMDNameMap                         mStdLibSIMDInt32x4Map;
        SIMDNameMap                         mStdLibSIMDBool32x4Map;
        SIMDNameMap                         mStdLibSIMDBool16x8Map;
        SIMDNameMap                         mStdLibSIMDBool8x16Map;
        SIMDNameMap                         mStdLibSIMDFloat32x4Map;
        SIMDNameMap                         mStdLibSIMDFloat64x2Map;
        SIMDNameMap                         mStdLibSIMDInt16x8Map;
        SIMDNameMap                         mStdLibSIMDInt8x16Map;
        SIMDNameMap                         mStdLibSIMDUint32x4Map;
        SIMDNameMap                         mStdLibSIMDUint16x8Map;
        SIMDNameMap                         mStdLibSIMDUint8x16Map;
        // global SIMD values space.
        ModuleSIMDVars                  mSimdVarSpace;
        BVStatic<ASMSIMD_BUILTIN_SIZE>  mAsmSimdBuiltinUsedBV;

        ModuleExportArray               mExports;
        RegSlot                         mExportFuncIndex; // valid only if export object is empty
        ModuleFunctionTableArray        mFunctionTableArray;
        int                             mVarImportCount;
        int                             mVarCount;
        int32                           mFuncPtrTableCount;
        AsmJsModuleMemory               mModuleMemory;
        AsmJsCompileTime                mCompileTime;
        AsmJsCompileTime                mCompileTimeLastTick;
        int32                            mMaxAstSize;
        BVStatic<ASMMATH_BUILTIN_SIZE>  mAsmMathBuiltinUsedBV;
        BVStatic<ASMARRAY_BUILTIN_SIZE> mAsmArrayBuiltinUsedBV;
        AsmJsCompileTime                mPhaseCompileTime[AsmJsCompilation::Phases_COUNT];
        ModuleArrayViewList             mArrayViews;
        uint                            mMaxHeapAccess;
#if DBG
        bool mStdLibArgNameInit : 1;
        bool mForeignArgNameInit : 1;
        bool mBufferArgNameInit : 1;
#endif
        bool mInitialised : 1;
        bool mUsesChangeHeap : 1;
        bool mUsesHeapBuffer : 1;
    public:
        AsmJsModuleCompiler( ExclusiveContext *cx, AsmJSParser &parser );
        bool Init();
        bool InitSIMDBuiltins();

        // Resolves a SIMD function name to its symbol
        bool LookupStdLibSIMDName(PropertyId baseId, PropertyName fieldName, AsmJsSIMDFunction **simdFunc);
        bool LookupStdLibSIMDName(AsmJsSIMDBuiltinFunction baseId, PropertyName fieldName, AsmJsSIMDFunction **simdFunc);

        // Resolves a symbol name to SIMD constructor/operation and perform checks
        AsmJsSIMDFunction *LookupSimdConstructor(PropertyName name);
        AsmJsSIMDFunction *LookupSimdTypeCheck(PropertyName name);
        AsmJsSIMDFunction *LookupSimdOperation(PropertyName name);

        void AddSimdBuiltinUse(int index){TRACE_IT(46834); mAsmSimdBuiltinUsedBV.Set(index); }
        // adds SIMD constant var to module
        bool AddSimdValueVar(PropertyName name, ParseNode* pnode, AsmJsSIMDFunction* simdFunc);

        AsmJsCompileTime GetTick();
        void AccumulateCompileTime();
        void AccumulateCompileTime(AsmJsCompilation::Phases phase);
        // Return compile time in ms
        uint64 GetCompileTime() const;
        void PrintCompileTrace() const;

        // A valid module may have a NULL name
        inline PropertyName GetModuleFunctionName() const{TRACE_IT(46835);return mModuleFunctionName;}
        inline ParseNode *GetModuleFunctionNode() const{TRACE_IT(46836);return mModuleFunctionNode;}

        inline ArenaAllocator* GetAllocator() {TRACE_IT(46837);return &mAllocator;}
        inline int32 GetMaxAstSize() const{TRACE_IT(46838);return mMaxAstSize;}
        inline void UpdateMaxAstSize( int32 val ){TRACE_IT(46839);mMaxAstSize = val>mMaxAstSize?val:mMaxAstSize;}

        //Mutable interface
        inline void InitModuleName( PropertyName name ){TRACE_IT(46840);mModuleFunctionName = name;}
        inline void InitModuleNode( AsmJSParser &parser ){TRACE_IT(46841);mModuleFunctionNode = parser;}
        inline AsmJSParser& GetCurrentParserNode(){TRACE_IT(46842);return mCurrentParserNode;}
        inline void SetCurrentParseNode( AsmJSParser & val ){TRACE_IT(46843);mCurrentParserNode = val;}

        void InitStdLibArgName( PropertyName n );
        void InitForeignArgName( PropertyName n );
        void InitBufferArgName( PropertyName n );
        PropertyName GetBufferArgName()  const;
        PropertyName GetForeignArgName() const;
        PropertyName GetStdLibArgName()  const;
        BVStatic<ASMMATH_BUILTIN_SIZE> GetAsmMathBuiltinUsedBV();
        void AddMathBuiltinUse(int index){TRACE_IT(46844); mAsmMathBuiltinUsedBV.Set(index); }
        BVStatic<ASMARRAY_BUILTIN_SIZE> GetAsmArrayBuiltinUsedBV();
        void AddArrayBuiltinUse(int index){TRACE_IT(46845); mAsmArrayBuiltinUsedBV.Set(index); }
        bool LookupStandardLibraryMathName(PropertyName name, MathBuiltin *mathBuiltin) const;
        bool LookupStandardLibraryArrayName(PropertyName name, TypedArrayBuiltin *builtin) const;

        // Lookup the name in the function environment if provided, then the module environment
        // indicate the origin of the symbol if specified
        AsmJsSymbol* LookupIdentifier( PropertyName name, AsmJsFunc* func = nullptr, AsmJsLookupSource::Source* lookupSource = nullptr );
        AsmJsFunctionDeclaration* LookupFunction( PropertyName name );
        bool DefineIdentifier( PropertyName name, AsmJsSymbol* symbol );
        bool AddNumericVar( PropertyName name, ParseNode* pnode, bool isFloat, bool isMutable = true);
        bool AddGlobalVarImport( PropertyName name, PropertyName field, AsmJSCoercion coercion );
        bool AddModuleFunctionImport( PropertyName name, PropertyName field );
        bool AddNumericConst( PropertyName name, const double* cst );
        bool AddArrayView( PropertyName name, ArrayBufferView::ViewType type );
        bool AddExport( PropertyName name, RegSlot location );
        bool SetExportFunc( AsmJsFunc* func );
        bool AddFunctionTable( PropertyName name, const int size );
        void AddMathLibName(PropertyId pid);
        //Immutable interface
        Parser *GetParser() const;
        ByteCodeGenerator* GetByteCodeGenerator() const;
        ScriptContext  *GetScriptContext() const;

        bool FailName( ParseNode *usepn, const wchar *fmt, PropertyName name );
        bool Fail( ParseNode* usepn, const wchar *error );

        bool AreAllFuncTableDefined();
        bool UsesChangeHeap() {TRACE_IT(46846); return mUsesChangeHeap; }
        bool UsesHeapBuffer() {TRACE_IT(46847); return mUsesHeapBuffer; }
        void SetUsesHeapBuffer(bool val) {TRACE_IT(46848); mUsesHeapBuffer = val; }
        void UpdateMaxHeapAccess(uint index);
        uint GetMaxHeapAccess() {TRACE_IT(46849); return mMaxHeapAccess; }
        // Compile/Validate function name and arguments (define their types)
        bool CompileFunction(AsmJsFunc * func, int funcIndex);
        bool CompileAllFunctions();
        void RevertAllFunctions();
        bool CommitFunctions();
        bool CommitModule();
        bool FinalizeModule();
        AsmJsFunc* CreateNewFunctionEntry(ParseNode* pnodeFnc);
        bool CheckChangeHeap(AsmJsFunc * func);


        void InitMemoryOffsets           ();
        inline int32 GetIntOffset        () const{TRACE_IT(46850);return mModuleMemory.mIntOffset;}
        inline int32 GetFloatOffset        () const{TRACE_IT(46851);return mModuleMemory.mFloatOffset;}
        inline int32 GetFuncPtrOffset    () const{TRACE_IT(46852);return mModuleMemory.mFuncPtrOffset;}
        inline int32 GetFFIOffset        () const{TRACE_IT(46853);return mModuleMemory.mFFIOffset;}
        inline int32 GetFuncOffset       () const{TRACE_IT(46854);return mModuleMemory.mFuncOffset;}
        inline int32 GetDoubleOffset     () const{TRACE_IT(46855);return mModuleMemory.mDoubleOffset; }
        inline int32 GetSimdOffset       () const{TRACE_IT(46856); return mModuleMemory.mSimdOffset;  }

        inline int32 GetFuncPtrTableCount() const{TRACE_IT(46857);return mFuncPtrTableCount;}
        inline void SetFuncPtrTableCount ( int32 val ){TRACE_IT(46858);mFuncPtrTableCount = val;}

    private:
        void RevertFunction(int funcIndex);
        bool SetupFunctionArguments(AsmJsFunc * func, ParseNodePtr pnode);
        bool SetupLocalVariables(AsmJsFunc * func);
        void ASTPrepass(ParseNodePtr pnode, AsmJsFunc * func);
        void BindArguments(ParseNode* argList);
        bool AddStandardLibraryMathName(PropertyId id, AsmJsMathFunction* func, AsmJSMathBuiltinFunction mathLibFunctionName);
        bool AddStandardLibraryMathName(PropertyId id, const double* cstAddr, AsmJSMathBuiltinFunction mathLibFunctionName);
        bool AddStandardLibraryArrayName(PropertyId id, AsmJsTypedArrayFunction * func, AsmJSTypedArrayBuiltinFunction mathLibFunctionName);
        bool CheckByteLengthCall(ParseNode * node, ParseNode * newBufferDecl);
        bool ValidateSimdConstructor(ParseNode* pnode, AsmJsSIMDFunction* simdFunc, AsmJsSIMDValue& value);
        bool IsSimdjsEnabled() {TRACE_IT(46859); return GetScriptContext()->GetConfig()->IsSimdjsEnabled(); }
    };

    struct AsmJsSlot
    {
        Field(RegSlot) location;
        Field(AsmJsSymbol::SymbolType) symType;
        union
        {
            Field(AsmJsVarType::Which) varType;
            Field(ArrayBufferView::ViewType) viewType;
            Field(double) mathConstVal;
            Field(uint) funcTableSize;
            Field(AsmJsModuleArg::ArgType) argType;
            Field(AsmJSMathBuiltinFunction) builtinMathFunc;
            Field(AsmJSTypedArrayBuiltinFunction) builtinArrayFunc;
            Field(AsmJsSIMDBuiltinFunction) builtinSIMDFunc;
        };
        Field(bool) isConstVar = false;
    };

    class AsmJsModuleInfo
    {
    public:
        /// proxy of asmjs module
        struct ModuleVar
        {
            Field(RegSlot) location;
            Field(AsmJsVarType::Which) type;
            union InitialiserType
            {
                Field(int) intInit;
                Field(float) floatInit;
                Field(double) doubleInit;
                Field(AsmJsSIMDValue) simdInit;
            };
            Field(InitialiserType) initialiser; // (leish)(swb) false positive found here
            Field(bool) isMutable;
        };
        struct ModuleVarImport
        {
            Field(RegSlot) location;
            Field(AsmJsVarType::Which) type;
            Field(PropertyId) field;
        };
        struct ModuleFunctionImport
        {
            Field(RegSlot) location;
            Field(PropertyId) field;
        };
        struct ModuleFunction
        {
            Field(RegSlot) location;
        };
        struct ModuleExport
        {
            PropertyId* id;
            RegSlot* location;
        };
        struct ModuleFunctionTable
        {
            Field(uint) size;
            Field(RegSlot*) moduleFunctionIndex;
        };

        typedef JsUtil::BaseDictionary<PropertyId, AsmJsSlot*, Memory::Recycler> AsmJsSlotMap;

    private:
        FieldNoBarrier(Recycler*) mRecycler;
        Field(int) mArgInCount; // for runtime validation of arguments in
        Field(int) mVarCount, mVarImportCount, mFunctionImportCount, mFunctionCount, mFunctionTableCount, mExportsCount, mSlotsCount;
        Field(int) mSimdRegCount; // part of mVarCount

        Field(PropertyIdArray*)             mExports;
        Field(RegSlot*)                     mExportsFunctionLocation;
        Field(RegSlot)                      mExportFunctionIndex; // valid only if export object is empty
        Field(ModuleVar*)                   mVars;
        Field(ModuleVarImport*)             mVarImports;
        Field(ModuleFunctionImport*)        mFunctionImports;
        Field(ModuleFunction*)              mFunctions;
        Field(ModuleFunctionTable*)         mFunctionTables;
        Field(AsmJsModuleMemory)            mModuleMemory;
        Field(AsmJsSlotMap*)                mSlotMap;
        Field(BVStatic<ASMMATH_BUILTIN_SIZE>)  mAsmMathBuiltinUsed;
        Field(BVStatic<ASMARRAY_BUILTIN_SIZE>) mAsmArrayBuiltinUsed;
        Field(BVStatic<ASMSIMD_BUILTIN_SIZE>)  mAsmSimdBuiltinUsed;

        Field(uint)                         mMaxHeapAccess;
        Field(bool)                         mUsesChangeHeap;
        Field(bool)                         mIsProcessed;
    public:
        AsmJsModuleInfo( Recycler* recycler ) :
            mRecycler( recycler )
            , mArgInCount( 0 )
            , mVarCount( 0 )
            , mVarImportCount( 0 )
            , mFunctionImportCount( 0 )
            , mFunctionCount( 0 )
            , mFunctionTableCount( 0 )
            , mSimdRegCount(0)

            , mVars( nullptr )
            , mVarImports( nullptr )
            , mFunctionImports( nullptr )
            , mFunctions( nullptr )
            , mMaxHeapAccess(0)
            , mUsesChangeHeap(false)
            , mIsProcessed(false)
            , mSlotMap(nullptr)
        {TRACE_IT(46860);

        }

        ModuleVar& GetVar( int i )
        {TRACE_IT(46861);
            Assert( i < mVarCount );
            return mVars[i];
        }

        void SetVar(int i, ModuleVar var)
        {TRACE_IT(46862);
            Assert(i < mVarCount);
            mVars[i] = var;
        }

        ModuleVarImport& GetVarImport( int i )
        {TRACE_IT(46863);
            Assert( i < mVarImportCount );
            return mVarImports[i];
        }

        void SetVarImport(int i, ModuleVarImport var)
        {TRACE_IT(46864);
            Assert(i < mVarImportCount);
            mVarImports[i] = var;
        }

        ModuleFunctionImport& GetFunctionImport( int i )
        {TRACE_IT(46865);
            Assert( i < mFunctionImportCount );
            return mFunctionImports[i];
        }
        void SetFunctionImport(int i, ModuleFunctionImport var)
        {TRACE_IT(46866);
            Assert(i < mFunctionImportCount);
            mFunctionImports[i] = var;
        }
        ModuleFunction& GetFunction( int i )
        {TRACE_IT(46867);
            Assert( i < mFunctionCount );
            return mFunctions[i];
        }
        void SetFunction(int i, ModuleFunction var)
        {TRACE_IT(46868);
            Assert(i < mFunctionCount);
            mFunctions[i] = var;
        }
        ModuleFunctionTable& GetFunctionTable( int i )
        {TRACE_IT(46869);
            Assert( i < mFunctionTableCount );
            return mFunctionTables[i];
        }
        void SetFunctionTable(int i, ModuleFunctionTable var)
        {TRACE_IT(46870);
            Assert(i < mFunctionTableCount);
            mFunctionTables[i] = var;
        }
        void SetFunctionTableSize( int index, uint size );
        ModuleExport GetExport( int i )
        {TRACE_IT(46871);
            ModuleExport ex;
            ex.id = &mExports->elements[i];
            ex.location = &mExportsFunctionLocation[i];
            return ex;
        }
        RegSlot* GetExportsFunctionLocation() const
        {TRACE_IT(46872);
            return mExportsFunctionLocation;
        }
        PropertyIdArray* GetExportsIdArray() const
        {TRACE_IT(46873);
            return mExports;
        }

        AsmJsSlotMap* GetAsmJsSlotMap()
        {TRACE_IT(46874);
            return mSlotMap;
        }

        // Accessors
    public:
        inline Js::RegSlot GetExportFunctionIndex() const{TRACE_IT(46875);return mExportFunctionIndex;}
        inline void SetExportFunctionIndex( Js::RegSlot val ){TRACE_IT(46876);mExportFunctionIndex = val;}
        void SetExportsCount(int count);
        inline int GetExportsCount()const
        {TRACE_IT(46877);
            return mExportsCount;
        }
        inline int GetArgInCount() const
        {TRACE_IT(46878);
            return mArgInCount;
        }
        inline void SetArgInCount( int val )
        {TRACE_IT(46879);
            mArgInCount = val;
        }
        inline int GetFunctionCount() const
        {TRACE_IT(46880);
            return mFunctionCount;
        }
        void SetFunctionCount( int val );
        inline int GetFunctionTableCount() const
        {TRACE_IT(46881);
            return mFunctionTableCount;
        }
        void SetFunctionTableCount( int val );
        inline int GetFunctionImportCount() const
        {TRACE_IT(46882);
            return mFunctionImportCount;
        }
        void SetFunctionImportCount( int val );
        inline int GetVarImportCount() const
        {TRACE_IT(46883);
            return mVarImportCount;
        }
        void SetVarImportCount( int val );
        inline int GetVarCount() const
        {TRACE_IT(46884);
            return mVarCount;
        }
        void SetVarCount( int val );

        inline int GetSlotsCount() const
        {TRACE_IT(46885);
            return mSlotsCount;
        }
        void InitializeSlotMap(int val);
        inline bool IsRuntimeProcessed() const
        {TRACE_IT(46886);
            return mIsProcessed;
        }
        void SetIsRuntimeProcessed(bool val)
        {TRACE_IT(46887);
            mIsProcessed = val;
        }
        inline AsmJsModuleMemory& GetModuleMemory()
        {TRACE_IT(46888);
            return mModuleMemory;
        }
        inline void SetModuleMemory( const AsmJsModuleMemory& val )
        {TRACE_IT(46889);
            mModuleMemory = val;
        }
        inline void SetAsmMathBuiltinUsed(const BVStatic<ASMMATH_BUILTIN_SIZE> val)
        {TRACE_IT(46890);
            mAsmMathBuiltinUsed = val;
        }
        inline BVStatic<ASMMATH_BUILTIN_SIZE> GetAsmMathBuiltinUsed()const
        {TRACE_IT(46891);
            return mAsmMathBuiltinUsed;
        }
        inline void SetAsmArrayBuiltinUsed(const BVStatic<ASMARRAY_BUILTIN_SIZE> val)
        {TRACE_IT(46892);
            mAsmArrayBuiltinUsed = val;
        }
        inline BVStatic<ASMARRAY_BUILTIN_SIZE> GetAsmArrayBuiltinUsed()const
        {TRACE_IT(46893);
            return mAsmArrayBuiltinUsed;
        }
        void SetUsesChangeHeap(bool val)
        {TRACE_IT(46894);
            mUsesChangeHeap = val;
        }
        inline bool GetUsesChangeHeap() const
        {TRACE_IT(46895);
            return mUsesChangeHeap;
        }
        void SetMaxHeapAccess(uint val)
        {TRACE_IT(46896);
            mMaxHeapAccess = val;
        }
        inline uint GetMaxHeapAccess() const
        {TRACE_IT(46897);
            return mMaxHeapAccess;
        }

        inline void SetSimdRegCount(int val) {TRACE_IT(46898); mSimdRegCount = val;  }
        inline int GetSimdRegCount() const   {TRACE_IT(46899); return mSimdRegCount; }
        inline void SetAsmSimdBuiltinUsed(const BVStatic<ASMSIMD_BUILTIN_SIZE> val)
        {TRACE_IT(46900);
            mAsmSimdBuiltinUsed = val;
        }
        inline BVStatic<ASMSIMD_BUILTIN_SIZE> GetAsmSimdBuiltinUsed()const
        {TRACE_IT(46901);
            return mAsmSimdBuiltinUsed;
        }

        static void EnsureHeapAttached(ScriptFunction * func);
        static void * ConvertFrameForJavascript(void* asmJsMemory, ScriptFunction * func);
    };
};
#endif
