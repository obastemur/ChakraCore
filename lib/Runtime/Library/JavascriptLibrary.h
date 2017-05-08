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
        UndeclaredBlockVariable(Type* type) : RecyclableObject(type) {TRACE_IT(59688); }
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
        {TRACE_IT(59689);
            cache[count++] = segment;
            return count;
        }

        SparseArraySegment<int32> *GetSegmentByIndex(byte index)
        {TRACE_IT(59690);
            Assert(index <= MAX_SIZE);
            return cache[index - 1];
        }

        bool IsNotOverHardLimit()
        {TRACE_IT(59691);
            return count < MAX_SIZE;
        }

        bool IsNotFull()
        {TRACE_IT(59692);
            return count < (uint32) CONFIG_FLAG(CopyOnAccessArraySegmentCacheSize);
        }

        bool IsValidIndex(uint32 index)
        {TRACE_IT(59693);
            return count && index && index <= count;
        }

#if ENABLE_DEBUG_CONFIG_OPTIONS
        uint32 GetCount()
        {TRACE_IT(59694);
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

        static DWORD GetScriptContextOffset() {TRACE_IT(59695); return offsetof(JavascriptLibrary, scriptContext); }
        static DWORD GetUndeclBlockVarOffset() {TRACE_IT(59696); return offsetof(JavascriptLibrary, undeclBlockVarSentinel); }
        static DWORD GetEmptyStringOffset() {TRACE_IT(59697); return offsetof(JavascriptLibrary, emptyString); }
        static DWORD GetUndefinedValueOffset() {TRACE_IT(59698); return offsetof(JavascriptLibrary, undefinedValue); }
        static DWORD GetNullValueOffset() {TRACE_IT(59699); return offsetof(JavascriptLibrary, nullValue); }
        static DWORD GetBooleanTrueOffset() {TRACE_IT(59700); return offsetof(JavascriptLibrary, booleanTrue); }
        static DWORD GetBooleanFalseOffset() {TRACE_IT(59701); return offsetof(JavascriptLibrary, booleanFalse); }
        static DWORD GetNegativeZeroOffset() {TRACE_IT(59702); return offsetof(JavascriptLibrary, negativeZero); }
        static DWORD GetNumberTypeStaticOffset() {TRACE_IT(59703); return offsetof(JavascriptLibrary, numberTypeStatic); }
        static DWORD GetStringTypeStaticOffset() {TRACE_IT(59704); return offsetof(JavascriptLibrary, stringTypeStatic); }
        static DWORD GetObjectTypesOffset() {TRACE_IT(59705); return offsetof(JavascriptLibrary, objectTypes); }
        static DWORD GetObjectHeaderInlinedTypesOffset() {TRACE_IT(59706); return offsetof(JavascriptLibrary, objectHeaderInlinedTypes); }
        static DWORD GetRegexTypeOffset() {TRACE_IT(59707); return offsetof(JavascriptLibrary, regexType); }
        static DWORD GetArrayConstructorOffset() {TRACE_IT(59708); return offsetof(JavascriptLibrary, arrayConstructor); }
        static DWORD GetPositiveInfinityOffset() {TRACE_IT(59709); return offsetof(JavascriptLibrary, positiveInfinite); }
        static DWORD GetNaNOffset() {TRACE_IT(59710); return offsetof(JavascriptLibrary, nan); }
        static DWORD GetNativeIntArrayTypeOffset() {TRACE_IT(59711); return offsetof(JavascriptLibrary, nativeIntArrayType); }
#if ENABLE_COPYONACCESS_ARRAY
        static DWORD GetCopyOnAccessNativeIntArrayTypeOffset() {TRACE_IT(59712); return offsetof(JavascriptLibrary, copyOnAccessNativeIntArrayType); }
#endif
        static DWORD GetNativeFloatArrayTypeOffset() {TRACE_IT(59713); return offsetof(JavascriptLibrary, nativeFloatArrayType); }
        static DWORD GetVTableAddressesOffset() {TRACE_IT(59714); return offsetof(JavascriptLibrary, vtableAddresses); }
        static DWORD GetConstructorCacheDefaultInstanceOffset() {TRACE_IT(59715); return offsetof(JavascriptLibrary, constructorCacheDefaultInstance); }
        static DWORD GetAbsDoubleCstOffset() {TRACE_IT(59716); return offsetof(JavascriptLibrary, absDoubleCst); }
        static DWORD GetUintConvertConstOffset() {TRACE_IT(59717); return offsetof(JavascriptLibrary, uintConvertConst); }
        static DWORD GetBuiltinFunctionsOffset() {TRACE_IT(59718); return offsetof(JavascriptLibrary, builtinFunctions); }
        static DWORD GetCharStringCacheOffset() {TRACE_IT(59719); return offsetof(JavascriptLibrary, charStringCache); }
        static DWORD GetCharStringCacheAOffset() {TRACE_IT(59720); return GetCharStringCacheOffset() + CharStringCache::GetCharStringCacheAOffset(); }
        const  JavascriptLibraryBase* GetLibraryBase() const {TRACE_IT(59721); return static_cast<const JavascriptLibraryBase*>(this); }
        void SetGlobalObject(GlobalObject* globalObject) {TRACE_IT(59722);this->globalObject = globalObject; }
        static DWORD GetRandSeed0Offset() {TRACE_IT(59723); return offsetof(JavascriptLibrary, randSeed0); }
        static DWORD GetRandSeed1Offset() {TRACE_IT(59724); return offsetof(JavascriptLibrary, randSeed1); }
        static DWORD GetTypeDisplayStringsOffset() {TRACE_IT(59725); return offsetof(JavascriptLibrary, typeDisplayStrings); }
        typedef bool (CALLBACK *PromiseContinuationCallback)(Var task, void *callbackState);

        Var GetUndeclBlockVar() const {TRACE_IT(59726); return undeclBlockVarSentinel; }
        bool IsUndeclBlockVar(Var var) const {TRACE_IT(59727); return var == undeclBlockVarSentinel; }

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
        {TRACE_IT(59728);
            this->globalObject = globalObject;
        }

        void Initialize(ScriptContext* scriptContext, GlobalObject * globalObject);
        void Uninitialize();
        GlobalObject* GetGlobalObject() const {TRACE_IT(59729); return globalObject; }
        ScriptContext* GetScriptContext() const {TRACE_IT(59730); return scriptContext; }

        Recycler * GetRecycler() const {TRACE_IT(59731); return recycler; }
        Var GetPI() {TRACE_IT(59732); return pi; }
        Var GetNaN() {TRACE_IT(59733); return nan; }
        Var GetNegativeInfinite() {TRACE_IT(59734); return negativeInfinite; }
        Var GetPositiveInfinite() {TRACE_IT(59735); return positiveInfinite; }
        Var GetMaxValue() {TRACE_IT(59736); return maxValue; }
        Var GetMinValue() {TRACE_IT(59737); return minValue; }
        Var GetNegativeZero() {TRACE_IT(59738); return negativeZero; }
        RecyclableObject* GetUndefined() {TRACE_IT(59739); return undefinedValue; }
        RecyclableObject* GetNull() {TRACE_IT(59740); return nullValue; }
        JavascriptBoolean* GetTrue() {TRACE_IT(59741); return booleanTrue; }
        JavascriptBoolean* GetFalse() {TRACE_IT(59742); return booleanFalse; }
        Var GetTrueOrFalse(BOOL value) {TRACE_IT(59743); return value ? booleanTrue : booleanFalse; }
        JavascriptSymbol* GetSymbolHasInstance() {TRACE_IT(59744); return symbolHasInstance; }
        JavascriptSymbol* GetSymbolIsConcatSpreadable() {TRACE_IT(59745); return symbolIsConcatSpreadable; }
        JavascriptSymbol* GetSymbolIterator() {TRACE_IT(59746); return symbolIterator; }
        JavascriptSymbol* GetSymbolMatch() {TRACE_IT(59747); return symbolMatch; }
        JavascriptSymbol* GetSymbolReplace() {TRACE_IT(59748); return symbolReplace; }
        JavascriptSymbol* GetSymbolSearch() {TRACE_IT(59749); return symbolSearch; }
        JavascriptSymbol* GetSymbolSplit() {TRACE_IT(59750); return symbolSplit; }
        JavascriptSymbol* GetSymbolSpecies() {TRACE_IT(59751); return symbolSpecies; }
        JavascriptSymbol* GetSymbolToPrimitive() {TRACE_IT(59752); return symbolToPrimitive; }
        JavascriptSymbol* GetSymbolToStringTag() {TRACE_IT(59753); return symbolToStringTag; }
        JavascriptSymbol* GetSymbolUnscopables() {TRACE_IT(59754); return symbolUnscopables; }
        JavascriptString* GetNullString() {TRACE_IT(59755); return nullString; }
        JavascriptString* GetEmptyString() const;
        JavascriptString* GetWhackString() {TRACE_IT(59756); return whackString; }
        JavascriptString* GetUndefinedDisplayString() {TRACE_IT(59757); return undefinedDisplayString; }
        JavascriptString* GetNaNDisplayString() {TRACE_IT(59758); return nanDisplayString; }
        JavascriptString* GetQuotesString() {TRACE_IT(59759); return quotesString; }
        JavascriptString* GetNullDisplayString() {TRACE_IT(59760); return nullDisplayString; }
        JavascriptString* GetUnknownDisplayString() {TRACE_IT(59761); return unknownDisplayString; }
        JavascriptString* GetCommaDisplayString() {TRACE_IT(59762); return commaDisplayString; }
        JavascriptString* GetCommaSpaceDisplayString() {TRACE_IT(59763); return commaSpaceDisplayString; }
        JavascriptString* GetTrueDisplayString() {TRACE_IT(59764); return trueDisplayString; }
        JavascriptString* GetFalseDisplayString() {TRACE_IT(59765); return falseDisplayString; }
        JavascriptString* GetLengthDisplayString() {TRACE_IT(59766); return lengthDisplayString; }
        JavascriptString* GetObjectDisplayString() {TRACE_IT(59767); return objectDisplayString; }
        JavascriptString* GetStringTypeDisplayString() {TRACE_IT(59768); return stringTypeDisplayString; }
        JavascriptString* GetErrorDisplayString() const {TRACE_IT(59769); return errorDisplayString; }
        JavascriptString* GetFunctionPrefixString() {TRACE_IT(59770); return functionPrefixString; }
        JavascriptString* GetGeneratorFunctionPrefixString() {TRACE_IT(59771); return generatorFunctionPrefixString; }
        JavascriptString* GetAsyncFunctionPrefixString() {TRACE_IT(59772); return asyncFunctionPrefixString; }
        JavascriptString* GetFunctionDisplayString() {TRACE_IT(59773); return functionDisplayString; }
        JavascriptString* GetXDomainFunctionDisplayString() {TRACE_IT(59774); return xDomainFunctionDisplayString; }
        JavascriptString* GetInvalidDateString() {TRACE_IT(59775); return invalidDateString; }
        JavascriptString* GetObjectTypeDisplayString() const {TRACE_IT(59776); return objectTypeDisplayString; }
        JavascriptString* GetFunctionTypeDisplayString() const {TRACE_IT(59777); return functionTypeDisplayString; }
        JavascriptString* GetBooleanTypeDisplayString() const {TRACE_IT(59778); return booleanTypeDisplayString; }
        JavascriptString* GetNumberTypeDisplayString() const {TRACE_IT(59779); return numberTypeDisplayString; }
        JavascriptString* GetModuleTypeDisplayString() const {TRACE_IT(59780); return moduleTypeDisplayString; }
        JavascriptString* GetVariantDateTypeDisplayString() const {TRACE_IT(59781); return variantDateTypeDisplayString; }

        // SIMD_JS
        JavascriptString* GetSIMDFloat32x4DisplayString() const {TRACE_IT(59782); return simdFloat32x4DisplayString; }
        JavascriptString* GetSIMDFloat64x2DisplayString() const {TRACE_IT(59783); return simdFloat64x2DisplayString; }
        JavascriptString* GetSIMDInt32x4DisplayString()   const {TRACE_IT(59784); return simdInt32x4DisplayString; }
        JavascriptString* GetSIMDInt16x8DisplayString()   const {TRACE_IT(59785); return simdInt16x8DisplayString; }
        JavascriptString* GetSIMDInt8x16DisplayString()   const {TRACE_IT(59786); return simdInt8x16DisplayString; }

        JavascriptString* GetSIMDBool32x4DisplayString()   const {TRACE_IT(59787); return simdBool32x4DisplayString; }
        JavascriptString* GetSIMDBool16x8DisplayString()   const {TRACE_IT(59788); return simdBool16x8DisplayString; }
        JavascriptString* GetSIMDBool8x16DisplayString()   const {TRACE_IT(59789); return simdBool8x16DisplayString; }

        JavascriptString* GetSIMDUint32x4DisplayString()   const {TRACE_IT(59790); return simdUint32x4DisplayString; }
        JavascriptString* GetSIMDUint16x8DisplayString()   const {TRACE_IT(59791); return simdUint16x8DisplayString; }
        JavascriptString* GetSIMDUint8x16DisplayString()   const {TRACE_IT(59792); return simdUint8x16DisplayString; }

        JavascriptString* GetSymbolTypeDisplayString() const {TRACE_IT(59793); return symbolTypeDisplayString; }
        JavascriptString* GetDebuggerDeadZoneBlockVariableString() {TRACE_IT(59794); Assert(debuggerDeadZoneBlockVariableString); return debuggerDeadZoneBlockVariableString; }
        JavascriptRegExp* CreateEmptyRegExp();
        JavascriptFunction* GetObjectConstructor() const {TRACE_IT(59795);return objectConstructor; }
        JavascriptFunction* GetBooleanConstructor() const {TRACE_IT(59796);return booleanConstructor; }
        JavascriptFunction* GetDateConstructor() const {TRACE_IT(59797);return dateConstructor; }
        JavascriptFunction* GetFunctionConstructor() const {TRACE_IT(59798);return functionConstructor; }
        JavascriptFunction* GetNumberConstructor() const {TRACE_IT(59799);return numberConstructor; }
        JavascriptRegExpConstructor* GetRegExpConstructor() const {TRACE_IT(59800);return regexConstructor; }
        JavascriptFunction* GetStringConstructor() const {TRACE_IT(59801);return stringConstructor; }
        JavascriptFunction* GetArrayBufferConstructor() const {TRACE_IT(59802);return arrayBufferConstructor; }
        JavascriptFunction* GetErrorConstructor() const {TRACE_IT(59803); return errorConstructor; }
        JavascriptFunction* GetInt8ArrayConstructor() const {TRACE_IT(59804);return Int8ArrayConstructor; }
        JavascriptFunction* GetUint8ArrayConstructor() const {TRACE_IT(59805);return Uint8ArrayConstructor; }
        JavascriptFunction* GetInt16ArrayConstructor() const {TRACE_IT(59806);return Int16ArrayConstructor; }
        JavascriptFunction* GetUint16ArrayConstructor() const {TRACE_IT(59807);return Uint16ArrayConstructor; }
        JavascriptFunction* GetInt32ArrayConstructor() const {TRACE_IT(59808);return Int32ArrayConstructor; }
        JavascriptFunction* GetUint32ArrayConstructor() const {TRACE_IT(59809);return Uint32ArrayConstructor; }
        JavascriptFunction* GetFloat32ArrayConstructor() const {TRACE_IT(59810);return Float32ArrayConstructor; }
        JavascriptFunction* GetFloat64ArrayConstructor() const {TRACE_IT(59811);return Float64ArrayConstructor; }
        JavascriptFunction* GetWeakMapConstructor() const {TRACE_IT(59812);return weakMapConstructor; }
        JavascriptFunction* GetMapConstructor() const {TRACE_IT(59813);return mapConstructor; }
        JavascriptFunction* GetSetConstructor() const {TRACE_IT(59814);return  setConstructor; }
        JavascriptFunction* GetSymbolConstructor() const {TRACE_IT(59815);return symbolConstructor; }
        JavascriptFunction* GetEvalFunctionObject() {TRACE_IT(59816); return evalFunctionObject; }
        JavascriptFunction* GetArrayPrototypeValuesFunction() {TRACE_IT(59817); return EnsureArrayPrototypeValuesFunction(); }
        JavascriptFunction* GetArrayIteratorPrototypeBuiltinNextFunction() {TRACE_IT(59818); return arrayIteratorPrototypeBuiltinNextFunction; }
        DynamicObject* GetMathObject() const {TRACE_IT(59819);return mathObject; }
        DynamicObject* GetJSONObject() const {TRACE_IT(59820);return JSONObject; }
        DynamicObject* GetReflectObject() const {TRACE_IT(59821); return reflectObject; }
        const PropertyDescriptor* GetDefaultPropertyDescriptor() const {TRACE_IT(59822); return &defaultPropertyDescriptor; }
        DynamicObject* GetMissingPropertyHolder() const {TRACE_IT(59823); return missingPropertyHolder; }

        JavascriptFunction* GetSharedArrayBufferConstructor() {TRACE_IT(59824); return sharedArrayBufferConstructor; }
        DynamicObject* GetAtomicsObject() {TRACE_IT(59825); return atomicsObject; }

        DynamicObject* GetWebAssemblyCompileErrorPrototype() const {TRACE_IT(59826); return webAssemblyCompileErrorPrototype; }
        DynamicObject* GetWebAssemblyCompileErrorConstructor() const {TRACE_IT(59827); return webAssemblyCompileErrorConstructor; }
        DynamicObject* GetWebAssemblyRuntimeErrorPrototype() const {TRACE_IT(59828); return webAssemblyRuntimeErrorPrototype; }
        DynamicObject* GetWebAssemblyRuntimeErrorConstructor() const {TRACE_IT(59829); return webAssemblyRuntimeErrorConstructor; }
        DynamicObject* GetWebAssemblyLinkErrorPrototype() const {TRACE_IT(59830); return webAssemblyLinkErrorPrototype; }
        DynamicObject* GetWebAssemblyLinkErrorConstructor() const {TRACE_IT(59831); return webAssemblyLinkErrorConstructor; }

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
        DynamicObject* GetINTLObject() const {TRACE_IT(59832); return IntlObject; }
        void ResetIntlObject();
        void EnsureIntlObjectReady();
        template <class Fn>
        void InitializeIntlForPrototypes(Fn fn);
        void InitializeIntlForStringPrototype();
        void InitializeIntlForDatePrototype();
        void InitializeIntlForNumberPrototype();
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        DynamicType * GetDebugDisposableObjectType() {TRACE_IT(59833); return debugDisposableObjectType; }
        DynamicType * GetDebugFuncExecutorInDisposeObjectType() {TRACE_IT(59834); return debugFuncExecutorInDisposeObjectType; }
#endif

        DynamicType* GetErrorType(ErrorTypeEnum typeToCreate) const;
        StaticType  * GetBooleanTypeStatic() const {TRACE_IT(59835); return booleanTypeStatic; }
        DynamicType * GetBooleanTypeDynamic() const {TRACE_IT(59836); return booleanTypeDynamic; }
        DynamicType * GetDateType() const {TRACE_IT(59837); return dateType; }
        DynamicType * GetBoundFunctionType() const {TRACE_IT(59838); return boundFunctionType; }
        DynamicType * GetRegExpConstructorType() const {TRACE_IT(59839); return regexConstructorType; }
        StaticType  * GetEnumeratorType() const {TRACE_IT(59840); return enumeratorType; }
        DynamicType * GetSpreadArgumentType() const {TRACE_IT(59841); return SpreadArgumentType; }
        StaticType  * GetWithType() const {TRACE_IT(59842); return withType; }
        DynamicType * GetErrorType() const {TRACE_IT(59843); return errorType; }
        DynamicType * GetEvalErrorType() const {TRACE_IT(59844); return evalErrorType; }
        DynamicType * GetRangeErrorType() const {TRACE_IT(59845); return rangeErrorType; }
        DynamicType * GetReferenceErrorType() const {TRACE_IT(59846); return referenceErrorType; }
        DynamicType * GetSyntaxErrorType() const {TRACE_IT(59847); return syntaxErrorType; }
        DynamicType * GetTypeErrorType() const {TRACE_IT(59848); return typeErrorType; }
        DynamicType * GetURIErrorType() const {TRACE_IT(59849); return uriErrorType; }
        DynamicType * GetWebAssemblyCompileErrorType() const {TRACE_IT(59850); return webAssemblyCompileErrorType; }
        DynamicType * GetWebAssemblyRuntimeErrorType() const {TRACE_IT(59851); return webAssemblyRuntimeErrorType; }
        DynamicType * GetWebAssemblyLinkErrorType() const {TRACE_IT(59852); return webAssemblyLinkErrorType; }
        StaticType  * GetNumberTypeStatic() const {TRACE_IT(59853); return numberTypeStatic; }
        StaticType  * GetInt64TypeStatic() const {TRACE_IT(59854); return int64NumberTypeStatic; }
        StaticType  * GetUInt64TypeStatic() const {TRACE_IT(59855); return uint64NumberTypeStatic; }
        DynamicType * GetNumberTypeDynamic() const {TRACE_IT(59856); return numberTypeDynamic; }
        DynamicType * GetPromiseType() const {TRACE_IT(59857); return promiseType; }

        DynamicType * GetWebAssemblyModuleType()  const {TRACE_IT(59858); return webAssemblyModuleType; }
        DynamicType * GetWebAssemblyInstanceType()  const {TRACE_IT(59859); return webAssemblyInstanceType; }
        DynamicType * GetWebAssemblyMemoryType() const {TRACE_IT(59860); return webAssemblyMemoryType; }
        DynamicType * GetWebAssemblyTableType() const {TRACE_IT(59861); return webAssemblyTableType; }

        // SIMD_JS
        DynamicType * GetSIMDBool8x16TypeDynamic()  const {TRACE_IT(59862); return simdBool8x16TypeDynamic;  }
        DynamicType * GetSIMDBool16x8TypeDynamic()  const {TRACE_IT(59863); return simdBool16x8TypeDynamic;  }
        DynamicType * GetSIMDBool32x4TypeDynamic()  const {TRACE_IT(59864); return simdBool32x4TypeDynamic;  }
        DynamicType * GetSIMDInt8x16TypeDynamic()   const {TRACE_IT(59865); return simdInt8x16TypeDynamic;   }
        DynamicType * GetSIMDInt16x8TypeDynamic()   const {TRACE_IT(59866); return simdInt16x8TypeDynamic;   }
        DynamicType * GetSIMDInt32x4TypeDynamic()   const {TRACE_IT(59867); return simdInt32x4TypeDynamic;   }
        DynamicType * GetSIMDUint8x16TypeDynamic()  const {TRACE_IT(59868); return simdUint8x16TypeDynamic;  }
        DynamicType * GetSIMDUint16x8TypeDynamic()  const {TRACE_IT(59869); return simdUint16x8TypeDynamic;  }
        DynamicType * GetSIMDUint32x4TypeDynamic()  const {TRACE_IT(59870); return simdUint32x4TypeDynamic;  }
        DynamicType * GetSIMDFloat32x4TypeDynamic() const {TRACE_IT(59871); return simdFloat32x4TypeDynamic; }

        StaticType* GetSIMDFloat32x4TypeStatic() const {TRACE_IT(59872); return simdFloat32x4TypeStatic; }
        StaticType* GetSIMDFloat64x2TypeStatic() const {TRACE_IT(59873); return simdFloat64x2TypeStatic; }
        StaticType* GetSIMDInt32x4TypeStatic()   const {TRACE_IT(59874); return simdInt32x4TypeStatic; }
        StaticType* GetSIMDInt16x8TypeStatic()   const {TRACE_IT(59875); return simdInt16x8TypeStatic; }
        StaticType* GetSIMDInt8x16TypeStatic()   const {TRACE_IT(59876); return simdInt8x16TypeStatic; }
        StaticType* GetSIMDBool32x4TypeStatic() const {TRACE_IT(59877); return simdBool32x4TypeStatic; }
        StaticType* GetSIMDBool16x8TypeStatic() const {TRACE_IT(59878); return simdBool16x8TypeStatic; }
        StaticType* GetSIMDBool8x16TypeStatic() const {TRACE_IT(59879); return simdBool8x16TypeStatic; }
        StaticType* GetSIMDUInt32x4TypeStatic()   const {TRACE_IT(59880); return simdUint32x4TypeStatic; }
        StaticType* GetSIMDUint16x8TypeStatic()   const {TRACE_IT(59881); return simdUint16x8TypeStatic; }
        StaticType* GetSIMDUint8x16TypeStatic()   const {TRACE_IT(59882); return simdUint8x16TypeStatic; }

        DynamicType * GetObjectLiteralType(uint16 requestedInlineSlotCapacity);
        DynamicType * GetObjectHeaderInlinedLiteralType(uint16 requestedInlineSlotCapacity);
        DynamicType * GetObjectType() const {TRACE_IT(59883); return objectTypes[0]; }
        DynamicType * GetObjectHeaderInlinedType() const {TRACE_IT(59884); return objectHeaderInlinedTypes[0]; }
        StaticType  * GetSymbolTypeStatic() const {TRACE_IT(59885); return symbolTypeStatic; }
        DynamicType * GetSymbolTypeDynamic() const {TRACE_IT(59886); return symbolTypeDynamic; }
        DynamicType * GetProxyType() const {TRACE_IT(59887); return proxyType; }
        DynamicType * GetHeapArgumentsObjectType() const {TRACE_IT(59888); return heapArgumentsType; }
        DynamicType * GetActivationObjectType() const {TRACE_IT(59889); return activationObjectType; }
        DynamicType * GetModuleNamespaceType() const {TRACE_IT(59890); return moduleNamespaceType; }
        DynamicType * GetArrayType() const {TRACE_IT(59891); return arrayType; }
        DynamicType * GetNativeIntArrayType() const {TRACE_IT(59892); return nativeIntArrayType; }
#if ENABLE_COPYONACCESS_ARRAY
        DynamicType * GetCopyOnAccessNativeIntArrayType() const {TRACE_IT(59893); return copyOnAccessNativeIntArrayType; }
#endif
        DynamicType * GetNativeFloatArrayType() const {TRACE_IT(59894); return nativeFloatArrayType; }
        DynamicType * GetRegexPrototypeType() const {TRACE_IT(59895); return regexPrototypeType; }
        DynamicType * GetRegexType() const {TRACE_IT(59896); return regexType; }
        DynamicType * GetRegexResultType() const {TRACE_IT(59897); return regexResultType; }
        DynamicType * GetArrayBufferType() const {TRACE_IT(59898); return arrayBufferType; }
        StaticType  * GetStringTypeStatic() const { AssertMsg(stringTypeStatic, "Where's stringTypeStatic?"); return stringTypeStatic; }
        DynamicType * GetStringTypeDynamic() const {TRACE_IT(59899); return stringTypeDynamic; }
        StaticType  * GetVariantDateType() const {TRACE_IT(59900); return variantDateType; }
        void EnsureDebugObject(DynamicObject* newDebugObject);
        DynamicObject* GetDebugObject() const {TRACE_IT(59901); Assert(debugObject != nullptr); return debugObject; }
        DynamicType * GetMapType() const {TRACE_IT(59902); return mapType; }
        DynamicType * GetSetType() const {TRACE_IT(59903); return setType; }
        DynamicType * GetWeakMapType() const {TRACE_IT(59904); return weakMapType; }
        DynamicType * GetWeakSetType() const {TRACE_IT(59905); return weakSetType; }
        DynamicType * GetArrayIteratorType() const {TRACE_IT(59906); return arrayIteratorType; }
        DynamicType * GetMapIteratorType() const {TRACE_IT(59907); return mapIteratorType; }
        DynamicType * GetSetIteratorType() const {TRACE_IT(59908); return setIteratorType; }
        DynamicType * GetStringIteratorType() const {TRACE_IT(59909); return stringIteratorType; }
        DynamicType * GetListIteratorType() const {TRACE_IT(59910); return listIteratorType; }
        JavascriptFunction* GetDefaultAccessorFunction() const {TRACE_IT(59911); return defaultAccessorFunction; }
        JavascriptFunction* GetStackTraceAccessorFunction() const {TRACE_IT(59912); return stackTraceAccessorFunction; }
        JavascriptFunction* GetThrowTypeErrorRestrictedPropertyAccessorFunction() const {TRACE_IT(59913); return throwTypeErrorRestrictedPropertyAccessorFunction; }
        JavascriptFunction* Get__proto__getterFunction() const {TRACE_IT(59914); return __proto__getterFunction; }
        JavascriptFunction* Get__proto__setterFunction() const {TRACE_IT(59915); return __proto__setterFunction; }

        JavascriptFunction* GetObjectValueOfFunction() const {TRACE_IT(59916); return objectValueOfFunction; }
        JavascriptFunction* GetObjectToStringFunction() const {TRACE_IT(59917); return objectToStringFunction; }

        // SIMD_JS
        JavascriptFunction* GetSIMDFloat32x4ToStringFunction() const {TRACE_IT(59918); return simdFloat32x4ToStringFunction;  }
        JavascriptFunction* GetSIMDFloat64x2ToStringFunction() const {TRACE_IT(59919); return simdFloat64x2ToStringFunction; }
        JavascriptFunction* GetSIMDInt32x4ToStringFunction()   const {TRACE_IT(59920); return simdInt32x4ToStringFunction; }
        JavascriptFunction* GetSIMDInt16x8ToStringFunction()   const {TRACE_IT(59921); return simdInt16x8ToStringFunction; }
        JavascriptFunction* GetSIMDInt8x16ToStringFunction()   const {TRACE_IT(59922); return simdInt8x16ToStringFunction; }
        JavascriptFunction* GetSIMDBool32x4ToStringFunction()   const {TRACE_IT(59923); return simdBool32x4ToStringFunction; }
        JavascriptFunction* GetSIMDBool16x8ToStringFunction()   const {TRACE_IT(59924); return simdBool16x8ToStringFunction; }
        JavascriptFunction* GetSIMDBool8x16ToStringFunction()   const {TRACE_IT(59925); return simdBool8x16ToStringFunction; }
        JavascriptFunction* GetSIMDUint32x4ToStringFunction()   const {TRACE_IT(59926); return simdUint32x4ToStringFunction; }
        JavascriptFunction* GetSIMDUint16x8ToStringFunction()   const {TRACE_IT(59927); return simdUint16x8ToStringFunction; }
        JavascriptFunction* GetSIMDUint8x16ToStringFunction()   const {TRACE_IT(59928); return simdUint8x16ToStringFunction; }

        JavascriptFunction* GetDebugObjectNonUserGetterFunction() const {TRACE_IT(59929); return debugObjectNonUserGetterFunction; }
        JavascriptFunction* GetDebugObjectNonUserSetterFunction() const {TRACE_IT(59930); return debugObjectNonUserSetterFunction; }

        UnifiedRegex::RegexPattern * GetEmptyRegexPattern() const {TRACE_IT(59931); return emptyRegexPattern; }
        JavascriptFunction* GetRegexExecFunction() const {TRACE_IT(59932); return regexExecFunction; }
        JavascriptFunction* GetRegexFlagsGetterFunction() const {TRACE_IT(59933); return regexFlagsGetterFunction; }
        JavascriptFunction* GetRegexGlobalGetterFunction() const {TRACE_IT(59934); return regexGlobalGetterFunction; }
        JavascriptFunction* GetRegexStickyGetterFunction() const {TRACE_IT(59935); return regexStickyGetterFunction; }
        JavascriptFunction* GetRegexUnicodeGetterFunction() const {TRACE_IT(59936); return regexUnicodeGetterFunction; }

        int GetRegexConstructorSlotIndex() const {TRACE_IT(59937); return regexConstructorSlotIndex;  }
        int GetRegexExecSlotIndex() const {TRACE_IT(59938); return regexExecSlotIndex;  }
        int GetRegexFlagsGetterSlotIndex() const {TRACE_IT(59939); return regexFlagsGetterSlotIndex;  }
        int GetRegexGlobalGetterSlotIndex() const {TRACE_IT(59940); return regexGlobalGetterSlotIndex;  }
        int GetRegexStickyGetterSlotIndex() const {TRACE_IT(59941); return regexStickyGetterSlotIndex;  }
        int GetRegexUnicodeGetterSlotIndex() const {TRACE_IT(59942); return regexUnicodeGetterSlotIndex;  }

        TypePath* GetRootPath() const {TRACE_IT(59943); return rootPath; }
        void BindReference(void * addr);
        void CleanupForClose();
        void BeginDynamicFunctionReferences();
        void EndDynamicFunctionReferences();
        void RegisterDynamicFunctionReference(FunctionProxy* func);

        void SetDebugObjectNonUserAccessor(FunctionInfo *funcGetter, FunctionInfo *funcSetter);

        JavascriptFunction* GetDebugObjectDebugModeGetterFunction() const {TRACE_IT(59944); return debugObjectDebugModeGetterFunction; }
        void SetDebugObjectDebugModeAccessor(FunctionInfo *funcGetter);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        JavascriptFunction* GetDebugObjectFaultInjectionCookieGetterFunction() const {TRACE_IT(59945); return debugObjectFaultInjectionCookieGetterFunction; }
        JavascriptFunction* GetDebugObjectFaultInjectionCookieSetterFunction() const {TRACE_IT(59946); return debugObjectFaultInjectionCookieSetterFunction; }
        void SetDebugObjectFaultInjectionCookieGetterAccessor(FunctionInfo *funcGetter, FunctionInfo *funcSetter);
#endif

        JavascriptFunction* GetArrayPrototypeToStringFunction() const {TRACE_IT(59947); return arrayPrototypeToStringFunction; }
        JavascriptFunction* GetArrayPrototypeToLocaleStringFunction() const {TRACE_IT(59948); return arrayPrototypeToLocaleStringFunction; }
        JavascriptFunction* GetIdentityFunction() const {TRACE_IT(59949); return identityFunction; }
        JavascriptFunction* GetThrowerFunction() const {TRACE_IT(59950); return throwerFunction; }

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

        template<> inline DynamicType* GetTypedArrayType<int8,false>(int8) {TRACE_IT(59951); return int8ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint8,false>(uint8) {TRACE_IT(59952); return uint8ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint8,true>(uint8) {TRACE_IT(59953); return uint8ClampedArrayType; };
        template<> inline DynamicType* GetTypedArrayType<int16,false>(int16) {TRACE_IT(59954); return int16ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint16,false>(uint16) {TRACE_IT(59955); return uint16ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<int32,false>(int32) {TRACE_IT(59956); return int32ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint32,false>(uint32) {TRACE_IT(59957); return uint32ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<float,false>(float) {TRACE_IT(59958); return float32ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<double,false>(double) {TRACE_IT(59959); return float64ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<int64,false>(int64) {TRACE_IT(59960); return int64ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<uint64,false>(uint64) {TRACE_IT(59961); return uint64ArrayType; };
        template<> inline DynamicType* GetTypedArrayType<bool,false>(bool) {TRACE_IT(59962); return boolArrayType; };

        DynamicType* GetCharArrayType() {TRACE_IT(59963); return charArrayType; };

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

        bool IsPRNGSeeded() {TRACE_IT(59964); return isPRNGSeeded; }
        uint64 GetRandSeed0() {TRACE_IT(59965); return randSeed0; }
        uint64 GetRandSeed1() {TRACE_IT(59966); return randSeed1; }
        void SetIsPRNGSeeded(bool val);
        void SetRandSeed0(uint64 rs) {TRACE_IT(59967); randSeed0 = rs;}
        void SetRandSeed1(uint64 rs) {TRACE_IT(59968); randSeed1 = rs; }

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
        {TRACE_IT(59969);
            Assert(index < _countof(JavascriptLibrary::LibraryFunctionArgC));
            return JavascriptLibrary::LibraryFunctionArgC[index];
        }
        static BuiltInFlags GetFlagsForBuiltIn(BuiltinFunction index)
        {TRACE_IT(59970);
            Assert(index < _countof(JavascriptLibrary::LibraryFunctionFlags));
            return (BuiltInFlags)JavascriptLibrary::LibraryFunctionFlags[index];
        }
        static BuiltinFunction GetBuiltInInlineCandidateId(Js::OpCode opCode);
        static BuiltInArgSpecializationType GetBuiltInArgType(BuiltInFlags flags, BuiltInArgShift argGroup);
        static bool IsTypeSpecRequired(BuiltInFlags flags)
        {TRACE_IT(59971);
            return GetBuiltInArgType(flags, BuiltInArgShift::BIAS_Src1) || GetBuiltInArgType(flags, BuiltInArgShift::BIAS_Src2) || GetBuiltInArgType(flags, BuiltInArgShift::BIAS_Dst);
        }
#if ENABLE_DEBUG_CONFIG_OPTIONS
        static char16 const * GetNameForBuiltIn(BuiltinFunction index)
        {TRACE_IT(59972);
            Assert(index < _countof(JavascriptLibrary::LibraryFunctionName));
            return JavascriptLibrary::LibraryFunctionName[index];
        }
#endif

        PropertyStringCacheMap* EnsurePropertyStringMap();
        PropertyStringCacheMap* GetPropertyStringMap() {TRACE_IT(59973); return this->propertyStringMap; }

        void TypeAndPrototypesAreEnsuredToHaveOnlyWritableDataProperties(Type *const type);
        void NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();

        static bool ArrayIteratorPrototypeHasUserDefinedNext(ScriptContext *scriptContext);

        CharStringCache& GetCharStringCache() {TRACE_IT(59974); return charStringCache;  }
        static JavascriptLibrary * FromCharStringCache(CharStringCache * cache)
        {TRACE_IT(59975);
            return (JavascriptLibrary *)((uintptr_t)cache - offsetof(JavascriptLibrary, charStringCache));
        }

        bool GetArrayObjectHasUserDefinedSpecies() const {TRACE_IT(59976); return arrayObjectHasUserDefinedSpecies; }
        void SetArrayObjectHasUserDefinedSpecies(bool val) {TRACE_IT(59977); arrayObjectHasUserDefinedSpecies = val; }

        FunctionBody* GetFakeGlobalFuncForUndefer()const {TRACE_IT(59978); return this->fakeGlobalFuncForUndefer; }
        void SetFakeGlobalFuncForUndefer(FunctionBody* functionBody) {TRACE_IT(59979); this->fakeGlobalFuncForUndefer = functionBody; }        

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
        {TRACE_IT(59980);
            ReferencedPropertyRecordHashSet* pidList = this->referencedPropertyRecords;
            if (pidList == nullptr)
            {TRACE_IT(59981);
                pidList = RecyclerNew(this->recycler, ReferencedPropertyRecordHashSet, this->recycler, 173);
                this->referencedPropertyRecords = pidList;
            }
            return pidList;
        }

        ReferencedPropertyRecordHashSet * GetReferencedPropertyRecordList() const
        {TRACE_IT(59982);
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
