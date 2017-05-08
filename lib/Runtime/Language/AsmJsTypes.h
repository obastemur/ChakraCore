//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
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
#include "Language/WAsmjsUtils.h"

#ifdef ASMJS_PLAT
namespace Wasm
{
    struct WasmReaderInfo;
};

namespace Js
{
    typedef uint32 uint32_t;
    typedef IdentPtr PropertyName;
    typedef ParseNode* AsmJSParser;

    // These EcmaScript-defined coercions form the basis of the asm.js type system.
    enum AsmJSCoercion
    {
        AsmJS_ToInt32,
        AsmJS_ToNumber,
        AsmJS_FRound,
        AsmJS_Int32x4,
        AsmJS_Bool32x4,
        AsmJS_Bool16x8,
        AsmJS_Bool8x16,
        AsmJS_Float32x4,
        AsmJS_Float64x2,
        AsmJS_Int16x8,
        AsmJS_Int8x16,
        AsmJS_Uint32x4,
        AsmJS_Uint16x8,
        AsmJS_Uint8x16,
    };

    namespace ArrayBufferView
    {
        enum ViewType: int
        {
            TYPE_INT8 = 0,
            TYPE_UINT8,
            TYPE_INT16,
            TYPE_UINT16,
            TYPE_INT32,
            TYPE_UINT32,
            TYPE_FLOAT32,
            TYPE_FLOAT64,
            TYPE_INT64,
            TYPE_INT8_TO_INT64,
            TYPE_UINT8_TO_INT64,
            TYPE_INT16_TO_INT64,
            TYPE_UINT16_TO_INT64,
            TYPE_INT32_TO_INT64,
            TYPE_UINT32_TO_INT64,
            TYPE_COUNT
        };

        const uint32 ViewMask[] =
        {
            (uint32)~0 //TYPE_INT8
            , (uint32)~0 //TYPE_UINT8
            , (uint32)~1 //TYPE_INT16
            , (uint32)~1 //TYPE_UINT16
            , (uint32)~3 //TYPE_INT32
            , (uint32)~3 //TYPE_UINT32
            , (uint32)~3 //TYPE_FLOAT32
            , (uint32)~7 //TYPE_FLOAT64
            , (uint32)~7 //TYPE_INT64
            , (uint32)~0 //TYPE_INT8_TO_INT64
            , (uint32)~0 //TYPE_UINT8_TO_UINT64
            , (uint32)~1 //TYPE_INT16_TO_INT64
            , (uint32)~1 //TYPE_UINT16_TO_UINT64
            , (uint32)~3 //TYPE_INT32_TO_INT64
            , (uint32)~3 //TYPE_UINT32_TO_UINT64
        };

    } /* namespace ArrayBufferView */
    // The asm.js spec recognizes this set of builtin Math functions.
    enum AsmJSMathBuiltinFunction: int
    {
#define ASMJS_MATH_FUNC_NAMES(name, propertyName) AsmJSMathBuiltin_##name,
#include "AsmJsBuiltInNames.h"
        AsmJSMathBuiltinFunction_COUNT,
#define ASMJS_MATH_CONST_NAMES(name, propertyName) AsmJSMathBuiltin_##name,
#include "AsmJsBuiltInNames.h"
        AsmJSMathBuiltin_COUNT
    };
    enum AsmJSTypedArrayBuiltinFunction
    {
#define ASMJS_ARRAY_NAMES(name, propertyName) AsmJSTypedArrayBuiltin_##name,
#include "AsmJsBuiltInNames.h"
        AsmJSTypedArrayBuiltin_COUNT
    };
    // Represents the type of a general asm.js expression.
    class AsmJsType
    {
    public:
        enum Which
        {
            Int,
            Int64,
            Double,
            Float,
            MaybeDouble,
            DoubleLit,          // Double literal. Needed for SIMD.js. Sub-type of Double
            MaybeFloat,
            Floatish,
            FloatishDoubleLit,  // A sum-type for Floatish and DoubleLit. Needed for float32x4(..) arg types.
            Fixnum,
            Signed,
            Unsigned,
            Intish,
            Void,
            Int32x4,
            Uint32x4,
            Int16x8,
            Int8x16,
            Uint16x8,
            Uint8x16,
            Bool32x4,
            Bool16x8,
            Bool8x16,
            Float32x4,
            Float64x2
        };

    private:
        Which which_;

    public:
        AsmJsType() : which_( Which( -1 ) ){TRACE_IT(47080);}
        AsmJsType( Which w ) : which_( w ){TRACE_IT(47081);}

        bool operator==( AsmJsType rhs ) const;
        bool operator!=( AsmJsType rhs ) const;
        inline Js::AsmJsType::Which GetWhich() const{TRACE_IT(47082);return which_;}
        bool isSigned() const;
        bool isUnsigned() const;
        bool isInt() const;
        bool isIntish() const;
        bool isDouble() const;
        bool isMaybeDouble() const;
        bool isDoubleLit() const;
        bool isFloat() const;
        bool isMaybeFloat() const;
        bool isFloatish() const;
        bool isFloatishDoubleLit() const;
        bool isVoid() const;
        bool isExtern() const;
        bool isVarAsmJsType() const;
        bool isSubType( AsmJsType type ) const;
        bool isSuperType( AsmJsType type ) const;
        const char16 *toChars() const;
        bool isSIMDType() const;
        bool isSIMDInt32x4() const;
        bool isSIMDBool32x4() const;
        bool isSIMDBool16x8() const;
        bool isSIMDBool8x16() const;
        bool isSIMDFloat32x4() const;
        bool isSIMDFloat64x2() const;
        bool isSIMDInt16x8() const;
        bool isSIMDInt8x16() const;
        bool isSIMDUint32x4() const;
        bool isSIMDUint16x8() const;
        bool isSIMDUint8x16() const;
        AsmJsRetType toRetType() const;
    };

    // Represents the subset of AsmJsType that can be used as the return AsmJsType of a
    // function.
    class AsmJsRetType
    {
    public:
        enum Which
        {
            Void = AsmJsType::Void,
            Signed = AsmJsType::Signed,
            Int64 = AsmJsType::Int64,
            Double = AsmJsType::Double,
            Float = AsmJsType::Float,
            Fixnum = AsmJsType::Fixnum,
            Unsigned = AsmJsType::Unsigned,
            Floatish = AsmJsType::Floatish,
            Int32x4 = AsmJsType::Int32x4,
            Bool32x4 = AsmJsType::Bool32x4,
            Bool16x8 = AsmJsType::Bool16x8,
            Bool8x16 = AsmJsType::Bool8x16,
            Float32x4 = AsmJsType::Float32x4,
            Float64x2 = AsmJsType::Float64x2,
            Int16x8 = AsmJsType::Int16x8,
            Int8x16 = AsmJsType::Int8x16,
            Uint32x4 = AsmJsType::Uint32x4,
            Uint16x8 = AsmJsType::Uint16x8,
            Uint8x16 = AsmJsType::Uint8x16
        };

    private:
        Field(Which) which_;

    public:
        AsmJsRetType();
        AsmJsRetType( Which w );
        AsmJsRetType( AsmJSCoercion coercion );
        Which which() const;
        AsmJsType toType() const;
        AsmJsVarType toVarType() const;

        bool operator==( AsmJsRetType rhs ) const;
        bool operator!=( AsmJsRetType rhs ) const;
    };

    // Represents the subset of Type that can be used as a variable or
    // argument's type. Note: AsmJSCoercion and VarType are kept separate to
    // make very clear the signed/int distinction: a coercion may explicitly sign
    // an *expression* but, when stored as a variable, this signedness information
    // is explicitly thrown away by the asm.js type system. E.g., in
    //
    //   function f(i) {
    //     i = i | 0;             (1)
    //     if (...)
    //         i = foo() >>> 0;
    //     else
    //         i = bar() | 0;
    //     return i | 0;          (2)
    //   }
    //
    // the AsmJSCoercion of (1) is Signed (since | performs ToInt32) but, when
    // translated to a VarType, the result is a plain Int since, as shown, it
    // is legal to assign both Signed and Unsigned (or some other Int) values to
    // it. For (2), the AsmJSCoercion is also Signed but, when translated to an
    // RetType, the result is Signed since callers (asm.js and non-asm.js) can
    // rely on the return value being Signed.
    class AsmJsVarType
    {
    public:
        enum Which : byte
        {
            Int = AsmJsType::Int,
            Int64 = AsmJsType::Int64,
            Double = AsmJsType::Double,
            Float = AsmJsType::Float,
            Int32x4 = AsmJsType::Int32x4,
            Bool32x4 = AsmJsType::Bool32x4,
            Bool16x8 = AsmJsType::Bool16x8,
            Bool8x16 = AsmJsType::Bool8x16,
            Float32x4 = AsmJsType::Float32x4,
            Float64x2 = AsmJsType::Float64x2,
            Int16x8 = AsmJsType::Int16x8,
            Int8x16 = AsmJsType::Int8x16,
            Uint32x4 = AsmJsType::Uint32x4,
            Uint16x8 = AsmJsType::Uint16x8,
            Uint8x16 = AsmJsType::Uint8x16
        };

    private:
        Which which_;

    public:
        AsmJsVarType();
        AsmJsVarType( Which w );
        AsmJsVarType( AsmJSCoercion coercion );
        Which which() const;
        AsmJsType toType() const;
        AsmJSCoercion toCoercion() const;
        static AsmJsVarType FromCheckedType( AsmJsType type );
        inline bool isInt()const {TRACE_IT(47083);return which_ == Int; }
        inline bool isInt64()const {TRACE_IT(47084);return which_ == Int64; }
        inline bool isDouble()const {TRACE_IT(47085);return which_ == Double; }
        inline bool isFloat()const {TRACE_IT(47086);return which_ == Float; }
        inline bool isInt32x4()const    {TRACE_IT(47087); return which_ == Int32x4; }
        inline bool isBool32x4()const   {TRACE_IT(47088); return which_ == Bool32x4; }
        inline bool isBool16x8()const   {TRACE_IT(47089); return which_ == Bool16x8; }
        inline bool isBool8x16()const   {TRACE_IT(47090); return which_ == Bool8x16; }
        inline bool isFloat32x4()const  {TRACE_IT(47091); return which_ == Float32x4; }
        inline bool isFloat64x2()const  {TRACE_IT(47092); return which_ == Float64x2; }
        inline bool isInt16x8() const   {TRACE_IT(47093); return which_ == Int16x8; }
        inline bool isInt8x16() const   {TRACE_IT(47094); return which_ == Int8x16; }
        inline bool isUint32x4() const  {TRACE_IT(47095); return which_ == Uint32x4; }
        inline bool isUint16x8() const  {TRACE_IT(47096); return which_ == Uint16x8; }
        inline bool isUint8x16() const  {TRACE_IT(47097); return which_ == Uint8x16; }
        inline bool isSIMD()    const   {TRACE_IT(47098); return isInt32x4()  || isInt16x8()  || isInt8x16()  ||
                                                 isUint32x4() || isUint16x8() || isUint8x16() ||
                                                 isBool32x4() || isBool16x8() || isBool8x16() ||
                                                 isFloat32x4() || isFloat64x2() ; }
        bool operator==( AsmJsVarType rhs ) const;
        bool operator!=( AsmJsVarType rhs ) const;
    };

    // Implements <: (subtype) operator when the RHS is a VarType
    static inline bool
        operator<=( AsmJsType lhs, AsmJsVarType rhs )
    {TRACE_IT(47099);
        switch( rhs.which() )
        {
        case AsmJsVarType::Int:    return lhs.isInt();
        case AsmJsVarType::Double: return lhs.isDouble();
        case AsmJsVarType::Float:  return lhs.isFloat();
        }
        AssertMsg( false, "Unexpected RHS type" );
    }

    // Base class for all the symbol in Asm.Js during compilation
    // Defined by a type and a name
    class AsmJsSymbol
    {
    public:
        enum SymbolType
        {
            Variable,
            Argument,
            MathConstant,
            ConstantImport,
            ImportFunction,
            FuncPtrTable,
            ModuleFunction,
            ArrayView,
            MathBuiltinFunction,
            TypedArrayBuiltinFunction,
            /*SIMDVariable,*/
            SIMDBuiltinFunction,
            ModuleArgument,
            ClosureFunction
        };
    private:
        // name of the symbol, all symbols must have unique names
        PropertyName mName;
        // Type of the symbol, used for casting
        SymbolType   mType;
    public:
        // Constructor
        AsmJsSymbol(PropertyName name, SymbolType type) : mName(name), mType(type) {TRACE_IT(47100); }

        // Accessor for the name
        inline PropertyName GetName() const{TRACE_IT(47101);return mName;}
        // Sets the name of the symbol
        inline void SetName(PropertyName name) {TRACE_IT(47102);mName = name;}
        // Returns the type of the symbol
        inline SymbolType GetSymbolType()const {TRACE_IT(47103); return mType; }
        // Casts the symbol to a derived class, additional test done to make sure is it the right type
        template<typename T>
        T* Cast();

        // AsmJsSymbol interface
    public:
        // retrieve the type of the symbol when it is use in an expression
        virtual AsmJsType GetType() const = 0;
        // if the symbol is mutable, it can be on the LHS of an assignment operation
        virtual bool isMutable() const = 0;
    };

    // Symbol representing a module argument
    class AsmJsModuleArg : public AsmJsSymbol
    {
    public:
        enum ArgType: int8
        {
            StdLib,
            Import,
            Heap
        };
    private:
        ArgType mArgType;
    public:
        // Constructor
        AsmJsModuleArg(PropertyName name, ArgType type) : AsmJsSymbol(name, AsmJsSymbol::ModuleArgument), mArgType(type) {TRACE_IT(47104); }
        // Accessor
        inline const ArgType GetArgType()const {TRACE_IT(47105); return mArgType; }

        // AsmJsSymbol interface
    public:
        virtual AsmJsType GetType() const override;
        virtual bool isMutable() const override;
    };

    // Symbol representing a double constant from the standard library
    class AsmJsMathConst : public AsmJsSymbol
    {
        // address of the constant, lifetime of this address must be for the whole execution of the program (global var)
        const double* mVal;
    public:
        // Constructor
        AsmJsMathConst(PropertyName name, const double* val) : AsmJsSymbol(name, AsmJsSymbol::MathConstant), mVal(val) {TRACE_IT(47106); }
        // Accessor
        inline const double* GetVal()const {TRACE_IT(47107); return mVal; }

        // AsmJsSymbol interface
    public:
        virtual AsmJsType GetType() const override;
        virtual bool isMutable() const override;
    };

    // Base class defining Variables in asm.js, can be a variable of the module or a function argument
    class AsmJsVarBase : public AsmJsSymbol
    {
        // type of the variable, isDouble => double registerSpace, isInt => int registerSpace
        AsmJsVarType    mType;
        // register where the value of this variable resides
        RegSlot      mLocation;
        bool         mIsMutable;
    public:
        // Constructor
        AsmJsVarBase(PropertyName name, AsmJsSymbol::SymbolType type, bool isMutable = true) :
            AsmJsSymbol(name, type)
            , mType(AsmJsVarType::Double)
            , mLocation(Js::Constants::NoRegister)
            , mIsMutable(isMutable)
        {TRACE_IT(47108);
        }

        // Accessors
        inline Js::RegSlot GetLocation() const            {TRACE_IT(47109); return mLocation; }
        inline void SetLocation( Js::RegSlot val )        {TRACE_IT(47110); mLocation = val; }
        inline AsmJsVarType GetVarType() const            {TRACE_IT(47111); return mType; }
        inline void SetVarType( const AsmJsVarType& type ){TRACE_IT(47112); mType = type; }

        // AsmJsSymbol interface
    public:
        virtual AsmJsType GetType() const override
        {
            return GetVarType().toType();
        }
        virtual bool isMutable() const override
        {
            return mIsMutable;
        }
    };

    // Defines a Variable, a variable can be changed and has a default value used to initialize the variable.
    // Function and the module can have variables
    class AsmJsVar : public AsmJsVarBase
    {
        // register of the const value that initialize this variable, NoRegister for Args
        union
        {
            double doubleVal;
            float floatVal;
            int intVal;
            AsmJsSIMDValue simdVal;
        }mConstInitialiser;
    public:
        // Constructors
        AsmJsVar( PropertyName name, bool isMutable = true) :
            AsmJsVarBase(name, AsmJsSymbol::Variable, isMutable)
        {TRACE_IT(47113);
            mConstInitialiser.doubleVal = 0;
        }

        // Accessors
        inline void   SetConstInitialiser ( double val ){TRACE_IT(47114); mConstInitialiser.doubleVal = val; }
        inline double GetDoubleInitialiser() const      {TRACE_IT(47115); return mConstInitialiser.doubleVal; }
        inline void   SetConstInitialiser(float val)   {TRACE_IT(47116); mConstInitialiser.floatVal = val; }
        inline float    GetFloatInitialiser() const      {TRACE_IT(47117); return mConstInitialiser.floatVal; }
        inline void   SetConstInitialiser ( int val )   {TRACE_IT(47118); mConstInitialiser.intVal = val; }
        inline int    GetIntInitialiser   () const      {TRACE_IT(47119); return mConstInitialiser.intVal; }

        inline void SetConstInitialiser(AsmJsSIMDValue val) {TRACE_IT(47120); mConstInitialiser.simdVal = val; }
        inline AsmJsSIMDValue GetSimdConstInitialiser()      {TRACE_IT(47121); return mConstInitialiser.simdVal; }
    };

    // AsmJsArgument defines the arguments of a function
    class AsmJsArgument : public AsmJsVarBase
    {
    public:
        // Constructor
        AsmJsArgument( PropertyName name ) :
            AsmJsVarBase( name, AsmJsSymbol::Argument )
        {TRACE_IT(47122);
        }
    };

    // AsmJsConstantImport defines a variable that is initialized by an import from the foreign object
    class AsmJsConstantImport : public AsmJsVarBase
    {
        // name of the field used to initialize the variable, i.e.: var i1 = foreign.mField;
        PropertyName mField;

    public:
        // Constructor
        AsmJsConstantImport( PropertyName name, PropertyName field ) :
            AsmJsVarBase( name, AsmJsSymbol::ConstantImport ),
            mField( field )
        {TRACE_IT(47123);
        }

        // Accessor
        inline Js::PropertyName GetField() const {TRACE_IT(47124); return mField; }
    };

    template <typename T>
    struct AsmJsComparer : public DefaultComparer<T> {};

    template <>
    struct AsmJsComparer<float>
    {
        inline static bool Equals(float x, float y)
        {TRACE_IT(47125);
            int32 i32x = *(int32*)&x;
            int32 i32y = *(int32*)&y;
            return i32x == i32y;
        }

        inline static hash_t GetHashCode(float i)
        {TRACE_IT(47126);
            return (hash_t)i;
        }
    };

    template <>
    struct AsmJsComparer<double>
    {
        inline static bool Equals(double x, double y)
        {TRACE_IT(47127);
            int64 i64x = *(int64*)&x;
            int64 i64y = *(int64*)&y;
            return i64x == i64y;
        }

        inline static hash_t GetHashCode(double d)
        {TRACE_IT(47128);
            __int64 i64 = *(__int64*)&d;
            return (uint)((i64 >> 32) ^ (uint)i64);
        }
    };

    // Register space use by the function, include a map to quickly find the location assigned to constants
    template<typename T>
    class AsmJsRegisterSpace : public WAsmJs::RegisterSpace
    {
        typedef JsUtil::BaseDictionary<T, RegSlot, ArenaAllocator, PowerOf2SizePolicy, AsmJsComparer> ConstMap;
        // Map for constant and their location
        ConstMap mConstMap;
    public:
        // Constructor
        AsmJsRegisterSpace( ArenaAllocator* allocator ) :
            // reserves 1 location for return
            WAsmJs::RegisterSpace(Js::FunctionBody::FirstRegSlot),
            mConstMap( allocator )
        {TRACE_IT(47129);
        }

        inline void AddConst( T val )
        {TRACE_IT(47130);
            if( !mConstMap.ContainsKey( val ) )
            {TRACE_IT(47131);
                mConstMap.Add( val, this->AcquireConstRegister() );
            }
        }

        inline RegSlot GetConstRegister( T val ) const
        {TRACE_IT(47132);
            return mConstMap.LookupWithKey( val, Constants::NoRegister );
        }
        inline const ConstMap GetConstMap()
        {TRACE_IT(47133);
            return mConstMap;
        }
    };

    class AsmJsFunctionDeclaration : public AsmJsSymbol
    {
        AsmJsRetType    mReturnType;
        ArgSlot         mArgCount;
        RegSlot         mLocation;
        AsmJsType*      mArgumentsType;
        bool            mReturnTypeKnown : 1;
    protected:
        ArenaAllocator* mAllocator;
    public:
        AsmJsFunctionDeclaration( PropertyName name, AsmJsSymbol::SymbolType type,  ArenaAllocator* allocator):
            AsmJsSymbol( name, type )
            , mAllocator(allocator)
            , mReturnType( AsmJsRetType::Void )
            , mArgCount(Constants::InvalidArgSlot)
            , mLocation( 0 )
            , mReturnTypeKnown( false )
            , mArgumentsType(nullptr)
        {TRACE_IT(47134); }
        // returns false if the current return type is known and different
        virtual bool CheckAndSetReturnType( Js::AsmJsRetType val );
        inline Js::AsmJsRetType GetReturnType() const{TRACE_IT(47135);return mReturnType;}
        bool EnsureArgCount(ArgSlot count);
        void SetArgCount(ArgSlot count );

        ArgSlot GetArgCount() const
        {TRACE_IT(47136);
            return mArgCount;
        }
        AsmJsType* GetArgTypeArray();

        const AsmJsType& GetArgType( ArgSlot index ) const
        {TRACE_IT(47137);
            Assert( mArgumentsType && index < GetArgCount() );
            return mArgumentsType[index];
        }
        void SetArgType(const AsmJsType& arg, ArgSlot index)
        {TRACE_IT(47138);
            Assert( index < GetArgCount() ); mArgumentsType[index] = arg;
        }
        void SetArgType(AsmJsVarBase* arg, ArgSlot index)
        {TRACE_IT(47139);
            Assert( mArgumentsType != nullptr && index < GetArgCount() );
            SetArgType( arg->GetType(), index );
        }
        bool EnsureArgType(AsmJsVarBase* arg, ArgSlot index);
        inline Js::RegSlot GetFunctionIndex() const{TRACE_IT(47140);return mLocation;}
        inline void SetFunctionIndex( Js::RegSlot val ){TRACE_IT(47141);mLocation = val;}

        // argCount : number of arguments to check
        // args : dynamic array with the argument type
        // retType : returnType associated with this function signature
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType);
        // Return the size in bytes of the arguments, inArgCount is the number of argument in the call ( can be different than mArgCount for FFI )
        ArgSlot GetArgByteSize(ArgSlot inArgCount) const;

        //AsmJsSymbol interface
        virtual AsmJsType GetType() const;
        virtual bool isMutable() const;
    };


    class AsmJsMathFunction : public AsmJsFunctionDeclaration
    {
        AsmJSMathBuiltinFunction mBuiltIn;
        // chain list of supported signature (t1,t2,...) -> retType
        // careful not to create a cycle in the chain
        AsmJsMathFunction* mOverload;
        OpCodeAsmJs mOpCode;
    public:
        AsmJsMathFunction(PropertyName name, ArenaAllocator* allocator, ArgSlot argCount, AsmJSMathBuiltinFunction builtIn, OpCodeAsmJs op, AsmJsRetType retType, ...);

        void SetOverload( AsmJsMathFunction* val );
        AsmJSMathBuiltinFunction GetMathBuiltInFunction(){TRACE_IT(47142); return mBuiltIn; };
        virtual bool CheckAndSetReturnType( Js::AsmJsRetType val ) override;
        bool SupportsMathCall(ArgSlot argCount, AsmJsType* args, OpCodeAsmJs& op, AsmJsRetType& retType);
    private:
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType ) override;

    };

    class AsmJsTypedArrayFunction : public AsmJsFunctionDeclaration
    {
        AsmJSTypedArrayBuiltinFunction mBuiltIn;
        ArrayBufferView::ViewType mType;
    public:
        AsmJsTypedArrayFunction(PropertyName name, ArenaAllocator* allocator, AsmJSTypedArrayBuiltinFunction builtIn, ArrayBufferView::ViewType type) :
            AsmJsFunctionDeclaration(name, AsmJsSymbol::TypedArrayBuiltinFunction, allocator), mBuiltIn(builtIn), mType(type) {TRACE_IT(47143); }

        AsmJSTypedArrayBuiltinFunction GetArrayBuiltInFunction(){TRACE_IT(47144); return mBuiltIn; };
        ArrayBufferView::ViewType GetViewType(){TRACE_IT(47145); return mType; };

    };

    class AsmJsImportFunction : public AsmJsFunctionDeclaration
    {
        PropertyName mField;
    public:
        AsmJsImportFunction( PropertyName name, PropertyName field, ArenaAllocator* allocator );

        inline Js::PropertyName GetField() const
        {TRACE_IT(47146);
            return mField;
        }

        // We cannot know the return type of an Import Function so always think its return type is correct
        virtual bool CheckAndSetReturnType( Js::AsmJsRetType val ) override{return true;}
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType ) override;
    };

    class AsmJsFunctionTable : public AsmJsFunctionDeclaration
    {
        typedef JsUtil::List<RegSlot, ArenaAllocator> FuncIndexTable;
        FuncIndexTable  mTable;
        unsigned int    mSize;
        bool            mIsDefined : 1;
        bool            mAreArgumentsKnown : 1;
    public:
        AsmJsFunctionTable( PropertyName name, ArenaAllocator* allocator ) :
            AsmJsFunctionDeclaration( name, AsmJsSymbol::FuncPtrTable, allocator )
            , mTable(allocator)
            , mSize( 0 )
            , mIsDefined( false )
            , mAreArgumentsKnown( false )
        {TRACE_IT(47147);

        }

        inline bool IsDefined() const{TRACE_IT(47148);return mIsDefined;}
        inline void Define(){TRACE_IT(47149);mIsDefined = true;}
        inline uint GetSize() const{TRACE_IT(47150);return mSize;}
        inline void SetSize( unsigned int val )
        {TRACE_IT(47151);
            mSize = val;
            mTable.EnsureArray( mSize );
        }
        inline void SetModuleFunctionIndex( RegSlot funcIndex, unsigned int index )
        {TRACE_IT(47152);
            Assert( index < mSize );
            mTable.SetItem( index, funcIndex );
        }
        inline RegSlot GetModuleFunctionIndex( unsigned int index )
        {TRACE_IT(47153);
            Assert( index < mSize );
            return mTable.Item( index );
        }
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType );

    };

    class AsmJsFunc : public AsmJsFunctionDeclaration
    {
        typedef JsUtil::BaseDictionary<PropertyId, AsmJsVarBase*, ArenaAllocator> VarNameMap;

        unsigned        mCompileTime; //unused
        VarNameMap      mVarMap;
        ParseNode*      mBodyNode;
        ParseNode*      mFncNode;
        WAsmJs::TypedRegisterAllocator mTypedRegisterAllocator;
        typedef JsUtil::List<AsmJsVarBase*, ArenaAllocator> SIMDVarsList;
        SIMDVarsList    mSimdVarsList;

        FuncInfo*       mFuncInfo;
        FunctionBody*   mFuncBody;
        int             mArgOutDepth;
        int             mMaxArgOutDepth;
        ULONG           mOrigParseFlags;
        bool            mDeferred;
        bool            mDefined : 1; // true when compiled completely without any errors
    public:
        AsmJsFunc( PropertyName name, ParseNode* pnodeFnc, ArenaAllocator* allocator, ScriptContext* scriptContext );

        unsigned GetCompileTime() const {TRACE_IT(47154); return mCompileTime; }
        void AccumulateCompileTime(unsigned ms) {TRACE_IT(47155); mCompileTime += ms; }

        inline ParseNode* GetFncNode() const{TRACE_IT(47156); return mFncNode; }
        inline void       SetFncNode(ParseNode* fncNode) {TRACE_IT(47157); mFncNode = fncNode; }
        inline FuncInfo*  GetFuncInfo() const{TRACE_IT(47158); return mFuncInfo; }
        inline void       SetFuncInfo(FuncInfo* fncInfo) {TRACE_IT(47159); mFuncInfo = fncInfo; }
        inline FunctionBody*GetFuncBody() const{TRACE_IT(47160); return mFuncBody; }
        inline void       SetFuncBody(FunctionBody* fncBody) {TRACE_IT(47161); mFuncBody = fncBody; }
        inline ULONG      GetOrigParseFlags() const{TRACE_IT(47162); return mOrigParseFlags; }
        inline void       SetOrigParseFlags(ULONG parseFlags) {TRACE_IT(47163); mOrigParseFlags = parseFlags; }

        inline ParseNode* GetBodyNode() const{TRACE_IT(47164);return mBodyNode;}
        inline void SetBodyNode( ParseNode* val ){TRACE_IT(47165);mBodyNode = val;}
        inline void Finish() {TRACE_IT(47166); mDefined = true; }
        inline bool IsDefined()const {TRACE_IT(47167); return mDefined; }
        inline void SetDeferred() {TRACE_IT(47168); mDeferred = true; }
        inline bool IsDeferred()const {TRACE_IT(47169); return mDeferred; }
        template<typename T> inline AsmJsRegisterSpace<T>& GetRegisterSpace() {TRACE_IT(47170);
            return *(AsmJsRegisterSpace<T>*)mTypedRegisterAllocator.GetRegisterSpace(WAsmJs::RegisterSpace::GetRegisterSpaceType<T>());
        }
        const WAsmJs::TypedRegisterAllocator& GetTypedRegisterAllocator() const {TRACE_IT(47171); return mTypedRegisterAllocator; }
        inline SIMDVarsList& GetSimdVarsList()    {TRACE_IT(47172); return mSimdVarsList;  }

        /// Wrapper for RegisterSpace methods
        template<typename T> inline RegSlot AcquireRegister   (){TRACE_IT(47173);return GetRegisterSpace<T>().AcquireRegister();}
        template<typename T> inline void AddConst             ( T val ){TRACE_IT(47174);GetRegisterSpace<T>().AddConst( val );}
        template<typename T> inline RegSlot GetConstRegister  ( T val ){TRACE_IT(47175);return GetRegisterSpace<T>().GetConstRegister( val );}
        template<typename T> inline RegSlot AcquireTmpRegister(){TRACE_IT(47176);return GetRegisterSpace<T>().AcquireTmpRegister();}
        template<typename T> inline void ReleaseTmpRegister   ( Js::RegSlot tmpReg ){TRACE_IT(47177);GetRegisterSpace<T>().ReleaseTmpRegister( tmpReg );}
        template<typename T> inline void ReleaseLocation      ( const EmitExpressionInfo* pnode ){TRACE_IT(47178);GetRegisterSpace<T>().ReleaseLocation( pnode );}
        template<typename T> inline bool IsTmpLocation        ( const EmitExpressionInfo* pnode ){TRACE_IT(47179);return GetRegisterSpace<T>().IsTmpLocation( pnode );}
        template<typename T> inline bool IsConstLocation      ( const EmitExpressionInfo* pnode ){TRACE_IT(47180);return GetRegisterSpace<T>().IsConstLocation( pnode );}
        template<typename T> inline bool IsVarLocation        ( const EmitExpressionInfo* pnode ){TRACE_IT(47181);return GetRegisterSpace<T>().IsVarLocation( pnode );}
        template<typename T> inline bool IsValidLocation      ( const EmitExpressionInfo* pnode ){TRACE_IT(47182);return GetRegisterSpace<T>().IsValidLocation( pnode );}
        void ReleaseLocationGeneric( const EmitExpressionInfo* pnode );

        // Search for a var in the varMap of the function, return nullptr if not found
        AsmJsVarBase* FindVar( const PropertyName name ) const;
        // Defines a new variable int the function, return nullptr if already exists or theres an error
        AsmJsVarBase* DefineVar(PropertyName name, bool isArg = false, bool isMutable = true);
        AsmJsSymbol* LookupIdentifier( const PropertyName name, AsmJsLookupSource::Source* lookupSource = nullptr ) const;
        void SetArgOutDepth(int outParamsCount);
        void UpdateMaxArgOutDepth(int outParamsCount);
        inline int GetArgOutDepth() const{TRACE_IT(47183); return mArgOutDepth; }
        inline int GetMaxArgOutDepth() const{TRACE_IT(47184); return mMaxArgOutDepth; }
        void CommitToFunctionInfo(Js::AsmJsFunctionInfo* funcInfo, FunctionBody* body) {TRACE_IT(47185);mTypedRegisterAllocator.CommitToFunctionInfo(funcInfo, body);}
        void CommitToFunctionBody(FunctionBody* body) {TRACE_IT(47186); mTypedRegisterAllocator.CommitToFunctionBody(body); }
    };

    struct MathBuiltin
    {
        enum Kind
        {
            Function, Constant
        };
        Kind kind;
        AsmJSMathBuiltinFunction mathLibFunctionName;
        union
        {
            const double* cst;
            AsmJsMathFunction* func;
        } u;

        MathBuiltin() : kind( Kind( -1 ) )
        {TRACE_IT(47187);
        }
        MathBuiltin(AsmJSMathBuiltinFunction mathLibFunctionName, const double* cst) : kind(Constant), mathLibFunctionName(mathLibFunctionName)
        {TRACE_IT(47188);
            u.cst = cst;
        }
        MathBuiltin(AsmJSMathBuiltinFunction mathLibFunctionName, AsmJsMathFunction* func) : kind(Function), mathLibFunctionName(mathLibFunctionName)
        {TRACE_IT(47189);
            u.func = func;
        }
    };

    struct TypedArrayBuiltin
    {
        AsmJSTypedArrayBuiltinFunction mArrayLibFunctionName;
        AsmJsTypedArrayFunction* mFunc;

        TypedArrayBuiltin() {TRACE_IT(47190); }
        TypedArrayBuiltin(AsmJSTypedArrayBuiltinFunction arrayLibFunctionName, AsmJsTypedArrayFunction* func) :
            mArrayLibFunctionName(arrayLibFunctionName),
            mFunc(func)
        {TRACE_IT(47191); }
    };

    class AsmJsArrayView : public AsmJsSymbol
    {
        ArrayBufferView::ViewType mViewType;

    public:
        AsmJsArrayView( PropertyName name, ArrayBufferView::ViewType viewType ) :
            AsmJsSymbol( name, AsmJsSymbol::ArrayView )
            , mViewType( viewType )
        {TRACE_IT(47192);

        }

        virtual AsmJsType GetType() const;
        virtual bool isMutable() const;
        inline ArrayBufferView::ViewType GetViewType() const
        {TRACE_IT(47193);
            return mViewType;
        }
    };

    class AsmJsFunctionInfo
    {
        Field(WAsmJs::TypedSlotInfo) mTypedSlotInfos[WAsmJs::LIMIT];
        Field(ArgSlot) mArgCount;
        Field(AsmJsVarType::Which *) mArgType;
        Field(ArgSlot) mArgSizesLength;
        Field(uint *) mArgSizes;
        Field(ArgSlot) mArgByteSize;
        Field(AsmJsRetType) mReturnType;
#ifdef ENABLE_WASM
        Field(Wasm::WasmSignature *) mSignature;
        Field(Wasm::WasmReaderInfo*) mWasmReaderInfo;
        Field(WebAssemblyModule*) mWasmModule;
#endif
        Field(bool) mIsHeapBufferConst;
        Field(bool) mUsesHeapBuffer;

        Field(FunctionBody*) asmJsModuleFunctionBody;
        Field(Js::JavascriptError *) mLazyError;

    public:
        AsmJsFunctionInfo() : mArgCount(0),
                              mArgSizesLength(0),
                              mReturnType(AsmJsRetType::Void),
                              mArgByteSize(0),
                              asmJsModuleFunctionBody(nullptr),
                              mTJBeginAddress(nullptr),
#ifdef ENABLE_WASM
                              mWasmReaderInfo(nullptr),
                              mSignature(nullptr),
                              mWasmModule(nullptr),
#endif
                              mUsesHeapBuffer(false),
                              mIsHeapBufferConst(false),
                              mArgType(nullptr),
                              mArgSizes(nullptr) {TRACE_IT(47194);}
        // the key is the bytecode address
        typedef JsUtil::BaseDictionary<int, ptrdiff_t, Recycler> ByteCodeToTJMap;
        Field(ByteCodeToTJMap*) mbyteCodeTJMap;
        Field(BYTE*) mTJBeginAddress;
        WAsmJs::TypedSlotInfo* GetTypedSlotInfo(WAsmJs::Types type);

#define TYPED_SLOT_INFO_GETTER(name, type) \
        int Get##name##ByteOffset() const   {TRACE_IT(47195); return mTypedSlotInfos[WAsmJs::##type].byteOffset; }\
        int Get##name##ConstCount() const   {TRACE_IT(47196); return mTypedSlotInfos[WAsmJs::##type].constCount; }\
        int Get##name##TmpCount() const     {TRACE_IT(47197); return mTypedSlotInfos[WAsmJs::##type].tmpCount; }\
        int Get##name##VarCount() const     {TRACE_IT(47198); return mTypedSlotInfos[WAsmJs::##type].varCount; }

        TYPED_SLOT_INFO_GETTER(Double, FLOAT64);
        TYPED_SLOT_INFO_GETTER(Float, FLOAT32);
        TYPED_SLOT_INFO_GETTER(Int, INT32);
        TYPED_SLOT_INFO_GETTER(Int64, INT64);
        TYPED_SLOT_INFO_GETTER(Simd, SIMD);
#undef TYPED_SLOT_INFO_GETTER

        inline ArgSlot GetArgCount() const{TRACE_IT(47199); return mArgCount; }
        inline void SetArgCount(ArgSlot val) {TRACE_IT(47200); mArgCount = val; }
        inline AsmJsRetType GetReturnType() const{TRACE_IT(47201);return mReturnType;}
        inline void SetReturnType(AsmJsRetType val) {TRACE_IT(47202); mReturnType = val; }
        inline ArgSlot GetArgByteSize() const{TRACE_IT(47203);return mArgByteSize;}
        inline void SetArgByteSize(ArgSlot val) {TRACE_IT(47204); mArgByteSize = val; }

        inline void SetIsHeapBufferConst(bool val) {TRACE_IT(47205); mIsHeapBufferConst = val; }
        inline bool IsHeapBufferConst() const{TRACE_IT(47206); return mIsHeapBufferConst; }

        inline void SetUsesHeapBuffer(bool val) {TRACE_IT(47207); mUsesHeapBuffer = val; }
        inline bool UsesHeapBuffer() const{TRACE_IT(47208); return mUsesHeapBuffer; }

        inline int GetSimdAllCount() const {TRACE_IT(47209); return GetSimdConstCount() + GetSimdVarCount() + GetSimdTmpCount(); }

        Js::JavascriptError * GetLazyError() const {TRACE_IT(47210); return mLazyError; }
        void SetLazyError(Js::JavascriptError * val) {TRACE_IT(47211); mLazyError = val; }

        int GetTotalSizeinBytes()const;
        void SetArgType(AsmJsVarType type, ArgSlot index);
        inline AsmJsVarType GetArgType(ArgSlot index ) const
        {TRACE_IT(47212);
            Assert(mArgCount != Constants::InvalidArgSlot);
            AnalysisAssert( index < mArgCount);
            return mArgType[index];
        }
        bool Init( AsmJsFunc* func );
        void SetModuleFunctionBody(FunctionBody* body){TRACE_IT(47213); asmJsModuleFunctionBody = body; };
        FunctionBody* GetModuleFunctionBody()const{TRACE_IT(47214); return asmJsModuleFunctionBody; };

        ArgSlot GetArgSizeArrayLength()
        {TRACE_IT(47215);
            return mArgSizesLength;
        }
        void SetArgSizeArrayLength(ArgSlot val)
        {TRACE_IT(47216);
            mArgSizesLength = val;
        }

        uint* GetArgsSizesArray()
        {TRACE_IT(47217);
            return mArgSizes;
        }
        void SetArgsSizesArray(uint* val)
        {TRACE_IT(47218);
            mArgSizes = val;
        }
        AsmJsVarType::Which * GetArgTypeArray()
        {TRACE_IT(47219);
            return mArgType;
        }
        void SetArgTypeArray(AsmJsVarType::Which* val)
        {TRACE_IT(47220);
            mArgType = val;
        }
#ifdef ENABLE_WASM
        Wasm::WasmSignature * GetWasmSignature()
        {TRACE_IT(47221);
            return mSignature;
        }
        void SetWasmSignature(Wasm::WasmSignature * sig)
        {TRACE_IT(47222);
            mSignature = sig;
        }

        Wasm::WasmReaderInfo* GetWasmReaderInfo() const {TRACE_IT(47223);return mWasmReaderInfo;}
        void SetWasmReaderInfo(Wasm::WasmReaderInfo* reader) {TRACE_IT(47224);mWasmReaderInfo = reader;}
        WebAssemblyModule* GetWebAssemblyModule() const {TRACE_IT(47225); return mWasmModule; }
        void SetWebAssemblyModule(WebAssemblyModule * module) {TRACE_IT(47226); mWasmModule= module; }
        bool IsWasmDeferredParse() const {TRACE_IT(47227); return mWasmReaderInfo != nullptr; }
#endif
    };

    // The asm.js spec recognizes this set of builtin SIMD functions.
    // !! Note: keep these grouped by SIMD type
    enum AsmJsSIMDBuiltinFunction
    {
#define ASMJS_SIMD_NAMES(name, propertyName, libName, entryPoint) AsmJsSIMDBuiltin_##name,
#define ASMJS_SIMD_MARKERS(name) AsmJsSIMDBuiltin_##name,
#include "AsmJsBuiltInNames.h"
        AsmJsSIMDBuiltin_COUNT
    };

    // SIMD built-in function symbol
    // Do we have overloads for any SIMD function ?
    class AsmJsSIMDFunction : public AsmJsFunctionDeclaration
    {
        AsmJsSIMDBuiltinFunction mBuiltIn;
        AsmJsSIMDFunction* mOverload;
        OpCodeAsmJs mOpCode;
    public:
        AsmJsSIMDFunction(PropertyName name, ArenaAllocator* allocator, ArgSlot argCount, AsmJsSIMDBuiltinFunction builtIn, OpCodeAsmJs op, AsmJsRetType retType, ...);

        PropertyId GetBuiltinPropertyId();
        void SetOverload(AsmJsSIMDFunction* val);
        AsmJsSIMDBuiltinFunction GetSimdBuiltInFunction(){TRACE_IT(47228); return mBuiltIn; };
        virtual bool CheckAndSetReturnType(Js::AsmJsRetType val) override;

        bool SupportsSIMDCall(ArgSlot argCount, AsmJsType* args, OpCodeAsmJs& op, AsmJsRetType& retType);

        bool IsConstructor();
        bool IsConstructor(uint argCount);
        bool IsTypeCheck();  // e.g. float32x4(x)
        bool IsUnsignedTypeCheck();
        bool IsInt32x4Func()  {TRACE_IT(47229); return mBuiltIn >  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Int32x4_Start   && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Int32x4_End;   }
        bool IsBool32x4Func() {TRACE_IT(47230); return mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Bool32x4_Start  && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Bool32x4_End;  }
        bool IsBool16x8Func() {TRACE_IT(47231); return mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Bool16x8_Start  && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Bool16x8_End;  }
        bool IsBool8x16Func() {TRACE_IT(47232); return mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Bool8x16_Start  && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Bool8x16_End; }
        bool IsFloat32x4Func(){TRACE_IT(47233); return mBuiltIn >  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Float32x4_Start && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Float32x4_End; }
        bool IsFloat64x2Func(){TRACE_IT(47234); return mBuiltIn >  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Float64x2_Start && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Float64x2_End; }

        bool IsInt16x8Func()  {TRACE_IT(47235); return mBuiltIn >  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Int16x8_Start   && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Int16x8_End;   }
        bool IsInt8x16Func() {TRACE_IT(47236); return mBuiltIn > AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Int8x16_Start && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Int8x16_End; }
        bool IsUint32x4Func() {TRACE_IT(47237); return mBuiltIn >  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Uint32x4_Start  && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Uint32x4_End;  }
        bool IsUint16x8Func() {TRACE_IT(47238); return mBuiltIn >  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Uint16x8_Start  && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Uint16x8_End;  }
        bool IsUint8x16Func() {TRACE_IT(47239); return mBuiltIn >  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Uint8x16_Start  && mBuiltIn < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_Uint8x16_End;  }

        bool IsSimdLoadFunc()
        {TRACE_IT(47240);
            return (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_load && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_load3) ||
                (mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int16x8_load) ||
                (mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int8x16_load) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint32x4_load && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint32x4_load3) ||
                (mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint16x8_load) ||
                (mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint8x16_load) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_load && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_load3) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_load && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_load1);
        }
        bool IsSimdStoreFunc()
        {TRACE_IT(47241);
            return (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_store && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_store3) ||
                (mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int16x8_store) ||
                (mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int8x16_store) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint32x4_store && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint32x4_store3) ||
                (mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint16x8_store) ||
                (mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint8x16_store) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_store && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_store3) ||
                (mBuiltIn >= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_store && mBuiltIn <= AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_store1);
        }
        bool IsExtractLaneFunc()
        {TRACE_IT(47242);
            return (
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int16x8_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int8x16_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint32x4_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint16x8_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint8x16_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_bool32x4_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_bool16x8_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_bool8x16_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_extractLane
                );
        }
        bool IsReplaceLaneFunc()
        {TRACE_IT(47243);
            return (
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int16x8_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int8x16_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint32x4_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint16x8_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint8x16_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_bool32x4_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_bool16x8_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_bool8x16_replaceLane ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_replaceLane
                );
        }
        bool IsLaneAccessFunc()
        {TRACE_IT(47244);
            return (
                IsExtractLaneFunc() || IsReplaceLaneFunc()
                );
        }

        uint32 LanesCount()
        {TRACE_IT(47245);
            if (IsInt32x4Func() || IsFloat32x4Func() || IsUint32x4Func() || IsBool32x4Func())
            {TRACE_IT(47246);
                return 4;
            }
            if (IsInt16x8Func() || IsUint16x8Func() || IsBool16x8Func())
            {TRACE_IT(47247);
                return 8;
            }
            if (IsUint8x16Func() || IsInt8x16Func() || IsBool8x16Func())
            {TRACE_IT(47248);
                return 16;
            }
            if (IsFloat64x2Func())
            {TRACE_IT(47249);
                return 2;
            }
            Assert(UNREACHED);
            return 0;
        }

       bool IsShuffleFunc()
       {TRACE_IT(47250);

           return (
               mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_shuffle ||
               mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int16x8_shuffle ||
               mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int8x16_shuffle ||
               mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint32x4_shuffle ||
               mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint16x8_shuffle ||
               mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint8x16_shuffle ||
               mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_shuffle ||
               mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_shuffle
               );
        }

        bool IsSwizzleFunc()
        {TRACE_IT(47251);
            return  (
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int32x4_swizzle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int16x8_swizzle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_int8x16_swizzle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint32x4_swizzle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint16x8_swizzle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_uint8x16_swizzle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float32x4_swizzle ||
                mBuiltIn == AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_float64x2_swizzle
                );
        }

        bool ReturnsBool()
        {TRACE_IT(47252);
            return (
                mBuiltIn == AsmJsSIMDBuiltin_bool32x4_allTrue || mBuiltIn == AsmJsSIMDBuiltin_bool32x4_anyTrue ||
                mBuiltIn == AsmJsSIMDBuiltin_bool16x8_allTrue || mBuiltIn == AsmJsSIMDBuiltin_bool16x8_anyTrue ||
                mBuiltIn == AsmJsSIMDBuiltin_bool8x16_allTrue || mBuiltIn == AsmJsSIMDBuiltin_bool8x16_anyTrue ||
                mBuiltIn == AsmJsSIMDBuiltin_bool32x4_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltin_bool16x8_extractLane ||
                mBuiltIn == AsmJsSIMDBuiltin_bool8x16_extractLane );
        }

        AsmJsVarType GetTypeCheckVarType();
        AsmJsVarType GetConstructorVarType();
        OpCodeAsmJs GetOpcode() {TRACE_IT(47253); return mOpCode;  }

    private:
        virtual bool SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType) override;
    };
};
#endif
