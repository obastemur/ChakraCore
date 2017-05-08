//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if ENABLE_NATIVE_CODEGEN
// forward ref
struct FunctionBodyDataIDL;

namespace Js
{
    // - Data generated for jitting purposes
    // - Recycler-allocated, lifetime is from when a code gen work item is added to the jit queue, to when jitting is complete
    //     - Also keeps the function body and inlinee function bodies alive while jitting.
    class FunctionCodeGenJitTimeData
    {
    private:
        Field(FunctionInfo *) const functionInfo;

        // Point's to an entry point if the work item needs the entry point alive- null for cases where the entry point isn't used
        Field(EntryPointInfo *) const entryPointInfo;

        // These cloned inline caches are guaranteed to have stable data while jitting, but will be collectible after jitting
        Field(ObjTypeSpecFldInfoArray) objTypeSpecFldInfoArray;

        // Globally ordered list of all object type specialized property access information (monomorphic and polymorphic caches combined).
        Field(uint) globalObjTypeSpecFldInfoCount;
        Field(Field(ObjTypeSpecFldInfo*)*) globalObjTypeSpecFldInfoArray;

        // There will be a non-null entry for each profiled call site where a function is to be inlined
        Field(Field(FunctionCodeGenJitTimeData*)*) inlinees;
        Field(Field(FunctionCodeGenJitTimeData*)*) ldFldInlinees;
        Field(RecyclerWeakReference<FunctionBody>*) weakFuncRef;

        Field(PolymorphicInlineCacheInfoIDL*) inlineeInfo;
        Field(PolymorphicInlineCacheInfoIDL*) selfInfo;
        Field(PolymorphicInlineCacheIDL*) polymorphicInlineCaches;

        // current value of global this object, may be changed in case of script engine invalidation
        Field(Var) globalThisObject;

        // Number of functions that are to be inlined (this is not the length of the 'inlinees' array above, includes getter setter inlinee count)
        Field(uint) inlineeCount;
        // Number of counts of getter setter to be inlined. This is not an exact count as inline caches are shared and we have no way of knowing
        // accurate count.
        Field(uint) ldFldInlineeCount;

        // For polymorphic call site we will have linked list of FunctionCodeGenJitTimeData
        // Each is differentiated by id starting from 0, 1
        Field(FunctionCodeGenJitTimeData *) next;
        Field(bool) isInlined;

        // This indicates the function is aggressively Inlined(see NativeCodeGenerator::TryAggressiveInlining) .
        Field(bool) isAggressiveInliningEnabled;

        // The profiled iterations need to be determined at the time of gathering code gen data on the main thread
        Field(const uint16) profiledIterations;

#ifdef FIELD_ACCESS_STATS
    public:
        Field(FieldAccessStatsPtr) inlineCacheStats;

        void EnsureInlineCacheStats(Recycler* recycler);
        void AddInlineeInlineCacheStats(FunctionCodeGenJitTimeData* inlineeJitTimeData);
#endif

    public:
        FunctionCodeGenJitTimeData(FunctionInfo *const functionInfo, EntryPointInfo *const entryPoint, bool isInlined = true);

    public:
        Field(BVFixed *) inlineesBv;

        Field(Js::PropertyId*) sharedPropertyGuards;
        Field(uint) sharedPropertyGuardCount;

        FunctionInfo *GetFunctionInfo() const;
        FunctionBody *GetFunctionBody() const;
        Var GetGlobalThisObject() const;
        FunctionBodyDataIDL *GetJITBody() const;
        FunctionCodeGenJitTimeData *GetNext() const {TRACE_IT(48187); return next; }

        const ObjTypeSpecFldInfoArray* GetObjTypeSpecFldInfoArray() const {TRACE_IT(48188); return &this->objTypeSpecFldInfoArray; }
        ObjTypeSpecFldInfoArray* GetObjTypeSpecFldInfoArray() {TRACE_IT(48189); return &this->objTypeSpecFldInfoArray; }
        EntryPointInfo* GetEntryPointInfo() const {TRACE_IT(48190); return this->entryPointInfo; }

    public:
        const FunctionCodeGenJitTimeData *GetInlinee(const ProfileId profiledCallSiteId) const;
        const FunctionCodeGenJitTimeData *GetLdFldInlinee(const InlineCacheIndex inlineCacheIndex) const;
        FunctionCodeGenJitTimeData *AddInlinee(
            Recycler *const recycler,
            const ProfileId profiledCallSiteId,
            FunctionInfo *const inlinee,
            bool isInlined = true);
        uint InlineeCount() const;
        uint LdFldInlineeCount() const;
        bool IsLdFldInlineePresent() const {TRACE_IT(48191); return ldFldInlineeCount != 0; }

        RecyclerWeakReference<FunctionBody> *GetWeakFuncRef() const {TRACE_IT(48192); return this->weakFuncRef; }
        void SetWeakFuncRef(RecyclerWeakReference<FunctionBody> *weakFuncRef)
        {TRACE_IT(48193);
            Assert(this->weakFuncRef == nullptr || weakFuncRef == nullptr || this->weakFuncRef == weakFuncRef);
            this->weakFuncRef = weakFuncRef;
        }

        void SetPolymorphicInlineInfo(PolymorphicInlineCacheInfoIDL* inlineeInfo, PolymorphicInlineCacheInfoIDL* selfInfo, PolymorphicInlineCacheIDL* polymorphicInlineCaches)
        {TRACE_IT(48194);
            this->inlineeInfo = inlineeInfo;
            this->selfInfo = selfInfo;
            this->polymorphicInlineCaches = polymorphicInlineCaches;
        }

        FunctionCodeGenJitTimeData *AddLdFldInlinee(
            Recycler *const recycler,
            const InlineCacheIndex inlineCacheIndex,
            FunctionInfo *const inlinee);

        bool IsPolymorphicCallSite(const ProfileId profiledCallSiteId) const;
        // This function walks all the chained jittimedata and returns the one which match the functionInfo.
        // This can return null, if the functionInfo doesn't match.
        const FunctionCodeGenJitTimeData *GetJitTimeDataFromFunctionInfo(FunctionInfo *polyFunctioInfoy) const;

        uint GetGlobalObjTypeSpecFldInfoCount() const {TRACE_IT(48195); return this->globalObjTypeSpecFldInfoCount; }
        Field(ObjTypeSpecFldInfo*)* GetGlobalObjTypeSpecFldInfoArray() const {TRACE_IT(48196);return this->globalObjTypeSpecFldInfoArray; }

        ObjTypeSpecFldInfo* GetGlobalObjTypeSpecFldInfo(uint propertyInfoId) const
        {TRACE_IT(48197);
            Assert(this->globalObjTypeSpecFldInfoArray != nullptr && propertyInfoId < this->globalObjTypeSpecFldInfoCount);
            return this->globalObjTypeSpecFldInfoArray[propertyInfoId];
        }

        void SetGlobalObjTypeSpecFldInfo(uint propertyInfoId, ObjTypeSpecFldInfo* info) const
        {TRACE_IT(48198);
            Assert(this->globalObjTypeSpecFldInfoArray != nullptr && propertyInfoId < this->globalObjTypeSpecFldInfoCount);
            this->globalObjTypeSpecFldInfoArray[propertyInfoId] = info;
        }

        void SetGlobalObjTypeSpecFldInfoArray(Field(ObjTypeSpecFldInfo*)* array, uint count)
        {TRACE_IT(48199);
            Assert(array != nullptr);
            this->globalObjTypeSpecFldInfoArray = array;
            this->globalObjTypeSpecFldInfoCount = count;
        }

        bool GetIsInlined() const
        {TRACE_IT(48200);
            return isInlined;
        }
        bool GetIsAggressiveInliningEnabled() const
        {TRACE_IT(48201);
            return isAggressiveInliningEnabled;
        }
        void SetIsAggressiveInliningEnabled()
        {TRACE_IT(48202);
            isAggressiveInliningEnabled = true;
        }

        void SetupRecursiveInlineeChain(
            Recycler *const recycler,
            const ProfileId profiledCallSiteId)
        {TRACE_IT(48203);
            if (!inlinees)
            {TRACE_IT(48204);
                inlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenJitTimeData *), GetFunctionBody()->GetProfiledCallSiteCount());
            }
            inlinees[profiledCallSiteId] = this;
            inlineeCount++;
        }

        uint16 GetProfiledIterations() const;

        PREVENT_COPY(FunctionCodeGenJitTimeData)
    };
}
#endif

