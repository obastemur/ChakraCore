//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// JavascriptLibraryBase.h is used by static lib shared between Trident and Chakra. We need to keep
// the size consistent and try not to change its size. We need to have matching mshtml.dll
// if the size changed here.
#pragma once

namespace Js
{
    class EngineInterfaceObject;

    class JavascriptLibraryBase : public FinalizableObject
    {
        friend class JavascriptLibrary;
        friend class ScriptSite;
    public:
        JavascriptLibraryBase(GlobalObject* globalObject):
            globalObject(globalObject)
        {TRACE_IT(59992);
        }
        Var GetPI() {TRACE_IT(59993); return pi; }
        Var GetNaN() {TRACE_IT(59994); return nan; }
        Var GetNegativeInfinite() {TRACE_IT(59995); return negativeInfinite; }
        Var GetPositiveInfinite() {TRACE_IT(59996); return positiveInfinite; }
        Var GetMaxValue() {TRACE_IT(59997); return maxValue; }
        Var GetMinValue() {TRACE_IT(59998); return minValue; }
        Var GetNegativeZero() {TRACE_IT(59999); return negativeZero; }
        RecyclableObject* GetUndefined() {TRACE_IT(60000); return undefinedValue; }
        RecyclableObject* GetNull() {TRACE_IT(60001); return nullValue; }
        JavascriptBoolean* GetTrue() {TRACE_IT(60002); return booleanTrue; }
        JavascriptBoolean* GetFalse() {TRACE_IT(60003); return booleanFalse; }

        JavascriptSymbol* GetSymbolHasInstance() {TRACE_IT(60004); return symbolHasInstance; }
        JavascriptSymbol* GetSymbolIsConcatSpreadable() {TRACE_IT(60005); return symbolIsConcatSpreadable; }
        JavascriptSymbol* GetSymbolIterator() {TRACE_IT(60006); return symbolIterator; }
        JavascriptSymbol* GetSymbolToPrimitive() {TRACE_IT(60007); return symbolToPrimitive; }
        JavascriptSymbol* GetSymbolToStringTag() {TRACE_IT(60008); return symbolToStringTag; }
        JavascriptSymbol* GetSymbolUnscopables() {TRACE_IT(60009); return symbolUnscopables; }

        JavascriptFunction* GetObjectConstructor() {TRACE_IT(60010); return objectConstructor; }
        JavascriptFunction* GetArrayConstructor() {TRACE_IT(60011); return arrayConstructor; }
        JavascriptFunction* GetBooleanConstructor() {TRACE_IT(60012); return booleanConstructor; }
        JavascriptFunction* GetDateConstructor() {TRACE_IT(60013); return dateConstructor; }
        JavascriptFunction* GetFunctionConstructor() {TRACE_IT(60014); return functionConstructor; }
        JavascriptFunction* GetNumberConstructor() {TRACE_IT(60015); return numberConstructor; }
        JavascriptRegExpConstructor* GetRegExpConstructor() {TRACE_IT(60016); return regexConstructor; }
        JavascriptFunction* GetStringConstructor() {TRACE_IT(60017); return stringConstructor; }
        JavascriptFunction* GetArrayBufferConstructor() {TRACE_IT(60018); return arrayBufferConstructor; }
        JavascriptFunction* GetPixelArrayConstructor() {TRACE_IT(60019); return pixelArrayConstructor; }
        JavascriptFunction* GetTypedArrayConstructor() const {TRACE_IT(60020); return typedArrayConstructor; }
        JavascriptFunction* GetInt8ArrayConstructor() {TRACE_IT(60021); return Int8ArrayConstructor; }
        JavascriptFunction* GetUint8ArrayConstructor() {TRACE_IT(60022); return Uint8ArrayConstructor; }
        JavascriptFunction* GetUint8ClampedArrayConstructor() {TRACE_IT(60023); return Uint8ClampedArrayConstructor; }
        JavascriptFunction* GetInt16ArrayConstructor() {TRACE_IT(60024); return Int16ArrayConstructor; }
        JavascriptFunction* GetUint16ArrayConstructor() {TRACE_IT(60025); return Uint16ArrayConstructor; }
        JavascriptFunction* GetInt32ArrayConstructor() {TRACE_IT(60026); return Int32ArrayConstructor; }
        JavascriptFunction* GetUint32ArrayConstructor() {TRACE_IT(60027); return Uint32ArrayConstructor; }
        JavascriptFunction* GetFloat32ArrayConstructor() {TRACE_IT(60028); return Float32ArrayConstructor; }
        JavascriptFunction* GetFloat64ArrayConstructor() {TRACE_IT(60029); return Float64ArrayConstructor; }
        JavascriptFunction* GetMapConstructor() {TRACE_IT(60030); return mapConstructor; }
        JavascriptFunction* GetSetConstructor() {TRACE_IT(60031); return setConstructor; }
        JavascriptFunction* GetWeakMapConstructor() {TRACE_IT(60032); return weakMapConstructor; }
        JavascriptFunction* GetWeakSetConstructor() {TRACE_IT(60033); return weakSetConstructor; }
        JavascriptFunction* GetSymbolConstructor() {TRACE_IT(60034); return symbolConstructor; }
        JavascriptFunction* GetProxyConstructor() const {TRACE_IT(60035); return proxyConstructor; }
        JavascriptFunction* GetPromiseConstructor() const {TRACE_IT(60036); return promiseConstructor; }
        JavascriptFunction* GetGeneratorFunctionConstructor() const {TRACE_IT(60037); return generatorFunctionConstructor; }
        JavascriptFunction* GetAsyncFunctionConstructor() const {TRACE_IT(60038); return asyncFunctionConstructor; }

        JavascriptFunction* GetErrorConstructor() const {TRACE_IT(60039); return errorConstructor; }
        JavascriptFunction* GetEvalErrorConstructor() const {TRACE_IT(60040); return evalErrorConstructor; }
        JavascriptFunction* GetRangeErrorConstructor() const {TRACE_IT(60041); return rangeErrorConstructor; }
        JavascriptFunction* GetReferenceErrorConstructor() const {TRACE_IT(60042); return referenceErrorConstructor; }
        JavascriptFunction* GetSyntaxErrorConstructor() const {TRACE_IT(60043); return syntaxErrorConstructor; }
        JavascriptFunction* GetTypeErrorConstructor() const {TRACE_IT(60044); return typeErrorConstructor; }
        JavascriptFunction* GetURIErrorConstructor() const {TRACE_IT(60045); return uriErrorConstructor; }

        DynamicObject* GetMathObject() {TRACE_IT(60046); return mathObject; }
        DynamicObject* GetJSONObject() {TRACE_IT(60047); return JSONObject; }
#ifdef ENABLE_INTL_OBJECT
        DynamicObject* GetINTLObject() {TRACE_IT(60048); return IntlObject; }
#endif
#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_PROJECTION)
        EngineInterfaceObject* GetEngineInterfaceObject() {TRACE_IT(60049); return engineInterfaceObject; }
#endif

        DynamicObject* GetArrayPrototype() {TRACE_IT(60050); return arrayPrototype; }
        DynamicObject* GetBooleanPrototype() {TRACE_IT(60051); return booleanPrototype; }
        DynamicObject* GetDatePrototype() {TRACE_IT(60052); return datePrototype; }
        DynamicObject* GetFunctionPrototype() {TRACE_IT(60053); return functionPrototype; }
        DynamicObject* GetNumberPrototype() {TRACE_IT(60054); return numberPrototype; }
        DynamicObject* GetSIMDBool8x16Prototype()  {TRACE_IT(60055); return simdBool8x16Prototype;  }
        DynamicObject* GetSIMDBool16x8Prototype()  {TRACE_IT(60056); return simdBool16x8Prototype;  }
        DynamicObject* GetSIMDBool32x4Prototype()  {TRACE_IT(60057); return simdBool32x4Prototype;  }
        DynamicObject* GetSIMDInt8x16Prototype()   {TRACE_IT(60058); return simdInt8x16Prototype;   }
        DynamicObject* GetSIMDInt16x8Prototype()   {TRACE_IT(60059); return simdInt16x8Prototype;   }
        DynamicObject* GetSIMDInt32x4Prototype()   {TRACE_IT(60060); return simdInt32x4Prototype;   }
        DynamicObject* GetSIMDUint8x16Prototype()  {TRACE_IT(60061); return simdUint8x16Prototype;  }
        DynamicObject* GetSIMDUint16x8Prototype()  {TRACE_IT(60062); return simdUint16x8Prototype;  }
        DynamicObject* GetSIMDUint32x4Prototype()  {TRACE_IT(60063); return simdUint32x4Prototype;  }
        DynamicObject* GetSIMDFloat32x4Prototype() {TRACE_IT(60064); return simdFloat32x4Prototype; }
        DynamicObject* GetSIMDFloat64x2Prototype() {TRACE_IT(60065); return simdFloat64x2Prototype; }
        ObjectPrototypeObject* GetObjectPrototypeObject() {TRACE_IT(60066); return objectPrototype; }
        DynamicObject* GetObjectPrototype();
        DynamicObject* GetRegExpPrototype() {TRACE_IT(60067); return regexPrototype; }
        DynamicObject* GetStringPrototype() {TRACE_IT(60068); return stringPrototype; }
        DynamicObject* GetMapPrototype() {TRACE_IT(60069); return mapPrototype; }
        DynamicObject* GetSetPrototype() {TRACE_IT(60070); return setPrototype; }
        DynamicObject* GetWeakMapPrototype() {TRACE_IT(60071); return weakMapPrototype; }
        DynamicObject* GetWeakSetPrototype() {TRACE_IT(60072); return weakSetPrototype; }
        DynamicObject* GetSymbolPrototype() {TRACE_IT(60073); return symbolPrototype; }
        DynamicObject* GetArrayIteratorPrototype() const {TRACE_IT(60074); return arrayIteratorPrototype; }
        DynamicObject* GetMapIteratorPrototype() const {TRACE_IT(60075); return mapIteratorPrototype; }
        DynamicObject* GetSetIteratorPrototype() const {TRACE_IT(60076); return setIteratorPrototype; }
        DynamicObject* GetStringIteratorPrototype() const {TRACE_IT(60077); return stringIteratorPrototype; }
        DynamicObject* GetPromisePrototype() const {TRACE_IT(60078); return promisePrototype; }
        DynamicObject* GetGeneratorFunctionPrototype() const {TRACE_IT(60079); return generatorFunctionPrototype; }
        DynamicObject* GetGeneratorPrototype() const {TRACE_IT(60080); return generatorPrototype; }
        DynamicObject* GetAsyncFunctionPrototype() const {TRACE_IT(60081); return asyncFunctionPrototype; }

        DynamicObject* GetErrorPrototype() const {TRACE_IT(60082); return errorPrototype; }
        DynamicObject* GetEvalErrorPrototype() const {TRACE_IT(60083); return evalErrorPrototype; }
        DynamicObject* GetRangeErrorPrototype() const {TRACE_IT(60084); return rangeErrorPrototype; }
        DynamicObject* GetReferenceErrorPrototype() const {TRACE_IT(60085); return referenceErrorPrototype; }
        DynamicObject* GetSyntaxErrorPrototype() const {TRACE_IT(60086); return syntaxErrorPrototype; }
        DynamicObject* GetTypeErrorPrototype() const {TRACE_IT(60087); return typeErrorPrototype; }
        DynamicObject* GetURIErrorPrototype() const {TRACE_IT(60088); return uriErrorPrototype; }

    protected:
        Field(GlobalObject*) globalObject;
        Field(RuntimeFunction*) mapConstructor;
        Field(RuntimeFunction*) setConstructor;
        Field(RuntimeFunction*) weakMapConstructor;
        Field(RuntimeFunction*) weakSetConstructor;
        Field(RuntimeFunction*) arrayConstructor;
        Field(RuntimeFunction*) typedArrayConstructor;
        Field(RuntimeFunction*) Int8ArrayConstructor;
        Field(RuntimeFunction*) Uint8ArrayConstructor;
        Field(RuntimeFunction*) Uint8ClampedArrayConstructor;
        Field(RuntimeFunction*) Int16ArrayConstructor;
        Field(RuntimeFunction*) Uint16ArrayConstructor;
        Field(RuntimeFunction*) Int32ArrayConstructor;
        Field(RuntimeFunction*) Uint32ArrayConstructor;
        Field(RuntimeFunction*) Float32ArrayConstructor;
        Field(RuntimeFunction*) Float64ArrayConstructor;
        Field(RuntimeFunction*) arrayBufferConstructor;
        Field(RuntimeFunction*) dataViewConstructor;
        Field(RuntimeFunction*) booleanConstructor;
        Field(RuntimeFunction*) dateConstructor;
        Field(RuntimeFunction*) functionConstructor;
        Field(RuntimeFunction*) numberConstructor;
        Field(RuntimeFunction*) objectConstructor;
        Field(RuntimeFunction*) symbolConstructor;
        Field(JavascriptRegExpConstructor*) regexConstructor;
        Field(RuntimeFunction*) stringConstructor;
        Field(RuntimeFunction*) pixelArrayConstructor;

        Field(RuntimeFunction*) errorConstructor;
        Field(RuntimeFunction*) evalErrorConstructor;
        Field(RuntimeFunction*) rangeErrorConstructor;
        Field(RuntimeFunction*) referenceErrorConstructor;
        Field(RuntimeFunction*) syntaxErrorConstructor;
        Field(RuntimeFunction*) typeErrorConstructor;
        Field(RuntimeFunction*) uriErrorConstructor;
        Field(RuntimeFunction*) proxyConstructor;
        Field(RuntimeFunction*) promiseConstructor;
        Field(RuntimeFunction*) generatorFunctionConstructor;
        Field(RuntimeFunction*) asyncFunctionConstructor;

        Field(JavascriptFunction*) defaultAccessorFunction;
        Field(JavascriptFunction*) stackTraceAccessorFunction;
        Field(JavascriptFunction*) throwTypeErrorRestrictedPropertyAccessorFunction;
        Field(JavascriptFunction*) debugObjectNonUserGetterFunction;
        Field(JavascriptFunction*) debugObjectNonUserSetterFunction;
        Field(JavascriptFunction*) debugObjectDebugModeGetterFunction;
        Field(JavascriptFunction*) __proto__getterFunction;
        Field(JavascriptFunction*) __proto__setterFunction;
        Field(JavascriptFunction*) arrayIteratorPrototypeBuiltinNextFunction;
        Field(DynamicObject*) mathObject;
        // SIMD_JS
        Field(DynamicObject*) simdObject;

        Field(DynamicObject*) debugObject;
        Field(DynamicObject*) JSONObject;
#ifdef ENABLE_INTL_OBJECT
        Field(DynamicObject*) IntlObject;
#endif
#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_PROJECTION)
        Field(EngineInterfaceObject*) engineInterfaceObject;
#endif
        Field(DynamicObject*) reflectObject;

        Field(DynamicObject*) arrayPrototype;

        Field(DynamicObject*) typedArrayPrototype;
        Field(DynamicObject*) Int8ArrayPrototype;
        Field(DynamicObject*) Uint8ArrayPrototype;
        Field(DynamicObject*) Uint8ClampedArrayPrototype;
        Field(DynamicObject*) Int16ArrayPrototype;
        Field(DynamicObject*) Uint16ArrayPrototype;
        Field(DynamicObject*) Int32ArrayPrototype;
        Field(DynamicObject*) Uint32ArrayPrototype;
        Field(DynamicObject*) Float32ArrayPrototype;
        Field(DynamicObject*) Float64ArrayPrototype;
        Field(DynamicObject*) Int64ArrayPrototype;
        Field(DynamicObject*) Uint64ArrayPrototype;
        Field(DynamicObject*) BoolArrayPrototype;
        Field(DynamicObject*) CharArrayPrototype;
        Field(DynamicObject*) arrayBufferPrototype;
        Field(DynamicObject*) dataViewPrototype;
        Field(DynamicObject*) pixelArrayPrototype;
        Field(DynamicObject*) booleanPrototype;
        Field(DynamicObject*) datePrototype;
        Field(DynamicObject*) functionPrototype;
        Field(DynamicObject*) numberPrototype;
        Field(ObjectPrototypeObject*) objectPrototype;
        Field(DynamicObject*) regexPrototype;
        Field(DynamicObject*) stringPrototype;
        Field(DynamicObject*) mapPrototype;
        Field(DynamicObject*) setPrototype;
        Field(DynamicObject*) weakMapPrototype;
        Field(DynamicObject*) weakSetPrototype;
        Field(DynamicObject*) symbolPrototype;
        Field(DynamicObject*) iteratorPrototype;           // aka %IteratorPrototype%
        Field(DynamicObject*) arrayIteratorPrototype;
        Field(DynamicObject*) mapIteratorPrototype;
        Field(DynamicObject*) setIteratorPrototype;
        Field(DynamicObject*) stringIteratorPrototype;
        Field(DynamicObject*) promisePrototype;
        Field(DynamicObject*) generatorFunctionPrototype;  // aka %Generator%
        Field(DynamicObject*) generatorPrototype;          // aka %GeneratorPrototype%
        Field(DynamicObject*) asyncFunctionPrototype;      // aka %AsyncFunctionPrototype%

        Field(DynamicObject*) errorPrototype;
        Field(DynamicObject*) evalErrorPrototype;
        Field(DynamicObject*) rangeErrorPrototype;
        Field(DynamicObject*) referenceErrorPrototype;
        Field(DynamicObject*) syntaxErrorPrototype;
        Field(DynamicObject*) typeErrorPrototype;
        Field(DynamicObject*) uriErrorPrototype;

        //SIMD Prototypes
        Field(DynamicObject*) simdBool8x16Prototype;
        Field(DynamicObject*) simdBool16x8Prototype;
        Field(DynamicObject*) simdBool32x4Prototype;
        Field(DynamicObject*) simdInt8x16Prototype;
        Field(DynamicObject*) simdInt16x8Prototype;
        Field(DynamicObject*) simdInt32x4Prototype;
        Field(DynamicObject*) simdUint8x16Prototype;
        Field(DynamicObject*) simdUint16x8Prototype;
        Field(DynamicObject*) simdUint32x4Prototype;
        Field(DynamicObject*) simdFloat32x4Prototype;
        Field(DynamicObject*) simdFloat64x2Prototype;

        Field(JavascriptBoolean*) booleanTrue;
        Field(JavascriptBoolean*) booleanFalse;

        Field(Var) nan;
        Field(Var) negativeInfinite;
        Field(Var) positiveInfinite;
        Field(Var) pi;
        Field(Var) minValue;
        Field(Var) maxValue;
        Field(Var) negativeZero;
        Field(RecyclableObject*) undefinedValue;
        Field(RecyclableObject*) nullValue;

        Field(JavascriptSymbol*) symbolHasInstance;
        Field(JavascriptSymbol*) symbolIsConcatSpreadable;
        Field(JavascriptSymbol*) symbolIterator;
        Field(JavascriptSymbol*) symbolSpecies;
        Field(JavascriptSymbol*) symbolToPrimitive;
        Field(JavascriptSymbol*) symbolToStringTag;
        Field(JavascriptSymbol*) symbolUnscopables;

    public:
        Field(ScriptContext*) scriptContext;

    private:
        virtual void Dispose(bool isShutdown) override;
        virtual void Finalize(bool isShutdown) override;
        virtual void Mark(Recycler *recycler) override { AssertMsg(false, "Mark called on object that isn't TrackableObject"); }

    };
}
