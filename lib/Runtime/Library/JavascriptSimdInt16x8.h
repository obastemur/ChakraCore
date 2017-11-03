//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#ifdef ENABLE_SIMDJS
class JavascriptSIMDFloat32x4;
class JavascriptSIMDFloat64x2;

namespace Js
{
    class JavascriptSIMDInt16x8 sealed : public JavascriptSIMDType
    {
    private:
        DEFINE_VTABLE_CTOR(JavascriptSIMDInt16x8, JavascriptSIMDType);
        static const CHAR_T TypeName[];
    public:
        class EntryInfo
        {
        public:
            static FunctionInfo ToString;
            static FunctionInfo ToLocaleString;
            static FunctionInfo ValueOf;
            static FunctionInfo SymbolToPrimitive;
            static FunctionInfo Bool;
        };

        JavascriptSIMDInt16x8(StaticType *type);
        JavascriptSIMDInt16x8(SIMDValue *val, StaticType *type);
        static JavascriptSIMDInt16x8* New(SIMDValue *val, ScriptContext* requestContext);
        static bool Is(Var instance);
        static JavascriptSIMDInt16x8* FromVar(Var aValue);
        static Var CallToLocaleString(RecyclableObject& obj, ScriptContext& requestContext, SIMDValue simdValue, const Var* args, uint numArgs, CallInfo callInfo);

        virtual RecyclableObject * CloneToScriptContext(ScriptContext* requestContext) override;

        static const CHAR_T* GetTypeName();
        inline SIMDValue GetValue() { return value; }

        static void ToStringBuffer(SIMDValue& value, __out_ecount(countBuffer) CHAR_T* stringBuffer, size_t countBuffer, ScriptContext* scriptContext = nullptr)
        {
            swprintf_s(stringBuffer, countBuffer, _u("SIMD.Int16x8(%d, %d, %d, %d, %d, %d, %d, %d)"), value.i16[0], value.i16[1], value.i16[2], value.i16[3],
                value.i16[4], value.i16[5], value.i16[6], value.i16[7]);
        }

        Var Copy(ScriptContext* requestContext);
        Var CopyAndSetLane(uint index, int value, ScriptContext* requestContext);

    private:
        Var GetLaneAsNumber(uint index, ScriptContext* requestContext);
    };
}
#endif
