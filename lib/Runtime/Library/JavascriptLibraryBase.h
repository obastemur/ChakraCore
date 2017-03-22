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
        {LOGMEIN("JavascriptLibraryBase.h] 21\n");
        }
        Var GetPI() {LOGMEIN("JavascriptLibraryBase.h] 23\n"); return pi; }
        Var GetNaN() {LOGMEIN("JavascriptLibraryBase.h] 24\n"); return nan; }
        Var GetNegativeInfinite() {LOGMEIN("JavascriptLibraryBase.h] 25\n"); return negativeInfinite; }
        Var GetPositiveInfinite() {LOGMEIN("JavascriptLibraryBase.h] 26\n"); return positiveInfinite; }
        Var GetMaxValue() {LOGMEIN("JavascriptLibraryBase.h] 27\n"); return maxValue; }
        Var GetMinValue() {LOGMEIN("JavascriptLibraryBase.h] 28\n"); return minValue; }
        Var GetNegativeZero() {LOGMEIN("JavascriptLibraryBase.h] 29\n"); return negativeZero; }
        RecyclableObject* GetUndefined() {LOGMEIN("JavascriptLibraryBase.h] 30\n"); return undefinedValue; }
        RecyclableObject* GetNull() {LOGMEIN("JavascriptLibraryBase.h] 31\n"); return nullValue; }
        JavascriptBoolean* GetTrue() {LOGMEIN("JavascriptLibraryBase.h] 32\n"); return booleanTrue; }
        JavascriptBoolean* GetFalse() {LOGMEIN("JavascriptLibraryBase.h] 33\n"); return booleanFalse; }

        JavascriptSymbol* GetSymbolHasInstance() {LOGMEIN("JavascriptLibraryBase.h] 35\n"); return symbolHasInstance; }
        JavascriptSymbol* GetSymbolIsConcatSpreadable() {LOGMEIN("JavascriptLibraryBase.h] 36\n"); return symbolIsConcatSpreadable; }
        JavascriptSymbol* GetSymbolIterator() {LOGMEIN("JavascriptLibraryBase.h] 37\n"); return symbolIterator; }
        JavascriptSymbol* GetSymbolToPrimitive() {LOGMEIN("JavascriptLibraryBase.h] 38\n"); return symbolToPrimitive; }
        JavascriptSymbol* GetSymbolToStringTag() {LOGMEIN("JavascriptLibraryBase.h] 39\n"); return symbolToStringTag; }
        JavascriptSymbol* GetSymbolUnscopables() {LOGMEIN("JavascriptLibraryBase.h] 40\n"); return symbolUnscopables; }

        JavascriptFunction* GetObjectConstructor() {LOGMEIN("JavascriptLibraryBase.h] 42\n"); return objectConstructor; }
        JavascriptFunction* GetArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 43\n"); return arrayConstructor; }
        JavascriptFunction* GetBooleanConstructor() {LOGMEIN("JavascriptLibraryBase.h] 44\n"); return booleanConstructor; }
        JavascriptFunction* GetDateConstructor() {LOGMEIN("JavascriptLibraryBase.h] 45\n"); return dateConstructor; }
        JavascriptFunction* GetFunctionConstructor() {LOGMEIN("JavascriptLibraryBase.h] 46\n"); return functionConstructor; }
        JavascriptFunction* GetNumberConstructor() {LOGMEIN("JavascriptLibraryBase.h] 47\n"); return numberConstructor; }
        JavascriptRegExpConstructor* GetRegExpConstructor() {LOGMEIN("JavascriptLibraryBase.h] 48\n"); return regexConstructor; }
        JavascriptFunction* GetStringConstructor() {LOGMEIN("JavascriptLibraryBase.h] 49\n"); return stringConstructor; }
        JavascriptFunction* GetArrayBufferConstructor() {LOGMEIN("JavascriptLibraryBase.h] 50\n"); return arrayBufferConstructor; }
        JavascriptFunction* GetPixelArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 51\n"); return pixelArrayConstructor; }
        JavascriptFunction* GetTypedArrayConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 52\n"); return typedArrayConstructor; }
        JavascriptFunction* GetInt8ArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 53\n"); return Int8ArrayConstructor; }
        JavascriptFunction* GetUint8ArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 54\n"); return Uint8ArrayConstructor; }
        JavascriptFunction* GetUint8ClampedArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 55\n"); return Uint8ClampedArrayConstructor; }
        JavascriptFunction* GetInt16ArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 56\n"); return Int16ArrayConstructor; }
        JavascriptFunction* GetUint16ArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 57\n"); return Uint16ArrayConstructor; }
        JavascriptFunction* GetInt32ArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 58\n"); return Int32ArrayConstructor; }
        JavascriptFunction* GetUint32ArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 59\n"); return Uint32ArrayConstructor; }
        JavascriptFunction* GetFloat32ArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 60\n"); return Float32ArrayConstructor; }
        JavascriptFunction* GetFloat64ArrayConstructor() {LOGMEIN("JavascriptLibraryBase.h] 61\n"); return Float64ArrayConstructor; }
        JavascriptFunction* GetMapConstructor() {LOGMEIN("JavascriptLibraryBase.h] 62\n"); return mapConstructor; }
        JavascriptFunction* GetSetConstructor() {LOGMEIN("JavascriptLibraryBase.h] 63\n"); return setConstructor; }
        JavascriptFunction* GetWeakMapConstructor() {LOGMEIN("JavascriptLibraryBase.h] 64\n"); return weakMapConstructor; }
        JavascriptFunction* GetWeakSetConstructor() {LOGMEIN("JavascriptLibraryBase.h] 65\n"); return weakSetConstructor; }
        JavascriptFunction* GetSymbolConstructor() {LOGMEIN("JavascriptLibraryBase.h] 66\n"); return symbolConstructor; }
        JavascriptFunction* GetProxyConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 67\n"); return proxyConstructor; }
        JavascriptFunction* GetPromiseConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 68\n"); return promiseConstructor; }
        JavascriptFunction* GetGeneratorFunctionConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 69\n"); return generatorFunctionConstructor; }
        JavascriptFunction* GetAsyncFunctionConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 70\n"); return asyncFunctionConstructor; }

        JavascriptFunction* GetErrorConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 72\n"); return errorConstructor; }
        JavascriptFunction* GetEvalErrorConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 73\n"); return evalErrorConstructor; }
        JavascriptFunction* GetRangeErrorConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 74\n"); return rangeErrorConstructor; }
        JavascriptFunction* GetReferenceErrorConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 75\n"); return referenceErrorConstructor; }
        JavascriptFunction* GetSyntaxErrorConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 76\n"); return syntaxErrorConstructor; }
        JavascriptFunction* GetTypeErrorConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 77\n"); return typeErrorConstructor; }
        JavascriptFunction* GetURIErrorConstructor() const {LOGMEIN("JavascriptLibraryBase.h] 78\n"); return uriErrorConstructor; }

        DynamicObject* GetMathObject() {LOGMEIN("JavascriptLibraryBase.h] 80\n"); return mathObject; }
        DynamicObject* GetJSONObject() {LOGMEIN("JavascriptLibraryBase.h] 81\n"); return JSONObject; }
#ifdef ENABLE_INTL_OBJECT
        DynamicObject* GetINTLObject() {LOGMEIN("JavascriptLibraryBase.h] 83\n"); return IntlObject; }
#endif
#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_PROJECTION)
        EngineInterfaceObject* GetEngineInterfaceObject() {LOGMEIN("JavascriptLibraryBase.h] 86\n"); return engineInterfaceObject; }
#endif

        DynamicObject* GetArrayPrototype() {LOGMEIN("JavascriptLibraryBase.h] 89\n"); return arrayPrototype; }
        DynamicObject* GetBooleanPrototype() {LOGMEIN("JavascriptLibraryBase.h] 90\n"); return booleanPrototype; }
        DynamicObject* GetDatePrototype() {LOGMEIN("JavascriptLibraryBase.h] 91\n"); return datePrototype; }
        DynamicObject* GetFunctionPrototype() {LOGMEIN("JavascriptLibraryBase.h] 92\n"); return functionPrototype; }
        DynamicObject* GetNumberPrototype() {LOGMEIN("JavascriptLibraryBase.h] 93\n"); return numberPrototype; }
        DynamicObject* GetSIMDBool8x16Prototype()  {LOGMEIN("JavascriptLibraryBase.h] 94\n"); return simdBool8x16Prototype;  }
        DynamicObject* GetSIMDBool16x8Prototype()  {LOGMEIN("JavascriptLibraryBase.h] 95\n"); return simdBool16x8Prototype;  }
        DynamicObject* GetSIMDBool32x4Prototype()  {LOGMEIN("JavascriptLibraryBase.h] 96\n"); return simdBool32x4Prototype;  }
        DynamicObject* GetSIMDInt8x16Prototype()   {LOGMEIN("JavascriptLibraryBase.h] 97\n"); return simdInt8x16Prototype;   }
        DynamicObject* GetSIMDInt16x8Prototype()   {LOGMEIN("JavascriptLibraryBase.h] 98\n"); return simdInt16x8Prototype;   }
        DynamicObject* GetSIMDInt32x4Prototype()   {LOGMEIN("JavascriptLibraryBase.h] 99\n"); return simdInt32x4Prototype;   }
        DynamicObject* GetSIMDUint8x16Prototype()  {LOGMEIN("JavascriptLibraryBase.h] 100\n"); return simdUint8x16Prototype;  }
        DynamicObject* GetSIMDUint16x8Prototype()  {LOGMEIN("JavascriptLibraryBase.h] 101\n"); return simdUint16x8Prototype;  }
        DynamicObject* GetSIMDUint32x4Prototype()  {LOGMEIN("JavascriptLibraryBase.h] 102\n"); return simdUint32x4Prototype;  }
        DynamicObject* GetSIMDFloat32x4Prototype() {LOGMEIN("JavascriptLibraryBase.h] 103\n"); return simdFloat32x4Prototype; }
        DynamicObject* GetSIMDFloat64x2Prototype() {LOGMEIN("JavascriptLibraryBase.h] 104\n"); return simdFloat64x2Prototype; }
        ObjectPrototypeObject* GetObjectPrototypeObject() {LOGMEIN("JavascriptLibraryBase.h] 105\n"); return objectPrototype; }
        DynamicObject* GetObjectPrototype();
        DynamicObject* GetRegExpPrototype() {LOGMEIN("JavascriptLibraryBase.h] 107\n"); return regexPrototype; }
        DynamicObject* GetStringPrototype() {LOGMEIN("JavascriptLibraryBase.h] 108\n"); return stringPrototype; }
        DynamicObject* GetMapPrototype() {LOGMEIN("JavascriptLibraryBase.h] 109\n"); return mapPrototype; }
        DynamicObject* GetSetPrototype() {LOGMEIN("JavascriptLibraryBase.h] 110\n"); return setPrototype; }
        DynamicObject* GetWeakMapPrototype() {LOGMEIN("JavascriptLibraryBase.h] 111\n"); return weakMapPrototype; }
        DynamicObject* GetWeakSetPrototype() {LOGMEIN("JavascriptLibraryBase.h] 112\n"); return weakSetPrototype; }
        DynamicObject* GetSymbolPrototype() {LOGMEIN("JavascriptLibraryBase.h] 113\n"); return symbolPrototype; }
        DynamicObject* GetArrayIteratorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 114\n"); return arrayIteratorPrototype; }
        DynamicObject* GetMapIteratorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 115\n"); return mapIteratorPrototype; }
        DynamicObject* GetSetIteratorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 116\n"); return setIteratorPrototype; }
        DynamicObject* GetStringIteratorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 117\n"); return stringIteratorPrototype; }
        DynamicObject* GetPromisePrototype() const {LOGMEIN("JavascriptLibraryBase.h] 118\n"); return promisePrototype; }
        DynamicObject* GetGeneratorFunctionPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 119\n"); return generatorFunctionPrototype; }
        DynamicObject* GetGeneratorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 120\n"); return generatorPrototype; }
        DynamicObject* GetAsyncFunctionPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 121\n"); return asyncFunctionPrototype; }

        DynamicObject* GetErrorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 123\n"); return errorPrototype; }
        DynamicObject* GetEvalErrorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 124\n"); return evalErrorPrototype; }
        DynamicObject* GetRangeErrorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 125\n"); return rangeErrorPrototype; }
        DynamicObject* GetReferenceErrorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 126\n"); return referenceErrorPrototype; }
        DynamicObject* GetSyntaxErrorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 127\n"); return syntaxErrorPrototype; }
        DynamicObject* GetTypeErrorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 128\n"); return typeErrorPrototype; }
        DynamicObject* GetURIErrorPrototype() const {LOGMEIN("JavascriptLibraryBase.h] 129\n"); return uriErrorPrototype; }

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
