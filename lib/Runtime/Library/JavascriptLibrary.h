//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#define InlineSlotCountIncrement (HeapConstants::ObjectGranularity / sizeof(Var))

#define MaxPreInitializedObjectTypeInlineSlotCount 16
#define MaxPreInitializedObjectHeaderInlinedTypeInlineSlotCount \
    (Js::DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity() + MaxPreInitializedObjectTypeInlineSlotCount)
#define PreInitializedObjectTypeCount ((MaxPreInitializedObjectTypeInlineSlotCount / InlineSlotCountIncrement) + 1)
CompileAssert(MaxPreInitializedObjectTypeInlineSlotCount <= USHRT_MAX);

class ScriptSite;
class ActiveScriptExternalLibrary;
class ProjectionExternalLibrary;
class EditAndContinue;
class ChakraHostScriptContext;

#ifdef ENABLE_PROJECTION
namespace Projection
{
    class ProjectionContext;
    class WinRTPromiseEngineInterfaceExtensionObject;
}
#endif

namespace Js
{
    static const unsigned int EvalMRUSize = 15;
    typedef JsUtil::BaseDictionary<DWORD_PTR, SourceContextInfo *, Recycler, PowerOf2SizePolicy> SourceContextInfoMap;
    typedef JsUtil::BaseDictionary<uint, SourceContextInfo *, Recycler, PowerOf2SizePolicy> DynamicSourceContextInfoMap;

    typedef JsUtil::BaseDictionary<EvalMapString, ScriptFunction*, RecyclerNonLeafAllocator, PrimeSizePolicy> SecondLevelEvalCache;
    typedef TwoLevelHashRecord<FastEvalMapString, ScriptFunction*, SecondLevelEvalCache, EvalMapString> EvalMapRecord;
    typedef JsUtil::Cache<FastEvalMapString, EvalMapRecord*, RecyclerNonLeafAllocator, PrimeSizePolicy, JsUtil::MRURetentionPolicy<FastEvalMapString, EvalMRUSize>, FastEvalMapStringComparer> EvalCacheTopLevelDictionary;
    typedef JsUtil::Cache<EvalMapString, FunctionInfo*, RecyclerNonLeafAllocator, PrimeSizePolicy, JsUtil::MRURetentionPolicy<EvalMapString, EvalMRUSize>> NewFunctionCache;
    typedef JsUtil::BaseDictionary<ParseableFunctionInfo*, ParseableFunctionInfo*, Recycler, PrimeSizePolicy, RecyclerPointerComparer> ParseableFunctionInfoMap;
    // This is the dictionary used by script context to cache the eval.
    typedef TwoLevelHashDictionary<FastEvalMapString, ScriptFunction*, EvalMapRecord, EvalCacheTopLevelDictionary, EvalMapString> EvalCacheDictionary;

    typedef JsUtil::BaseDictionary<JavascriptMethod, JavascriptFunction*, Recycler, PowerOf2SizePolicy> BuiltInLibraryFunctionMap;

    // valid if object!= NULL
    struct EnumeratedObjectCache 
    {
        static const int kMaxCachedPropStrings = 16;
        Field(DynamicObject*) object;
        Field(DynamicType*) type;
        Field(PropertyString*) propertyStrings[kMaxCachedPropStrings];
        Field(int) validPropStrings;
    };

    struct Cache
    {
        Field(JavascriptString *) lastNumberToStringRadix10String;
        Field(EnumeratedObjectCache) enumObjCache;
        Field(JavascriptString *) lastUtcTimeFromStrString;
        Field(EvalCacheDictionary*) evalCacheDictionary;
        Field(EvalCacheDictionary*) indirectEvalCacheDictionary;
        Field(NewFunctionCache*) newFunctionCache;
        Field(RegexPatternMruMap *) dynamicRegexMap;
        Field(SourceContextInfoMap*) sourceContextInfoMap;   // maps host provided context cookie to the URL of the script buffer passed.
        Field(DynamicSourceContextInfoMap*) dynamicSourceContextInfoMap;
        Field(SourceContextInfo*) noContextSourceContextInfo;
        Field(SRCINFO*) noContextGlobalSourceInfo;
        Field(Field(SRCINFO const *)*) moduleSrcInfo;
        Field(BuiltInLibraryFunctionMap*) builtInLibraryFunctions;
#if ENABLE_PROFILE_INFO
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        Field(DynamicProfileInfoList*) profileInfoList;
#endif
#endif
    };

    class MissingPropertyTypeHandler;
    class SourceTextModuleRecord;
    class ArrayBufferBase;
    class SharedContents;
    typedef RecyclerFastAllocator<JavascriptNumber, LeafBit> RecyclerJavascriptNumberAllocator;
    typedef JsUtil::List<Var, Recycler> ListForListIterator;

    class UndeclaredBlockVariable : public RecyclableObject
    {
        friend class JavascriptLibrary;
        UndeclaredBlockVariable(Type* type) : RecyclableObject(type) {LOGMEIN("JavascriptLibrary.h] 86\n"); }
    };

    class SourceTextModuleRecord;
    typedef JsUtil::List<SourceTextModuleRecord*> ModuleRecordList;

#if ENABLE_COPYONACCESS_ARRAY
    struct CacheForCopyOnAccessArraySegments
    {
        static const uint32 MAX_SIZE = 31;
        Field(SparseArraySegment<int32> *) cache[MAX_SIZE];
        Field(uint32) count;

        uint32 AddSegment(SparseArraySegment<int32> *segment)
        {LOGMEIN("JavascriptLibrary.h] 100\n");
            cache[count++] = segment;
            return count;
        }

        SparseArraySegment<int32> *GetSegmentByIndex(byte index)
        {LOGMEIN("JavascriptLibrary.h] 106\n");
            Assert(index <= MAX_SIZE);
            return cache[index - 1];
        }

        bool IsNotOverHardLimit()
        {LOGMEIN("JavascriptLibrary.h] 112\n");
            return count < MAX_SIZE;
        }

        bool IsNotFull()
        {LOGMEIN("JavascriptLibrary.h] 117\n");
            return count < (uint32) CONFIG_FLAG(CopyOnAccessArraySegmentCacheSize);
        }

        bool IsValidIndex(uint32 index)
        {LOGMEIN("JavascriptLibrary.h] 122\n");
            return count && index && index <= count;
        }

#if ENABLE_DEBUG_CONFIG_OPTIONS
        uint32 GetCount()
        {LOGMEIN("JavascriptLibrary.h] 128\n");
            return count;
        }
#endif
    };
#endif

    template <typename T>
    struct StringTemplateCallsiteObjectComparer
    {
        static bool Equals(T x, T y)
        {
            static_assert(false, "Unexpected type T");
        }
        static hash_t GetHashCode(T i)
        {
            static_assert(false, "Unexpected type T");
        }
    };

    template <>
    struct StringTemplateCallsiteObjectComparer<ParseNodePtr>
    {
        static bool Equals(ParseNodePtr x, RecyclerWeakReference<Js::RecyclableObject>* y);
        static bool Equals(ParseNodePtr x, ParseNodePtr y);
        static hash_t GetHashCode(ParseNodePtr i);
    };

    template <>
    struct StringTemplateCallsiteObjectComparer<RecyclerWeakReference<Js::RecyclableObject>*>
    {
        static bool Equals(RecyclerWeakReference<Js::RecyclableObject>* x, RecyclerWeakReference<Js::RecyclableObject>* y);
        static bool Equals(RecyclerWeakReference<Js::RecyclableObject>* x, ParseNodePtr y);
        static hash_t GetHashCode(RecyclerWeakReference<Js::RecyclableObject>* o);
    };

    class JavascriptLibrary : public JavascriptLibraryBase
    {
        friend class EditAndContinue;
        friend class ScriptSite;
        friend class GlobalObject;
        friend class ScriptContext;
        friend class EngineInterfaceObject;
        friend class ExternalLibraryBase;
        friend class ActiveScriptExternalLibrary;
        friend class IntlEngineInterfaceExtensionObject;
        friend class ChakraHostScriptContext;
#ifdef ENABLE_PROJECTION
        friend class ProjectionExternalLibrary;
        friend class Projection::WinRTPromiseEngineInterfaceExtensionObject;
        friend class Projection::ProjectionContext;
#endif
        static const char16* domBuiltinPropertyNames[];

    public:
#if ENABLE_COPYONACCESS_ARRAY
        Field(CacheForCopyOnAccessArraySegments *) cacheForCopyOnAccessArraySegments;
#endif

        static DWORD GetScriptContextOffset() {LOGMEIN("JavascriptLibrary.h] 187\n"); return offsetof(JavascriptLibrary, scriptContext); }
        static DWORD GetUndeclBlockVarOffset() {LOGMEIN("JavascriptLibrary.h] 188\n"); return offsetof(JavascriptLibrary, undeclBlockVarSentinel); }
        static DWORD GetEmptyStringOffset() {LOGMEIN("JavascriptLibrary.h] 189\n"); return offsetof(JavascriptLibrary, emptyString); }
        static DWORD GetUndefinedValueOffset() {LOGMEIN("JavascriptLibrary.h] 190\n"); return offsetof(JavascriptLibrary, undefinedValue); }
        static DWORD GetNullValueOffset() {LOGMEIN("JavascriptLibrary.h] 191\n"); return offsetof(JavascriptLibrary, nullValue); }
        static DWORD GetBooleanTrueOffset() {LOGMEIN("JavascriptLibrary.h] 192\n"); return offsetof(JavascriptLibrary, booleanTrue); }
        static DWORD GetBooleanFalseOffset() {LOGMEIN("JavascriptLibrary.h] 193\n"); return offsetof(JavascriptLibrary, booleanFalse); }
        static DWORD GetNegativeZeroOffset() {LOGMEIN("JavascriptLibrary.h] 194\n"); return offsetof(JavascriptLibrary, negativeZero); }
        static DWORD GetNumberTypeStaticOffset() {LOGMEIN("JavascriptLibrary.h] 195\n"); return offsetof(JavascriptLibrary, numberTypeStatic); }
        static DWORD GetStringTypeStaticOffset() {LOGMEIN("JavascriptLibrary.h] 196\n"); return offsetof(JavascriptLibrary, stringTypeStatic); }
        static DWORD GetObjectTypesOffset() {LOGMEIN("JavascriptLibrary.h] 197\n"); return offsetof(JavascriptLibrary, objectTypes); }
        static DWORD GetObjectHeaderInlinedTypesOffset() {LOGMEIN("JavascriptLibrary.h] 198\n"); return offsetof(JavascriptLibrary, objectHeaderInlinedTypes); }
        static DWORD GetRegexTypeOffset() {LOGMEIN("JavascriptLibrary.h] 199\n"); return offsetof(JavascriptLibrary, regexType); }
        static DWORD GetArrayConstructorOffset() {LOGMEIN("JavascriptLibrary.h] 200\n"); return offsetof(JavascriptLibrary, arrayConstructor); }
        static DWORD GetPositiveInfinityOffset() {LOGMEIN("JavascriptLibrary.h] 201\n"); return offsetof(JavascriptLibrary, positiveInfinite); }
        static DWORD GetNaNOffset() {LOGMEIN("JavascriptLibrary.h] 202\n"); return offsetof(JavascriptLibrary, nan); }
        static DWORD GetNativeIntArrayTypeOffset() {LOGMEIN("JavascriptLibrary.h] 203\n"); return offsetof(JavascriptLibrary, nativeIntArrayType); }
#if ENABLE_COPYONACCESS_ARRAY
        static DWORD GetCopyOnAccessNativeIntArrayTypeOffset() {LOGMEIN("JavascriptLibrary.h] 205\n"); return offsetof(JavascriptLibrary, copyOnAccessNativeIntArrayType); }
#endif
        static DWORD GetNativeFloatArrayTypeOffset() {LOGMEIN("JavascriptLibrary.h] 207\n"); return offsetof(JavascriptLibrary, nativeFloatArrayType); }
        static DWORD GetVTableAddressesOffset() {LOGMEIN("JavascriptLibrary.h] 208\n"); return offsetof(JavascriptLibrary, vtableAddresses); }
        static DWORD GetConstructorCacheDefaultInstanceOffset() {LOGMEIN("JavascriptLibrary.h] 209\n"); return offsetof(JavascriptLibrary, constructorCacheDefaultInstance); }
        static DWORD GetAbsDoubleCstOffset() {LOGMEIN("JavascriptLibrary.h] 210\n"); return offsetof(JavascriptLibrary, absDoubleCst); }
        static DWORD GetUintConvertConstOffset() {LOGMEIN("JavascriptLibrary.h] 211\n"); return offsetof(JavascriptLibrary, uintConvertConst); }
        static DWORD GetBuiltinFunctionsOffset() {LOGMEIN("JavascriptLibrary.h] 212\n"); return offsetof(JavascriptLibrary, builtinFunctions); }
        static DWORD GetCharStringCacheOffset() {LOGMEIN("JavascriptLibrary.h] 213\n"); return offsetof(JavascriptLibrary, charStringCache); }
        static DWORD GetCharStringCacheAOffset() {LOGMEIN("JavascriptLibrary.h] 214\n"); return GetCharStringCacheOffset() + CharStringCache::GetCharStringCacheAOffset(); }
        const  JavascriptLibraryBase* GetLibraryBase() const {LOGMEIN("JavascriptLibrary.h] 215\n"); return static_cast<const JavascriptLibraryBase*>(this); }
        void SetGlobalObject(GlobalObject* globalObject) {LOGMEIN("JavascriptLibrary.h] 216\n");this->globalObject = globalObject; }
        static DWORD GetRandSeed0Offset() {LOGMEIN("JavascriptLibrary.h] 217\n"); return offsetof(JavascriptLibrary, randSeed0); }
        static DWORD GetRandSeed1Offset() {LOGMEIN("JavascriptLibrary.h] 218\n"); return offsetof(JavascriptLibrary, randSeed1); }
        static DWORD GetTypeDisplayStringsOffset() {LOGMEIN("JavascriptLibrary.h] 219\n"); return offsetof(JavascriptLibrary, typeDisplayStrings); }
        typedef bool (CALLBACK *PromiseContinuationCallback)(Var task, void *callbackState);

        Var GetUndeclBlockVar() const {LOGMEIN("JavascriptLibrary.h] 222\n"); return undeclBlockVarSentinel; }
        bool IsUndeclBlockVar(Var var) const {LOGMEIN("JavascriptLibrary.h] 223\n"); return var == undeclBlockVarSentinel; }

        static bool IsTypedArrayConstructor(Var constructor, ScriptContext* scriptContext);

    private:
        FieldNoBarrier(Recycler *) recycler;
        Field(ExternalLibraryBase*) externalLibraryList;

        Field(UndeclaredBlockVariable*) undeclBlockVarSentinel;

        Field(DynamicType *) generatorConstructorPrototypeObjectType;
        Field(DynamicType *) constructorPrototypeObjectType;
        Field(DynamicType *) heapArgumentsType;
        Field(DynamicType *) activationObjectType;
        Field(DynamicType *) arrayType;
        Field(DynamicType *) nativeIntArrayType;
#if ENABLE_COPYONACCESS_ARRAY
        Field(DynamicType *) copyOnAccessNativeIntArrayType;
#endif
        Field(DynamicType *) nativeFloatArrayType;
        Field(DynamicType *) arrayBufferType;
        Field(DynamicType *) sharedArrayBufferType;
        Field(DynamicType *) dataViewType;
        Field(DynamicType *) typedArrayType;
        Field(DynamicType *) int8ArrayType;
        Field(DynamicType *) uint8ArrayType;
        Field(DynamicType *) uint8ClampedArrayType;
        Field(DynamicType *) int16ArrayType;
        Field(DynamicType *) uint16ArrayType;
        Field(DynamicType *) int32ArrayType;
        Field(DynamicType *) uint32ArrayType;
        Field(DynamicType *) float32ArrayType;
        Field(DynamicType *) float64ArrayType;
        Field(DynamicType *) int64ArrayType;
        Field(DynamicType *) uint64ArrayType;
        Field(DynamicType *) boolArrayType;
        Field(DynamicType *) charArrayType;
        Field(StaticType *) booleanTypeStatic;
        Field(DynamicType *) booleanTypeDynamic;
        Field(DynamicType *) dateType;
        Field(StaticType *) variantDateType;
        Field(DynamicType *) symbolTypeDynamic;
        Field(StaticType *) symbolTypeStatic;
        Field(DynamicType *) iteratorResultType;
        Field(DynamicType *) arrayIteratorType;
        Field(DynamicType *) mapIteratorType;
        Field(DynamicType *) setIteratorType;
        Field(DynamicType *) stringIteratorType;
        Field(DynamicType *) promiseType;
        Field(DynamicType *) listIteratorType;

        Field(JavascriptFunction*) builtinFunctions[BuiltinFunction::Count];

        Field(INT_PTR) vtableAddresses[VTableValue::Count];
        Field(JavascriptString*) typeDisplayStrings[TypeIds_Limit];
        Field(ConstructorCache *) constructorCacheDefaultInstance;
        __declspec(align(16)) Field(const BYTE *) absDoubleCst;
        Field(double const *) uintConvertConst;

        // Function Types
        Field(DynamicTypeHandler *) anonymousFunctionTypeHandler;
        Field(DynamicTypeHandler *) anonymousFunctionWithPrototypeTypeHandler;
        Field(DynamicTypeHandler *) functionTypeHandler;
        Field(DynamicTypeHandler *) functionWithPrototypeTypeHandler;
        Field(DynamicType *) externalFunctionWithDeferredPrototypeType;
        Field(DynamicType *) wrappedFunctionWithDeferredPrototypeType;
        Field(DynamicType *) stdCallFunctionWithDeferredPrototypeType;
        Field(DynamicType *) idMappedFunctionWithPrototypeType;
        Field(DynamicType *) externalConstructorFunctionWithDeferredPrototypeType;
        Field(DynamicType *) defaultExternalConstructorFunctionWithDeferredPrototypeType;
        Field(DynamicType *) boundFunctionType;
        Field(DynamicType *) regexConstructorType;
        Field(DynamicType *) crossSiteDeferredPrototypeFunctionType;
        Field(DynamicType *) crossSiteIdMappedFunctionWithPrototypeType;
        Field(DynamicType *) crossSiteExternalConstructFunctionWithPrototypeType;

        Field(StaticType  *) enumeratorType;
        Field(DynamicType *) errorType;
        Field(DynamicType *) evalErrorType;
        Field(DynamicType *) rangeErrorType;
        Field(DynamicType *) referenceErrorType;
        Field(DynamicType *) syntaxErrorType;
        Field(DynamicType *) typeErrorType;
        Field(DynamicType *) uriErrorType;
        Field(DynamicType *) webAssemblyCompileErrorType;
        Field(DynamicType *) webAssemblyRuntimeErrorType;
        Field(DynamicType *) webAssemblyLinkErrorType;
        Field(StaticType  *) numberTypeStatic;
        Field(StaticType  *) int64NumberTypeStatic;
        Field(StaticType  *) uint64NumberTypeStatic;

        Field(DynamicType *) webAssemblyModuleType;
        Field(DynamicType *) webAssemblyInstanceType;
        Field(DynamicType *) webAssemblyMemoryType;
        Field(DynamicType *) webAssemblyTableType;

        // SIMD_JS
        Field(DynamicType *) simdBool8x16TypeDynamic;
        Field(DynamicType *) simdBool16x8TypeDynamic;
        Field(DynamicType *) simdBool32x4TypeDynamic;
        Field(DynamicType *) simdInt8x16TypeDynamic;
        Field(DynamicType *) simdInt16x8TypeDynamic;
        Field(DynamicType *) simdInt32x4TypeDynamic;
        Field(DynamicType *) simdUint8x16TypeDynamic;
        Field(DynamicType *) simdUint16x8TypeDynamic;
        Field(DynamicType *) simdUint32x4TypeDynamic;
        Field(DynamicType *) simdFloat32x4TypeDynamic;

        Field(StaticType *) simdFloat32x4TypeStatic;
        Field(StaticType *) simdInt32x4TypeStatic;
        Field(StaticType *) simdInt8x16TypeStatic;
        Field(StaticType *) simdFloat64x2TypeStatic;
        Field(StaticType *) simdInt16x8TypeStatic;
        Field(StaticType *) simdBool32x4TypeStatic;
        Field(StaticType *) simdBool16x8TypeStatic;
        Field(StaticType *) simdBool8x16TypeStatic;
        Field(StaticType *) simdUint32x4TypeStatic;
        Field(StaticType *) simdUint16x8TypeStatic;
        Field(StaticType *) simdUint8x16TypeStatic;

        Field(DynamicType *) numberTypeDynamic;
        Field(DynamicType *) objectTypes[PreInitializedObjectTypeCount];
        Field(DynamicType *) objectHeaderInlinedTypes[PreInitializedObjectTypeCount];
        Field(DynamicType *) regexPrototypeType;
        Field(DynamicType *) regexType;
        Field(DynamicType *) regexResultType;
        Field(StaticType  *) stringTypeStatic;
        Field(DynamicType *) stringTypeDynamic;
        Field(DynamicType *) mapType;
        Field(DynamicType *) setType;
        Field(DynamicType *) weakMapType;
        Field(DynamicType *) weakSetType;
        Field(DynamicType *) proxyType;
        Field(StaticType  *) withType;
        Field(DynamicType *) SpreadArgumentType;
        Field(DynamicType *) moduleNamespaceType;
        Field(PropertyDescriptor) defaultPropertyDescriptor;

        Field(JavascriptString*) nullString;
        Field(JavascriptString*) emptyString;
        Field(JavascriptString*) quotesString;
        Field(JavascriptString*) whackString;
        Field(JavascriptString*) objectDisplayString;
        Field(JavascriptString*) stringTypeDisplayString;
        Field(JavascriptString*) errorDisplayString;
        Field(JavascriptString*) functionPrefixString;
        Field(JavascriptString*) generatorFunctionPrefixString;
        Field(JavascriptString*) asyncFunctionPrefixString;
        Field(JavascriptString*) functionDisplayString;
        Field(JavascriptString*) xDomainFunctionDisplayString;
        Field(JavascriptString*) undefinedDisplayString;
        Field(JavascriptString*) nanDisplayString;
        Field(JavascriptString*) nullDisplayString;
        Field(JavascriptString*) unknownDisplayString;
        Field(JavascriptString*) commaDisplayString;
        Field(JavascriptString*) commaSpaceDisplayString;
        Field(JavascriptString*) trueDisplayString;
        Field(JavascriptString*) falseDisplayString;
        Field(JavascriptString*) lengthDisplayString;
        Field(JavascriptString*) invalidDateString;
        Field(JavascriptString*) objectTypeDisplayString;
        Field(JavascriptString*) functionTypeDisplayString;
        Field(JavascriptString*) booleanTypeDisplayString;
        Field(JavascriptString*) numberTypeDisplayString;
        Field(JavascriptString*) moduleTypeDisplayString;
        Field(JavascriptString*) variantDateTypeDisplayString;

        // SIMD_JS
        Field(JavascriptString*) simdFloat32x4DisplayString;
        Field(JavascriptString*) simdFloat64x2DisplayString;
        Field(JavascriptString*) simdInt32x4DisplayString;
        Field(JavascriptString*) simdInt16x8DisplayString;
        Field(JavascriptString*) simdInt8x16DisplayString;
        Field(JavascriptString*) simdBool32x4DisplayString;
        Field(JavascriptString*) simdBool16x8DisplayString;
        Field(JavascriptString*) simdBool8x16DisplayString;
        Field(JavascriptString*) simdUint32x4DisplayString;
        Field(JavascriptString*) simdUint16x8DisplayString;
        Field(JavascriptString*) simdUint8x16DisplayString;



        Field(JavascriptString*) symbolTypeDisplayString;
        Field(JavascriptString*) debuggerDeadZoneBlockVariableString;

        Field(DynamicObject*) missingPropertyHolder;

        Field(StaticType*) throwErrorObjectType;

        Field(PropertyStringCacheMap*) propertyStringMap;

        Field(ConstructorCache*) builtInConstructorCache;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        Field(JavascriptFunction*) debugObjectFaultInjectionCookieGetterFunction;
        Field(JavascriptFunction*) debugObjectFaultInjectionCookieSetterFunction;
#endif

        Field(JavascriptFunction*) evalFunctionObject;
        Field(JavascriptFunction*) arrayPrototypeValuesFunction;
        Field(JavascriptFunction*) parseIntFunctionObject;
        Field(JavascriptFunction*) parseFloatFunctionObject;
        Field(JavascriptFunction*) arrayPrototypeToStringFunction;
        Field(JavascriptFunction*) arrayPrototypeToLocaleStringFunction;
        Field(JavascriptFunction*) identityFunction;
        Field(JavascriptFunction*) throwerFunction;
        Field(JavascriptFunction*) promiseResolveFunction;
        Field(JavascriptFunction*) generatorNextFunction;
        Field(JavascriptFunction*) generatorThrowFunction;

        Field(JavascriptFunction*) objectValueOfFunction;
        Field(JavascriptFunction*) objectToStringFunction;

#ifdef ENABLE_WASM
        Field(DynamicObject*) webAssemblyObject;
#endif

        // SIMD_JS
        Field(JavascriptFunction*) simdFloat32x4ToStringFunction;
        Field(JavascriptFunction*) simdFloat64x2ToStringFunction;
        Field(JavascriptFunction*) simdInt32x4ToStringFunction;
        Field(JavascriptFunction*) simdInt16x8ToStringFunction;
        Field(JavascriptFunction*) simdInt8x16ToStringFunction;
        Field(JavascriptFunction*) simdBool32x4ToStringFunction;
        Field(JavascriptFunction*) simdBool16x8ToStringFunction;
        Field(JavascriptFunction*) simdBool8x16ToStringFunction;
        Field(JavascriptFunction*) simdUint32x4ToStringFunction;
        Field(JavascriptFunction*) simdUint16x8ToStringFunction;
        Field(JavascriptFunction*) simdUint8x16ToStringFunction;



        Field(JavascriptSymbol*) symbolMatch;
        Field(JavascriptSymbol*) symbolReplace;
        Field(JavascriptSymbol*) symbolSearch;
        Field(JavascriptSymbol*) symbolSplit;

        Field(UnifiedRegex::RegexPattern *) emptyRegexPattern;
        Field(JavascriptFunction*) regexExecFunction;
        Field(JavascriptFunction*) regexFlagsGetterFunction;
        Field(JavascriptFunction*) regexGlobalGetterFunction;
        Field(JavascriptFunction*) regexStickyGetterFunction;
        Field(JavascriptFunction*) regexUnicodeGetterFunction;

        Field(RuntimeFunction*) sharedArrayBufferConstructor;
        Field(DynamicObject*) sharedArrayBufferPrototype;
        Field(DynamicObject*) atomicsObject;

        Field(DynamicObject*) webAssemblyCompileErrorPrototype;
        Field(RuntimeFunction*) webAssemblyCompileErrorConstructor;
        Field(DynamicObject*) webAssemblyRuntimeErrorPrototype;
        Field(RuntimeFunction*) webAssemblyRuntimeErrorConstructor;
        Field(DynamicObject*) webAssemblyLinkErrorPrototype;
        Field(RuntimeFunction*) webAssemblyLinkErrorConstructor;

        Field(DynamicObject*) webAssemblyMemoryPrototype;
        Field(RuntimeFunction*) webAssemblyMemoryConstructor;
        Field(DynamicObject*) webAssemblyModulePrototype;
        Field(RuntimeFunction*) webAssemblyModuleConstructor;
        Field(DynamicObject*) webAssemblyInstancePrototype;
        Field(RuntimeFunction*) webAssemblyInstanceConstructor;
        Field(DynamicObject*) webAssemblyTablePrototype;
        Field(RuntimeFunction*) webAssemblyTableConstructor;

        Field(int) regexConstructorSlotIndex;
        Field(int) regexExecSlotIndex;
        Field(int) regexFlagsGetterSlotIndex;
        Field(int) regexGlobalGetterSlotIndex;
        Field(int) regexStickyGetterSlotIndex;
        Field(int) regexUnicodeGetterSlotIndex;

        mutable Field(CharStringCache) charStringCache;

        FieldNoBarrier(PromiseContinuationCallback) nativeHostPromiseContinuationFunction;
        Field(void *) nativeHostPromiseContinuationFunctionState;

        typedef SList<Js::FunctionProxy*, Recycler> FunctionReferenceList;
        typedef JsUtil::WeakReferenceDictionary<uintptr_t, DynamicType, DictionarySizePolicy<PowerOf2Policy, 1>> JsrtExternalTypesCache;

        Field(void *) bindRefChunkBegin;
        Field(Field(void*)*) bindRefChunkCurrent;
        Field(Field(void*)*) bindRefChunkEnd;
        Field(TypePath*) rootPath;         // this should be in library instead of ScriptContext::Cache
        Field(Js::Cache) cache;
        Field(FunctionReferenceList*) dynamicFunctionReference;
        Field(uint) dynamicFunctionReferenceDepth;
        Field(FinalizableObject*) jsrtContextObject;
        Field(JsrtExternalTypesCache*) jsrtExternalTypesCache;
        Field(FunctionBody*) fakeGlobalFuncForUndefer;

        typedef JsUtil::BaseHashSet<RecyclerWeakReference<RecyclableObject>*, Recycler, PowerOf2SizePolicy, RecyclerWeakReference<RecyclableObject>*, StringTemplateCallsiteObjectComparer> StringTemplateCallsiteObjectList;

        // Used to store a list of template callsite objects.
        // We use the raw strings in the callsite object (or a string template parse node) to identify unique callsite objects in the list.
        // See abstract operation GetTemplateObject in ES6 Spec (RC1) 12.2.8.3
        Field(StringTemplateCallsiteObjectList*) stringTemplateCallsiteObjectList;

        Field(ModuleRecordList*) moduleRecordList;

        // This list contains types ensured to have only writable data properties in it and all objects in its prototype chain
        // (i.e., no readonly properties or accessors). Only prototype objects' types are stored in the list. When something
        // in the script context adds a readonly property or accessor to an object that is used as a prototype object, this
        // list is cleared. The list is also cleared before garbage collection so that it does not keep growing, and so, it can
        // hold strong references to the types.
        //
        // The cache is used by the type-without-property local inline cache. When setting a property on a type that doesn't
        // have the property, to determine whether to promote the object like an object of that type was last promoted, we need
        // to ensure that objects in the prototype chain have not acquired a readonly property or setter (ideally, only for that
        // property ID, but we just check for any such property). This cache is used to avoid doing this many times, especially
        // when the prototype chain is not short.
        //
        // This list is only used to invalidate the status of types. The type itself contains a boolean indicating whether it
        // and prototypes contain only writable data properties, which is reset upon invalidating the status.
        Field(JsUtil::List<Type *> *) typesEnsuredToHaveOnlyWritableDataPropertiesInItAndPrototypeChain;

        Field(uint64) randSeed0, randSeed1;
        Field(bool) isPRNGSeeded;
        Field(bool) inProfileMode;
        Field(bool) inDispatchProfileMode;
        Field(bool) arrayObjectHasUserDefinedSpecies;

        JavascriptFunction * AddFunctionToLibraryObjectWithPrototype(DynamicObject * object, PropertyId propertyId, FunctionInfo * functionInfo, int length, DynamicObject * prototype = nullptr, DynamicType * functionType = nullptr);
        JavascriptFunction * AddFunctionToLibraryObject(DynamicObject* object, PropertyId propertyId, FunctionInfo * functionInfo, int length, PropertyAttributes attributes = PropertyBuiltInMethodDefaults);

        JavascriptFunction * AddFunctionToLibraryObjectWithName(DynamicObject* object, PropertyId propertyId, PropertyId nameId, FunctionInfo * functionInfo, int length);
        RuntimeFunction* AddGetterToLibraryObject(DynamicObject* object, PropertyId propertyId, FunctionInfo* functionInfo);
        void AddAccessorsToLibraryObject(DynamicObject* object, PropertyId propertyId, FunctionInfo * getterFunctionInfo, FunctionInfo * setterFunctionInfo);
        void AddAccessorsToLibraryObject(DynamicObject* object, PropertyId propertyId, RecyclableObject * getterFunction, RecyclableObject * setterFunction);
        void AddAccessorsToLibraryObjectWithName(DynamicObject* object, PropertyId propertyId, PropertyId nameId, FunctionInfo * getterFunctionInfo, FunctionInfo * setterFunction);
        RuntimeFunction * CreateGetterFunction(PropertyId nameId, FunctionInfo* functionInfo);
        RuntimeFunction * CreateSetterFunction(PropertyId nameId, FunctionInfo* functionInfo);

        template <size_t N>
        JavascriptFunction * AddFunctionToLibraryObjectWithPropertyName(DynamicObject* object, const char16(&propertyName)[N], FunctionInfo * functionInfo, int length);

        static SimpleTypeHandler<1> SharedPrototypeTypeHandler;
        static SimpleTypeHandler<1> SharedFunctionWithoutPrototypeTypeHandler;
        static SimpleTypeHandler<1> SharedFunctionWithPrototypeTypeHandlerV11;
        static SimpleTypeHandler<2> SharedFunctionWithPrototypeTypeHandler;
        static SimpleTypeHandler<1> SharedFunctionWithLengthTypeHandler;
        static SimpleTypeHandler<2> SharedFunctionWithLengthAndNameTypeHandler;
        static SimpleTypeHandler<1> SharedIdMappedFunctionWithPrototypeTypeHandler;
        static SimpleTypeHandler<2> SharedNamespaceSymbolTypeHandler;
        static MissingPropertyTypeHandler MissingPropertyHolderTypeHandler;

        static SimplePropertyDescriptor const SharedFunctionPropertyDescriptors[2];
        static SimplePropertyDescriptor const HeapArgumentsPropertyDescriptors[3];
        static SimplePropertyDescriptor const FunctionWithLengthAndPrototypeTypeDescriptors[2];
        static SimplePropertyDescriptor const FunctionWithLengthAndNameTypeDescriptors[2];
        static SimplePropertyDescriptor const ModuleNamespaceTypeDescriptors[2];

    public:


        static const ObjectInfoBits EnumFunctionClass = EnumClass_1_Bit;

        static void InitializeProperties(ThreadContext * threadContext);

        JavascriptLibrary(GlobalObject* globalObject) :
            JavascriptLibraryBase(globalObject),
            inProfileMode(false),
            inDispatchProfileMode(false),
            propertyStringMap(nullptr),
            parseIntFunctionObject(nullptr),
            evalFunctionObject(nullptr),
            parseFloatFunctionObject(nullptr),
            arrayPrototypeToLocaleStringFunction(nullptr),
            arrayPrototypeToStringFunction(nullptr),
            identityFunction(nullptr),
            throwerFunction(nullptr),
            jsrtContextObject(nullptr),
            jsrtExternalTypesCache(nullptr),
            fakeGlobalFuncForUndefer(nullptr),
            externalLibraryList(nullptr),
#if ENABLE_COPYONACCESS_ARRAY
            cacheForCopyOnAccessArraySegments(nullptr),
#endif
            referencedPropertyRecords(nullptr),
            stringTemplateCallsiteObjectList(nullptr),
            moduleRecordList(nullptr),
            rootPath(nullptr),
            bindRefChunkBegin(nullptr),
            bindRefChunkCurrent(nullptr),
            bindRefChunkEnd(nullptr),
            dynamicFunctionReference(nullptr)
        {LOGMEIN("JavascriptLibrary.h] 608\n");
            this->globalObject = globalObject;
        }

        void Initialize(ScriptContext* scriptContext, GlobalObject * globalObject);
        void Uninitialize();
        GlobalObject* GetGlobalObject() const {LOGMEIN("JavascriptLibrary.h] 614\n"); return globalObject; }
        ScriptContext* GetScriptContext() const {LOGMEIN("JavascriptLibrary.h] 615\n"); return scriptContext; }

        Recycler * GetRecycler() const {LOGMEIN("JavascriptLibrary.h] 617\n"); return recycler; }
        Var GetPI() {LOGMEIN("JavascriptLibrary.h] 618\n"); return pi; }
        Var GetNaN() {LOGMEIN("JavascriptLibrary.h] 619\n"); return nan; }
        Var GetNegativeInfinite() {LOGMEIN("JavascriptLibrary.h] 620\n"); return negativeInfinite; }
        Var GetPositiveInfinite() {LOGMEIN("JavascriptLibrary.h] 621\n"); return positiveInfinite; }
        Var GetMaxValue() {LOGMEIN("JavascriptLibrary.h] 622\n"); return maxValue; }
        Var GetMinValue() {LOGMEIN("JavascriptLibrary.h] 623\n"); return minValue; }
        Var GetNegativeZero() {LOGMEIN("JavascriptLibrary.h] 624\n"); return negativeZero; }
        RecyclableObject* GetUndefined() {LOGMEIN("JavascriptLibrary.h] 625\n"); return undefinedValue; }
        RecyclableObject* GetNull() {LOGMEIN("JavascriptLibrary.h] 626\n"); return nullValue; }
        JavascriptBoolean* GetTrue() {LOGMEIN("JavascriptLibrary.h] 627\n"); return booleanTrue; }
        JavascriptBoolean* GetFalse() {LOGMEIN("JavascriptLibrary.h] 628\n"); return booleanFalse; }
        Var GetTrueOrFalse(BOOL value) {LOGMEIN("JavascriptLibrary.h] 629\n"); return value ? booleanTrue : booleanFalse; }
        JavascriptSymbol* GetSymbolHasInstance() {LOGMEIN("JavascriptLibrary.h] 630\n"); return symbolHasInstance; }
        JavascriptSymbol* GetSymbolIsConcatSpreadable() {LOGMEIN("JavascriptLibrary.h] 631\n"); return symbolIsConcatSpreadable; }
        JavascriptSymbol* GetSymbolIterator() {LOGMEIN("JavascriptLibrary.h] 632\n"); return symbolIterator; }
        JavascriptSymbol* GetSymbolMatch() {LOGMEIN("JavascriptLibrary.h] 633\n"); return symbolMatch; }
        JavascriptSymbol* GetSymbolReplace() {LOGMEIN("JavascriptLibrary.h] 634\n"); return symbolReplace; }
        JavascriptSymbol* GetSymbolSearch() {LOGMEIN("JavascriptLibrary.h] 635\n"); return symbolSearch; }
        JavascriptSymbol* GetSymbolSplit() {LOGMEIN("JavascriptLibrary.h] 636\n"); return symbolSplit; }
        JavascriptSymbol* GetSymbolSpecies() {LOGMEIN("JavascriptLibrary.h] 637\n"); return symbolSpecies; }
        JavascriptSymbol* GetSymbolToPrimitive() {LOGMEIN("JavascriptLibrary.h] 638\n"); return symbolToPrimitive; }
        JavascriptSymbol* GetSymbolToStringTag() {LOGMEIN("JavascriptLibrary.h] 639\n"); return symbolToStringTag; }
        JavascriptSymbol* GetSymbolUnscopables() {LOGMEIN("JavascriptLibrary.h] 640\n"); return symbolUnscopables; }
        JavascriptString* GetNullString() {LOGMEIN("JavascriptLibrary.h] 641\n"); return nullString; }
        JavascriptString* GetEmptyString() const;
        JavascriptString* GetWhackString() {LOGMEIN("JavascriptLibrary.h] 643\n"); return whackString; }
        JavascriptString* GetUndefinedDisplayString() {LOGMEIN("JavascriptLibrary.h] 644\n"); return undefinedDisplayString; }
        JavascriptString* GetNaNDisplayString() {LOGMEIN("JavascriptLibrary.h] 645\n"); return nanDisplayString; }
        JavascriptString* GetQuotesString() {LOGMEIN("JavascriptLibrary.h] 646\n"); return quotesString; }
        JavascriptString* GetNullDisplayString() {LOGMEIN("JavascriptLibrary.h] 647\n"); return nullDisplayString; }
        JavascriptString* GetUnknownDisplayString() {LOGMEIN("JavascriptLibrary.h] 648\n"); return unknownDisplayString; }
        JavascriptString* GetCommaDisplayString() {LOGMEIN("JavascriptLibrary.h] 649\n"); return commaDisplayString; }
        JavascriptString* GetCommaSpaceDisplayString() {LOGMEIN("JavascriptLibrary.h] 650\n"); return commaSpaceDisplayString; }
        JavascriptString* GetTrueDisplayString() {LOGMEIN("JavascriptLibrary.h] 651\n"); return trueDisplayString; }
        JavascriptString* GetFalseDisplayString() {LOGMEIN("JavascriptLibrary.h] 652\n"); return falseDisplayString; }
        JavascriptString* GetLengthDisplayString() {LOGMEIN("JavascriptLibrary.h] 653\n"); return lengthDisplayString; }
        JavascriptString* GetObjectDisplayString() {LOGMEIN("JavascriptLibrary.h] 654\n"); return objectDisplayString; }
        JavascriptString* GetStringTypeDisplayString() {LOGMEIN("JavascriptLibrary.h] 655\n"); return stringTypeDisplayString; }
        JavascriptString* GetErrorDisplayString() const {LOGMEIN("JavascriptLibrary.h] 656\n"); return errorDisplayString; }
        JavascriptString* GetFunctionPrefixString() {LOGMEIN("JavascriptLibrary.h] 657\n"); return functionPrefixString; }
        JavascriptString* GetGeneratorFunctionPrefixString() {LOGMEIN("JavascriptLibrary.h] 658\n"); return generatorFunctionPrefixString; }
        JavascriptString* GetAsyncFunctionPrefixString() {LOGMEIN("JavascriptLibrary.h] 659\n"); return asyncFunctionPrefixString; }
        JavascriptString* GetFunctionDisplayString() {LOGMEIN("JavascriptLibrary.h] 660\n"); return functionDisplayString; }
        JavascriptString* GetXDomainFunctionDisplayString() {LOGMEIN("JavascriptLibrary.h] 661\n"); return xDomainFunctionDisplayString; }
        JavascriptString* GetInvalidDateString() {LOGMEIN("JavascriptLibrary.h] 662\n"); return invalidDateString; }
        JavascriptString* GetObjectTypeDisplayString() const {LOGMEIN("JavascriptLibrary.h] 663\n"); return objectTypeDisplayString; }
        JavascriptString* GetFunctionTypeDisplayString() const {LOGMEIN("JavascriptLibrary.h] 664\n"); return functionTypeDisplayString; }
        JavascriptString* GetBooleanTypeDisplayString() const {LOGMEIN("JavascriptLibrary.h] 665\n"); return booleanTypeDisplayString; }
        JavascriptString* GetNumberTypeDisplayString() const {LOGMEIN("JavascriptLibrary.h] 666\n"); return numberTypeDisplayString; }
        JavascriptString* GetModuleTypeDisplayString() const {LOGMEIN("JavascriptLibrary.h] 667\n"); return moduleTypeDisplayString; }
        JavascriptString* GetVariantDateTypeDisplayString() const {LOGMEIN("JavascriptLibrary.h] 668\n"); return variantDateTypeDisplayString; }

        // SIMD_JS
        JavascriptString* GetSIMDFloat32x4DisplayString() const {LOGMEIN("JavascriptLibrary.h] 671\n"); return simdFloat32x4DisplayString; }
        JavascriptString* GetSIMDFloat64x2DisplayString() const {LOGMEIN("JavascriptLibrary.h] 672\n"); return simdFloat64x2DisplayString; }
        JavascriptString* GetSIMDInt32x4DisplayString()   const {LOGMEIN("JavascriptLibrary.h] 673\n"); return simdInt32x4DisplayString; }
        JavascriptString* GetSIMDInt16x8DisplayString()   const {LOGMEIN("JavascriptLibrary.h] 674\n"); return simdInt16x8DisplayString; }
        JavascriptString* GetSIMDInt8x16DisplayString()   const {LOGMEIN("JavascriptLibrary.h] 675\n"); return simdInt8x16DisplayString; }

        JavascriptString* GetSIMDBool32x4DisplayString()   const {LOGMEIN("JavascriptLibrary.h] 677\n"); return simdBool32x4DisplayString; }
        JavascriptString* GetSIMDBool16x8DisplayString()   const {LOGMEIN("JavascriptLibrary.h] 678\n"); return simdBool16x8DisplayString; }
        JavascriptString* GetSIMDBool8x16DisplayString()   const {LOGMEIN("JavascriptLibrary.h] 679\n"); return simdBool8x16DisplayString; }

        JavascriptString* GetSIMDUint32x4DisplayString()   const {LOGMEIN("JavascriptLibrary.h] 681\n"); return simdUint32x4DisplayString; }
        JavascriptString* GetSIMDUint16x8DisplayString()   const {LOGMEIN("JavascriptLibrary.h] 682\n"); return simdUint16x8DisplayString; }
        JavascriptString* GetSIMDUint8x16DisplayString()   const {LOGMEIN("JavascriptLibrary.h] 683\n"); return simdUint8x16DisplayString; }

        JavascriptString* GetSymbolTypeDisplayString() const {LOGMEIN("JavascriptLibrary.h] 685\n"); return symbolTypeDisplayString; }
        JavascriptString* GetDebuggerDeadZoneBlockVariableString() {LOGMEIN("JavascriptLibrary.h] 686\n"); Assert(debuggerDeadZoneBlockVariableString); return debuggerDeadZoneBlockVariableString; }
        JavascriptRegExp* CreateEmptyRegExp();
        JavascriptFunction* GetObjectConstructor() const {LOGMEIN("JavascriptLibrary.h] 688\n");return objectConstructor; }
        JavascriptFunction* GetBooleanConstructor() const {LOGMEIN("JavascriptLibrary.h] 689\n");return booleanConstructor; }
        JavascriptFunction* GetDateConstructor() const {LOGMEIN("JavascriptLibrary.h] 690\n");return dateConstructor; }
        JavascriptFunction* GetFunctionConstructor() const {LOGMEIN("JavascriptLibrary.h] 691\n");return functionConstructor; }
        JavascriptFunction* GetNumberConstructor() const {LOGMEIN("JavascriptLibrary.h] 692\n");return numberConstructor; }
        JavascriptRegExpConstructor* GetRegExpConstructor() const {LOGMEIN("JavascriptLibrary.h] 693\n");return regexConstructor; }
        JavascriptFunction* GetStringConstructor() const {LOGMEIN("JavascriptLibrary.h] 694\n");return stringConstructor; }
        JavascriptFunction* GetArrayBufferConstructor() const {LOGMEIN("JavascriptLibrary.h] 695\n");return arrayBufferConstructor; }
        JavascriptFunction* GetErrorConstructor() const {LOGMEIN("JavascriptLibrary.h] 696\n"); return errorConstructor; }
        JavascriptFunction* GetInt8ArrayConstructor() const {LOGMEIN("JavascriptLibrary.h] 697\n");return Int8ArrayConstructor; }
        JavascriptFunction* GetUint8ArrayConstructor() const {LOGMEIN("JavascriptLibrary.h] 698\n");return Uint8ArrayConstructor; }
        JavascriptFunction* GetInt16ArrayConstructor() const {LOGMEIN("JavascriptLibrary.h] 699\n");return Int16ArrayConstructor; }
        JavascriptFunction* GetUint16ArrayConstructor() const {LOGMEIN("JavascriptLibrary.h] 700\n");return Uint16ArrayConstructor; }
        JavascriptFunction* GetInt32ArrayConstructor() const {LOGMEIN("JavascriptLibrary.h] 701\n");return Int32ArrayConstructor; }
        JavascriptFunction* GetUint32ArrayConstructor() const {LOGMEIN("JavascriptLibrary.h] 702\n");return Uint32ArrayConstructor; }
        JavascriptFunction* GetFloat32ArrayConstructor() const {LOGMEIN("JavascriptLibrary.h] 703\n");return Float32ArrayConstructor; }
        JavascriptFunction* GetFloat64ArrayConstructor() const {LOGMEIN("JavascriptLibrary.h] 704\n");return Float64ArrayConstructor; }
        JavascriptFunction* GetWeakMapConstructor() const {LOGMEIN("JavascriptLibrary.h] 705\n");return weakMapConstructor; }
        JavascriptFunction* GetMapConstructor() const {LOGMEIN("JavascriptLibrary.h] 706\n");return mapConstructor; }
        JavascriptFunction* GetSetConstructor() const {LOGMEIN("JavascriptLibrary.h] 707\n");return  setConstructor; }
        JavascriptFunction* GetSymbolConstructor() const {LOGMEIN("JavascriptLibrary.h] 708\n");return symbolConstructor; }
        JavascriptFunction* GetEvalFunctionObject() {LOGMEIN("JavascriptLibrary.h] 709\n"); return evalFunctionObject; }
        JavascriptFunction* GetArrayPrototypeValuesFunction() {LOGMEIN("JavascriptLibrary.h] 710\n"); return EnsureArrayPrototypeValuesFunction(); }
        JavascriptFunction* GetArrayIteratorPrototypeBuiltinNextFunction() {LOGMEIN("JavascriptLibrary.h] 711\n"); return arrayIteratorPrototypeBuiltinNextFunction; }
        DynamicObject* GetMathObject() const {LOGMEIN("JavascriptLibrary.h] 712\n");return mathObject; }
        DynamicObject* GetJSONObject() const {LOGMEIN("JavascriptLibrary.h] 713\n");return JSONObject; }
        DynamicObject* GetReflectObject() const {LOGMEIN("JavascriptLibrary.h] 714\n"); return reflectObject; }
        const PropertyDescriptor* GetDefaultPropertyDescriptor() const {LOGMEIN("JavascriptLibrary.h] 715\n"); return &defaultPropertyDescriptor; }
        DynamicObject* GetMissingPropertyHolder() const {LOGMEIN("JavascriptLibrary.h] 716\n"); return missingPropertyHolder; }

        JavascriptFunction* GetSharedArrayBufferConstructor() {LOGMEIN("JavascriptLibrary.h] 718\n"); return sharedArrayBufferConstructor; }
        DynamicObject* GetAtomicsObject() {LOGMEIN("JavascriptLibrary.h] 719\n"); return atomicsObject; }

        DynamicObject* GetWebAssemblyCompileErrorPrototype() const {LOGMEIN("JavascriptLibrary.h] 721\n"); return webAssemblyCompileErrorPrototype; }
        DynamicObject* GetWebAssemblyCompileErrorConstructor() const {LOGMEIN("JavascriptLibrary.h] 722\n"); return webAssemblyCompileErrorConstructor; }
        DynamicObject* GetWebAssemblyRuntimeErrorPrototype() const {LOGMEIN("JavascriptLibrary.h] 723\n"); return webAssemblyRuntimeErrorPrototype; }
        DynamicObject* GetWebAssemblyRuntimeErrorConstructor() const {LOGMEIN("JavascriptLibrary.h] 724\n"); return webAssemblyRuntimeErrorConstructor; }
        DynamicObject* GetWebAssemblyLinkErrorPrototype() const {LOGMEIN("JavascriptLibrary.h] 725\n"); return webAssemblyLinkErrorPrototype; }
        DynamicObject* GetWebAssemblyLinkErrorConstructor() const {LOGMEIN("JavascriptLibrary.h] 726\n"); return webAssemblyLinkErrorConstructor; }

#if ENABLE_TTD
        Js::PropertyId ExtractPrimitveSymbolId_TTD(Var value);
        Js::RecyclableObject* CreatePrimitveSymbol_TTD(Js::PropertyId pid);
        Js::RecyclableObject* CreatePrimitveSymbol_TTD(Js::JavascriptString* str);

        Js::RecyclableObject* CreateDefaultBoxedObject_TTD(Js::TypeId kind);
        void SetBoxedObjectValue_TTD(Js::RecyclableObject* obj, Js::Var value);

        Js::RecyclableObject* CreateDate_TTD(double value);
        Js::RecyclableObject* CreateRegex_TTD(const char16* patternSource, uint32 patternLength, UnifiedRegex::RegexFlags flags, CharCount lastIndex, Js::Var lastVar);
        Js::RecyclableObject* CreateError_TTD();

        Js::RecyclableObject* CreateES5Array_TTD();
        static void SetLengthWritableES5Array_TTD(Js::RecyclableObject* es5Array, bool isLengthWritable);

        Js::RecyclableObject* CreateSet_TTD();
        Js::RecyclableObject* CreateWeakSet_TTD();
        static void AddSetElementInflate_TTD(Js::JavascriptSet* set, Var value);
        static void AddWeakSetElementInflate_TTD(Js::JavascriptWeakSet* set, Var value);

        Js::RecyclableObject* CreateMap_TTD();
        Js::RecyclableObject* CreateWeakMap_TTD();
        static void AddMapElementInflate_TTD(Js::JavascriptMap* map, Var key, Var value);
        static void AddWeakMapElementInflate_TTD(Js::JavascriptWeakMap* map, Var key, Var value);

        Js::RecyclableObject* CreateExternalFunction_TTD(Js::Var fname);
        Js::RecyclableObject* CreateBoundFunction_TTD(RecyclableObject* function, Var bThis, uint32 ct, Var* args);

        Js::RecyclableObject* CreateProxy_TTD(RecyclableObject* handler, RecyclableObject* target);
        Js::RecyclableObject* CreateRevokeFunction_TTD(RecyclableObject* proxy);

        Js::RecyclableObject* CreateHeapArguments_TTD(uint32 numOfArguments, uint32 formalCount, ActivationObject* frameObject, byte* deletedArray);
        Js::RecyclableObject* CreateES5HeapArguments_TTD(uint32 numOfArguments, uint32 formalCount, ActivationObject* frameObject, byte* deletedArray);

        Js::JavascriptPromiseCapability* CreatePromiseCapability_TTD(Var promise, Var resolve, Var reject);
        Js::JavascriptPromiseReaction* CreatePromiseReaction_TTD(RecyclableObject* handler, JavascriptPromiseCapability* capabilities);

        Js::RecyclableObject* CreatePromise_TTD(uint32 status, Var result, JsUtil::List<Js::JavascriptPromiseReaction*, HeapAllocator>& resolveReactions, JsUtil::List<Js::JavascriptPromiseReaction*, HeapAllocator>& rejectReactions);
        JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* CreateAlreadyDefinedWrapper_TTD(bool alreadyDefined);
        Js::RecyclableObject* CreatePromiseResolveOrRejectFunction_TTD(RecyclableObject* promise, bool isReject, JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* alreadyResolved);
        Js::RecyclableObject* CreatePromiseReactionTaskFunction_TTD(JavascriptPromiseReaction* reaction, Var argument);

        Js::JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper* CreateRemainingElementsWrapper_TTD(Js::ScriptContext* ctx, uint32 value);
        Js::RecyclableObject* JavascriptLibrary::CreatePromiseAllResolveElementFunction_TTD(Js::JavascriptPromiseCapability* capabilities, uint32 index, Js::JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper* wrapper, Js::RecyclableObject* values, bool alreadyCalled);
#endif

#ifdef ENABLE_INTL_OBJECT
        DynamicObject* GetINTLObject() const {LOGMEIN("JavascriptLibrary.h] 775\n"); return IntlObject; }
        void ResetIntlObject();
        void EnsureIntlObjectReady();
        template <class Fn>
        void InitializeIntlForPrototypes(Fn fn);
        void InitializeIntlForStringPrototype();
        void InitializeIntlForDatePrototype();
        void InitializeIntlForNumberPrototype();
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        DynamicType * GetDebugDisposableObjectType() {LOGMEIN("JavascriptLibrary.h] 786\n"); return debugDisposableObjectType; }
        DynamicType * GetDebugFuncExecutorInDisposeObjectType() {LOGMEIN("JavascriptLibrary.h] 787\n"); return debugFuncExecutorInDisposeObjectType; }
#endif

        DynamicType* GetErrorType(ErrorTypeEnum typeToCreate) const;
        StaticType  * GetBooleanTypeStatic() const {LOGMEIN("JavascriptLibrary.h] 791\n"); return booleanTypeStatic; }
        DynamicType * GetBooleanTypeDynamic() const {LOGMEIN("JavascriptLibrary.h] 792\n"); return booleanTypeDynamic; }
        DynamicType * GetDateType() const {LOGMEIN("JavascriptLibrary.h] 793\n"); return dateType; }
        DynamicType * GetBoundFunctionType() const {LOGMEIN("JavascriptLibrary.h] 794\n"); return boundFunctionType; }
        DynamicType * GetRegExpConstructorType() const {LOGMEIN("JavascriptLibrary.h] 795\n"); return regexConstructorType; }
        StaticType  * GetEnumeratorType() const {LOGMEIN("JavascriptLibrary.h] 796\n"); return enumeratorType; }
        DynamicType * GetSpreadArgumentType() const {LOGMEIN("JavascriptLibrary.h] 797\n"); return SpreadArgumentType; }
        StaticType  * GetWithType() const {LOGMEIN("JavascriptLibrary.h] 798\n"); return withType; }
        DynamicType * GetErrorType() const {LOGMEIN("JavascriptLibrary.h] 799\n"); return errorType; }
        DynamicType * GetEvalErrorType() const {LOGMEIN("JavascriptLibrary.h] 800\n"); return evalErrorType; }
        DynamicType * GetRangeErrorType() const {LOGMEIN("JavascriptLibrary.h] 801\n"); return rangeErrorType; }
        DynamicType * GetReferenceErrorType() const {LOGMEIN("JavascriptLibrary.h] 802\n"); return referenceErrorType; }
        DynamicType * GetSyntaxErrorType() const {LOGMEIN("JavascriptLibrary.h] 803\n"); return syntaxErrorType; }
        DynamicType * GetTypeErrorType() const {LOGMEIN("JavascriptLibrary.h] 804\n"); return typeErrorType; }
        DynamicType * GetURIErrorType() const {LOGMEIN("JavascriptLibrary.h] 805\n"); return uriErrorType; }
        DynamicType * GetWebAssemblyCompileErrorType() const {LOGMEIN("JavascriptLibrary.h] 806\n"); return webAssemblyCompileErrorType; }
        DynamicType * GetWebAssemblyRuntimeErrorType() const {LOGMEIN("JavascriptLibrary.h] 807\n"); return webAssemblyRuntimeErrorType; }
        DynamicType * GetWebAssemblyLinkErrorType() const {LOGMEIN("JavascriptLibrary.h] 808\n"); return webAssemblyLinkErrorType; }
        StaticType  * GetNumberTypeStatic() const {LOGMEIN("JavascriptLibrary.h] 809\n"); return numberTypeStatic; }
        StaticType  * GetInt64TypeStatic() const {LOGMEIN("JavascriptLibrary.h] 810\n"); return int64NumberTypeStatic; }
        StaticType  * GetUInt64TypeStatic() const {LOGMEIN("JavascriptLibrary.h] 811\n"); return uint64NumberTypeStatic; }
        DynamicType * GetNumberTypeDynamic() const {LOGMEIN("JavascriptLibrary.h] 812\n"); return numberTypeDynamic; }
        DynamicType * GetPromiseType() const {LOGMEIN("JavascriptLibrary.h] 813\n"); return promiseType; }

        DynamicType * GetWebAssemblyModuleType()  const {LOGMEIN("JavascriptLibrary.h] 815\n"); return webAssemblyModuleType; }
        DynamicType * GetWebAssemblyInstanceType()  const {LOGMEIN("JavascriptLibrary.h] 816\n"); return webAssemblyInstanceType; }
        DynamicType * GetWebAssemblyMemoryType() const {LOGMEIN("JavascriptLibrary.h] 817\n"); return webAssemblyMemoryType; }
        DynamicType * GetWebAssemblyTableType() const {LOGMEIN("JavascriptLibrary.h] 818\n"); return webAssemblyTableType; }

        // SIMD_JS
        DynamicType * GetSIMDBool8x16TypeDynamic()  const {LOGMEIN("JavascriptLibrary.h] 821\n"); return simdBool8x16TypeDynamic;  }
        DynamicType * GetSIMDBool16x8TypeDynamic()  const {LOGMEIN("JavascriptLibrary.h] 822\n"); return simdBool16x8TypeDynamic;  }
        DynamicType * GetSIMDBool32x4TypeDynamic()  const {LOGMEIN("JavascriptLibrary.h] 823\n"); return simdBool32x4TypeDynamic;  }
        DynamicType * GetSIMDInt8x16TypeDynamic()   const {LOGMEIN("JavascriptLibrary.h] 824\n"); return simdInt8x16TypeDynamic;   }
        DynamicType * GetSIMDInt16x8TypeDynamic()   const {LOGMEIN("JavascriptLibrary.h] 825\n"); return simdInt16x8TypeDynamic;   }
        DynamicType * GetSIMDInt32x4TypeDynamic()   const {LOGMEIN("JavascriptLibrary.h] 826\n"); return simdInt32x4TypeDynamic;   }
        DynamicType * GetSIMDUint8x16TypeDynamic()  const {LOGMEIN("JavascriptLibrary.h] 827\n"); return simdUint8x16TypeDynamic;  }
        DynamicType * GetSIMDUint16x8TypeDynamic()  const {LOGMEIN("JavascriptLibrary.h] 828\n"); return simdUint16x8TypeDynamic;  }
        DynamicType * GetSIMDUint32x4TypeDynamic()  const {LOGMEIN("JavascriptLibrary.h] 829\n"); return simdUint32x4TypeDynamic;  }
        DynamicType * GetSIMDFloat32x4TypeDynamic() const {LOGMEIN("JavascriptLibrary.h] 830\n"); return simdFloat32x4TypeDynamic; }

        StaticType* GetSIMDFloat32x4TypeStatic() const {LOGMEIN("JavascriptLibrary.h] 832\n"); return simdFloat32x4TypeStatic; }
        StaticType* GetSIMDFloat64x2TypeStatic() const {LOGMEIN("JavascriptLibrary.h] 833\n"); return simdFloat64x2TypeStatic; }
        StaticType* GetSIMDInt32x4TypeStatic()   const {LOGMEIN("JavascriptLibrary.h] 834\n"); return simdInt32x4TypeStatic; }
        StaticType* GetSIMDInt16x8TypeStatic()   const {LOGMEIN("JavascriptLibrary.h] 835\n"); return simdInt16x8TypeStatic; }
        StaticType* GetSIMDInt8x16TypeStatic()   const {LOGMEIN("JavascriptLibrary.h] 836\n"); return simdInt8x16TypeStatic; }
        StaticType* GetSIMDBool32x4TypeStatic() const {LOGMEIN("JavascriptLibrary.h] 837\n"); return simdBool32x4TypeStatic; }
        StaticType* GetSIMDBool16x8TypeStatic() const {LOGMEIN("JavascriptLibrary.h] 838\n"); return simdBool16x8TypeStatic; }
        StaticType* GetSIMDBool8x16TypeStatic() const {LOGMEIN("JavascriptLibrary.h] 839\n"); return simdBool8x16TypeStatic; }
        StaticType* GetSIMDUInt32x4TypeStatic()   const {LOGMEIN("JavascriptLibrary.h] 840\n"); return simdUint32x4TypeStatic; }
        StaticType* GetSIMDUint16x8TypeStatic()   const {LOGMEIN("JavascriptLibrary.h] 841\n"); return simdUint16x8TypeStatic; }
        StaticType* GetSIMDUint8x16TypeStatic()   const {LOGMEIN("JavascriptLibrary.h] 842\n"); return simdUint8x16TypeStatic; }

        DynamicType * GetObjectLiteralType(uint16 requestedInlineSlotCapacity);
        DynamicType * GetObjectHeaderInlinedLiteralType(uint16 requestedInlineSlotCapacity);
        DynamicType * GetObjectType() const {LOGMEIN("JavascriptLibrary.h] 846\n"); return objectTypes[0]; }
        DynamicType * GetObjectHeaderInlinedType() const {LOGMEIN("JavascriptLibrary.h] 847\n"); return objectHeaderInlinedTypes[0]; }
        StaticType  * GetSymbolTypeStatic() const {LOGMEIN("JavascriptLibrary.h] 848\n"); return symbolTypeStatic; }
        DynamicType * GetSymbolTypeDynamic() const {LOGMEIN("JavascriptLibrary.h] 849\n"); return symbolTypeDynamic; }
        DynamicType * GetProxyType() const {LOGMEIN("JavascriptLibrary.h] 850\n"); return proxyType; }
        DynamicType * GetHeapArgumentsObjectType() const {LOGMEIN("JavascriptLibrary.h] 851\n"); return heapArgumentsType; }
        DynamicType * GetActivationObjectType() const {LOGMEIN("JavascriptLibrary.h] 852\n"); return activationObjectType; }
        DynamicType * GetModuleNamespaceType() const {LOGMEIN("JavascriptLibrary.h] 853\n"); return moduleNamespaceType; }
        DynamicType * GetArrayType() const {LOGMEIN("JavascriptLibrary.h] 854\n"); return arrayType; }
        DynamicType * GetNativeIntArrayType() const {LOGMEIN("JavascriptLibrary.h] 855\n"); return nativeIntArrayType; }
#if ENABLE_COPYONACCESS_ARRAY
        DynamicType * GetCopyOnAccessNativeIntArrayType() const {LOGMEIN("JavascriptLibrary.h] 857\n"); return copyOnAccessNativeIntArrayType; }
#endif
        DynamicType * GetNativeFloatArrayType() const {LOGMEIN("JavascriptLibrary.h] 859\n"); return nativeFloatArrayType; }
        DynamicType * GetRegexPrototypeType() const {LOGMEIN("JavascriptLibrary.h] 860\n"); return regexPrototypeType; }
        DynamicType * GetRegexType() const {LOGMEIN("JavascriptLibrary.h] 861\n"); return regexType; }
        DynamicType * GetRegexResultType() const {LOGMEIN("JavascriptLibrary.h] 862\n"); return regexResultType; }
        DynamicType * GetArrayBufferType() const {LOGMEIN("JavascriptLibrary.h] 863\n"); return arrayBufferType; }
        StaticType  * GetStringTypeStatic() const { AssertMsg(stringTypeStatic, "Where's stringTypeStatic?"); return stringTypeStatic; }
        DynamicType * GetStringTypeDynamic() const {LOGMEIN("JavascriptLibrary.h] 865\n"); return stringTypeDynamic; }
        StaticType  * GetVariantDateType() const {LOGMEIN("JavascriptLibrary.h] 866\n"); return variantDateType; }
        void EnsureDebugObject(DynamicObject* newDebugObject);
        DynamicObject* GetDebugObject() const {LOGMEIN("JavascriptLibrary.h] 868\n"); Assert(debugObject != nullptr); return debugObject; }
        DynamicType * GetMapType() const {LOGMEIN("JavascriptLibrary.h] 869\n"); return mapType; }
        DynamicType * GetSetType() const {LOGMEIN("JavascriptLibrary.h] 870\n"); return setType; }
        DynamicType * GetWeakMapType() const {LOGMEIN("JavascriptLibrary.h] 871\n"); return weakMapType; }
        DynamicType * GetWeakSetType() const {LOGMEIN("JavascriptLibrary.h] 872\n"); return weakSetType; }
        DynamicType * GetArrayIteratorType() const {LOGMEIN("JavascriptLibrary.h] 873\n"); return arrayIteratorType; }
        DynamicType * GetMapIteratorType() const {LOGMEIN("JavascriptLibrary.h] 874\n"); return mapIteratorType; }
        DynamicType * GetSetIteratorType() const {LOGMEIN("JavascriptLibrary.h] 875\n"); return setIteratorType; }
        DynamicType * GetStringIteratorType() const {LOGMEIN("JavascriptLibrary.h] 876\n"); return stringIteratorType; }
        DynamicType * GetListIteratorType() const {LOGMEIN("JavascriptLibrary.h] 877\n"); return listIteratorType; }
        JavascriptFunction* GetDefaultAccessorFunction() const {LOGMEIN("JavascriptLibrary.h] 878\n"); return defaultAccessorFunction; }
        JavascriptFunction* GetStackTraceAccessorFunction() const {LOGMEIN("JavascriptLibrary.h] 879\n"); return stackTraceAccessorFunction; }
        JavascriptFunction* GetThrowTypeErrorRestrictedPropertyAccessorFunction() const {LOGMEIN("JavascriptLibrary.h] 880\n"); return throwTypeErrorRestrictedPropertyAccessorFunction; }
        JavascriptFunction* Get__proto__getterFunction() const {LOGMEIN("JavascriptLibrary.h] 881\n"); return __proto__getterFunction; }
        JavascriptFunction* Get__proto__setterFunction() const {LOGMEIN("JavascriptLibrary.h] 882\n"); return __proto__setterFunction; }

        JavascriptFunction* GetObjectValueOfFunction() const {LOGMEIN("JavascriptLibrary.h] 884\n"); return objectValueOfFunction; }
        JavascriptFunction* GetObjectToStringFunction() const {LOGMEIN("JavascriptLibrary.h] 885\n"); return objectToStringFunction; }

        // SIMD_JS
        JavascriptFunction* GetSIMDFloat32x4ToStringFunction() const {LOGMEIN("JavascriptLibrary.h] 888\n"); return simdFloat32x4ToStringFunction;  }
        JavascriptFunction* GetSIMDFloat64x2ToStringFunction() const {LOGMEIN("JavascriptLibrary.h] 889\n"); return simdFloat64x2ToStringFunction; }
        JavascriptFunction* GetSIMDInt32x4ToStringFunction()   const {LOGMEIN("JavascriptLibrary.h] 890\n"); return simdInt32x4ToStringFunction; }
        JavascriptFunction* GetSIMDInt16x8ToStringFunction()   const {LOGMEIN("JavascriptLibrary.h] 891\n"); return simdInt16x8ToStringFunction; }
        JavascriptFunction* GetSIMDInt8x16ToStringFunction()   const {LOGMEIN("JavascriptLibrary.h] 892\n"); return simdInt8x16ToStringFunction; }
        JavascriptFunction* GetSIMDBool32x4ToStringFunction()   const {LOGMEIN("JavascriptLibrary.h] 893\n"); return simdBool32x4ToStringFunction; }
        JavascriptFunction* GetSIMDBool16x8ToStringFunction()   const {LOGMEIN("JavascriptLibrary.h] 894\n"); return simdBool16x8ToStringFunction; }
        JavascriptFunction* GetSIMDBool8x16ToStringFunction()   const {LOGMEIN("JavascriptLibrary.h] 895\n"); return simdBool8x16ToStringFunction; }
        JavascriptFunction* GetSIMDUint32x4ToStringFunction()   const {LOGMEIN("JavascriptLibrary.h] 896\n"); return simdUint32x4ToStringFunction; }
        JavascriptFunction* GetSIMDUint16x8ToStringFunction()   const {LOGMEIN("JavascriptLibrary.h] 897\n"); return simdUint16x8ToStringFunction; }
        JavascriptFunction* GetSIMDUint8x16ToStringFunction()   const {LOGMEIN("JavascriptLibrary.h] 898\n"); return simdUint8x16ToStringFunction; }

        JavascriptFunction* GetDebugObjectNonUserGetterFunction() const {LOGMEIN("JavascriptLibrary.h] 900\n"); return debugObjectNonUserGetterFunction; }
        JavascriptFunction* GetDebugObjectNonUserSetterFunction() const {LOGMEIN("JavascriptLibrary.h] 901\n"); return debugObjectNonUserSetterFunction; }

        UnifiedRegex::RegexPattern * GetEmptyRegexPattern() const {LOGMEIN("JavascriptLibrary.h] 903\n"); return emptyRegexPattern; }
        JavascriptFunction* GetRegexExecFunction() const {LOGMEIN("JavascriptLibrary.h] 904\n"); return regexExecFunction; }
        JavascriptFunction* GetRegexFlagsGetterFunction() const {LOGMEIN("JavascriptLibrary.h] 905\n"); return regexFlagsGetterFunction; }
        JavascriptFunction* GetRegexGlobalGetterFunction() const {LOGMEIN("JavascriptLibrary.h] 906\n"); return regexGlobalGetterFunction; }
        JavascriptFunction* GetRegexStickyGetterFunction() const {LOGMEIN("JavascriptLibrary.h] 907\n"); return regexStickyGetterFunction; }
        JavascriptFunction* GetRegexUnicodeGetterFunction() const {LOGMEIN("JavascriptLibrary.h] 908\n"); return regexUnicodeGetterFunction; }

        int GetRegexConstructorSlotIndex() const {LOGMEIN("JavascriptLibrary.h] 910\n"); return regexConstructorSlotIndex;  }
        int GetRegexExecSlotIndex() const {LOGMEIN("JavascriptLibrary.h] 911\n"); return regexExecSlotIndex;  }
        int GetRegexFlagsGetterSlotIndex() const {LOGMEIN("JavascriptLibrary.h] 912\n"); return regexFlagsGetterSlotIndex;  }
        int GetRegexGlobalGetterSlotIndex() const {LOGMEIN("JavascriptLibrary.h] 913\n"); return regexGlobalGetterSlotIndex;  }
        int GetRegexStickyGetterSlotIndex() const {LOGMEIN("JavascriptLibrary.h] 914\n"); return regexStickyGetterSlotIndex;  }
        int GetRegexUnicodeGetterSlotIndex() const {LOGMEIN("JavascriptLibrary.h] 915\n"); return regexUnicodeGetterSlotIndex;  }

        TypePath* GetRootPath() const {LOGMEIN("JavascriptLibrary.h] 917\n"); return rootPath; }
        void BindReference(void * addr);
        void CleanupForClose();
        void BeginDynamicFunctionReferences();
        void EndDynamicFunctionReferences();
        void RegisterDynamicFunctionReference(FunctionProxy* func);

        void SetDebugObjectNonUserAccessor(FunctionInfo *funcGetter, FunctionInfo *funcSetter);

        JavascriptFunction* GetDebugObjectDebugModeGetterFunction() const {LOGMEIN("JavascriptLibrary.h] 926\n"); return debugObjectDebugModeGetterFunction; }
        void SetDebugObjectDebugModeAccessor(FunctionInfo *funcGetter);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        JavascriptFunction* GetDebugObjectFaultInjectionCookieGetterFunction() const {LOGMEIN("JavascriptLibrary.h] 930\n"); return debugObjectFaultInjectionCookieGetterFunction; }
        JavascriptFunction* GetDebugObjectFaultInjectionCookieSetterFunction() const {LOGMEIN("JavascriptLibrary.h] 931\n"); return debugObjectFaultInjectionCookieSetterFunction; }
        void SetDebugObjectFaultInjectionCookieGetterAccessor(FunctionInfo *funcGetter, FunctionInfo *funcSetter);
#endif

        JavascriptFunction* GetArrayPrototypeToStringFunction() const {LOGMEIN("JavascriptLibrary.h] 935\n"); return arrayPrototypeToStringFunction; }
        JavascriptFunction* GetArrayPrototypeToLocaleStringFunction() const {LOGMEIN("JavascriptLibrary.h] 936\n"); return arrayPrototypeToLocaleStringFunction; }
        JavascriptFunction* GetIdentityFunction() const {LOGMEIN("JavascriptLibrary.h] 937\n"); return identityFunction; }
        JavascriptFunction* GetThrowerFunction() const {LOGMEIN("JavascriptLibrary.h] 938\n"); return throwerFunction; }

        void SetNativeHostPromiseContinuationFunction(PromiseContinuationCallback function, void *state);

        void SetJsrtContext(FinalizableObject* jsrtContext);
        FinalizableObject* GetJsrtContext();
        void EnqueueTask(Var taskVar);

        HeapArgumentsObject* CreateHeapArguments(Var frameObj, uint formalCount, bool isStrictMode = false);
        JavascriptArray* CreateArray();
        JavascriptArray* CreateArray(uint32 length);
        JavascriptArray *CreateArrayOnStack(void *const stackAllocationPointer);
        JavascriptNativeIntArray* CreateNativeIntArray();
        JavascriptNativeIntArray* CreateNativeIntArray(uint32 length);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptCopyOnAccessNativeIntArray* CreateCopyOnAccessNativeIntArray();
        JavascriptCopyOnAccessNativeIntArray* CreateCopyOnAccessNativeIntArray(uint32 length);
#endif
        JavascriptNativeFloatArray* CreateNativeFloatArray();
        JavascriptNativeFloatArray* CreateNativeFloatArray(uint32 length);
        JavascriptArray* CreateArray(uint32 length, uint32 size);
        ArrayBuffer* CreateArrayBuffer(uint32 length);
        ArrayBuffer* CreateArrayBuffer(byte* buffer, uint32 length);
        ArrayBuffer* CreateWebAssemblyArrayBuffer(uint32 length);
        ArrayBuffer* CreateWebAssemblyArrayBuffer(byte* buffer, uint32 length);
        SharedArrayBuffer* CreateSharedArrayBuffer(uint32 length);
        SharedArrayBuffer* CreateSharedArrayBuffer(SharedContents *contents);
        ArrayBuffer* CreateProjectionArraybuffer(uint32 length);
        ArrayBuffer* CreateProjectionArraybuffer(byte* buffer, uint32 length);
        DataView* CreateDataView(ArrayBufferBase* arrayBuffer, uint32 offSet, uint32 mappedLength);

        template <typename TypeName, bool clamped>
        inline DynamicType* GetTypedArrayType(TypeName);

        template<> inline DynamicType* GetTypedArrayType<int8,false>(int8) {LOGMEIN("JavascriptLibrary.h] 972\n"); return int8ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint8,false>(uint8) {LOGMEIN("JavascriptLibrary.h] 973\n"); return uint8ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint8,true>(uint8) {LOGMEIN("JavascriptLibrary.h] 974\n"); return uint8ClampedArrayType; };
        template<> inline DynamicType* GetTypedArrayType<int16,false>(int16) {LOGMEIN("JavascriptLibrary.h] 975\n"); return int16ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint16,false>(uint16) {LOGMEIN("JavascriptLibrary.h] 976\n"); return uint16ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<int32,false>(int32) {LOGMEIN("JavascriptLibrary.h] 977\n"); return int32ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint32,false>(uint32) {LOGMEIN("JavascriptLibrary.h] 978\n"); return uint32ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<float,false>(float) {LOGMEIN("JavascriptLibrary.h] 979\n"); return float32ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<double,false>(double) {LOGMEIN("JavascriptLibrary.h] 980\n"); return float64ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<int64,false>(int64) {LOGMEIN("JavascriptLibrary.h] 981\n"); return int64ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint64,false>(uint64) {LOGMEIN("JavascriptLibrary.h] 982\n"); return uint64ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<bool,false>(bool) {LOGMEIN("JavascriptLibrary.h] 983\n"); return boolArrayType; };

        DynamicType* GetCharArrayType() {LOGMEIN("JavascriptLibrary.h] 985\n"); return charArrayType; };

        //
        // This method would be used for creating array literals, when we really need to create a huge array
        // Avoids checks at runtime.
        //
        JavascriptArray*            CreateArrayLiteral(uint32 length);
        JavascriptNativeIntArray*   CreateNativeIntArrayLiteral(uint32 length);

#if ENABLE_PROFILE_INFO
        JavascriptNativeIntArray*   CreateCopyOnAccessNativeIntArrayLiteral(ArrayCallSiteInfo *arrayInfo, FunctionBody *functionBody, const Js::AuxArray<int32> *ints);
#endif

        JavascriptNativeFloatArray* CreateNativeFloatArrayLiteral(uint32 length);

        JavascriptBoolean* CreateBoolean(BOOL value);
        JavascriptDate* CreateDate();
        JavascriptDate* CreateDate(double value);
        JavascriptDate* CreateDate(SYSTEMTIME* pst);
        JavascriptMap* CreateMap();
        JavascriptSet* CreateSet();
        JavascriptWeakMap* CreateWeakMap();
        JavascriptWeakSet* CreateWeakSet();
        JavascriptError* CreateError();
        JavascriptError* CreateError(DynamicType* errorType, BOOL isExternal = FALSE);
        JavascriptError* CreateExternalError(ErrorTypeEnum errorTypeEnum);
        JavascriptError* CreateEvalError();
        JavascriptError* CreateRangeError();
        JavascriptError* CreateReferenceError();
        JavascriptError* CreateSyntaxError();
        JavascriptError* CreateTypeError();
        JavascriptError* CreateURIError();
        JavascriptError* CreateStackOverflowError();
        JavascriptError* CreateOutOfMemoryError();
        JavascriptError* CreateWebAssemblyCompileError();
        JavascriptError* CreateWebAssemblyRuntimeError();
        JavascriptError* CreateWebAssemblyLinkError();
        JavascriptSymbol* CreateSymbol(JavascriptString* description);
        JavascriptSymbol* CreateSymbol(const char16* description, int descriptionLength);
        JavascriptSymbol* CreateSymbol(const PropertyRecord* propertyRecord);
        JavascriptPromise* CreatePromise();
        JavascriptGenerator* CreateGenerator(Arguments& args, ScriptFunction* scriptFunction, RecyclableObject* prototype);
        JavascriptFunction* CreateNonProfiledFunction(FunctionInfo * functionInfo);
        template <class MethodType>
        JavascriptExternalFunction* CreateIdMappedExternalFunction(MethodType entryPoint, DynamicType *pPrototypeType);
        JavascriptExternalFunction* CreateExternalConstructor(Js::ExternalMethod entryPoint, PropertyId nameId, RecyclableObject * prototype);
        JavascriptExternalFunction* CreateExternalConstructor(Js::ExternalMethod entryPoint, PropertyId nameId, InitializeMethod method, unsigned short deferredTypeSlots, bool hasAccessors);
        DynamicType* GetCachedJsrtExternalType(uintptr_t finalizeCallback);
        void CacheJsrtExternalType(uintptr_t finalizeCallback, DynamicType* dynamicType);
        static DynamicTypeHandler * GetDeferredPrototypeGeneratorFunctionTypeHandler(ScriptContext* scriptContext);
        static DynamicTypeHandler * GetDeferredPrototypeAsyncFunctionTypeHandler(ScriptContext* scriptContext);
        DynamicType * CreateDeferredPrototypeGeneratorFunctionType(JavascriptMethod entrypoint, bool isAnonymousFunction, bool isShared = false);
        DynamicType * CreateDeferredPrototypeAsyncFunctionType(JavascriptMethod entrypoint, bool isAnonymousFunction, bool isShared = false);

        static DynamicTypeHandler * GetDeferredPrototypeFunctionTypeHandler(ScriptContext* scriptContext);
        static DynamicTypeHandler * GetDeferredAnonymousPrototypeFunctionTypeHandler();
        static DynamicTypeHandler * GetDeferredAnonymousPrototypeGeneratorFunctionTypeHandler();
        static DynamicTypeHandler * GetDeferredAnonymousPrototypeAsyncFunctionTypeHandler();

        DynamicTypeHandler * GetDeferredFunctionTypeHandler();
        DynamicTypeHandler * ScriptFunctionTypeHandler(bool noPrototypeProperty, bool isAnonymousFunction);
        DynamicTypeHandler * GetDeferredAnonymousFunctionTypeHandler();
        template<bool isNameAvailable, bool isPrototypeAvailable = true>
        static DynamicTypeHandler * GetDeferredFunctionTypeHandlerBase();
        template<bool isNameAvailable, bool isPrototypeAvailable = true>
        static DynamicTypeHandler * GetDeferredGeneratorFunctionTypeHandlerBase();
        template<bool isNameAvailable>
        static DynamicTypeHandler * GetDeferredAsyncFunctionTypeHandlerBase();

        DynamicType * CreateDeferredPrototypeFunctionType(JavascriptMethod entrypoint);
        DynamicType * CreateDeferredPrototypeFunctionTypeNoProfileThunk(JavascriptMethod entrypoint, bool isShared = false);
        DynamicType * CreateFunctionType(JavascriptMethod entrypoint, RecyclableObject* prototype = nullptr);
        DynamicType * CreateFunctionWithLengthType(FunctionInfo * functionInfo);
        DynamicType * CreateFunctionWithLengthAndNameType(FunctionInfo * functionInfo);
        DynamicType * CreateFunctionWithLengthAndPrototypeType(FunctionInfo * functionInfo);
        DynamicType * CreateFunctionWithLengthType(DynamicObject * prototype, FunctionInfo * functionInfo);
        DynamicType * CreateFunctionWithLengthAndNameType(DynamicObject * prototype, FunctionInfo * functionInfo);
        DynamicType * CreateFunctionWithLengthAndPrototypeType(DynamicObject * prototype, FunctionInfo * functionInfo);
        ScriptFunction * CreateScriptFunction(FunctionProxy* proxy);
        AsmJsScriptFunction * CreateAsmJsScriptFunction(FunctionProxy* proxy);
        ScriptFunctionWithInlineCache * CreateScriptFunctionWithInlineCache(FunctionProxy* proxy);
        GeneratorVirtualScriptFunction * CreateGeneratorVirtualScriptFunction(FunctionProxy* proxy);
        DynamicType * CreateGeneratorType(RecyclableObject* prototype);

#if 0
        JavascriptNumber* CreateNumber(double value);
#endif
        JavascriptNumber* CreateNumber(double value, RecyclerJavascriptNumberAllocator * numberAllocator);
        JavascriptGeneratorFunction* CreateGeneratorFunction(JavascriptMethod entryPoint, GeneratorVirtualScriptFunction* scriptFunction);
        JavascriptAsyncFunction* CreateAsyncFunction(JavascriptMethod entryPoint, GeneratorVirtualScriptFunction* scriptFunction);
        JavascriptExternalFunction* CreateExternalFunction(ExternalMethod entryPointer, PropertyId nameId, Var signature, JavascriptTypeId prototypeTypeId, UINT64 flags);
        JavascriptExternalFunction* CreateExternalFunction(ExternalMethod entryPointer, Var nameId, Var signature, JavascriptTypeId prototypeTypeId, UINT64 flags);
        JavascriptExternalFunction* CreateStdCallExternalFunction(StdCallJavascriptMethod entryPointer, PropertyId nameId, void *callbackState);
        JavascriptExternalFunction* CreateStdCallExternalFunction(StdCallJavascriptMethod entryPointer, Var nameId, void *callbackState);
        JavascriptPromiseAsyncSpawnExecutorFunction* CreatePromiseAsyncSpawnExecutorFunction(JavascriptMethod entryPoint, JavascriptGenerator* generator, Var target);
        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* CreatePromiseAsyncSpawnStepArgumentExecutorFunction(JavascriptMethod entryPoint, JavascriptGenerator* generator, Var argument, JavascriptFunction* resolve = NULL, JavascriptFunction* reject = NULL, bool isReject = false);
        JavascriptPromiseCapabilitiesExecutorFunction* CreatePromiseCapabilitiesExecutorFunction(JavascriptMethod entryPoint, JavascriptPromiseCapability* capability);
        JavascriptPromiseResolveOrRejectFunction* CreatePromiseResolveOrRejectFunction(JavascriptMethod entryPoint, JavascriptPromise* promise, bool isReject, JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* alreadyResolvedRecord);
        JavascriptPromiseReactionTaskFunction* CreatePromiseReactionTaskFunction(JavascriptMethod entryPoint, JavascriptPromiseReaction* reaction, Var argument);
        JavascriptPromiseResolveThenableTaskFunction* CreatePromiseResolveThenableTaskFunction(JavascriptMethod entryPoint, JavascriptPromise* promise, RecyclableObject* thenable, RecyclableObject* thenFunction);
        JavascriptPromiseAllResolveElementFunction* CreatePromiseAllResolveElementFunction(JavascriptMethod entryPoint, uint32 index, JavascriptArray* values, JavascriptPromiseCapability* capabilities, JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper* remainingElements);
        JavascriptExternalFunction* CreateWrappedExternalFunction(JavascriptExternalFunction* wrappedFunction);

#if ENABLE_NATIVE_CODEGEN
#if !FLOATVAR
        JavascriptNumber* CreateCodeGenNumber(CodeGenNumberAllocator *alloc, double value);
#endif
#endif

        DynamicObject* CreateGeneratorConstructorPrototypeObject();
        DynamicObject* CreateConstructorPrototypeObject(JavascriptFunction * constructor);
        DynamicObject* CreateObject(const bool allowObjectHeaderInlining = false, const PropertyIndex requestedInlineSlotCapacity = 0);
        DynamicObject* CreateObject(DynamicTypeHandler * typeHandler);
        DynamicObject* CreateActivationObject();
        DynamicObject* CreatePseudoActivationObject();
        DynamicObject* CreateBlockActivationObject();
        DynamicObject* CreateConsoleScopeActivationObject();
        DynamicType* CreateObjectType(RecyclableObject* prototype, Js::TypeId typeId, uint16 requestedInlineSlotCapacity);
        DynamicType* CreateObjectTypeNoCache(RecyclableObject* prototype, Js::TypeId typeId);
        DynamicType* CreateObjectType(RecyclableObject* prototype, uint16 requestedInlineSlotCapacity);
        DynamicObject* CreateObject(RecyclableObject* prototype, uint16 requestedInlineSlotCapacity = 0);

        typedef JavascriptString* LibStringType; // used by diagnostics template
        template< size_t N > JavascriptString* CreateStringFromCppLiteral(const char16 (&value)[N]) const;
        template<> JavascriptString* CreateStringFromCppLiteral(const char16 (&value)[1]) const; // Specialization for empty string
        template<> JavascriptString* CreateStringFromCppLiteral(const char16 (&value)[2]) const; // Specialization for single-char strings
        PropertyString* CreatePropertyString(const Js::PropertyRecord* propertyRecord);
        PropertyString* CreatePropertyString(const Js::PropertyRecord* propertyRecord, ArenaAllocator *arena);

        JavascriptVariantDate* CreateVariantDate(const double value);

        JavascriptBooleanObject* CreateBooleanObject(BOOL value);
        JavascriptBooleanObject* CreateBooleanObject();
        JavascriptNumberObject* CreateNumberObjectWithCheck(double value);
        JavascriptNumberObject* CreateNumberObject(Var number);
        JavascriptSIMDObject* CreateSIMDObject(Var simdValue, TypeId typeDescriptor);
        JavascriptStringObject* CreateStringObject(JavascriptString* value);
        JavascriptStringObject* CreateStringObject(const char16* value, charcount_t length);
        JavascriptSymbolObject* CreateSymbolObject(JavascriptSymbol* value);
        JavascriptArrayIterator* CreateArrayIterator(Var iterable, JavascriptArrayIteratorKind kind);
        JavascriptMapIterator* CreateMapIterator(JavascriptMap* map, JavascriptMapIteratorKind kind);
        JavascriptSetIterator* CreateSetIterator(JavascriptSet* set, JavascriptSetIteratorKind kind);
        JavascriptStringIterator* CreateStringIterator(JavascriptString* string);
        JavascriptListIterator* CreateListIterator(ListForListIterator* list);

        JavascriptRegExp* CreateRegExp(UnifiedRegex::RegexPattern* pattern);

        DynamicObject* CreateIteratorResultObject(Var value, Var done);
        DynamicObject* CreateIteratorResultObjectValueFalse(Var value);
        DynamicObject* CreateIteratorResultObjectUndefinedTrue();

        RecyclableObject* CreateThrowErrorObject(JavascriptError* error);

        JavascriptFunction* EnsurePromiseResolveFunction();
        JavascriptFunction* EnsureGeneratorNextFunction();
        JavascriptFunction* EnsureGeneratorThrowFunction();

        void SetCrossSiteForSharedFunctionType(JavascriptFunction * function);

        bool IsPRNGSeeded() {LOGMEIN("JavascriptLibrary.h] 1144\n"); return isPRNGSeeded; }
        uint64 GetRandSeed0() {LOGMEIN("JavascriptLibrary.h] 1145\n"); return randSeed0; }
        uint64 GetRandSeed1() {LOGMEIN("JavascriptLibrary.h] 1146\n"); return randSeed1; }
        void SetIsPRNGSeeded(bool val);
        void SetRandSeed0(uint64 rs) {LOGMEIN("JavascriptLibrary.h] 1148\n"); randSeed0 = rs;}
        void SetRandSeed1(uint64 rs) {LOGMEIN("JavascriptLibrary.h] 1149\n"); randSeed1 = rs; }

        void SetProfileMode(bool fSet);
        void SetDispatchProfile(bool fSet, JavascriptMethod dispatchInvoke);
        HRESULT ProfilerRegisterBuiltIns();

#if ENABLE_COPYONACCESS_ARRAY
        static bool IsCopyOnAccessArrayCallSite(JavascriptLibrary *lib, ArrayCallSiteInfo *arrayInfo, uint32 length);
        static bool IsCachedCopyOnAccessArrayCallSite(const JavascriptLibrary *lib, ArrayCallSiteInfo *arrayInfo);
        template <typename T>
        static void CheckAndConvertCopyOnAccessNativeIntArray(const T instance);
#endif

        void EnsureStringTemplateCallsiteObjectList();
        void AddStringTemplateCallsiteObject(RecyclableObject* callsite);
        RecyclableObject* TryGetStringTemplateCallsiteObject(ParseNodePtr pnode);
        RecyclableObject* TryGetStringTemplateCallsiteObject(RecyclableObject* callsite);

        static void CheckAndInvalidateIsConcatSpreadableCache(PropertyId propertyId, ScriptContext *scriptContext);

#if DBG_DUMP
        static const char16* GetStringTemplateCallsiteObjectKey(Var callsite);
#endif

        Field(JavascriptFunction*)* GetBuiltinFunctions();
        INT_PTR* GetVTableAddresses();
        static BuiltinFunction GetBuiltinFunctionForPropId(PropertyId id);
        static BuiltinFunction GetBuiltInForFuncInfo(intptr_t funcInfoAddr, ThreadContextInfo *context);
#if DBG
        static void CheckRegisteredBuiltIns(Field(JavascriptFunction*)* builtInFuncs, ScriptContext *scriptContext);
#endif
        static BOOL CanFloatPreferenceFunc(BuiltinFunction index);
        static BOOL IsFltFunc(BuiltinFunction index);
        static bool IsFloatFunctionCallsite(BuiltinFunction index, size_t argc);
        static bool IsFltBuiltInConst(PropertyId id);
        static size_t GetArgCForBuiltIn(BuiltinFunction index)
        {LOGMEIN("JavascriptLibrary.h] 1185\n");
            Assert(index < _countof(JavascriptLibrary::LibraryFunctionArgC));
            return JavascriptLibrary::LibraryFunctionArgC[index];
        }
        static BuiltInFlags GetFlagsForBuiltIn(BuiltinFunction index)
        {LOGMEIN("JavascriptLibrary.h] 1190\n");
            Assert(index < _countof(JavascriptLibrary::LibraryFunctionFlags));
            return (BuiltInFlags)JavascriptLibrary::LibraryFunctionFlags[index];
        }
        static BuiltinFunction GetBuiltInInlineCandidateId(Js::OpCode opCode);
        static BuiltInArgSpecializationType GetBuiltInArgType(BuiltInFlags flags, BuiltInArgShift argGroup);
        static bool IsTypeSpecRequired(BuiltInFlags flags)
        {LOGMEIN("JavascriptLibrary.h] 1197\n");
            return GetBuiltInArgType(flags, BuiltInArgShift::BIAS_Src1) || GetBuiltInArgType(flags, BuiltInArgShift::BIAS_Src2) || GetBuiltInArgType(flags, BuiltInArgShift::BIAS_Dst);
        }
#if ENABLE_DEBUG_CONFIG_OPTIONS
        static char16 const * GetNameForBuiltIn(BuiltinFunction index)
        {LOGMEIN("JavascriptLibrary.h] 1202\n");
            Assert(index < _countof(JavascriptLibrary::LibraryFunctionName));
            return JavascriptLibrary::LibraryFunctionName[index];
        }
#endif

        PropertyStringCacheMap* EnsurePropertyStringMap();
        PropertyStringCacheMap* GetPropertyStringMap() {LOGMEIN("JavascriptLibrary.h] 1209\n"); return this->propertyStringMap; }

        void TypeAndPrototypesAreEnsuredToHaveOnlyWritableDataProperties(Type *const type);
        void NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();

        static bool ArrayIteratorPrototypeHasUserDefinedNext(ScriptContext *scriptContext);

        CharStringCache& GetCharStringCache() {LOGMEIN("JavascriptLibrary.h] 1216\n"); return charStringCache;  }
        static JavascriptLibrary * FromCharStringCache(CharStringCache * cache)
        {LOGMEIN("JavascriptLibrary.h] 1218\n");
            return (JavascriptLibrary *)((uintptr_t)cache - offsetof(JavascriptLibrary, charStringCache));
        }

        bool GetArrayObjectHasUserDefinedSpecies() const {LOGMEIN("JavascriptLibrary.h] 1222\n"); return arrayObjectHasUserDefinedSpecies; }
        void SetArrayObjectHasUserDefinedSpecies(bool val) {LOGMEIN("JavascriptLibrary.h] 1223\n"); arrayObjectHasUserDefinedSpecies = val; }

        FunctionBody* GetFakeGlobalFuncForUndefer()const {LOGMEIN("JavascriptLibrary.h] 1225\n"); return this->fakeGlobalFuncForUndefer; }
        void SetFakeGlobalFuncForUndefer(FunctionBody* functionBody) {LOGMEIN("JavascriptLibrary.h] 1226\n"); this->fakeGlobalFuncForUndefer = functionBody; }        

        ModuleRecordList* EnsureModuleRecordList();
        SourceTextModuleRecord* GetModuleRecord(uint moduleId);

    private:
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        // Declare fretest/debug properties here since otherwise it can cause
        // a mismatch between fre mshtml and fretest jscript9 causing undefined behavior

        Field(DynamicType *) debugDisposableObjectType;
        Field(DynamicType *) debugFuncExecutorInDisposeObjectType;
#endif

        void InitializePrototypes();
        void InitializeTypes();
        void InitializeGlobal(GlobalObject * globalObject);
        static void PrecalculateArrayAllocationBuckets();

#define STANDARD_INIT(name) \
        static void __cdecl Initialize##name##Constructor(DynamicObject* arrayConstructor, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode); \
        static void __cdecl Initialize##name##Prototype(DynamicObject* arrayPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

        STANDARD_INIT(Array);
        STANDARD_INIT(SharedArrayBuffer);
        STANDARD_INIT(ArrayBuffer);
        STANDARD_INIT(DataView);
        STANDARD_INIT(Error);
        STANDARD_INIT(EvalError);
        STANDARD_INIT(RangeError);
        STANDARD_INIT(ReferenceError);
        STANDARD_INIT(SyntaxError);
        STANDARD_INIT(TypeError);
        STANDARD_INIT(URIError);
        STANDARD_INIT(RuntimeError);
        STANDARD_INIT(TypedArray);
        STANDARD_INIT(Int8Array);
        STANDARD_INIT(Uint8Array);
        STANDARD_INIT(Uint8ClampedArray);
        STANDARD_INIT(Int16Array);
        STANDARD_INIT(Uint16Array);
        STANDARD_INIT(Int32Array);
        STANDARD_INIT(Uint32Array);
        STANDARD_INIT(Float32Array);
        STANDARD_INIT(Float64Array);
        STANDARD_INIT(Boolean);
        STANDARD_INIT(Symbol);
        STANDARD_INIT(Date);
        STANDARD_INIT(Proxy);
        STANDARD_INIT(Function);
        STANDARD_INIT(Number);
        STANDARD_INIT(Object);
        STANDARD_INIT(Regex);
        STANDARD_INIT(String);
        STANDARD_INIT(Map);
        STANDARD_INIT(Set);
        STANDARD_INIT(WeakMap);
        STANDARD_INIT(WeakSet);
        STANDARD_INIT(Promise);
        STANDARD_INIT(GeneratorFunction);
        STANDARD_INIT(AsyncFunction);
        STANDARD_INIT(WebAssemblyCompileError);
        STANDARD_INIT(WebAssemblyRuntimeError);
        STANDARD_INIT(WebAssemblyLinkError);
        STANDARD_INIT(WebAssemblyMemory);
        STANDARD_INIT(WebAssemblyModule);
        STANDARD_INIT(WebAssemblyInstance);
        STANDARD_INIT(WebAssemblyTable);

#undef STANDARD_INIT

        static void __cdecl InitializeAtomicsObject(DynamicObject* atomicsObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

        static void __cdecl InitializeInt64ArrayPrototype(DynamicObject* prototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeUint64ArrayPrototype(DynamicObject* prototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeBoolArrayPrototype(DynamicObject* prototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeCharArrayPrototype(DynamicObject* prototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

        void InitializeComplexThings();
        void InitializeStaticValues();
        static void __cdecl InitializeMathObject(DynamicObject* mathObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
#ifdef ENABLE_WASM
        static void __cdecl InitializeWebAssemblyObject(DynamicObject* WasmObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
#endif
        // SIMD_JS
        static void __cdecl InitializeSIMDObject(DynamicObject* simdObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDOpCodeMaps();

        template<typename SIMDTypeName>
        static void SIMDPrototypeInitHelper(DynamicObject* simdPrototype, JavascriptLibrary* library, JavascriptFunction* constructorFn, JavascriptString* strLiteral);

        static void __cdecl InitializeSIMDBool8x16Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDBool16x8Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDBool32x4Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDInt8x16Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDInt16x8Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDInt32x4Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDUint8x16Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDUint16x8Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDUint32x4Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDFloat32x4Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSIMDFloat64x2Prototype(DynamicObject* simdPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

        static void __cdecl InitializeJSONObject(DynamicObject* JSONObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeEngineInterfaceObject(DynamicObject* engineInterface, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeReflectObject(DynamicObject* reflectObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
#ifdef ENABLE_INTL_OBJECT
        static void __cdecl InitializeIntlObject(DynamicObject* IntlEngineObject, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
#endif
#ifdef ENABLE_PROJECTION
        void InitializeWinRTPromiseConstructor();
#endif

        static void __cdecl InitializeIteratorPrototype(DynamicObject* iteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeArrayIteratorPrototype(DynamicObject* arrayIteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeMapIteratorPrototype(DynamicObject* mapIteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeSetIteratorPrototype(DynamicObject* setIteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        static void __cdecl InitializeStringIteratorPrototype(DynamicObject* stringIteratorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

        static void __cdecl InitializeGeneratorPrototype(DynamicObject* generatorPrototype, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

        static void __cdecl InitializeAsyncFunction(DynamicObject *function, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

        RuntimeFunction* CreateBuiltinConstructor(FunctionInfo * functionInfo, DynamicTypeHandler * typeHandler, DynamicObject* prototype = nullptr);
        RuntimeFunction* DefaultCreateFunction(FunctionInfo * functionInfo, int length, DynamicObject * prototype, DynamicType * functionType, PropertyId nameId);
        RuntimeFunction* DefaultCreateFunction(FunctionInfo * functionInfo, int length, DynamicObject * prototype, DynamicType * functionType, Var nameId);
        JavascriptFunction* AddFunction(DynamicObject* object, PropertyId propertyId, RuntimeFunction* function);
        void AddMember(DynamicObject* object, PropertyId propertyId, Var value);
        void AddMember(DynamicObject* object, PropertyId propertyId, Var value, PropertyAttributes attributes);
        JavascriptString* CreateEmptyString();


        static void __cdecl InitializeGeneratorFunction(DynamicObject* function, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);
        template<bool addPrototype>
        static void __cdecl InitializeFunction(DynamicObject* function, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

        static size_t const LibraryFunctionArgC[BuiltinFunction::Count + 1];
        static int const LibraryFunctionFlags[BuiltinFunction::Count + 1];   // returns enum BuiltInFlags.
#if ENABLE_DEBUG_CONFIG_OPTIONS
        static char16 const * const LibraryFunctionName[BuiltinFunction::Count + 1];
#endif

        JavascriptFunction* EnsureArrayPrototypeValuesFunction();


    public:
        virtual void Finalize(bool isShutdown) override;

#if DBG
        void DumpLibraryByteCode();
#endif
    private:
        typedef JsUtil::BaseHashSet<Js::PropertyRecord const *, Recycler, PowerOf2SizePolicy> ReferencedPropertyRecordHashSet;
        Field(ReferencedPropertyRecordHashSet*) referencedPropertyRecords;

        ReferencedPropertyRecordHashSet * EnsureReferencedPropertyRecordList()
        {LOGMEIN("JavascriptLibrary.h] 1382\n");
            ReferencedPropertyRecordHashSet* pidList = this->referencedPropertyRecords;
            if (pidList == nullptr)
            {LOGMEIN("JavascriptLibrary.h] 1385\n");
                pidList = RecyclerNew(this->recycler, ReferencedPropertyRecordHashSet, this->recycler, 173);
                this->referencedPropertyRecords = pidList;
            }
            return pidList;
        }

        ReferencedPropertyRecordHashSet * GetReferencedPropertyRecordList() const
        {LOGMEIN("JavascriptLibrary.h] 1393\n");
            return this->referencedPropertyRecords;
        }

        HRESULT ProfilerRegisterObject();
        HRESULT ProfilerRegisterArray();
        HRESULT ProfilerRegisterBoolean();
        HRESULT ProfilerRegisterDate();
        HRESULT ProfilerRegisterFunction();
        HRESULT ProfilerRegisterMath();
        HRESULT ProfilerRegisterNumber();
        HRESULT ProfilerRegisterString();
        HRESULT ProfilerRegisterRegExp();
        HRESULT ProfilerRegisterJSON();
        HRESULT ProfilerRegisterMap();
        HRESULT ProfilerRegisterSet();
        HRESULT ProfilerRegisterWeakMap();
        HRESULT ProfilerRegisterWeakSet();
        HRESULT ProfilerRegisterSymbol();
        HRESULT ProfilerRegisterIterator();
        HRESULT ProfilerRegisterArrayIterator();
        HRESULT ProfilerRegisterMapIterator();
        HRESULT ProfilerRegisterSetIterator();
        HRESULT ProfilerRegisterStringIterator();
        HRESULT ProfilerRegisterTypedArray();
        HRESULT ProfilerRegisterPromise();
        HRESULT ProfilerRegisterProxy();
        HRESULT ProfilerRegisterReflect();
        HRESULT ProfilerRegisterGenerator();
        HRESULT ProfilerRegisterSIMD();
        HRESULT ProfilerRegisterAtomics();

#ifdef IR_VIEWER
        HRESULT ProfilerRegisterIRViewer();
#endif /* IR_VIEWER */
    };
}
