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

#include "RuntimeLanguagePch.h"

#ifdef ASMJS_PLAT
#include "ByteCode/ByteCodeWriter.h"
#include "ByteCode/AsmJsByteCodeWriter.h"
#include "Language/AsmJsByteCodeGenerator.h"

namespace Js
{
    const char16 * AsmJsType::toChars() const
    {TRACE_IT(46902);
        switch (which_)
        {
        case Double:      return _u("double");
        case MaybeDouble: return _u("double?");
        case DoubleLit:   return _u("doublelit");
        case Float:       return _u("float");
        case Floatish:    return _u("floatish");
        case FloatishDoubleLit: return _u("FloatishDoubleLit");
        case MaybeFloat:  return _u("float?");
        case Fixnum:      return _u("fixnum");
        case Int:         return _u("int");
        case Signed:      return _u("signed");
        case Unsigned:    return _u("unsigned");
        case Intish:      return _u("intish");
        case Void:        return _u("void");
        case Int32x4:     return _u("SIMD.Int32x4");
        case Bool32x4:    return _u("SIMD.Bool32x4");
        case Bool16x8:    return _u("SIMD.Bool16x8");
        case Bool8x16:    return _u("SIMD.Bool8x16");
        case Float32x4:   return _u("SIMD.Float32x4");
        case Float64x2:   return _u("SIMD.Float64x2");
        case Int16x8:     return _u("SIMD.Int16x8");
        case Int8x16:     return _u("SIMD.Int8x16");
        case Uint32x4:    return _u("SIMD.Uint32x4");
        case Uint16x8:    return _u("SIMD.Uint16x8");
        case Uint8x16:    return _u("SIMD.Uint8x16");
        }
        Assert(false);
        return _u("none");
    }

    bool AsmJsType::isSIMDType() const
    {TRACE_IT(46903);
        return isSIMDInt32x4()  || isSIMDInt16x8()   || isSIMDInt8x16()   ||
               isSIMDBool32x4() || isSIMDBool16x8()  || isSIMDBool8x16()  ||
               isSIMDUint32x4() || isSIMDUint16x8()  || isSIMDUint8x16()  ||
               isSIMDFloat32x4()|| isSIMDFloat64x2();
    }

    bool AsmJsType::isSIMDInt32x4() const
    {TRACE_IT(46904);
        return which_ == Int32x4;
    }
    bool AsmJsType::isSIMDBool32x4() const
    {TRACE_IT(46905);
        return which_ == Bool32x4;
    }
    bool AsmJsType::isSIMDBool16x8() const
    {TRACE_IT(46906);
        return which_ == Bool16x8;
    }
    bool AsmJsType::isSIMDBool8x16() const
    {TRACE_IT(46907);
        return which_ == Bool8x16;
    }
    bool AsmJsType::isSIMDInt16x8() const
    {TRACE_IT(46908);
        return which_ == Int16x8;
    }
    bool AsmJsType::isSIMDInt8x16() const
    {TRACE_IT(46909);
        return which_ == Int8x16;
    }
    bool AsmJsType::isSIMDFloat32x4() const
    {TRACE_IT(46910);
        return which_ == Float32x4;
    }
    bool AsmJsType::isSIMDFloat64x2() const
    {TRACE_IT(46911);
        return which_ == Float64x2;
    }

    bool AsmJsType::isSIMDUint32x4() const
    {TRACE_IT(46912);
        return which_ == Uint32x4;
    }
    bool AsmJsType::isSIMDUint16x8() const
    {TRACE_IT(46913);
        return which_ == Uint16x8;
    }
    bool AsmJsType::isSIMDUint8x16() const
    {TRACE_IT(46914);
        return which_ == Uint8x16;
    }

    bool AsmJsType::isVarAsmJsType() const
    {TRACE_IT(46915);
        return isInt() || isMaybeDouble() || isMaybeFloat();
    }

    bool AsmJsType::isExtern() const
    {TRACE_IT(46916);
        return isDouble() || isSigned();
    }

    bool AsmJsType::isVoid() const
    {TRACE_IT(46917);
        return which_ == Void;
    }

    bool AsmJsType::isFloatish() const
    {TRACE_IT(46918);
        return isMaybeFloat() || which_ == Floatish;
    }

    bool AsmJsType::isFloatishDoubleLit() const
    {TRACE_IT(46919);
        return isFloatish() || isDoubleLit();
    }

    bool AsmJsType::isMaybeFloat() const
    {TRACE_IT(46920);
        return isFloat() || which_ == MaybeFloat;
    }

    bool AsmJsType::isFloat() const
    {TRACE_IT(46921);
        return which_ == Float;
    }

    bool AsmJsType::isMaybeDouble() const
    {TRACE_IT(46922);
        return isDouble() || which_ == MaybeDouble;
    }

    bool AsmJsType::isDouble() const
    {TRACE_IT(46923);
        return isDoubleLit() || which_ == Double;
    }

    bool AsmJsType::isDoubleLit() const
    {TRACE_IT(46924);
        return which_ == DoubleLit;
    }

    bool AsmJsType::isIntish() const
    {TRACE_IT(46925);
        return isInt() || which_ == Intish;
    }

    bool AsmJsType::isInt() const
    {TRACE_IT(46926);
        return isSigned() || isUnsigned() || which_ == Int;
    }

    bool AsmJsType::isUnsigned() const
    {TRACE_IT(46927);
        return which_ == Unsigned || which_ == Fixnum;
    }

    bool AsmJsType::isSigned() const
    {TRACE_IT(46928);
        return which_ == Signed || which_ == Fixnum;
    }

    bool AsmJsType::operator!=(AsmJsType rhs) const
    {TRACE_IT(46929);
        return which_ != rhs.which_;
    }

    bool AsmJsType::operator==(AsmJsType rhs) const
    {TRACE_IT(46930);
        return which_ == rhs.which_;
    }

    bool AsmJsType::isSubType(AsmJsType type) const
    {TRACE_IT(46931);
        switch (type.which_)
        {
        case Js::AsmJsType::Double:
            return isDouble();
            break;

        case Js::AsmJsType::MaybeDouble:
            return isMaybeDouble();
            break;
        case Js::AsmJsType::DoubleLit:
            return isDoubleLit();
            break;
        case Js::AsmJsType::Float:
            return isFloat();
            break;
        case Js::AsmJsType::MaybeFloat:
            return isMaybeFloat();
            break;
        case Js::AsmJsType::Floatish:
            return isFloatish();
            break;
        case Js::AsmJsType::FloatishDoubleLit:
            return isFloatishDoubleLit();
            break;
        case Js::AsmJsType::Fixnum:
            return which_ == Fixnum;
            break;
        case Js::AsmJsType::Int:
            return isInt();
            break;
        case Js::AsmJsType::Signed:
            return isSigned();
            break;
        case Js::AsmJsType::Unsigned:
            return isUnsigned();
            break;
        case Js::AsmJsType::Intish:
            return isIntish();
            break;
        case Js::AsmJsType::Void:
            return isVoid();
            break;
        case AsmJsType::Int32x4:
            return isSIMDInt32x4();
            break;
        case AsmJsType::Bool32x4:
            return isSIMDBool32x4();
            break;
        case AsmJsType::Bool16x8:
            return isSIMDBool16x8();
            break;
        case AsmJsType::Bool8x16:
            return isSIMDBool8x16();
            break;
        case AsmJsType::Float32x4:
            return isSIMDFloat32x4();
            break;
        case AsmJsType::Float64x2:
            return isSIMDFloat64x2();
            break;
        case AsmJsType::Int16x8:
            return isSIMDInt16x8();
            break;
        case AsmJsType::Int8x16:
            return isSIMDInt8x16();
            break;
        case AsmJsType::Uint32x4:
            return isSIMDUint32x4();
            break;
        case AsmJsType::Uint16x8:
            return isSIMDUint16x8();
            break;
        case AsmJsType::Uint8x16:
            return isSIMDUint8x16();
            break;
        default:
            break;
        }
        return false;
    }

    bool AsmJsType::isSuperType(AsmJsType type) const
    {TRACE_IT(46932);
        return type.isSubType(which_);
    }

    Js::AsmJsRetType AsmJsType::toRetType() const
    {TRACE_IT(46933);
        Which w = which_;
        // DoubleLit is for expressions only.
        if (w == DoubleLit)
        {TRACE_IT(46934);
            w = Double;
        }
        return AsmJsRetType::Which(w);
    }

    /// RetType

    bool AsmJsRetType::operator!=(AsmJsRetType rhs) const
    {TRACE_IT(46935);
        return which_ != rhs.which_;
    }

    bool AsmJsRetType::operator==(AsmJsRetType rhs) const
    {TRACE_IT(46936);
        return which_ == rhs.which_;
    }

    Js::AsmJsType AsmJsRetType::toType() const
    {TRACE_IT(46937);
        return AsmJsType::Which(which_);
    }

    Js::AsmJsVarType AsmJsRetType::toVarType() const
    {TRACE_IT(46938);
        return AsmJsVarType::Which(which_);
    }

    Js::AsmJsRetType::Which AsmJsRetType::which() const
    {TRACE_IT(46939);
        return which_;
    }

    AsmJsRetType::AsmJsRetType(AsmJSCoercion coercion)
    {TRACE_IT(46940);
        switch (coercion)
        {
        case AsmJS_ToInt32: which_   = Signed; break;
        case AsmJS_ToNumber: which_  = Double; break;
        case AsmJS_FRound: which_    = Float; break;
        case AsmJS_Int32x4: which_   = Int32x4; break;
        case AsmJS_Bool32x4: which_ = Bool32x4; break;
        case AsmJS_Bool16x8: which_ = Bool16x8; break;
        case AsmJS_Bool8x16: which_ = Bool8x16; break;
        case AsmJS_Float32x4: which_ = Float32x4; break;
        case AsmJS_Float64x2: which_ = Float64x2; break;
        case AsmJS_Int16x8: which_  = Int16x8; break;
        case AsmJS_Int8x16: which_  = Int8x16; break;
        case AsmJS_Uint32x4: which_ = Uint32x4; break;
        case AsmJS_Uint16x8: which_ = Uint16x8; break;
        case AsmJS_Uint8x16: which_ = Uint8x16; break;
        }
    }

    AsmJsRetType::AsmJsRetType(Which w) : which_(w)
    {TRACE_IT(46941);

    }

    AsmJsRetType::AsmJsRetType() : which_(Which(-1))
    {TRACE_IT(46942);

    }

    /// VarType

    bool AsmJsVarType::operator!=(AsmJsVarType rhs) const
    {TRACE_IT(46943);
        return which_ != rhs.which_;
    }

    bool AsmJsVarType::operator==(AsmJsVarType rhs) const
    {TRACE_IT(46944);
        return which_ == rhs.which_;
    }

    Js::AsmJsVarType AsmJsVarType::FromCheckedType(AsmJsType type)
    {TRACE_IT(46945);
        Assert( type.isInt() || type.isMaybeDouble() || type.isFloatish() || type.isSIMDType());

        if (type.isMaybeDouble())
            return Double;
        else if (type.isFloatish())
            return Float;
        else if (type.isInt())
            return Int;
        else
        {TRACE_IT(46946);
            // SIMD type
            return AsmJsVarType::Which(type.GetWhich());
        }

    }

    Js::AsmJSCoercion AsmJsVarType::toCoercion() const
    {TRACE_IT(46947);
        switch (which_)
        {
        case Int:     return AsmJS_ToInt32;
        case Double:  return AsmJS_ToNumber;
        case Float:   return AsmJS_FRound;
        case Int32x4:   return AsmJS_Int32x4;
        case Bool32x4:  return AsmJS_Bool32x4;
        case Bool16x8:  return AsmJS_Bool16x8;
        case Bool8x16:  return AsmJS_Bool8x16;
        case Float32x4: return AsmJS_Float32x4;
        case Float64x2: return AsmJS_Float64x2;
        case Int16x8:   return AsmJS_Int16x8;
        case Int8x16:   return AsmJS_Int8x16;
        case Uint32x4:   return AsmJS_Uint32x4;
        case Uint16x8:   return AsmJS_Uint16x8;
        case Uint8x16:   return AsmJS_Uint8x16;
        }
        Assert(false);
        return AsmJS_ToInt32;
    }

    Js::AsmJsType AsmJsVarType::toType() const
    {TRACE_IT(46948);
        return AsmJsType::Which(which_);
    }

    Js::AsmJsVarType::Which AsmJsVarType::which() const
    {TRACE_IT(46949);
        return which_;
    }

    AsmJsVarType::AsmJsVarType(AsmJSCoercion coercion)
    {TRACE_IT(46950);
        switch (coercion)
        {
        case AsmJS_ToInt32: which_ = Int; break;
        case AsmJS_ToNumber: which_ = Double; break;
        case AsmJS_FRound: which_ = Float; break;
        case AsmJS_Int32x4: which_ = Int32x4; break;
        case AsmJS_Bool32x4: which_ = Bool32x4; break;
        case AsmJS_Bool16x8: which_ = Bool16x8; break;
        case AsmJS_Bool8x16: which_ = Bool8x16; break;
        case AsmJS_Float32x4: which_ = Float32x4; break;
        case AsmJS_Float64x2: which_ = Float64x2; break;
        case AsmJS_Int16x8: which_ = Int16x8; break;
        case AsmJS_Int8x16: which_ = Int8x16; break;
        case AsmJS_Uint32x4: which_ = Uint32x4; break;
        case AsmJS_Uint16x8: which_ = Uint16x8; break;
        case AsmJS_Uint8x16: which_ = Uint8x16; break;
        }
    }

    AsmJsVarType::AsmJsVarType(Which w) : which_(w)
    {TRACE_IT(46951);

    }

    AsmJsVarType::AsmJsVarType() : which_(Which(-1))
    {TRACE_IT(46952);

    }

    template<>
    AsmJsMathConst* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46953);
        Assert(mType == MathConstant);
        return (AsmJsMathConst*)this;
    }

    template<>
    AsmJsVar* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46954);
        Assert(mType == Variable);
        return (AsmJsVar*)this;
    }

    template<>
    AsmJsVarBase* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46955);
        Assert( mType == Argument || mType == Variable || mType == ConstantImport);
        return ( AsmJsVarBase* )this;
    }

    template<>
    AsmJsFunctionDeclaration* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46956);
        Assert(mType == ModuleFunction || mType == ImportFunction || mType == MathBuiltinFunction || mType == SIMDBuiltinFunction || mType == FuncPtrTable);
        return (AsmJsFunctionDeclaration*)this;
    }

    template<>
    AsmJsFunc* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46957);
        Assert(mType == ModuleFunction);
        return (AsmJsFunc*)this;
    }

    template<>
    AsmJsImportFunction* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46958);
        Assert(mType == ImportFunction);
        return (AsmJsImportFunction*)this;
    }

    template<>
    AsmJsMathFunction* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46959);
        Assert(mType == MathBuiltinFunction);
        return (AsmJsMathFunction*)this;
    }

    template<>
    AsmJsSIMDFunction* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46960);
        Assert(mType == SIMDBuiltinFunction);
        return  (AsmJsSIMDFunction*) this;
    }
    template<>
    AsmJsArrayView* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46961);
        Assert(mType == ArrayView);
        return (AsmJsArrayView*)this;
    }

    template<>
    AsmJsConstantImport* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46962);
        Assert(mType == ConstantImport);
        return (AsmJsConstantImport*)this;
    }

    template<>
    AsmJsFunctionTable* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46963);
        Assert(mType == FuncPtrTable);
        return (AsmJsFunctionTable*)this;
    }

    template<>
    AsmJsTypedArrayFunction* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46964);
        Assert(mType == TypedArrayBuiltinFunction);
        return (AsmJsTypedArrayFunction*)this;
    }

    template<>
    AsmJsModuleArg* Js::AsmJsSymbol::Cast()
    {TRACE_IT(46965);
        Assert(mType == ModuleArgument);
        return (AsmJsModuleArg*)this;
    }

    Js::AsmJsType AsmJsModuleArg::GetType() const
    {TRACE_IT(46966);
        Assert(UNREACHED);
        return AsmJsType::Void;
    }

    bool AsmJsModuleArg::isMutable() const
    {TRACE_IT(46967);
        Assert(UNREACHED);
        return true;
    }

    Js::AsmJsType AsmJsMathConst::GetType() const
    {TRACE_IT(46968);
        return AsmJsType::Double;
    }

    bool AsmJsMathConst::isMutable() const
    {TRACE_IT(46969);
        return false;
    }

    bool AsmJsFunctionDeclaration::EnsureArgCount(ArgSlot count)
    {TRACE_IT(46970);
        if (mArgCount == Constants::InvalidArgSlot)
        {TRACE_IT(46971);
            SetArgCount(count);
            return true;
        }
        else
        {TRACE_IT(46972);
            return mArgCount == count;
        }
    }

    void AsmJsFunctionDeclaration::SetArgCount(ArgSlot count )
    {TRACE_IT(46973);
        Assert( mArgumentsType == nullptr );
        Assert(mArgCount == Constants::InvalidArgSlot);
        Assert(count != Constants::InvalidArgSlot);
        mArgCount = count;
        if( count > 0 )
        {TRACE_IT(46974);
            mArgumentsType = AnewArrayZ( mAllocator, AsmJsType, count );
        }
    }

    AsmJsType* AsmJsFunctionDeclaration::GetArgTypeArray()
    {TRACE_IT(46975);
        return mArgumentsType;
    }

    bool AsmJsFunctionDeclaration::CheckAndSetReturnType(Js::AsmJsRetType val)
    {TRACE_IT(46976);
        Assert((val != AsmJsRetType::Fixnum && val != AsmJsRetType::Unsigned && val != AsmJsRetType::Floatish) ||
               GetSymbolType() == AsmJsSymbol::MathBuiltinFunction ||
               GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction);
        if (mReturnTypeKnown)
        {TRACE_IT(46977);
            Assert((mReturnType != AsmJsRetType::Fixnum && mReturnType != AsmJsRetType::Unsigned && mReturnType != AsmJsRetType::Floatish) ||
                   GetSymbolType() == AsmJsSymbol::MathBuiltinFunction ||
                   GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction);
            return mReturnType.toType().isSubType(val.toType());
        }
        mReturnType = val;
        mReturnTypeKnown = true;
        return true;
    }

    Js::AsmJsType AsmJsFunctionDeclaration::GetType() const
    {TRACE_IT(46978);
        return mReturnType.toType();
    }

    bool AsmJsFunctionDeclaration::isMutable() const
    {TRACE_IT(46979);
        return false;
    }
    bool AsmJsFunctionDeclaration::EnsureArgType(AsmJsVarBase* arg, ArgSlot index)
    {TRACE_IT(46980);
        if (mArgumentsType[index].GetWhich() == -1)
        {
            SetArgType(arg, index);
            return true;
        }
        else
        {TRACE_IT(46981);
            return mArgumentsType[index] == arg->GetType();
        }
    }

    bool AsmJsFunctionDeclaration::SupportsArgCall( ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType )
    {TRACE_IT(46982);
        // we will assume the first reference to the function is correct, until proven wrong
        if (GetArgCount() == Constants::InvalidArgSlot)
        {TRACE_IT(46983);
            SetArgCount(argCount);

            for (ArgSlot i = 0; i < argCount; i++)
            {TRACE_IT(46984);
                if (args[i].isSubType(AsmJsType::Double))
                {TRACE_IT(46985);
                    mArgumentsType[i] = AsmJsType::Double;
                }
                else if (args[i].isSubType(AsmJsType::Float))
                {TRACE_IT(46986);
                    mArgumentsType[i] = AsmJsType::Float;
                }
                else if (args[i].isSubType(AsmJsType::Int))
                {TRACE_IT(46987);
                    mArgumentsType[i] = AsmJsType::Int;
                }
                else if (args[i].isSIMDType())
                {TRACE_IT(46988);
                    mArgumentsType[i] = args[i];
                }
                else
                {TRACE_IT(46989);
                    // call did not have valid argument type
                    return false;
                }
            }
            retType = mReturnType;
            return true;
        }
        else if( argCount == GetArgCount() )
        {TRACE_IT(46990);
            for(ArgSlot i = 0; i < argCount; i++ )
            {TRACE_IT(46991);
                if (!args[i].isSubType(mArgumentsType[i]))
                {TRACE_IT(46992);
                    return false;
                }
            }
            retType = mReturnType;
            return true;
        }
        return false;
    }

    ArgSlot AsmJsFunctionDeclaration::GetArgByteSize(ArgSlot inArgCount) const
    {TRACE_IT(46993);
        uint argSize = 0;
        if (GetSymbolType() == AsmJsSymbol::ImportFunction)
        {TRACE_IT(46994);
            Assert(inArgCount != Constants::InvalidArgSlot);
            argSize = inArgCount * MachPtr;
        }
#if _M_IX86
        else
        {TRACE_IT(46995);
            for (ArgSlot i = 0; i < GetArgCount(); i++)
            {TRACE_IT(46996);
                if( GetArgType(i).isMaybeDouble() )
                {TRACE_IT(46997);
                    argSize += sizeof(double);
                }
                else if (GetArgType(i).isIntish())
                {TRACE_IT(46998);
                    argSize += sizeof(int);
                }
                else if (GetArgType(i).isFloatish())
                {TRACE_IT(46999);
                    argSize += sizeof(float);
                }
                else if (GetArgType(i).isSIMDType())
                {TRACE_IT(47000);
                    argSize += sizeof(AsmJsSIMDValue);
                }
                else
                {TRACE_IT(47001);
                    Assume(UNREACHED);
                }
            }
        }
#elif _M_X64
        else
        {TRACE_IT(47002);
            for (ArgSlot i = 0; i < GetArgCount(); i++)
            {TRACE_IT(47003);
                if (GetArgType(i).isSIMDType())
                {TRACE_IT(47004);
                    argSize += sizeof(AsmJsSIMDValue);
                }
                else
                {TRACE_IT(47005);
                    argSize += MachPtr;
                }
            }
        }
#else
        Assert(UNREACHED);
#endif
        if (argSize >= (1 << 16))
        {TRACE_IT(47006);
            // throw OOM on overflow
            Throw::OutOfMemory();
        }
        return static_cast<ArgSlot>(argSize);
    }

    AsmJsMathFunction::AsmJsMathFunction( PropertyName name, ArenaAllocator* allocator, ArgSlot argCount, AsmJSMathBuiltinFunction builtIn, OpCodeAsmJs op, AsmJsRetType retType, ... ) :
        AsmJsFunctionDeclaration( name, AsmJsSymbol::MathBuiltinFunction, allocator )
        , mBuiltIn( builtIn )
        , mOverload( nullptr )
        , mOpCode(op)
    {TRACE_IT(47007);
        bool ret = CheckAndSetReturnType(retType);
        Assert(ret);
        va_list arguments;

        SetArgCount( argCount );
        va_start( arguments, retType );
        for(ArgSlot iArg = 0; iArg < argCount; iArg++)
        {TRACE_IT(47008);
            SetArgType(static_cast<AsmJsType::Which>(va_arg(arguments, int)), iArg);
        }
        va_end(arguments);
    }

    void AsmJsMathFunction::SetOverload(AsmJsMathFunction* val)
    {TRACE_IT(47009);
#if DBG
        AsmJsMathFunction* over = val->mOverload;
        while (over)
        {TRACE_IT(47010);
            if (over == this)
            {TRACE_IT(47011);
                Assert(false);
                break;
            }
            over = over->mOverload;
        }
#endif
        Assert(val->GetSymbolType() == GetSymbolType());
        if (this->mOverload)
        {TRACE_IT(47012);
            this->mOverload->SetOverload(val);
        }
        else
        {TRACE_IT(47013);
            mOverload = val;
        }
    }

    bool AsmJsMathFunction::CheckAndSetReturnType(Js::AsmJsRetType val)
    {TRACE_IT(47014);
        return AsmJsFunctionDeclaration::CheckAndSetReturnType(val) || (mOverload && mOverload->CheckAndSetReturnType(val));
    }

    bool AsmJsMathFunction::SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType )
    {TRACE_IT(47015);
        return AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType) || (mOverload && mOverload->SupportsArgCall(argCount, args, retType));
    }

    bool AsmJsMathFunction::SupportsMathCall(ArgSlot argCount, AsmJsType* args, OpCodeAsmJs& op, AsmJsRetType& retType )
    {TRACE_IT(47016);
        if (AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType))
        {TRACE_IT(47017);
            op = mOpCode;
            return true;
        }
        return mOverload && mOverload->SupportsMathCall(argCount, args, op, retType);
    }

    WAsmJs::RegisterSpace*
        AllocateRegisterSpace(ArenaAllocator* alloc, WAsmJs::Types type)
    {TRACE_IT(47018);
        switch(type)
        {
        case WAsmJs::INT32: return Anew(alloc, AsmJsRegisterSpace<int>, alloc);
        case WAsmJs::FLOAT32: return Anew(alloc, AsmJsRegisterSpace<float>, alloc);
        case WAsmJs::FLOAT64: return Anew(alloc, AsmJsRegisterSpace<double>, alloc);
        case WAsmJs::SIMD: return Anew(alloc, AsmJsRegisterSpace<AsmJsSIMDValue>, alloc);
        default:
            AssertMsg(false, "Invalid native asm.js type");
            Js::Throw::InternalError();
        }
    }

    AsmJsFunc::AsmJsFunc(PropertyName name, ParseNode* pnodeFnc, ArenaAllocator* allocator, ScriptContext* scriptContext) :
        AsmJsFunctionDeclaration(name, AsmJsSymbol::ModuleFunction, allocator)
        , mCompileTime(0)
        , mVarMap(allocator)
        , mBodyNode(nullptr)
        , mFncNode(pnodeFnc)
        , mTypedRegisterAllocator(
            allocator,
            AllocateRegisterSpace,
            // Exclude int64 and simd if not enabled
            1 << WAsmJs::INT64 | (scriptContext->GetConfig()->IsSimdjsEnabled() ? 0 : 1 << WAsmJs::SIMD)
        )
        , mFuncInfo(pnodeFnc->sxFnc.funcInfo)
        , mFuncBody(nullptr)
        , mSimdVarsList(allocator)
        , mArgOutDepth(0)
        , mMaxArgOutDepth(0)
        , mDefined( false )
    {TRACE_IT(47019);
    }

    /// AsmJsFunc
    AsmJsVarBase* AsmJsFunc::DefineVar( PropertyName name, bool isArg /*= false*/, bool isMutable /*= true*/ )
    {TRACE_IT(47020);
        AsmJsVarBase* var = FindVar(name);
        if (var)
        {TRACE_IT(47021);
            if (PHASE_TRACE1(AsmjsPhase))
            {TRACE_IT(47022);
                Output::Print(_u("Variable redefinition: %s\n"), name->Psz());
            }
            return nullptr;
        }

        if (isArg)
        {TRACE_IT(47023);
            // arg cannot be const
            Assert(isMutable);
            var = Anew( mAllocator, AsmJsArgument, name );
        }
        else
        {TRACE_IT(47024);
            var = Anew(mAllocator, AsmJsVar, name, isMutable);
        }
        int addResult = mVarMap.AddNew(name->GetPropertyId(), var);
        if( addResult == -1 )
        {TRACE_IT(47025);
            mAllocator->Free(var, isArg ? sizeof(AsmJsArgument) : sizeof(AsmJsVar));
            return nullptr;
        }
        return var;
    }


    AsmJsVarBase* AsmJsFunc::FindVar(const PropertyName name) const
    {TRACE_IT(47026);
        return mVarMap.LookupWithKey(name->GetPropertyId(), nullptr);
    }

    void AsmJsFunc::ReleaseLocationGeneric(const EmitExpressionInfo* pnode)
    {TRACE_IT(47027);
        if (pnode)
        {TRACE_IT(47028);
            if (pnode->type.isIntish())
            {TRACE_IT(47029);
                ReleaseLocation<int>(pnode);
            }
            else if (pnode->type.isMaybeDouble())
            {TRACE_IT(47030);
                ReleaseLocation<double>(pnode);
            }
            else if (pnode->type.isFloatish())
            {TRACE_IT(47031);
                ReleaseLocation<float>(pnode);
            }
            else if (pnode->type.isSIMDType())
            {TRACE_IT(47032);
                ReleaseLocation<AsmJsSIMDValue>(pnode);
            }
        }
    }

    AsmJsSymbol* AsmJsFunc::LookupIdentifier(const PropertyName name, AsmJsLookupSource::Source* lookupSource /*= nullptr */) const
    {TRACE_IT(47033);
        auto var = FindVar(name);
        if (var && lookupSource)
        {TRACE_IT(47034);
            *lookupSource = AsmJsLookupSource::AsmJsFunction;
        }
        return var;
    }

    void AsmJsFunc::SetArgOutDepth( int outParamsCount )
    {TRACE_IT(47035);
        mArgOutDepth = outParamsCount;
    }

    void AsmJsFunc::UpdateMaxArgOutDepth(int outParamsCount)
    {TRACE_IT(47036);
        if (mMaxArgOutDepth < outParamsCount)
        {TRACE_IT(47037);
            mMaxArgOutDepth = outParamsCount;
        }
    }

    bool AsmJsFunctionInfo::Init(AsmJsFunc* func)
    {TRACE_IT(47038);
        func->CommitToFunctionInfo(this, func->GetFuncBody());

        Recycler* recycler = func->GetFuncBody()->GetScriptContext()->GetRecycler();
        mArgCount = func->GetArgCount();
        if (mArgCount > 0)
        {TRACE_IT(47039);
            mArgType = RecyclerNewArrayLeaf(recycler, AsmJsVarType::Which, mArgCount);
        }

        // on x64, AsmJsExternalEntryPoint reads first 3 elements to figure out how to shadow args on stack
        // always alloc space for these such that we need to do less work in the entrypoint
        mArgSizesLength = max(mArgCount, 3ui16);
        mArgSizes = RecyclerNewArrayLeafZ(recycler, uint, mArgSizesLength);

        mReturnType = func->GetReturnType();
        mbyteCodeTJMap = RecyclerNew(recycler, ByteCodeToTJMap,recycler);

        for(ArgSlot i = 0; i < GetArgCount(); i++)
        {TRACE_IT(47040);
            AsmJsType varType = func->GetArgType(i);
            SetArgType(AsmJsVarType::FromCheckedType(varType), i);
        }

        return true;
    }

    WAsmJs::TypedSlotInfo* AsmJsFunctionInfo::GetTypedSlotInfo(WAsmJs::Types type)
    {TRACE_IT(47041);
        if ((uint32)type >= WAsmJs::LIMIT)
        {TRACE_IT(47042);
            Assert(false);
            Js::Throw::InternalError();
        }
        return &mTypedSlotInfos[type];
    }

    int AsmJsFunctionInfo::GetTotalSizeinBytes() const
    {TRACE_IT(47043);
        int size;

        // SIMD values are aligned
        Assert(GetSimdByteOffset() % sizeof(AsmJsSIMDValue) == 0);
        size = GetSimdByteOffset() + GetSimdAllCount()* sizeof(AsmJsSIMDValue);

        return size;
    }


    void AsmJsFunctionInfo::SetArgType(AsmJsVarType type, ArgSlot index)
    {TRACE_IT(47044);
        Assert(mArgCount != Constants::InvalidArgSlot);
        AnalysisAssert(index < mArgCount);

        Assert(
            type.isInt() ||
            type.isInt64() ||
            type.isFloat() ||
            type.isDouble() ||
            type.isSIMD()
        );

        mArgType[index] = type.which();
        ArgSlot argSize = 0;

        if (type.isDouble())
        {TRACE_IT(47045);
            argSize = sizeof(double);
        }
        else if (type.isSIMD())
        {TRACE_IT(47046);
            argSize = sizeof(AsmJsSIMDValue);
        }
        else if (type.isInt64())
        {TRACE_IT(47047);
            argSize = sizeof(int64);
        }
        else
        {TRACE_IT(47048);
            // int and float are Js::Var
            argSize = (ArgSlot)MachPtr;
        }

        mArgByteSize = UInt16Math::Add(mArgByteSize, argSize);
        mArgSizes[index] = argSize;
    }

    Js::AsmJsType AsmJsArrayView::GetType() const
    {TRACE_IT(47049);
        switch (mViewType)
        {
        case ArrayBufferView::TYPE_INT8:
        case ArrayBufferView::TYPE_INT16:
        case ArrayBufferView::TYPE_INT32:
        case ArrayBufferView::TYPE_UINT8:
        case ArrayBufferView::TYPE_UINT16:
        case ArrayBufferView::TYPE_UINT32:
            return AsmJsType::Intish;
        case ArrayBufferView::TYPE_FLOAT32:
            return AsmJsType::MaybeFloat;
        case ArrayBufferView::TYPE_FLOAT64:
            return AsmJsType::MaybeDouble;
        default:;
        }
        AssertMsg(false, "Unexpected array type");
        return AsmJsType::Intish;
    }

    bool AsmJsArrayView::isMutable() const
    {TRACE_IT(47050);
        return false;
    }


    bool AsmJsImportFunction::SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType )
    {TRACE_IT(47051);
        for (ArgSlot i = 0; i < argCount ; i++)
        {TRACE_IT(47052);
            if (!args[i].isExtern())
            {TRACE_IT(47053);
                return false;
            }
        }
        return true;
    }

    AsmJsImportFunction::AsmJsImportFunction(PropertyName name, PropertyName field, ArenaAllocator* allocator) :
        AsmJsFunctionDeclaration(name, AsmJsSymbol::ImportFunction, allocator)
        , mField(field)
    {TRACE_IT(47054);
        CheckAndSetReturnType(AsmJsRetType::Void);
    }


    bool AsmJsFunctionTable::SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType )
    {TRACE_IT(47055);
        if (mAreArgumentsKnown)
        {TRACE_IT(47056);
            return AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType);
        }

        Assert(GetArgCount() == Constants::InvalidArgSlot);
        SetArgCount( argCount );

        retType = this->GetReturnType();

        for (ArgSlot i = 0; i < argCount ; i++)
        {TRACE_IT(47057);
            if (args[i].isInt())
            {TRACE_IT(47058);
                this->SetArgType(AsmJsType::Int, i);
            }
            else if (args[i].isDouble())
            {TRACE_IT(47059);
                this->SetArgType(AsmJsType::Double, i);
            }
            else if (args[i].isFloat())
            {TRACE_IT(47060);
                this->SetArgType(AsmJsType::Float, i);
            }
            else
            {TRACE_IT(47061);
                // Function tables can only have int, double or float as arguments
                return false;
            }
        }
        mAreArgumentsKnown = true;
        return true;
    }

    AsmJsSIMDFunction::AsmJsSIMDFunction(PropertyName name, ArenaAllocator* allocator, ArgSlot argCount, AsmJsSIMDBuiltinFunction builtIn, OpCodeAsmJs op, AsmJsRetType retType, ...) :
        AsmJsFunctionDeclaration(name, AsmJsSymbol::SIMDBuiltinFunction, allocator)
        , mBuiltIn(builtIn)
        , mOverload(nullptr)
        , mOpCode(op)
    {TRACE_IT(47062);
        bool ret = CheckAndSetReturnType(retType);
        Assert(ret);
        va_list arguments;

        SetArgCount(argCount);
        va_start(arguments, retType);
        for (ArgSlot iArg = 0; iArg < argCount; iArg++)
        {TRACE_IT(47063);
            SetArgType(static_cast<AsmJsType::Which>(va_arg(arguments, int)), iArg);
        }
        va_end(arguments);
    }

    bool AsmJsSIMDFunction::SupportsSIMDCall(ArgSlot argCount, AsmJsType* args, OpCodeAsmJs& op, AsmJsRetType& retType)
    {TRACE_IT(47064);
        if (AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType))
        {TRACE_IT(47065);
            op = mOpCode;
            return true;
        }
        return mOverload && mOverload->SupportsSIMDCall(argCount, args, op, retType);
    }

    bool AsmJsSIMDFunction::SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType)
    {TRACE_IT(47066);
        return AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType) || (mOverload && mOverload->SupportsArgCall(argCount, args, retType));
    }

    bool AsmJsSIMDFunction::CheckAndSetReturnType(Js::AsmJsRetType val)
    {TRACE_IT(47067);
        return AsmJsFunctionDeclaration::CheckAndSetReturnType(val) || (mOverload && mOverload->CheckAndSetReturnType(val));
    }


    void AsmJsSIMDFunction::SetOverload(AsmJsSIMDFunction* val)
    {TRACE_IT(47068);
#if DBG
        AsmJsSIMDFunction* over = val->mOverload;
        while (over)
        {TRACE_IT(47069);
            if (over == this)
            {TRACE_IT(47070);
                Assert(false);
                break;
            }
            over = over->mOverload;
        }
#endif
        Assert(val->GetSymbolType() == GetSymbolType());
        if (this->mOverload)
        {TRACE_IT(47071);
            this->mOverload->SetOverload(val);
        }
        else
        {TRACE_IT(47072);
            mOverload = val;
        }
    }

    bool AsmJsSIMDFunction::IsTypeCheck()
    {TRACE_IT(47073);
        return mBuiltIn == AsmJsSIMDBuiltin_int32x4_check ||
               mBuiltIn == AsmJsSIMDBuiltin_float32x4_check ||
               mBuiltIn == AsmJsSIMDBuiltin_float64x2_check ||
               mBuiltIn == AsmJsSIMDBuiltin_int16x8_check ||
               mBuiltIn == AsmJsSIMDBuiltin_int8x16_check ||
               mBuiltIn == AsmJsSIMDBuiltin_uint32x4_check ||
               mBuiltIn == AsmJsSIMDBuiltin_uint16x8_check ||
               mBuiltIn == AsmJsSIMDBuiltin_uint8x16_check ||
               mBuiltIn == AsmJsSIMDBuiltin_bool32x4_check ||
               mBuiltIn == AsmJsSIMDBuiltin_bool16x8_check ||
               mBuiltIn == AsmJsSIMDBuiltin_bool8x16_check;
    }

    bool AsmJsSIMDFunction::IsUnsignedTypeCheck()
    {TRACE_IT(47074);
        return mBuiltIn == AsmJsSIMDBuiltin_uint32x4_check ||
               mBuiltIn == AsmJsSIMDBuiltin_uint16x8_check ||
               mBuiltIn == AsmJsSIMDBuiltin_uint8x16_check;
    }

    AsmJsVarType AsmJsSIMDFunction::GetTypeCheckVarType()
    {TRACE_IT(47075);
        Assert(this->IsTypeCheck());
        return GetReturnType().toVarType();
    }
    bool AsmJsSIMDFunction::IsConstructor()
    {TRACE_IT(47076);
        return mBuiltIn == AsmJsSIMDBuiltin_Int32x4 ||
            mBuiltIn == AsmJsSIMDBuiltin_Float32x4 ||
            mBuiltIn == AsmJsSIMDBuiltin_Float64x2 ||
            mBuiltIn == AsmJsSIMDBuiltin_Int16x8 ||
            mBuiltIn == AsmJsSIMDBuiltin_Int8x16 ||
            mBuiltIn == AsmJsSIMDBuiltin_Uint32x4 ||
            mBuiltIn == AsmJsSIMDBuiltin_Uint16x8 ||
            mBuiltIn == AsmJsSIMDBuiltin_Uint8x16 ||
            mBuiltIn == AsmJsSIMDBuiltin_Bool32x4 ||
            mBuiltIn == AsmJsSIMDBuiltin_Bool16x8 ||
            mBuiltIn == AsmJsSIMDBuiltin_Bool8x16 ;
    }

    // Is a constructor with the correct argCount ?
    bool AsmJsSIMDFunction::IsConstructor(uint argCount)
    {TRACE_IT(47077);
        if (!IsConstructor())
        {TRACE_IT(47078);
            return false;
        }

        switch (mBuiltIn)
        {
        case AsmJsSIMDBuiltin_Float64x2:
            return argCount == 2;
        case AsmJsSIMDBuiltin_Float32x4:
        case AsmJsSIMDBuiltin_Int32x4:
        case AsmJsSIMDBuiltin_Uint32x4:
        case AsmJsSIMDBuiltin_Bool32x4:
            return argCount == 4;
        case AsmJsSIMDBuiltin_Int16x8:
        case AsmJsSIMDBuiltin_Uint16x8:
        case AsmJsSIMDBuiltin_Bool16x8:
            return argCount == 8;
        case AsmJsSIMDBuiltin_Uint8x16:
        case AsmJsSIMDBuiltin_Int8x16:
        case AsmJsSIMDBuiltin_Bool8x16:
            return argCount == 16;
        };
        return false;
    }

    AsmJsVarType AsmJsSIMDFunction::GetConstructorVarType()
    {TRACE_IT(47079);
        Assert(this->IsConstructor());
        return GetReturnType().toVarType();
    }
}
#endif
