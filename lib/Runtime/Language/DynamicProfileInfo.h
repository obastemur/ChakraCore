//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// DisableJit-TODO
#if ENABLE_PROFILE_INFO

#ifdef DYNAMIC_PROFILE_MUTATOR
class DynamicProfileMutatorImpl;
#endif

#define PolymorphicInlineCacheUtilizationMinValue 0
#define PolymorphicInlineCacheUtilizationMaxValue 0xFF
#define PolymorphicInlineCacheUtilizationThreshold 0x80
#define PolymorphicInlineCacheUtilizationIncrement 10
#define PolymorphicInlineCacheUtilizationDecrement 1

namespace IR
{
    enum BailOutKind : uint
    {
    #define BAIL_OUT_KIND_LAST(n)               n
    #define BAIL_OUT_KIND(n, ...)               BAIL_OUT_KIND_LAST(n),
    #define BAIL_OUT_KIND_VALUE_LAST(n, v)      n = v
    #define BAIL_OUT_KIND_VALUE(n, v)           BAIL_OUT_KIND_VALUE_LAST(n, v),
    #include "BailOutKind.h"
    };
    ENUM_CLASS_HELPERS(BailOutKind, uint);

    CompileAssert(BailOutKind::BailOutKindEnd < BailOutKind::BailOutKindBitsStart);

    bool IsTypeCheckBailOutKind(BailOutKind kind);
    bool IsEquivalentTypeCheckBailOutKind(BailOutKind kind);
    BailOutKind EquivalentToMonoTypeCheckBailOutKind(BailOutKind kind);
}

#if ENABLE_DEBUG_CONFIG_OPTIONS
const char *GetBailOutKindName(IR::BailOutKind kind);
bool IsValidBailOutKindAndBits(IR::BailOutKind bailOutKind);
#endif

namespace Js
{
    enum CacheType : byte;
    enum SlotType : byte;

    struct PolymorphicCallSiteInfo;
    // Information about dynamic profile information loaded from cache.
    // Used to verify whether the loaded information matches the paired function body.
    class DynamicProfileFunctionInfo
    {
    public:
        Field(Js::ArgSlot) paramInfoCount;
        Field(ProfileId) ldElemInfoCount;
        Field(ProfileId) stElemInfoCount;
        Field(ProfileId) arrayCallSiteCount;
        Field(ProfileId) slotInfoCount;
        Field(ProfileId) callSiteInfoCount;
        Field(ProfileId) returnTypeInfoCount;
        Field(ProfileId) divCount;
        Field(ProfileId) switchCount;
        Field(uint) loopCount;
        Field(uint) fldInfoCount;
    };

    enum ThisType : BYTE
    {
        ThisType_Unknown = 0,
        ThisType_Simple,
        ThisType_Mapped
    };

    struct ThisInfo
    {
        Field(ValueType) valueType;
        Field(ThisType) thisType;

        ThisInfo() : thisType(ThisType_Unknown)
        {TRACE_IT(47820);
        }
    };

    struct CallSiteInfo
    {
        Field(ValueType) returnType;
        Field(uint16) isArgConstant : 13;
        Field(uint16) isConstructorCall : 1;
        Field(uint16) dontInline : 1;
        Field(uint16) isPolymorphic : 1;
        Field(InlineCacheIndex) ldFldInlineCacheId;
        union _u_type
        {
            struct
            {
                Field(Js::SourceId) sourceId;
                Field(Js::LocalFunctionId) functionId;
            } functionData;
            // As of now polymorphic info is allocated only if the source Id is current
            Field(PolymorphicCallSiteInfo*) polymorphicCallSiteInfo;
            _u_type() {TRACE_IT(47821);}
        } u;
    };


    // TODO: include ImplicitCallFlags in this structure
    struct LoopFlags
    {
        // maintain the bits and the enum at the same time, it must match
        bool isInterpreted : 1;
        bool memopMinCountReached : 1;
        enum
        {
            INTERPRETED,
            MEMOP_MIN_COUNT_FOUND,
            COUNT
        };

        LoopFlags() :
            isInterpreted(false),
            memopMinCountReached(false)
        {TRACE_IT(47822);
            CompileAssert((sizeof(LoopFlags) * 8) >= LoopFlags::COUNT);
        }
        // Right now supports up to 8 bits.
        typedef byte LoopFlags_t;
        LoopFlags(uint64 flags)
        {TRACE_IT(47823);
            Assert(flags >> LoopFlags::COUNT == 0);
            LoopFlags_t* thisFlags = (LoopFlags_t *)this;
            CompileAssert(sizeof(LoopFlags_t) == sizeof(LoopFlags));
            *thisFlags = (LoopFlags_t)flags;
        }
    };

    enum FldInfoFlags : BYTE
    {
        FldInfo_NoInfo                      = 0x00,
        FldInfo_FromLocal                   = 0x01,
        FldInfo_FromProto                   = 0x02,
        FldInfo_FromLocalWithoutProperty    = 0x04,
        FldInfo_FromAccessor                = 0x08,
        FldInfo_Polymorphic                 = 0x10,
        FldInfo_FromInlineSlots             = 0x20,
        FldInfo_FromAuxSlots                = 0x40,
        FldInfo_InlineCandidate             = 0x80
    };

    struct FldInfo
    {
        typedef struct { ValueType::TSize f1; byte f2; byte f3; } TSize;

        ValueType valueType;
        FldInfoFlags flags;
        byte polymorphicInlineCacheUtilization;

        bool ShouldUsePolymorphicInlineCache()
        {TRACE_IT(47824);
#if DBG
            if (PHASE_FORCE1(PolymorphicInlineCachePhase))
            {TRACE_IT(47825);
                return true;
            }
#endif
            return polymorphicInlineCacheUtilization > PolymorphicInlineCacheUtilizationThreshold;
        }

        bool WasLdFldProfiled() const
        {TRACE_IT(47826);
            return !valueType.IsUninitialized();
        }

        static uint32 GetOffsetOfFlags() {TRACE_IT(47827); return offsetof(FldInfo, flags); }
    };
    CompileAssert(sizeof(FldInfo::TSize) == sizeof(FldInfo));

    struct LdElemInfo
    {
        ValueType arrayType;
        ValueType elemType;

        union
        {
            struct
            {
                bool wasProfiled : 1;
                bool neededHelperCall : 1;
            };
            byte bits;
        };

        LdElemInfo() : bits(0)
        {TRACE_IT(47828);
            wasProfiled = true;
        }

        void Merge(const LdElemInfo &other)
        {TRACE_IT(47829);
            arrayType = arrayType.Merge(other.arrayType);
            elemType = elemType.Merge(other.elemType);
            bits |= other.bits;
        }

        ValueType GetArrayType() const
        {TRACE_IT(47830);
            return arrayType;
        }

        ValueType GetElementType() const
        {TRACE_IT(47831);
            return elemType;
        }

        bool WasProfiled() const
        {TRACE_IT(47832);
            return wasProfiled;
        }

        bool LikelyNeedsHelperCall() const
        {TRACE_IT(47833);
            return neededHelperCall;
        }
    };

    struct StElemInfo
    {
        ValueType arrayType;

        union
        {
            struct
            {
                bool wasProfiled : 1;
                bool createdMissingValue : 1;
                bool filledMissingValue : 1;
                bool neededHelperCall : 1;
                bool storedOutsideHeadSegmentBounds : 1;
                bool storedOutsideArrayBounds : 1;
            };
            byte bits;
        };

        StElemInfo() : bits(0)
        {TRACE_IT(47834);
            wasProfiled = true;
        }

        void Merge(const StElemInfo &other)
        {TRACE_IT(47835);
            arrayType = arrayType.Merge(other.arrayType);
            bits |= other.bits;
        }

        ValueType GetArrayType() const
        {TRACE_IT(47836);
            return arrayType;
        }

        bool WasProfiled() const
        {TRACE_IT(47837);
            return wasProfiled;
        }

        bool LikelyCreatesMissingValue() const
        {TRACE_IT(47838);
            return createdMissingValue;
        }

        bool LikelyFillsMissingValue() const
        {TRACE_IT(47839);
            return filledMissingValue;
        }

        bool LikelyNeedsHelperCall() const
        {TRACE_IT(47840);
            return createdMissingValue || filledMissingValue || neededHelperCall || storedOutsideHeadSegmentBounds;
        }

        bool LikelyStoresOutsideHeadSegmentBounds() const
        {TRACE_IT(47841);
            return createdMissingValue || storedOutsideHeadSegmentBounds;
        }

        bool LikelyStoresOutsideArrayBounds() const
        {TRACE_IT(47842);
            return storedOutsideArrayBounds;
        }
    };

    struct ArrayCallSiteInfo
    {
        union {
            struct {
                byte isNotNativeInt : 1;
                byte isNotNativeFloat : 1;
#if ENABLE_COPYONACCESS_ARRAY
                byte isNotCopyOnAccessArray : 1;
                byte copyOnAccessArrayCacheIndex : 5;
#endif
            };
            byte bits;
        };
#if DBG
        uint functionNumber;
        ProfileId callSiteNumber;
#endif

        bool IsNativeIntArray() const {TRACE_IT(47843); return !(bits & NotNativeIntBit) && !PHASE_OFF1(NativeArrayPhase); }
        bool IsNativeFloatArray() const {TRACE_IT(47844); return !(bits & NotNativeFloatBit) && !PHASE_OFF1(NativeArrayPhase); }
        bool IsNativeArray() const {TRACE_IT(47845); return IsNativeFloatArray(); }
        void SetIsNotNativeIntArray();
        void SetIsNotNativeFloatArray();
        void SetIsNotNativeArray();

        static uint32 GetOffsetOfBits() {TRACE_IT(47846); return offsetof(ArrayCallSiteInfo, bits); }
        static byte const NotNativeIntBit = 1;
        static byte const NotNativeFloatBit = 2;
    };

    class DynamicProfileInfo;
    typedef SListBase<DynamicProfileInfo*, Recycler> DynamicProfileInfoList;

    class DynamicProfileInfo
    {
    public:
        static DynamicProfileInfo* New(Recycler* recycler, FunctionBody* functionBody, bool persistsAcrossScriptContexts = false);
        void Initialize(FunctionBody *const functionBody);

    public:
        static bool IsEnabledForAtLeastOneFunction(const ScriptContext *const scriptContext);
        static bool IsEnabled(const FunctionBody *const functionBody);
    private:
        static bool IsEnabled_OptionalFunctionBody(const FunctionBody *const functionBody, const ScriptContext *const scriptContext);

    public:
        static bool IsEnabledForAtLeastOneFunction(const Js::Phase phase, const ScriptContext *const scriptContext);
        static bool IsEnabled(const Js::Phase phase, const FunctionBody *const functionBody);
    private:
        static bool IsEnabled_OptionalFunctionBody(const Js::Phase phase, const FunctionBody *const functionBody, const ScriptContext *const scriptContext);

    public:
        static bool EnableImplicitCallFlags(const FunctionBody *const functionBody);

        static Var EnsureDynamicProfileInfoThunk(RecyclableObject * function, CallInfo callInfo, ...);

#ifdef DYNAMIC_PROFILE_STORAGE
        bool HasFunctionBody() const {TRACE_IT(47847); return hasFunctionBody; }
        FunctionBody * GetFunctionBody() const {TRACE_IT(47848); Assert(hasFunctionBody); return functionBody; }
#endif

        void RecordElementLoad(FunctionBody* functionBody, ProfileId ldElemId, const LdElemInfo& info);
        void RecordElementLoadAsProfiled(FunctionBody *const functionBody, const ProfileId ldElemId);
        const LdElemInfo *GetLdElemInfo() const {TRACE_IT(47849); return ldElemInfo; }

        void RecordElementStore(FunctionBody* functionBody, ProfileId stElemId, const StElemInfo& info);
        void RecordElementStoreAsProfiled(FunctionBody *const functionBody, const ProfileId stElemId);
        const StElemInfo *GetStElemInfo() const {TRACE_IT(47850); return stElemInfo; }

        ArrayCallSiteInfo *GetArrayCallSiteInfo(FunctionBody *functionBody, ProfileId index) const;
        ArrayCallSiteInfo *GetArrayCallSiteInfo() const {TRACE_IT(47851); return arrayCallSiteInfo; }

        void RecordFieldAccess(FunctionBody* functionBody, uint fieldAccessId, Var object, FldInfoFlags flags);
        void RecordPolymorphicFieldAccess(FunctionBody *functionBody, uint fieldAccessid);
        bool HasPolymorphicFldAccess() const {TRACE_IT(47852); return bits.hasPolymorphicFldAccess; }
        FldInfo * GetFldInfo(FunctionBody* functionBody, uint fieldAccessId) const;
        FldInfo * GetFldInfo() const {TRACE_IT(47853); return fldInfo; }

        void RecordSlotLoad(FunctionBody* functionBody, ProfileId slotLoadId, Var object);
        ValueType GetSlotLoad(FunctionBody* functionBody, ProfileId slotLoadId) const;
        ValueType * GetSlotInfo() const {TRACE_IT(47854); return slotInfo; }

        void RecordThisInfo(Var object, ThisType thisType);
        ThisInfo GetThisInfo() const;

        void RecordDivideResultType(FunctionBody* body, ProfileId divideId, Var object);
        ValueType GetDivideResultType(FunctionBody* body, ProfileId divideId) const;
        ValueType * GetDivideTypeInfo() const {TRACE_IT(47855); return divideTypeInfo; }

        void RecordModulusOpType(FunctionBody* body, ProfileId profileId, bool isModByPowerOf2);

        bool IsModulusOpByPowerOf2(FunctionBody* body, ProfileId profileId) const;

        void RecordSwitchType(FunctionBody* body, ProfileId switchId, Var object);
        ValueType GetSwitchType(FunctionBody* body, ProfileId switchId) const;
        ValueType * GetSwitchTypeInfo() const {TRACE_IT(47856); return switchTypeInfo; }

        void RecordCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, FunctionInfo * calleeFunctionInfo, JavascriptFunction* calleeFunction, ArgSlot actualArgCount, bool isConstructorCall, InlineCacheIndex ldFldInlineCacheId = Js::Constants::NoInlineCacheIndex);
        void RecordConstParameterAtCallSite(ProfileId callSiteId, int argNum);
        static bool HasCallSiteInfo(FunctionBody* functionBody);
        bool HasCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId); // Does a particular callsite have ProfileInfo?
        FunctionInfo * GetCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, bool *isConstructorCall, bool *isPolymorphicCall);
        CallSiteInfo * GetCallSiteInfo() const {TRACE_IT(47857); return callSiteInfo; }
        uint16 GetConstantArgInfo(ProfileId callSiteId);
        uint GetLdFldCacheIndexFromCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId);
        bool GetPolymorphicCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, bool *isConstructorCall, __inout_ecount(functionBodyArrayLength) FunctionBody** functionBodyArray, uint functionBodyArrayLength);

        bool RecordLdFldCallSiteInfo(FunctionBody* functionBody, RecyclableObject* callee, bool callApplyTarget);

        bool HasLdFldCallSiteInfo() const;

        void RecordReturnTypeOnCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, Var object);
        void RecordReturnType(FunctionBody* functionBody, ProfileId callSiteId, Var object);
        ValueType GetReturnType(FunctionBody* functionBody, Js::OpCode opcode, ProfileId callSiteId) const;
        ValueType * GetReturnTypeInfo() const {TRACE_IT(47858); return returnTypeInfo; }

        void RecordParameterInfo(FunctionBody* functionBody, ArgSlot index, Var object);
        ValueType GetParameterInfo(FunctionBody* functionBody, ArgSlot index) const;
        ValueType * GetParameterInfo() const {TRACE_IT(47859); return parameterInfo; }

        void RecordLoopImplicitCallFlags(FunctionBody* functionBody, uint loopNum, ImplicitCallFlags flags);
        ImplicitCallFlags GetLoopImplicitCallFlags(FunctionBody* functionBody, uint loopNum) const;
        ImplicitCallFlags * GetLoopImplicitCallFlags() const {TRACE_IT(47860); return loopImplicitCallFlags; }

        void RecordImplicitCallFlags(ImplicitCallFlags flags);
        ImplicitCallFlags GetImplicitCallFlags() const;

        static void Save(ScriptContext * scriptContext);

        void UpdateFunctionInfo(FunctionBody* functionBody, Recycler* allocator);
        void    ResetAllPolymorphicCallSiteInfo();

        bool CallSiteHasProfileData(ProfileId callSiteId)
        {TRACE_IT(47861);
            return this->callSiteInfo[callSiteId].isPolymorphic
                || this->callSiteInfo[callSiteId].u.functionData.sourceId != NoSourceId
                || this->callSiteInfo[callSiteId].dontInline;
        }

        static bool IsProfiledCallOp(OpCode op);
        static bool IsProfiledReturnTypeOp(OpCode op);

        static FldInfoFlags FldInfoFlagsFromCacheType(CacheType cacheType);
        static FldInfoFlags FldInfoFlagsFromSlotType(SlotType slotType);
        static FldInfoFlags MergeFldInfoFlags(FldInfoFlags oldFlags, FldInfoFlags newFlags);

        const static uint maxPolymorphicInliningSize = 4;

#if DBG_DUMP
        static void DumpScriptContext(ScriptContext * scriptContext);
        static char16 const * GetImplicitCallFlagsString(ImplicitCallFlags flags);
#endif
#ifdef RUNTIME_DATA_COLLECTION
        static void DumpScriptContextToFile(ScriptContext * scriptContext);
#endif
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        static bool NeedProfileInfoList();
#endif
#ifdef DYNAMIC_PROFILE_MUTATOR
        friend class DynamicProfileMutatorImpl;
#endif
#if JS_PROFILE_DATA_INTERFACE
        friend class ProfileDataObject;
#endif
    private:
        // Have the dynamicProfileFunctionInfo after loaded from cache.
        // Replaced with the function body it is verified and matched (See DynamicProfileInfo::MatchFunctionBody)
        Field(DynamicProfileFunctionInfo *) dynamicProfileFunctionInfo;
        Field(CallSiteInfo *) callSiteInfo;
        Field(ValueType *) returnTypeInfo; // return type of calls for non inline call sites
        Field(ValueType *) divideTypeInfo;
        Field(ValueType *) switchTypeInfo;
        Field(LdElemInfo *) ldElemInfo;
        Field(StElemInfo *) stElemInfo;
        Field(ArrayCallSiteInfo *) arrayCallSiteInfo;
        Field(ValueType *) parameterInfo;
        Field(FldInfo *) fldInfo;
        Field(ValueType *) slotInfo;
        Field(ImplicitCallFlags *) loopImplicitCallFlags;
        Field(ImplicitCallFlags) implicitCallFlags;
        Field(BVFixed*) loopFlags;
        Field(ThisInfo) thisInfo;

        // TODO (jedmiad): Consider storing a pair of property ID bit vectors indicating which properties are
        // known to be non-fixed or non-equivalent. We could turn these on if we bailed out of fixed field type
        // checks and equivalent type checks in a way that indicates one of these failures as opposed to type
        // mismatch.

        struct Bits
        {
            Field(bool) disableAggressiveIntTypeSpec : 1;
            Field(bool) disableAggressiveIntTypeSpec_jitLoopBody : 1;
            Field(bool) disableAggressiveMulIntTypeSpec : 1;
            Field(bool) disableAggressiveMulIntTypeSpec_jitLoopBody : 1;
            Field(bool) disableDivIntTypeSpec : 1;
            Field(bool) disableDivIntTypeSpec_jitLoopBody : 1;
            Field(bool) disableLossyIntTypeSpec : 1;
            // TODO: put this flag in LoopFlags if we can find a reliable way to determine the loopNumber in bailout for a hoisted instr
            Field(bool) disableMemOp : 1;
            Field(bool) disableTrackCompoundedIntOverflow : 1;
            Field(bool) disableFloatTypeSpec : 1;
            Field(bool) disableCheckThis : 1;
            Field(bool) disableArrayCheckHoist : 1;
            Field(bool) disableArrayCheckHoist_jitLoopBody : 1;
            Field(bool) disableArrayMissingValueCheckHoist : 1;
            Field(bool) disableArrayMissingValueCheckHoist_jitLoopBody : 1;
            Field(bool) disableJsArraySegmentHoist : 1;
            Field(bool) disableJsArraySegmentHoist_jitLoopBody : 1;
            Field(bool) disableArrayLengthHoist : 1;
            Field(bool) disableArrayLengthHoist_jitLoopBody : 1;
            Field(bool) disableTypedArrayTypeSpec : 1;
            Field(bool) disableTypedArrayTypeSpec_jitLoopBody : 1;
            Field(bool) disableLdLenIntSpec : 1;
            Field(bool) disableBoundCheckHoist : 1;
            Field(bool) disableBoundCheckHoist_jitLoopBody : 1;
            Field(bool) disableLoopCountBasedBoundCheckHoist : 1;
            Field(bool) disableLoopCountBasedBoundCheckHoist_jitLoopBody : 1;
            Field(bool) hasPolymorphicFldAccess : 1;
            Field(bool) hasLdFldCallSite : 1; // getters, setters, .apply (possibly .call too in future)
            Field(bool) disableFloorInlining : 1;
            Field(bool) disableNoProfileBailouts : 1;
            Field(bool) disableSwitchOpt : 1;
            Field(bool) disableEquivalentObjTypeSpec : 1;
            Field(bool) disableObjTypeSpec_jitLoopBody : 1;
            Field(bool) disablePowIntIntTypeSpec : 1;
            Field(bool) disableLoopImplicitCallInfo : 1;
            Field(bool) disableStackArgOpt : 1;
            Field(bool) disableTagCheck : 1;
        };
        Field(Bits) bits;

        Field(uint32) m_recursiveInlineInfo; // Bit is set for each callsites where the function is called recursively
        Field(uint32) polymorphicCacheState;
        Field(uint32) bailOutOffsetForLastRejit;
        Field(bool) hasFunctionBody;  // this is likely 1, try avoid 4-byte GC force reference
        Field(BYTE) currentInlinerVersion; // Used to detect when inlining profile changes
        Field(uint16) rejitCount;
#if DBG
        Field(bool) persistsAcrossScriptContexts;
#endif

        static JavascriptMethod EnsureDynamicProfileInfo(Js::ScriptFunction * function);
#if DBG_DUMP
        static void DumpList(DynamicProfileInfoList * profileInfoList, ArenaAllocator * dynamicProfileInfoAllocator);
        static void DumpProfiledValue(char16 const * name, uint * value, uint count);
        static void DumpProfiledValue(char16 const * name, ValueType * value, uint count);
        static void DumpProfiledValue(char16 const * name, CallSiteInfo * callSiteInfo, uint count);
        static void DumpProfiledValue(char16 const * name, ArrayCallSiteInfo * arrayCallSiteInfo, uint count);
        static void DumpProfiledValue(char16 const * name, ImplicitCallFlags * loopImplicitCallFlags, uint count);
        template<class TData, class FGetValueType>
        static void DumpProfiledValuesGroupedByValue(const char16 *const name, const TData *const data, const uint count, const FGetValueType GetValueType, ArenaAllocator *const dynamicProfileInfoAllocator);
        static void DumpFldInfoFlags(char16 const * name, FldInfo * fldInfo, uint count, FldInfoFlags value, char16 const * valueName);

        static void DumpLoopInfo(FunctionBody *fbody);
#endif

        bool IsPolymorphicCallSite(Js::LocalFunctionId curFunctionId, Js::SourceId curSourceId, Js::LocalFunctionId oldFunctionId, Js::SourceId oldSourceId);
        void CreatePolymorphicDynamicProfileCallSiteInfo(FunctionBody * funcBody, ProfileId callSiteId, Js::LocalFunctionId functionId, Js::LocalFunctionId oldFunctionId, Js::SourceId sourceId, Js::SourceId oldSourceId);
        void ResetPolymorphicCallSiteInfo(ProfileId callSiteId, Js::LocalFunctionId functionId);
        void SetFunctionIdSlotForNewPolymorphicCall(ProfileId callSiteId, Js::LocalFunctionId curFunctionId, Js::SourceId curSourceId, Js::FunctionBody *inliner);
        void RecordPolymorphicCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, FunctionInfo * calleeFunctionInfo);
#ifdef RUNTIME_DATA_COLLECTION
        static CriticalSection s_csOutput;
        template <typename T>
        static void WriteData(const T& data, FILE * file);
#if defined(_MSC_VER) && !defined(__clang__)
        template <>
        static void WriteData<char16 const *>(char16 const * const& sz, FILE * file);
        template <>
        static void WriteData<FunctionInfo *>(FunctionInfo * const& functionInfo, FILE * file); // Not defined, to prevent accidentally writing function info
        template <>
        static void WriteData<FunctionBody *>(FunctionBody * const& functionInfo, FILE * file);
#endif
        template <typename T>
        static void WriteArray(uint count, T * arr, FILE * file);
        template <typename T>
        static void WriteArray(uint count, WriteBarrierPtr<T> arr, FILE * file);
#endif
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        Field(FunctionBody *) functionBody; // This will only be populated if NeedProfileInfoList is true
#endif
#ifdef DYNAMIC_PROFILE_STORAGE
        // Used by de-serialize
        DynamicProfileInfo();

        template <typename T>
        static DynamicProfileInfo * Deserialize(T * reader, Recycler* allocator, Js::LocalFunctionId * functionId);
        template <typename T>
        bool Serialize(T * writer);

        static void UpdateSourceDynamicProfileManagers(ScriptContext * scriptContext);
#endif
        static Js::LocalFunctionId const CallSiteMixed = (Js::LocalFunctionId)-1;
        static Js::LocalFunctionId const CallSiteCrossContext = (Js::LocalFunctionId)-2;
        static Js::LocalFunctionId const CallSiteNonFunction = (Js::LocalFunctionId)-3;
        static Js::LocalFunctionId const CallSiteNoInfo = (Js::LocalFunctionId)-4;
        static Js::LocalFunctionId const StartInvalidFunction = (Js::LocalFunctionId)-4;

        static Js::SourceId const NoSourceId        = (SourceId)-1;
        static Js::SourceId const BuiltInSourceId   = (SourceId)-2;
        static Js::SourceId const CurrentSourceId   = (SourceId)-3; // caller and callee in the same file
        static Js::SourceId const InvalidSourceId   = (SourceId)-4;

        bool MatchFunctionBody(FunctionBody * functionBody);

        DynamicProfileInfo(FunctionBody * functionBody);

        friend class SourceDynamicProfileManager;

    public:
        bool IsAggressiveIntTypeSpecDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47862);
            return
                isJitLoopBody
                    ? this->bits.disableAggressiveIntTypeSpec_jitLoopBody
                    : this->bits.disableAggressiveIntTypeSpec;
        }

        void DisableAggressiveIntTypeSpec(const bool isJitLoopBody)
        {TRACE_IT(47863);
            this->bits.disableAggressiveIntTypeSpec_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47864);
                this->bits.disableAggressiveIntTypeSpec = true;
            }
        }

        bool IsAggressiveMulIntTypeSpecDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47865);
            return
                isJitLoopBody
                    ? this->bits.disableAggressiveMulIntTypeSpec_jitLoopBody
                    : this->bits.disableAggressiveMulIntTypeSpec;
        }

        void DisableAggressiveMulIntTypeSpec(const bool isJitLoopBody)
        {TRACE_IT(47866);
            this->bits.disableAggressiveMulIntTypeSpec_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47867);
                this->bits.disableAggressiveMulIntTypeSpec = true;
            }
        }

        bool IsDivIntTypeSpecDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47868);
            return
                isJitLoopBody
                    ? this->bits.disableDivIntTypeSpec_jitLoopBody
                    : this->bits.disableDivIntTypeSpec;
        }

        void DisableDivIntTypeSpec(const bool isJitLoopBody)
        {TRACE_IT(47869);
            this->bits.disableDivIntTypeSpec_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47870);
                this->bits.disableDivIntTypeSpec = true;
            }
        }

        bool IsLossyIntTypeSpecDisabled() const {TRACE_IT(47871); return bits.disableLossyIntTypeSpec; }
        void DisableLossyIntTypeSpec() {TRACE_IT(47872); this->bits.disableLossyIntTypeSpec = true; }
        LoopFlags GetLoopFlags(int loopNumber) const
        {TRACE_IT(47873);
            Assert(loopFlags);
            return loopFlags->GetRange<LoopFlags>(loopNumber * LoopFlags::COUNT, LoopFlags::COUNT);
        }
        BVFixed * GetLoopFlags() const {TRACE_IT(47874); return loopFlags; }

        void SetLoopInterpreted(int loopNumber) {TRACE_IT(47875); loopFlags->Set(loopNumber * LoopFlags::COUNT + LoopFlags::INTERPRETED); }
        void SetMemOpMinReached(int loopNumber) {TRACE_IT(47876); loopFlags->Set(loopNumber * LoopFlags::COUNT + LoopFlags::MEMOP_MIN_COUNT_FOUND); }
        bool IsMemOpDisabled() const {TRACE_IT(47877); return this->bits.disableMemOp; }
        void DisableMemOp() {TRACE_IT(47878); this->bits.disableMemOp = true; }
        bool IsTrackCompoundedIntOverflowDisabled() const {TRACE_IT(47879); return this->bits.disableTrackCompoundedIntOverflow; }
        void DisableTrackCompoundedIntOverflow() {TRACE_IT(47880); this->bits.disableTrackCompoundedIntOverflow = true; }
        bool IsFloatTypeSpecDisabled() const {TRACE_IT(47881); return this->bits.disableFloatTypeSpec; }
        void DisableFloatTypeSpec() {TRACE_IT(47882); this->bits.disableFloatTypeSpec = true; }
        bool IsCheckThisDisabled() const {TRACE_IT(47883); return this->bits.disableCheckThis; }
        void DisableCheckThis() {TRACE_IT(47884); this->bits.disableCheckThis = true; }
        bool IsLoopImplicitCallInfoDisabled() const {TRACE_IT(47885); return this->bits.disableLoopImplicitCallInfo; }
        void DisableLoopImplicitCallInfo() {TRACE_IT(47886); this->bits.disableLoopImplicitCallInfo = true; }

        bool IsArrayCheckHoistDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47887);
            return
                isJitLoopBody
                    ? this->bits.disableArrayCheckHoist_jitLoopBody
                    : this->bits.disableArrayCheckHoist;
        }

        void DisableArrayCheckHoist(const bool isJitLoopBody)
        {TRACE_IT(47888);
            this->bits.disableArrayCheckHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47889);
                this->bits.disableArrayCheckHoist = true;
            }
        }

        bool IsArrayMissingValueCheckHoistDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47890);
            return
                isJitLoopBody
                    ? this->bits.disableArrayMissingValueCheckHoist_jitLoopBody
                    : this->bits.disableArrayMissingValueCheckHoist;
        }

        void DisableArrayMissingValueCheckHoist(const bool isJitLoopBody)
        {TRACE_IT(47891);
            this->bits.disableArrayMissingValueCheckHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47892);
                this->bits.disableArrayMissingValueCheckHoist = true;
            }
        }

        bool IsJsArraySegmentHoistDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47893);
            return
                isJitLoopBody
                    ? this->bits.disableJsArraySegmentHoist_jitLoopBody
                    : this->bits.disableJsArraySegmentHoist;
        }

        void DisableJsArraySegmentHoist(const bool isJitLoopBody)
        {TRACE_IT(47894);
            this->bits.disableJsArraySegmentHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47895);
                this->bits.disableJsArraySegmentHoist = true;
            }
        }

        bool IsArrayLengthHoistDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47896);
            return
                isJitLoopBody
                    ? this->bits.disableArrayLengthHoist_jitLoopBody
                    : this->bits.disableArrayLengthHoist;
        }

        void DisableArrayLengthHoist(const bool isJitLoopBody)
        {TRACE_IT(47897);
            this->bits.disableArrayLengthHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47898);
                this->bits.disableArrayLengthHoist = true;
            }
        }

        bool IsTypedArrayTypeSpecDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47899);
            return
                isJitLoopBody
                    ? this->bits.disableTypedArrayTypeSpec_jitLoopBody
                    : this->bits.disableTypedArrayTypeSpec;
        }

        void DisableTypedArrayTypeSpec(const bool isJitLoopBody)
        {TRACE_IT(47900);
            this->bits.disableTypedArrayTypeSpec_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47901);
                this->bits.disableTypedArrayTypeSpec = true;
            }
        }

        bool IsLdLenIntSpecDisabled() const {TRACE_IT(47902); return this->bits.disableLdLenIntSpec; }
        void DisableLdLenIntSpec() {TRACE_IT(47903); this->bits.disableLdLenIntSpec = true; }

        bool IsBoundCheckHoistDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47904);
            return
                isJitLoopBody
                    ? this->bits.disableBoundCheckHoist_jitLoopBody
                    : this->bits.disableBoundCheckHoist;
        }

        void DisableBoundCheckHoist(const bool isJitLoopBody)
        {TRACE_IT(47905);
            this->bits.disableBoundCheckHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47906);
                this->bits.disableBoundCheckHoist = true;
            }
        }

        bool IsLoopCountBasedBoundCheckHoistDisabled(const bool isJitLoopBody) const
        {TRACE_IT(47907);
            return
                isJitLoopBody
                    ? this->bits.disableLoopCountBasedBoundCheckHoist_jitLoopBody
                    : this->bits.disableLoopCountBasedBoundCheckHoist;
        }

        void DisableLoopCountBasedBoundCheckHoist(const bool isJitLoopBody)
        {TRACE_IT(47908);
            this->bits.disableLoopCountBasedBoundCheckHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {TRACE_IT(47909);
                this->bits.disableLoopCountBasedBoundCheckHoist = true;
            }
        }

        BYTE GetInlinerVersion() {TRACE_IT(47910); return this->currentInlinerVersion; }
        uint32 GetPolymorphicCacheState() const {TRACE_IT(47911); return this->polymorphicCacheState; }
        uint32 GetRecursiveInlineInfo() const {TRACE_IT(47912); return this->m_recursiveInlineInfo; }
        void SetHasNewPolyFieldAccess(FunctionBody *functionBody);
        bool IsFloorInliningDisabled() const {TRACE_IT(47913); return this->bits.disableFloorInlining; }
        void DisableFloorInlining() {TRACE_IT(47914); this->bits.disableFloorInlining = true; }
        bool IsNoProfileBailoutsDisabled() const {TRACE_IT(47915); return this->bits.disableNoProfileBailouts; }
        void DisableNoProfileBailouts() {TRACE_IT(47916); this->bits.disableNoProfileBailouts = true; }
        bool IsSwitchOptDisabled() const {TRACE_IT(47917); return this->bits.disableSwitchOpt; }
        void DisableSwitchOpt() {TRACE_IT(47918); this->bits.disableSwitchOpt = true; }
        bool IsStackArgOptDisabled() const {TRACE_IT(47919); return this->bits.disableStackArgOpt; }
        void DisableStackArgOpt() {TRACE_IT(47920); this->bits.disableStackArgOpt = true; }
        bool IsEquivalentObjTypeSpecDisabled() const {TRACE_IT(47921); return this->bits.disableEquivalentObjTypeSpec; }
        void DisableEquivalentObjTypeSpec() {TRACE_IT(47922); this->bits.disableEquivalentObjTypeSpec = true; }
        bool IsObjTypeSpecDisabledInJitLoopBody() const {TRACE_IT(47923); return this->bits.disableObjTypeSpec_jitLoopBody; }
        void DisableObjTypeSpecInJitLoopBody() {TRACE_IT(47924); this->bits.disableObjTypeSpec_jitLoopBody = true; }
        bool IsPowIntIntTypeSpecDisabled() const {TRACE_IT(47925); return bits.disablePowIntIntTypeSpec; }
        void DisablePowIntIntTypeSpec() {TRACE_IT(47926); this->bits.disablePowIntIntTypeSpec = true; }
        bool IsTagCheckDisabled() const {TRACE_IT(47927); return bits.disableTagCheck; }
        void DisableTagCheck() {TRACE_IT(47928); this->bits.disableTagCheck = true; }

        static bool IsCallSiteNoInfo(Js::LocalFunctionId functionId) {TRACE_IT(47929); return functionId == CallSiteNoInfo; }
        int IncRejitCount() {TRACE_IT(47930); return this->rejitCount++; }
        int GetRejitCount() {TRACE_IT(47931); return this->rejitCount; }
        void SetBailOutOffsetForLastRejit(uint32 offset) {TRACE_IT(47932); this->bailOutOffsetForLastRejit = offset; }
        uint32 GetBailOutOffsetForLastRejit() {TRACE_IT(47933); return this->bailOutOffsetForLastRejit; }

#if DBG_DUMP
        void Dump(FunctionBody* functionBody, ArenaAllocator * dynamicProfileInfoAllocator = nullptr);
#endif
    private:
        static void InstantiateForceInlinedMembers();
    };

    struct PolymorphicCallSiteInfo
    {
        Field(Js::LocalFunctionId) functionIds[DynamicProfileInfo::maxPolymorphicInliningSize];
        Field(Js::SourceId) sourceIds[DynamicProfileInfo::maxPolymorphicInliningSize];
        Field(PolymorphicCallSiteInfo *) next;
        bool GetFunction(uint index, Js::LocalFunctionId *functionId, Js::SourceId *sourceId)
        {TRACE_IT(47934);
            Assert(index < DynamicProfileInfo::maxPolymorphicInliningSize);
            Assert(functionId);
            Assert(sourceId);
            if (DynamicProfileInfo::IsCallSiteNoInfo(functionIds[index]))
            {TRACE_IT(47935);
                return false;
            }
            *functionId = functionIds[index];
            *sourceId = sourceIds[index];
            return true;
        }
    };

#ifdef DYNAMIC_PROFILE_STORAGE
    class BufferReader
    {
    public:
        BufferReader(__in_ecount(length) char const * buffer, size_t length) : current(buffer), lengthLeft(length) {TRACE_IT(47936);}

        template <typename T>
        bool Read(T * data)
        {TRACE_IT(47937);
            if (lengthLeft < sizeof(T))
            {TRACE_IT(47938);
                return false;
            }
            *data = *(T *)current;
            current += sizeof(T);
            lengthLeft -= sizeof(T);
            return true;
        }

        template <typename T>
        bool Peek(T * data)
        {TRACE_IT(47939);
            if (lengthLeft < sizeof(T))
            {TRACE_IT(47940);
                return false;
            }
            *data = *(T *)current;
            return true;
        }

        template <typename T>
        bool ReadArray(__inout_ecount(len) T * data, size_t len)
        {TRACE_IT(47941);
            size_t size = sizeof(T) * len;
            if (lengthLeft < size)
            {TRACE_IT(47942);
                return false;
            }
            memcpy_s(data, size, current, size);
            current += size;
            lengthLeft -= size;
            return true;
        }
    private:
        char const * current;
        size_t lengthLeft;
    };

    class BufferSizeCounter
    {
    public:
        BufferSizeCounter() : count(0) {TRACE_IT(47943);}
        size_t GetByteCount() const {TRACE_IT(47944); return count; }
        template <typename T>
        bool Write(T const& data)
        {TRACE_IT(47945);
            return WriteArray(&data, 1);
        }

#if DBG_DUMP
        void Log(DynamicProfileInfo* info) {TRACE_IT(47946);}
#endif

        template <typename T>
        bool WriteArray(__in_ecount(len) T * data, size_t len)
        {TRACE_IT(47947);
            count += sizeof(T) * len;
            return true;
        }

        template <typename T>
        bool WriteArray(WriteBarrierPtr<T> data, size_t len)
        {TRACE_IT(47948);
            return WriteArray(static_cast<T*>(data), len);
        }

    private:
        size_t count;
    };

    class BufferWriter
    {
    public:
        BufferWriter(__in_ecount(length) char * buffer, size_t length) : current(buffer), lengthLeft(length) {TRACE_IT(47949);}

        template <typename T>
        bool Write(T const& data)
        {TRACE_IT(47950);
            return WriteArray(&data, 1);
        }

#if DBG_DUMP
        void Log(DynamicProfileInfo* info);
#endif
        template <typename T>
        bool WriteArray(__in_ecount(len) T * data, size_t len)
        {TRACE_IT(47951);
            size_t size = sizeof(T) * len;
            if (lengthLeft < size)
            {TRACE_IT(47952);
                return false;
            }
            memcpy_s(current, size, data, size);
            current += size;
            lengthLeft -= size;
            return true;
        }

        template <typename T>
        bool WriteArray(WriteBarrierPtr<T> data, size_t len)
        {TRACE_IT(47953);
            return WriteArray(static_cast<T*>(data), len);
        }

    private:
        char * current;
        size_t lengthLeft;
    };
#endif
};
#endif
