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
    {LOGMEIN("AsmJsTypes.cpp] 33\n");
        switch (which_)
        {LOGMEIN("AsmJsTypes.cpp] 35\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 66\n");
        return isSIMDInt32x4()  || isSIMDInt16x8()   || isSIMDInt8x16()   ||
               isSIMDBool32x4() || isSIMDBool16x8()  || isSIMDBool8x16()  ||
               isSIMDUint32x4() || isSIMDUint16x8()  || isSIMDUint8x16()  ||
               isSIMDFloat32x4()|| isSIMDFloat64x2();
    }

    bool AsmJsType::isSIMDInt32x4() const
    {LOGMEIN("AsmJsTypes.cpp] 74\n");
        return which_ == Int32x4;
    }
    bool AsmJsType::isSIMDBool32x4() const
    {LOGMEIN("AsmJsTypes.cpp] 78\n");
        return which_ == Bool32x4;
    }
    bool AsmJsType::isSIMDBool16x8() const
    {LOGMEIN("AsmJsTypes.cpp] 82\n");
        return which_ == Bool16x8;
    }
    bool AsmJsType::isSIMDBool8x16() const
    {LOGMEIN("AsmJsTypes.cpp] 86\n");
        return which_ == Bool8x16;
    }
    bool AsmJsType::isSIMDInt16x8() const
    {LOGMEIN("AsmJsTypes.cpp] 90\n");
        return which_ == Int16x8;
    }
    bool AsmJsType::isSIMDInt8x16() const
    {LOGMEIN("AsmJsTypes.cpp] 94\n");
        return which_ == Int8x16;
    }
    bool AsmJsType::isSIMDFloat32x4() const
    {LOGMEIN("AsmJsTypes.cpp] 98\n");
        return which_ == Float32x4;
    }
    bool AsmJsType::isSIMDFloat64x2() const
    {LOGMEIN("AsmJsTypes.cpp] 102\n");
        return which_ == Float64x2;
    }

    bool AsmJsType::isSIMDUint32x4() const
    {LOGMEIN("AsmJsTypes.cpp] 107\n");
        return which_ == Uint32x4;
    }
    bool AsmJsType::isSIMDUint16x8() const
    {LOGMEIN("AsmJsTypes.cpp] 111\n");
        return which_ == Uint16x8;
    }
    bool AsmJsType::isSIMDUint8x16() const
    {LOGMEIN("AsmJsTypes.cpp] 115\n");
        return which_ == Uint8x16;
    }

    bool AsmJsType::isVarAsmJsType() const
    {LOGMEIN("AsmJsTypes.cpp] 120\n");
        return isInt() || isMaybeDouble() || isMaybeFloat();
    }

    bool AsmJsType::isExtern() const
    {LOGMEIN("AsmJsTypes.cpp] 125\n");
        return isDouble() || isSigned();
    }

    bool AsmJsType::isVoid() const
    {LOGMEIN("AsmJsTypes.cpp] 130\n");
        return which_ == Void;
    }

    bool AsmJsType::isFloatish() const
    {LOGMEIN("AsmJsTypes.cpp] 135\n");
        return isMaybeFloat() || which_ == Floatish;
    }

    bool AsmJsType::isFloatishDoubleLit() const
    {LOGMEIN("AsmJsTypes.cpp] 140\n");
        return isFloatish() || isDoubleLit();
    }

    bool AsmJsType::isMaybeFloat() const
    {LOGMEIN("AsmJsTypes.cpp] 145\n");
        return isFloat() || which_ == MaybeFloat;
    }

    bool AsmJsType::isFloat() const
    {LOGMEIN("AsmJsTypes.cpp] 150\n");
        return which_ == Float;
    }

    bool AsmJsType::isMaybeDouble() const
    {LOGMEIN("AsmJsTypes.cpp] 155\n");
        return isDouble() || which_ == MaybeDouble;
    }

    bool AsmJsType::isDouble() const
    {LOGMEIN("AsmJsTypes.cpp] 160\n");
        return isDoubleLit() || which_ == Double;
    }

    bool AsmJsType::isDoubleLit() const
    {LOGMEIN("AsmJsTypes.cpp] 165\n");
        return which_ == DoubleLit;
    }

    bool AsmJsType::isIntish() const
    {LOGMEIN("AsmJsTypes.cpp] 170\n");
        return isInt() || which_ == Intish;
    }

    bool AsmJsType::isInt() const
    {LOGMEIN("AsmJsTypes.cpp] 175\n");
        return isSigned() || isUnsigned() || which_ == Int;
    }

    bool AsmJsType::isUnsigned() const
    {LOGMEIN("AsmJsTypes.cpp] 180\n");
        return which_ == Unsigned || which_ == Fixnum;
    }

    bool AsmJsType::isSigned() const
    {LOGMEIN("AsmJsTypes.cpp] 185\n");
        return which_ == Signed || which_ == Fixnum;
    }

    bool AsmJsType::operator!=(AsmJsType rhs) const
    {LOGMEIN("AsmJsTypes.cpp] 190\n");
        return which_ != rhs.which_;
    }

    bool AsmJsType::operator==(AsmJsType rhs) const
    {LOGMEIN("AsmJsTypes.cpp] 195\n");
        return which_ == rhs.which_;
    }

    bool AsmJsType::isSubType(AsmJsType type) const
    {LOGMEIN("AsmJsTypes.cpp] 200\n");
        switch (type.which_)
        {LOGMEIN("AsmJsTypes.cpp] 202\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 283\n");
        return type.isSubType(which_);
    }

    Js::AsmJsRetType AsmJsType::toRetType() const
    {LOGMEIN("AsmJsTypes.cpp] 288\n");
        Which w = which_;
        // DoubleLit is for expressions only.
        if (w == DoubleLit)
        {LOGMEIN("AsmJsTypes.cpp] 292\n");
            w = Double;
        }
        return AsmJsRetType::Which(w);
    }

    /// RetType

    bool AsmJsRetType::operator!=(AsmJsRetType rhs) const
    {LOGMEIN("AsmJsTypes.cpp] 301\n");
        return which_ != rhs.which_;
    }

    bool AsmJsRetType::operator==(AsmJsRetType rhs) const
    {LOGMEIN("AsmJsTypes.cpp] 306\n");
        return which_ == rhs.which_;
    }

    Js::AsmJsType AsmJsRetType::toType() const
    {LOGMEIN("AsmJsTypes.cpp] 311\n");
        return AsmJsType::Which(which_);
    }

    Js::AsmJsVarType AsmJsRetType::toVarType() const
    {LOGMEIN("AsmJsTypes.cpp] 316\n");
        return AsmJsVarType::Which(which_);
    }

    Js::AsmJsRetType::Which AsmJsRetType::which() const
    {LOGMEIN("AsmJsTypes.cpp] 321\n");
        return which_;
    }

    AsmJsRetType::AsmJsRetType(AsmJSCoercion coercion)
    {LOGMEIN("AsmJsTypes.cpp] 326\n");
        switch (coercion)
        {LOGMEIN("AsmJsTypes.cpp] 328\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 347\n");

    }

    AsmJsRetType::AsmJsRetType() : which_(Which(-1))
    {LOGMEIN("AsmJsTypes.cpp] 352\n");

    }

    /// VarType

    bool AsmJsVarType::operator!=(AsmJsVarType rhs) const
    {LOGMEIN("AsmJsTypes.cpp] 359\n");
        return which_ != rhs.which_;
    }

    bool AsmJsVarType::operator==(AsmJsVarType rhs) const
    {LOGMEIN("AsmJsTypes.cpp] 364\n");
        return which_ == rhs.which_;
    }

    Js::AsmJsVarType AsmJsVarType::FromCheckedType(AsmJsType type)
    {LOGMEIN("AsmJsTypes.cpp] 369\n");
        Assert( type.isInt() || type.isMaybeDouble() || type.isFloatish() || type.isSIMDType());

        if (type.isMaybeDouble())
            return Double;
        else if (type.isFloatish())
            return Float;
        else if (type.isInt())
            return Int;
        else
        {LOGMEIN("AsmJsTypes.cpp] 379\n");
            // SIMD type
            return AsmJsVarType::Which(type.GetWhich());
        }

    }

    Js::AsmJSCoercion AsmJsVarType::toCoercion() const
    {LOGMEIN("AsmJsTypes.cpp] 387\n");
        switch (which_)
        {LOGMEIN("AsmJsTypes.cpp] 389\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 410\n");
        return AsmJsType::Which(which_);
    }

    Js::AsmJsVarType::Which AsmJsVarType::which() const
    {LOGMEIN("AsmJsTypes.cpp] 415\n");
        return which_;
    }

    AsmJsVarType::AsmJsVarType(AsmJSCoercion coercion)
    {LOGMEIN("AsmJsTypes.cpp] 420\n");
        switch (coercion)
        {LOGMEIN("AsmJsTypes.cpp] 422\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 441\n");

    }

    AsmJsVarType::AsmJsVarType() : which_(Which(-1))
    {LOGMEIN("AsmJsTypes.cpp] 446\n");

    }

    template<>
    AsmJsMathConst* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 452\n");
        Assert(mType == MathConstant);
        return (AsmJsMathConst*)this;
    }

    template<>
    AsmJsVar* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 459\n");
        Assert(mType == Variable);
        return (AsmJsVar*)this;
    }

    template<>
    AsmJsVarBase* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 466\n");
        Assert( mType == Argument || mType == Variable || mType == ConstantImport);
        return ( AsmJsVarBase* )this;
    }

    template<>
    AsmJsFunctionDeclaration* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 473\n");
        Assert(mType == ModuleFunction || mType == ImportFunction || mType == MathBuiltinFunction || mType == SIMDBuiltinFunction || mType == FuncPtrTable);
        return (AsmJsFunctionDeclaration*)this;
    }

    template<>
    AsmJsFunc* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 480\n");
        Assert(mType == ModuleFunction);
        return (AsmJsFunc*)this;
    }

    template<>
    AsmJsImportFunction* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 487\n");
        Assert(mType == ImportFunction);
        return (AsmJsImportFunction*)this;
    }

    template<>
    AsmJsMathFunction* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 494\n");
        Assert(mType == MathBuiltinFunction);
        return (AsmJsMathFunction*)this;
    }

    template<>
    AsmJsSIMDFunction* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 501\n");
        Assert(mType == SIMDBuiltinFunction);
        return  (AsmJsSIMDFunction*) this;
    }
    template<>
    AsmJsArrayView* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 507\n");
        Assert(mType == ArrayView);
        return (AsmJsArrayView*)this;
    }

    template<>
    AsmJsConstantImport* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 514\n");
        Assert(mType == ConstantImport);
        return (AsmJsConstantImport*)this;
    }

    template<>
    AsmJsFunctionTable* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 521\n");
        Assert(mType == FuncPtrTable);
        return (AsmJsFunctionTable*)this;
    }

    template<>
    AsmJsTypedArrayFunction* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 528\n");
        Assert(mType == TypedArrayBuiltinFunction);
        return (AsmJsTypedArrayFunction*)this;
    }

    template<>
    AsmJsModuleArg* Js::AsmJsSymbol::Cast()
    {LOGMEIN("AsmJsTypes.cpp] 535\n");
        Assert(mType == ModuleArgument);
        return (AsmJsModuleArg*)this;
    }

    Js::AsmJsType AsmJsModuleArg::GetType() const
    {LOGMEIN("AsmJsTypes.cpp] 541\n");
        Assert(UNREACHED);
        return AsmJsType::Void;
    }

    bool AsmJsModuleArg::isMutable() const
    {LOGMEIN("AsmJsTypes.cpp] 547\n");
        Assert(UNREACHED);
        return true;
    }

    Js::AsmJsType AsmJsMathConst::GetType() const
    {LOGMEIN("AsmJsTypes.cpp] 553\n");
        return AsmJsType::Double;
    }

    bool AsmJsMathConst::isMutable() const
    {LOGMEIN("AsmJsTypes.cpp] 558\n");
        return false;
    }

    bool AsmJsFunctionDeclaration::EnsureArgCount(ArgSlot count)
    {LOGMEIN("AsmJsTypes.cpp] 563\n");
        if (mArgCount == Constants::InvalidArgSlot)
        {LOGMEIN("AsmJsTypes.cpp] 565\n");
            SetArgCount(count);
            return true;
        }
        else
        {
            return mArgCount == count;
        }
    }

    void AsmJsFunctionDeclaration::SetArgCount(ArgSlot count )
    {LOGMEIN("AsmJsTypes.cpp] 576\n");
        Assert( mArgumentsType == nullptr );
        Assert(mArgCount == Constants::InvalidArgSlot);
        Assert(count != Constants::InvalidArgSlot);
        mArgCount = count;
        if( count > 0 )
        {LOGMEIN("AsmJsTypes.cpp] 582\n");
            mArgumentsType = AnewArrayZ( mAllocator, AsmJsType, count );
        }
    }

    AsmJsType* AsmJsFunctionDeclaration::GetArgTypeArray()
    {LOGMEIN("AsmJsTypes.cpp] 588\n");
        return mArgumentsType;
    }

    bool AsmJsFunctionDeclaration::CheckAndSetReturnType(Js::AsmJsRetType val)
    {LOGMEIN("AsmJsTypes.cpp] 593\n");
        Assert((val != AsmJsRetType::Fixnum && val != AsmJsRetType::Unsigned && val != AsmJsRetType::Floatish) ||
               GetSymbolType() == AsmJsSymbol::MathBuiltinFunction ||
               GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction);
        if (mReturnTypeKnown)
        {LOGMEIN("AsmJsTypes.cpp] 598\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 610\n");
        return mReturnType.toType();
    }

    bool AsmJsFunctionDeclaration::isMutable() const
    {LOGMEIN("AsmJsTypes.cpp] 615\n");
        return false;
    }
    bool AsmJsFunctionDeclaration::EnsureArgType(AsmJsVarBase* arg, ArgSlot index)
    {LOGMEIN("AsmJsTypes.cpp] 619\n");
        if (mArgumentsType[index].GetWhich() == -1)
        {
            SetArgType(arg, index);
            return true;
        }
        else
        {
            return mArgumentsType[index] == arg->GetType();
        }
    }

    bool AsmJsFunctionDeclaration::SupportsArgCall( ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType )
    {LOGMEIN("AsmJsTypes.cpp] 632\n");
        // we will assume the first reference to the function is correct, until proven wrong
        if (GetArgCount() == Constants::InvalidArgSlot)
        {LOGMEIN("AsmJsTypes.cpp] 635\n");
            SetArgCount(argCount);

            for (ArgSlot i = 0; i < argCount; i++)
            {LOGMEIN("AsmJsTypes.cpp] 639\n");
                if (args[i].isSubType(AsmJsType::Double))
                {LOGMEIN("AsmJsTypes.cpp] 641\n");
                    mArgumentsType[i] = AsmJsType::Double;
                }
                else if (args[i].isSubType(AsmJsType::Float))
                {LOGMEIN("AsmJsTypes.cpp] 645\n");
                    mArgumentsType[i] = AsmJsType::Float;
                }
                else if (args[i].isSubType(AsmJsType::Int))
                {LOGMEIN("AsmJsTypes.cpp] 649\n");
                    mArgumentsType[i] = AsmJsType::Int;
                }
                else if (args[i].isSIMDType())
                {LOGMEIN("AsmJsTypes.cpp] 653\n");
                    mArgumentsType[i] = args[i];
                }
                else
                {
                    // call did not have valid argument type
                    return false;
                }
            }
            retType = mReturnType;
            return true;
        }
        else if( argCount == GetArgCount() )
        {LOGMEIN("AsmJsTypes.cpp] 666\n");
            for(ArgSlot i = 0; i < argCount; i++ )
            {LOGMEIN("AsmJsTypes.cpp] 668\n");
                if (!args[i].isSubType(mArgumentsType[i]))
                {LOGMEIN("AsmJsTypes.cpp] 670\n");
                    return false;
                }
            }
            retType = mReturnType;
            return true;
        }
        return false;
    }

    ArgSlot AsmJsFunctionDeclaration::GetArgByteSize(ArgSlot inArgCount) const
    {LOGMEIN("AsmJsTypes.cpp] 681\n");
        uint argSize = 0;
        if (GetSymbolType() == AsmJsSymbol::ImportFunction)
        {LOGMEIN("AsmJsTypes.cpp] 684\n");
            Assert(inArgCount != Constants::InvalidArgSlot);
            argSize = inArgCount * MachPtr;
        }
#if _M_IX86
        else
        {
            for (ArgSlot i = 0; i < GetArgCount(); i++)
            {LOGMEIN("AsmJsTypes.cpp] 692\n");
                if( GetArgType(i).isMaybeDouble() )
                {LOGMEIN("AsmJsTypes.cpp] 694\n");
                    argSize += sizeof(double);
                }
                else if (GetArgType(i).isIntish())
                {LOGMEIN("AsmJsTypes.cpp] 698\n");
                    argSize += sizeof(int);
                }
                else if (GetArgType(i).isFloatish())
                {LOGMEIN("AsmJsTypes.cpp] 702\n");
                    argSize += sizeof(float);
                }
                else if (GetArgType(i).isSIMDType())
                {LOGMEIN("AsmJsTypes.cpp] 706\n");
                    argSize += sizeof(AsmJsSIMDValue);
                }
                else
                {
                    Assume(UNREACHED);
                }
            }
        }
#elif _M_X64
        else
        {
            for (ArgSlot i = 0; i < GetArgCount(); i++)
            {LOGMEIN("AsmJsTypes.cpp] 719\n");
                if (GetArgType(i).isSIMDType())
                {LOGMEIN("AsmJsTypes.cpp] 721\n");
                    argSize += sizeof(AsmJsSIMDValue);
                }
                else
                {
                    argSize += MachPtr;
                }
            }
        }
#else
        Assert(UNREACHED);
#endif
        if (argSize >= (1 << 16))
        {LOGMEIN("AsmJsTypes.cpp] 734\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 746\n");
        bool ret = CheckAndSetReturnType(retType);
        Assert(ret);
        va_list arguments;

        SetArgCount( argCount );
        va_start( arguments, retType );
        for(ArgSlot iArg = 0; iArg < argCount; iArg++)
        {LOGMEIN("AsmJsTypes.cpp] 754\n");
            SetArgType(static_cast<AsmJsType::Which>(va_arg(arguments, int)), iArg);
        }
        va_end(arguments);
    }

    void AsmJsMathFunction::SetOverload(AsmJsMathFunction* val)
    {LOGMEIN("AsmJsTypes.cpp] 761\n");
#if DBG
        AsmJsMathFunction* over = val->mOverload;
        while (over)
        {LOGMEIN("AsmJsTypes.cpp] 765\n");
            if (over == this)
            {LOGMEIN("AsmJsTypes.cpp] 767\n");
                Assert(false);
                break;
            }
            over = over->mOverload;
        }
#endif
        Assert(val->GetSymbolType() == GetSymbolType());
        if (this->mOverload)
        {LOGMEIN("AsmJsTypes.cpp] 776\n");
            this->mOverload->SetOverload(val);
        }
        else
        {
            mOverload = val;
        }
    }

    bool AsmJsMathFunction::CheckAndSetReturnType(Js::AsmJsRetType val)
    {LOGMEIN("AsmJsTypes.cpp] 786\n");
        return AsmJsFunctionDeclaration::CheckAndSetReturnType(val) || (mOverload && mOverload->CheckAndSetReturnType(val));
    }

    bool AsmJsMathFunction::SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType )
    {LOGMEIN("AsmJsTypes.cpp] 791\n");
        return AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType) || (mOverload && mOverload->SupportsArgCall(argCount, args, retType));
    }

    bool AsmJsMathFunction::SupportsMathCall(ArgSlot argCount, AsmJsType* args, OpCodeAsmJs& op, AsmJsRetType& retType )
    {LOGMEIN("AsmJsTypes.cpp] 796\n");
        if (AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType))
        {LOGMEIN("AsmJsTypes.cpp] 798\n");
            op = mOpCode;
            return true;
        }
        return mOverload && mOverload->SupportsMathCall(argCount, args, op, retType);
    }

    WAsmJs::RegisterSpace*
        AllocateRegisterSpace(ArenaAllocator* alloc, WAsmJs::Types type)
    {LOGMEIN("AsmJsTypes.cpp] 807\n");
        switch(type)
        {LOGMEIN("AsmJsTypes.cpp] 809\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 838\n");
    }

    /// AsmJsFunc
    AsmJsVarBase* AsmJsFunc::DefineVar( PropertyName name, bool isArg /*= false*/, bool isMutable /*= true*/ )
    {LOGMEIN("AsmJsTypes.cpp] 843\n");
        AsmJsVarBase* var = FindVar(name);
        if (var)
        {LOGMEIN("AsmJsTypes.cpp] 846\n");
            if (PHASE_TRACE1(AsmjsPhase))
            {LOGMEIN("AsmJsTypes.cpp] 848\n");
                Output::Print(_u("Variable redefinition: %s\n"), name->Psz());
            }
            return nullptr;
        }

        if (isArg)
        {LOGMEIN("AsmJsTypes.cpp] 855\n");
            // arg cannot be const
            Assert(isMutable);
            var = Anew( mAllocator, AsmJsArgument, name );
        }
        else
        {
            var = Anew(mAllocator, AsmJsVar, name, isMutable);
        }
        int addResult = mVarMap.AddNew(name->GetPropertyId(), var);
        if( addResult == -1 )
        {LOGMEIN("AsmJsTypes.cpp] 866\n");
            mAllocator->Free(var, isArg ? sizeof(AsmJsArgument) : sizeof(AsmJsVar));
            return nullptr;
        }
        return var;
    }


    AsmJsVarBase* AsmJsFunc::FindVar(const PropertyName name) const
    {LOGMEIN("AsmJsTypes.cpp] 875\n");
        return mVarMap.LookupWithKey(name->GetPropertyId(), nullptr);
    }

    void AsmJsFunc::ReleaseLocationGeneric(const EmitExpressionInfo* pnode)
    {LOGMEIN("AsmJsTypes.cpp] 880\n");
        if (pnode)
        {LOGMEIN("AsmJsTypes.cpp] 882\n");
            if (pnode->type.isIntish())
            {LOGMEIN("AsmJsTypes.cpp] 884\n");
                ReleaseLocation<int>(pnode);
            }
            else if (pnode->type.isMaybeDouble())
            {LOGMEIN("AsmJsTypes.cpp] 888\n");
                ReleaseLocation<double>(pnode);
            }
            else if (pnode->type.isFloatish())
            {LOGMEIN("AsmJsTypes.cpp] 892\n");
                ReleaseLocation<float>(pnode);
            }
            else if (pnode->type.isSIMDType())
            {LOGMEIN("AsmJsTypes.cpp] 896\n");
                ReleaseLocation<AsmJsSIMDValue>(pnode);
            }
        }
    }

    AsmJsSymbol* AsmJsFunc::LookupIdentifier(const PropertyName name, AsmJsLookupSource::Source* lookupSource /*= nullptr */) const
    {LOGMEIN("AsmJsTypes.cpp] 903\n");
        auto var = FindVar(name);
        if (var && lookupSource)
        {LOGMEIN("AsmJsTypes.cpp] 906\n");
            *lookupSource = AsmJsLookupSource::AsmJsFunction;
        }
        return var;
    }

    void AsmJsFunc::SetArgOutDepth( int outParamsCount )
    {LOGMEIN("AsmJsTypes.cpp] 913\n");
        mArgOutDepth = outParamsCount;
    }

    void AsmJsFunc::UpdateMaxArgOutDepth(int outParamsCount)
    {LOGMEIN("AsmJsTypes.cpp] 918\n");
        if (mMaxArgOutDepth < outParamsCount)
        {LOGMEIN("AsmJsTypes.cpp] 920\n");
            mMaxArgOutDepth = outParamsCount;
        }
    }

    bool AsmJsFunctionInfo::Init(AsmJsFunc* func)
    {LOGMEIN("AsmJsTypes.cpp] 926\n");
        func->CommitToFunctionInfo(this, func->GetFuncBody());

        Recycler* recycler = func->GetFuncBody()->GetScriptContext()->GetRecycler();
        mArgCount = func->GetArgCount();
        if (mArgCount > 0)
        {LOGMEIN("AsmJsTypes.cpp] 932\n");
            mArgType = RecyclerNewArrayLeaf(recycler, AsmJsVarType::Which, mArgCount);
        }

        // on x64, AsmJsExternalEntryPoint reads first 3 elements to figure out how to shadow args on stack
        // always alloc space for these such that we need to do less work in the entrypoint
        mArgSizesLength = max(mArgCount, 3ui16);
        mArgSizes = RecyclerNewArrayLeafZ(recycler, uint, mArgSizesLength);

        mReturnType = func->GetReturnType();
        mbyteCodeTJMap = RecyclerNew(recycler, ByteCodeToTJMap,recycler);

        for(ArgSlot i = 0; i < GetArgCount(); i++)
        {LOGMEIN("AsmJsTypes.cpp] 945\n");
            AsmJsType varType = func->GetArgType(i);
            SetArgType(AsmJsVarType::FromCheckedType(varType), i);
        }

        return true;
    }

    WAsmJs::TypedSlotInfo* AsmJsFunctionInfo::GetTypedSlotInfo(WAsmJs::Types type)
    {LOGMEIN("AsmJsTypes.cpp] 954\n");
        if ((uint32)type >= WAsmJs::LIMIT)
        {LOGMEIN("AsmJsTypes.cpp] 956\n");
            Assert(false);
            Js::Throw::InternalError();
        }
        return &mTypedSlotInfos[type];
    }

    int AsmJsFunctionInfo::GetTotalSizeinBytes() const
    {LOGMEIN("AsmJsTypes.cpp] 964\n");
        int size;

        // SIMD values are aligned
        Assert(GetSimdByteOffset() % sizeof(AsmJsSIMDValue) == 0);
        size = GetSimdByteOffset() + GetSimdAllCount()* sizeof(AsmJsSIMDValue);

        return size;
    }


    void AsmJsFunctionInfo::SetArgType(AsmJsVarType type, ArgSlot index)
    {LOGMEIN("AsmJsTypes.cpp] 976\n");
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
        {LOGMEIN("AsmJsTypes.cpp] 992\n");
            argSize = sizeof(double);
        }
        else if (type.isSIMD())
        {LOGMEIN("AsmJsTypes.cpp] 996\n");
            argSize = sizeof(AsmJsSIMDValue);
        }
        else if (type.isInt64())
        {LOGMEIN("AsmJsTypes.cpp] 1000\n");
            argSize = sizeof(int64);
        }
        else
        {
            // int and float are Js::Var
            argSize = (ArgSlot)MachPtr;
        }

        mArgByteSize = UInt16Math::Add(mArgByteSize, argSize);
        mArgSizes[index] = argSize;
    }

    Js::AsmJsType AsmJsArrayView::GetType() const
    {LOGMEIN("AsmJsTypes.cpp] 1014\n");
        switch (mViewType)
        {LOGMEIN("AsmJsTypes.cpp] 1016\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 1035\n");
        return false;
    }


    bool AsmJsImportFunction::SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType )
    {LOGMEIN("AsmJsTypes.cpp] 1041\n");
        for (ArgSlot i = 0; i < argCount ; i++)
        {LOGMEIN("AsmJsTypes.cpp] 1043\n");
            if (!args[i].isExtern())
            {LOGMEIN("AsmJsTypes.cpp] 1045\n");
                return false;
            }
        }
        return true;
    }

    AsmJsImportFunction::AsmJsImportFunction(PropertyName name, PropertyName field, ArenaAllocator* allocator) :
        AsmJsFunctionDeclaration(name, AsmJsSymbol::ImportFunction, allocator)
        , mField(field)
    {LOGMEIN("AsmJsTypes.cpp] 1055\n");
        CheckAndSetReturnType(AsmJsRetType::Void);
    }


    bool AsmJsFunctionTable::SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType )
    {LOGMEIN("AsmJsTypes.cpp] 1061\n");
        if (mAreArgumentsKnown)
        {LOGMEIN("AsmJsTypes.cpp] 1063\n");
            return AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType);
        }

        Assert(GetArgCount() == Constants::InvalidArgSlot);
        SetArgCount( argCount );

        retType = this->GetReturnType();

        for (ArgSlot i = 0; i < argCount ; i++)
        {LOGMEIN("AsmJsTypes.cpp] 1073\n");
            if (args[i].isInt())
            {LOGMEIN("AsmJsTypes.cpp] 1075\n");
                this->SetArgType(AsmJsType::Int, i);
            }
            else if (args[i].isDouble())
            {LOGMEIN("AsmJsTypes.cpp] 1079\n");
                this->SetArgType(AsmJsType::Double, i);
            }
            else if (args[i].isFloat())
            {LOGMEIN("AsmJsTypes.cpp] 1083\n");
                this->SetArgType(AsmJsType::Float, i);
            }
            else
            {
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
    {LOGMEIN("AsmJsTypes.cpp] 1101\n");
        bool ret = CheckAndSetReturnType(retType);
        Assert(ret);
        va_list arguments;

        SetArgCount(argCount);
        va_start(arguments, retType);
        for (ArgSlot iArg = 0; iArg < argCount; iArg++)
        {LOGMEIN("AsmJsTypes.cpp] 1109\n");
            SetArgType(static_cast<AsmJsType::Which>(va_arg(arguments, int)), iArg);
        }
        va_end(arguments);
    }

    bool AsmJsSIMDFunction::SupportsSIMDCall(ArgSlot argCount, AsmJsType* args, OpCodeAsmJs& op, AsmJsRetType& retType)
    {LOGMEIN("AsmJsTypes.cpp] 1116\n");
        if (AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType))
        {LOGMEIN("AsmJsTypes.cpp] 1118\n");
            op = mOpCode;
            return true;
        }
        return mOverload && mOverload->SupportsSIMDCall(argCount, args, op, retType);
    }

    bool AsmJsSIMDFunction::SupportsArgCall(ArgSlot argCount, AsmJsType* args, AsmJsRetType& retType)
    {LOGMEIN("AsmJsTypes.cpp] 1126\n");
        return AsmJsFunctionDeclaration::SupportsArgCall(argCount, args, retType) || (mOverload && mOverload->SupportsArgCall(argCount, args, retType));
    }

    bool AsmJsSIMDFunction::CheckAndSetReturnType(Js::AsmJsRetType val)
    {LOGMEIN("AsmJsTypes.cpp] 1131\n");
        return AsmJsFunctionDeclaration::CheckAndSetReturnType(val) || (mOverload && mOverload->CheckAndSetReturnType(val));
    }


    void AsmJsSIMDFunction::SetOverload(AsmJsSIMDFunction* val)
    {LOGMEIN("AsmJsTypes.cpp] 1137\n");
#if DBG
        AsmJsSIMDFunction* over = val->mOverload;
        while (over)
        {LOGMEIN("AsmJsTypes.cpp] 1141\n");
            if (over == this)
            {LOGMEIN("AsmJsTypes.cpp] 1143\n");
                Assert(false);
                break;
            }
            over = over->mOverload;
        }
#endif
        Assert(val->GetSymbolType() == GetSymbolType());
        if (this->mOverload)
        {LOGMEIN("AsmJsTypes.cpp] 1152\n");
            this->mOverload->SetOverload(val);
        }
        else
        {
            mOverload = val;
        }
    }

    bool AsmJsSIMDFunction::IsTypeCheck()
    {LOGMEIN("AsmJsTypes.cpp] 1162\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 1177\n");
        return mBuiltIn == AsmJsSIMDBuiltin_uint32x4_check ||
               mBuiltIn == AsmJsSIMDBuiltin_uint16x8_check ||
               mBuiltIn == AsmJsSIMDBuiltin_uint8x16_check;
    }

    AsmJsVarType AsmJsSIMDFunction::GetTypeCheckVarType()
    {LOGMEIN("AsmJsTypes.cpp] 1184\n");
        Assert(this->IsTypeCheck());
        return GetReturnType().toVarType();
    }
    bool AsmJsSIMDFunction::IsConstructor()
    {LOGMEIN("AsmJsTypes.cpp] 1189\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 1205\n");
        if (!IsConstructor())
        {LOGMEIN("AsmJsTypes.cpp] 1207\n");
            return false;
        }

        switch (mBuiltIn)
        {LOGMEIN("AsmJsTypes.cpp] 1212\n");
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
    {LOGMEIN("AsmJsTypes.cpp] 1233\n");
        Assert(this->IsConstructor());
        return GetReturnType().toVarType();
    }
}
#endif
