//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#define TypeWithAuxSlotTag(_t) \
    (reinterpret_cast<Type*>(reinterpret_cast<size_t>(_t) | InlineCacheAuxSlotTypeTag))
#define TypeWithoutAuxSlotTag(_t) \
    (reinterpret_cast<Js::Type*>(reinterpret_cast<size_t>(_t) & ~InlineCacheAuxSlotTypeTag))
#define TypeHasAuxSlotTag(_t) \
    (!!(reinterpret_cast<size_t>(_t) & InlineCacheAuxSlotTypeTag))

#if defined(_M_IX86_OR_ARM32)
#define PolymorphicInlineCacheShift 5 // On 32 bit architectures, the least 5 significant bits of a DynamicTypePointer is 0
#else
#define PolymorphicInlineCacheShift 6 // On 64 bit architectures, the least 6 significant bits of a DynamicTypePointer is 0
#endif

// TODO: OOP JIT, move equiv set to backend?
// forward decl
class JITType;
template <class TAllocator> class JITTypeHolderBase;
typedef JITTypeHolderBase<void> JITTypeHolder;
typedef JITTypeHolderBase<Recycler> RecyclerJITTypeHolder;

namespace Js
{
    enum CacheType : byte
    {
        CacheType_None,
        CacheType_Local,
        CacheType_Proto,
        CacheType_LocalWithoutProperty,
        CacheType_Getter,
        CacheType_Setter,
        CacheType_TypeProperty,
    };

    enum SlotType : byte
    {
        SlotType_None,
        SlotType_Inline,
        SlotType_Aux,
    };

    struct PropertyCacheOperationInfo
    {
        PropertyCacheOperationInfo()
            : cacheType(CacheType_None), slotType(SlotType_None), isPolymorphic(false)
        {TRACE_IT(48397);
        }

        CacheType cacheType;
        SlotType slotType;
        bool isPolymorphic;
    };

    struct JitTimeInlineCache;
    struct InlineCache
    {
        static const int CacheLayoutSelectorBitCount = 1;
        static const int RequiredAuxSlotCapacityBitCount = 15;
        static const bool IsPolymorphic = false;

        InlineCache() {TRACE_IT(48398);}

        union
        {
            // Invariants:
            // - Type* fields do not overlap.
            // - "next" field is non-null iff the cache is linked in a list of proto-caches
            //   (see ScriptContext::RegisterProtoInlineCache and ScriptContext::InvalidateProtoCaches).

            struct s_local
            {
                Type* type;

                // PatchPutValue caches here the type the object has before a new property is added.
                // If this type is hit again we can immediately change the object's type to "type"
                // and store the value into the slot "slotIndex".
                Type* typeWithoutProperty;

                union
                {
                    struct
                    {
                        uint16 isLocal : 1;
                        uint16 requiredAuxSlotCapacity : 15;     // Maximum auxiliary slot capacity (for a path type) must be < 2^16
                    };
                    struct
                    {
                        uint16 rawUInt16;                        // Required for access from JIT-ed code
                    };
                };
                uint16 slotIndex;
            } local;

            struct s_proto
            {
                uint16 isProto : 1;
                uint16 isMissing : 1;
                uint16 unused : 14;
                uint16 slotIndex;

                // It's OK for the type in proto layout to overlap with typeWithoutProperty in the local layout, because
                // we only use typeWithoutProperty on field stores, which can never have a proto layout.
                Type* type;

                DynamicObject* prototypeObject;
            } proto;

            struct s_accessor
            {
                DynamicObject *object;

                union
                {
                    struct {
                        uint16 isAccessor : 1;
                        uint16 flags : 2;
                        uint16 isOnProto : 1;
                        uint16 unused : 12;
                    };
                    uint16 rawUInt16;
                };
                uint16 slotIndex;

                Type * type;
            } accessor;

            CompileAssert(sizeof(s_local) == sizeof(s_proto));
            CompileAssert(sizeof(s_local) == sizeof(s_accessor));
        } u;

        InlineCache** invalidationListSlotPtr;

        bool IsEmpty() const
        {TRACE_IT(48399);
            return u.local.type == nullptr;
        }

        bool IsLocal() const
        {TRACE_IT(48400);
            return u.local.isLocal;
        }

        bool IsProto() const
        {TRACE_IT(48401);
            return u.proto.isProto;
        }

        DynamicObject * GetPrototypeObject() const
        {TRACE_IT(48402);
            Assert(IsProto());
            return u.proto.prototypeObject;
        }

        DynamicObject * GetAccessorObject() const
        {TRACE_IT(48403);
            Assert(IsAccessor());
            return u.accessor.object;
        }

        bool IsAccessor() const
        {TRACE_IT(48404);
            return u.accessor.isAccessor;
        }

        bool IsAccessorOnProto() const
        {TRACE_IT(48405);
            return IsAccessor() && u.accessor.isOnProto;
        }

        bool IsGetterAccessor() const
        {TRACE_IT(48406);
            return IsAccessor() && !!(u.accessor.flags & InlineCacheGetterFlag);
        }

        bool IsGetterAccessorOnProto() const
        {TRACE_IT(48407);
            return IsGetterAccessor() && u.accessor.isOnProto;
        }

        bool IsSetterAccessor() const
        {TRACE_IT(48408);
            return IsAccessor() && !!(u.accessor.flags & InlineCacheSetterFlag);
        }

        bool IsSetterAccessorOnProto() const
        {TRACE_IT(48409);
            return IsSetterAccessor() && u.accessor.isOnProto;
        }

        Type* GetRawType() const
        {TRACE_IT(48410);
            return IsLocal() ? u.local.type : (IsProto() ? u.proto.type : (IsAccessor() ? u.accessor.type : nullptr));
        }

        Type* GetType() const
        {TRACE_IT(48411);
            return TypeWithoutAuxSlotTag(GetRawType());
        }

        template<bool isAccessor>
        bool HasDifferentType(const bool isProto, const Type * type, const Type * typeWithoutProperty) const;

        bool HasType_Flags(const Type * type) const
        {TRACE_IT(48412);
            return u.accessor.type == type || u.accessor.type == TypeWithAuxSlotTag(type);
        }

        bool HasDifferentType(const Type * type) const
        {TRACE_IT(48413);
            return !IsEmpty() && GetType() != type;
        }

        bool RemoveFromInvalidationList()
        {TRACE_IT(48414);
            if (this->invalidationListSlotPtr == nullptr)
            {TRACE_IT(48415);
                return false;
            }

            *this->invalidationListSlotPtr = nullptr;
            this->invalidationListSlotPtr = nullptr;
            return true;
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        const char16 *LayoutString() const
        {TRACE_IT(48416);
            if (IsEmpty())
            {TRACE_IT(48417);
                return _u("Empty");
            }
            if (IsLocal())
            {TRACE_IT(48418);
                return _u("Local");
            }
            if (IsAccessor())
            {TRACE_IT(48419);
                return _u("Accessor");
            }
            return _u("Proto");
        }
#endif

    public:
        void CacheLocal(
            Type *const type,
            const PropertyId propertyId,
            const PropertyIndex propertyIndex,
            const bool isInlineSlot,
            Type *const typeWithoutProperty,
            int requiredAuxSlotCapacity,
            ScriptContext *const requestContext);

        void CacheProto(
            DynamicObject *const prototypeObjectWithProperty,
            const PropertyId propertyId,
            const PropertyIndex propertyIndex,
            const bool isInlineSlot,
            const bool isMissing,
            Type *const type,
            ScriptContext *const requestContext);

        void CacheMissing(
            DynamicObject *const missingPropertyHolder,
            const PropertyId propertyId,
            const PropertyIndex propertyIndex,
            const bool isInlineSlot,
            Type *const type,
            ScriptContext *const requestContext);

        void CacheAccessor(
            const bool isGetter,
            const PropertyId propertyId,
            const PropertyIndex propertyIndex,
            const bool isInlineSlot,
            Type *const type,
            DynamicObject *const object,
            const bool isOnProto,
            ScriptContext *const requestContext);

        template<
            bool CheckLocal,
            bool CheckProto,
            bool CheckAccessor,
            bool CheckMissing,
            bool ReturnOperationInfo>
        bool TryGetProperty(
            Var const instance,
            RecyclableObject *const propertyObject,
            const PropertyId propertyId,
            Var *const propertyValue,
            ScriptContext *const requestContext,
            PropertyCacheOperationInfo *const operationInfo);

        template<
            bool CheckLocal,
            bool CheckLocalTypeWithoutProperty,
            bool CheckAccessor,
            bool ReturnOperationInfo>
        bool TrySetProperty(
            RecyclableObject *const object,
            const PropertyId propertyId,
            Var propertyValue,
            ScriptContext *const requestContext,
            PropertyCacheOperationInfo *const operationInfo,
            const PropertyOperationFlags propertyOperationFlags = PropertyOperation_None);

        bool PretendTryGetProperty(Type *const type, PropertyCacheOperationInfo * operationInfo);
        bool PretendTrySetProperty(Type *const type, Type *const oldType, PropertyCacheOperationInfo * operationInfo);

        void Clear();
        template <class TAllocator>
        InlineCache *Clone(TAllocator *const allocator);
        InlineCache *Clone(Js::PropertyId propertyId, ScriptContext* scriptContext);
        void CopyTo(PropertyId propertyId, ScriptContext * scriptContext, InlineCache * const clone);
        bool TryGetFixedMethodFromCache(Js::FunctionBody* functionBody, uint cacheId, Js::JavascriptFunction** pFixedMethod);

        bool GetGetterSetter(Type *const type, RecyclableObject **callee);
        bool GetCallApplyTarget(RecyclableObject* obj, RecyclableObject **callee);

        static uint GetGetterFlagMask()
        {TRACE_IT(48420);
            // First bit is marked for isAccessor in the accessor cache layout.
            return  InlineCacheGetterFlag << 1;
        }

        static uint GetSetterFlagMask()
        {TRACE_IT(48421);
            // First bit is marked for isAccessor in the accessor cache layout.
            return  InlineCacheSetterFlag << 1;
        }

        static uint GetGetterSetterFlagMask()
        {TRACE_IT(48422);
            // First bit is marked for isAccessor in the accessor cache layout.
            return  (InlineCacheGetterFlag | InlineCacheSetterFlag) << 1;
        }

        bool NeedsToBeRegisteredForProtoInvalidation() const;
        bool NeedsToBeRegisteredForStoreFieldInvalidation() const;

#if DEBUG
        bool ConfirmCacheMiss(const Type * oldType, const PropertyValueInfo* info) const;
        bool NeedsToBeRegisteredForInvalidation() const;
        static void VerifyRegistrationForInvalidation(const InlineCache* cache, ScriptContext* scriptContext, Js::PropertyId propertyId);
#endif

#if DBG_DUMP
        void Dump();
#endif
    };

#if defined(_M_IX86_OR_ARM32)
    CompileAssert(sizeof(InlineCache) == 0x10);
#else
    CompileAssert(sizeof(InlineCache) == 0x20);
#endif

    CompileAssert(sizeof(InlineCache) == sizeof(InlineCacheAllocator::CacheLayout));
    CompileAssert(offsetof(InlineCache, invalidationListSlotPtr) == offsetof(InlineCacheAllocator::CacheLayout, strongRef));

    struct JitTimePolymorphicInlineCache;
    struct PolymorphicInlineCache sealed : public FinalizableObject
    {
#ifdef INLINE_CACHE_STATS
        friend class Js::ScriptContext;
#endif

    public:
        static const bool IsPolymorphic = true;

    private:
        FieldNoBarrier(InlineCache *) inlineCaches;
        Field(FunctionBody *) functionBody;
        Field(uint16) size;
        Field(bool) ignoreForEquivalentObjTypeSpec;
        Field(bool) cloneForJitTimeUse;

        Field(int32) inlineCachesFillInfo;

        // DList chaining all polymorphic inline caches of a FunctionBody together.
        // Since PolymorphicInlineCache is a leaf object, these references do not keep
        // the polymorphic inline caches alive. When a PolymorphicInlineCache is finalized,
        // it removes itself from the list and deletes its inline cache array.
        Field(PolymorphicInlineCache *) next;
        Field(PolymorphicInlineCache *) prev;

        PolymorphicInlineCache(InlineCache * inlineCaches, uint16 size, FunctionBody * functionBody)
            : inlineCaches(inlineCaches), functionBody(functionBody), size(size), ignoreForEquivalentObjTypeSpec(false), cloneForJitTimeUse(true), inlineCachesFillInfo(0), next(nullptr), prev(nullptr)
        {TRACE_IT(48423);
            Assert((size == 0 && inlineCaches == nullptr) ||
                (inlineCaches != nullptr && size >= MinPolymorphicInlineCacheSize && size <= MaxPolymorphicInlineCacheSize));
        }

    public:
        static PolymorphicInlineCache * New(uint16 size, FunctionBody * functionBody);

        static uint16 GetInitialSize() {TRACE_IT(48424); return MinPolymorphicInlineCacheSize; }
        bool CanAllocateBigger() {TRACE_IT(48425); return GetSize() < MaxPolymorphicInlineCacheSize; }
        static uint16 GetNextSize(uint16 currentSize)
        {TRACE_IT(48426);
            if (currentSize == MaxPolymorphicInlineCacheSize)
            {TRACE_IT(48427);
                return 0;
            }
            else
            {TRACE_IT(48428);
                Assert(currentSize >= MinPolymorphicInlineCacheSize && currentSize <= (MaxPolymorphicInlineCacheSize / 2));
                return currentSize * 2;
            }
        }

        template<bool isAccessor>
        bool HasDifferentType(const bool isProto, const Type * type, const Type * typeWithoutProperty) const;
        bool HasType_Flags(const Type * type) const;

        InlineCache * GetInlineCaches() const {TRACE_IT(48429); return inlineCaches; }
        uint16 GetSize() const {TRACE_IT(48430); return size; }
        PolymorphicInlineCache * GetNext() {TRACE_IT(48431); return next; }
        bool GetIgnoreForEquivalentObjTypeSpec() const {TRACE_IT(48432); return this->ignoreForEquivalentObjTypeSpec; }
        void SetIgnoreForEquivalentObjTypeSpec(bool value) {TRACE_IT(48433); this->ignoreForEquivalentObjTypeSpec = value; }
        bool GetCloneForJitTimeUse() const {TRACE_IT(48434); return this->cloneForJitTimeUse; }
        void SetCloneForJitTimeUse(bool value) {TRACE_IT(48435); this->cloneForJitTimeUse = value; }
        uint32 GetInlineCachesFillInfo() {TRACE_IT(48436); return this->inlineCachesFillInfo; }
        void UpdateInlineCachesFillInfo(uint32 index, bool set);
        bool IsFull();

        virtual void Finalize(bool isShutdown) override;
        virtual void Dispose(bool isShutdown) override { };
        virtual void Mark(Recycler *recycler) override { AssertMsg(false, "Mark called on object that isn't TrackableObject"); }

        void CacheLocal(
            Type *const type,
            const PropertyId propertyId,
            const PropertyIndex propertyIndex,
            const bool isInlineSlot,
            Type *const typeWithoutProperty,
            int requiredAuxSlotCapacity,
            ScriptContext *const requestContext);

        void CacheProto(
            DynamicObject *const prototypeObjectWithProperty,
            const PropertyId propertyId,
            const PropertyIndex propertyIndex,
            const bool isInlineSlot,
            const bool isMissing,
            Type *const type,
            ScriptContext *const requestContext);

        void CacheAccessor(
            const bool isGetter,
            const PropertyId propertyId,
            const PropertyIndex propertyIndex,
            const bool isInlineSlot,
            Type *const type,
            DynamicObject *const object,
            const bool isOnProto,
            ScriptContext *const requestContext);

        template<
            bool CheckLocal,
            bool CheckProto,
            bool CheckAccessor,
            bool CheckMissing,
            bool IsInlineCacheAvailable,
            bool ReturnOperationInfo>
        bool TryGetProperty(
            Var const instance,
            RecyclableObject *const propertyObject,
            const PropertyId propertyId,
            Var *const propertyValue,
            ScriptContext *const requestContext,
            PropertyCacheOperationInfo *const operationInfo,
            InlineCache *const inlineCacheToPopulate);

        template<
            bool CheckLocal,
            bool CheckLocalTypeWithoutProperty,
            bool CheckAccessor,
            bool ReturnOperationInfo,
            bool PopulateInlineCache>
        bool TrySetProperty(
            RecyclableObject *const object,
            const PropertyId propertyId,
            Var propertyValue,
            ScriptContext *const requestContext,
            PropertyCacheOperationInfo *const operationInfo,
            InlineCache *const inlineCacheToPopulate,
            const PropertyOperationFlags propertyOperationFlags = PropertyOperation_None);

        bool PretendTryGetProperty(Type *const type, PropertyCacheOperationInfo * operationInfo);
        bool PretendTrySetProperty(Type *const type, Type *const oldType, PropertyCacheOperationInfo * operationInfo);

        void CopyTo(PropertyId propertyId, ScriptContext* scriptContext, PolymorphicInlineCache *const clone);

#if DBG_DUMP
        void Dump();
#endif

        uint GetInlineCacheIndexForType(const Type * type) const
        {TRACE_IT(48437);
            return (((size_t)type) >> PolymorphicInlineCacheShift) & (GetSize() - 1);
        }

    private:
        uint GetNextInlineCacheIndex(uint index) const
        {TRACE_IT(48438);
            if (++index == GetSize())
            {TRACE_IT(48439);
                index = 0;
            }
            return index;
        }

        template<bool CheckLocal, bool CheckProto, bool CheckAccessor>
        void CloneInlineCacheToEmptySlotInCollision(Type *const type, uint index);

#ifdef CLONE_INLINECACHE_TO_EMPTYSLOT
        template <typename TDelegate>
        bool CheckClonedInlineCache(uint inlineCacheIndex, TDelegate mapper);
#endif
#if INTRUSIVE_TESTTRACE_PolymorphicInlineCache
        uint GetEntryCount()
        {TRACE_IT(48440);
            uint count = 0;
            for (uint i = 0; i < size; ++i)
            {TRACE_IT(48441);
                if (!inlineCaches[i].IsEmpty())
                {TRACE_IT(48442);
                    count++;
                }
            }
            return count;
        }
#endif
    };

#if ENABLE_NATIVE_CODEGEN
    class EquivalentTypeSet
    {
    private:
        Field(bool) sortedAndDuplicatesRemoved;
        Field(uint16) count;
        Field(RecyclerJITTypeHolder *) types;

    public:
        EquivalentTypeSet(RecyclerJITTypeHolder * types, uint16 count);

        uint16 GetCount() const
        {TRACE_IT(48443);
            return this->count;
        }

        JITTypeHolder GetFirstType() const;

        JITTypeHolder GetType(uint16 index) const;

        bool GetSortedAndDuplicatesRemoved() const
        {TRACE_IT(48444);
            return this->sortedAndDuplicatesRemoved;
        }
        bool Contains(const JITTypeHolder type, uint16 * pIndex = nullptr);

        static bool AreIdentical(EquivalentTypeSet * left, EquivalentTypeSet * right);
        static bool IsSubsetOf(EquivalentTypeSet * left, EquivalentTypeSet * right);
        void SortAndRemoveDuplicates();
    };
#endif
    enum class CtorCacheGuardValues : intptr_t
    {
        TagFlag = 0x01,

        Invalid = 0x00,
        Special = TagFlag
    };
    ENUM_CLASS_HELPERS(CtorCacheGuardValues, intptr_t);

#define MaxCachedSlotCount 65535

    struct ConstructorCache
    {
        friend class JavascriptFunction;

        struct GuardStruct
        {
            Field(CtorCacheGuardValues) value;
        };

        struct ContentStruct
        {
            Field(DynamicType*) type;
            Field(ScriptContext*) scriptContext;
            // In a pinch we could eliminate this and store type pending sharing in the type field as long
            // as the guard value flags fit below the object alignment boundary.  However, this wouldn't
            // keep the type alive, so it would only work if we zeroed constructor caches before GC.
            Field(DynamicType*) pendingType;

            // We cache only types whose slotCount < 64K to ensure the slotCount field doesn't look like a pointer to the recycler.
            Field(int) slotCount;

            // This layout (i.e. one-byte bit fields first, then the one-byte updateAfterCtor, and then the two byte inlineSlotCount) is
            // chosen intentionally to make sure the whole four bytes never look like a pointer and create a false reference pinning something
            // in recycler heap.  The isPopulated bit is always set when the cache holds any data - even if it got invalidated.
            Field(bool) isPopulated : 1;
            Field(bool) isPolymorphic : 1;
            Field(bool) typeUpdatePending : 1;
            Field(bool) ctorHasNoExplicitReturnValue : 1;
            Field(bool) skipDefaultNewObject : 1;
            // This field indicates that the type stored in this cache is the final type after constructor.
            Field(bool) typeIsFinal : 1;
            // This field indicates that the constructor cache has been invalidated due to a constructor's prototype property change.
            // We use this flag to determine if we should mark the cache as polymorphic and not attempt subsequent optimizations.
            // The cache may also be invalidated due to a guard invalidation resulting from some property change (e.g. in proto chain),
            // in which case we won't deem the cache polymorphic.
            Field(bool) hasPrototypeChanged : 1;

            Field(uint8) callCount;

            // Separate from the bit field below for convenient compare from the JIT-ed code. Doesn't currently increase the size.
            // If size becomes an issue, we could merge back into the bit field and use a TEST instead of CMP.
            Field(bool) updateAfterCtor;

            Field(int16) inlineSlotCount;
        };

        union
        {
            Field(GuardStruct) guard;
            Field(ContentStruct) content;
        };

        CompileAssert(offsetof(GuardStruct, value) == offsetof(ContentStruct, type));
        CompileAssert(sizeof(((GuardStruct*)nullptr)->value) == sizeof(((ContentStruct*)nullptr)->type));
        CompileAssert(static_cast<intptr_t>(CtorCacheGuardValues::Invalid) == static_cast<intptr_t>(NULL));

        static ConstructorCache DefaultInstance;

    public:
        ConstructorCache()
        {TRACE_IT(48445);
            this->content.type = nullptr;
            this->content.scriptContext = nullptr;
            this->content.slotCount = 0;
            this->content.inlineSlotCount = 0;
            this->content.updateAfterCtor = false;
            this->content.ctorHasNoExplicitReturnValue = false;
            this->content.skipDefaultNewObject = false;
            this->content.isPopulated = false;
            this->content.isPolymorphic = false;
            this->content.typeUpdatePending = false;
            this->content.typeIsFinal = false;
            this->content.hasPrototypeChanged = false;
            this->content.callCount = 0;
            Assert(IsConsistent());
        }

        ConstructorCache(ConstructorCache const * other)
        {TRACE_IT(48446);
            Assert(other != nullptr);
            this->content.type = other->content.type;
            this->content.scriptContext = other->content.scriptContext;
            this->content.slotCount = other->content.slotCount;
            this->content.inlineSlotCount = other->content.inlineSlotCount;
            this->content.updateAfterCtor = other->content.updateAfterCtor;
            this->content.ctorHasNoExplicitReturnValue = other->content.ctorHasNoExplicitReturnValue;
            this->content.skipDefaultNewObject = other->content.skipDefaultNewObject;
            this->content.isPopulated = other->content.isPopulated;
            this->content.isPolymorphic = other->content.isPolymorphic;
            this->content.typeUpdatePending = other->content.typeUpdatePending;
            this->content.typeIsFinal = other->content.typeIsFinal;
            this->content.hasPrototypeChanged = other->content.hasPrototypeChanged;
            this->content.callCount = other->content.callCount;
            Assert(IsConsistent());
        }

        static size_t const GetOffsetOfGuardValue() {TRACE_IT(48447); return offsetof(Js::ConstructorCache, guard.value); }
        static size_t const GetSizeOfGuardValue() {TRACE_IT(48448); return sizeof(((Js::ConstructorCache*)nullptr)->guard.value); }

        void Populate(DynamicType* type, ScriptContext* scriptContext, bool ctorHasNoExplicitReturnValue, bool updateAfterCtor)
        {TRACE_IT(48449);
            Assert(scriptContext == type->GetScriptContext());
            Assert(type->GetIsShared());
            Assert(IsConsistent());
            Assert(!this->content.isPopulated || this->content.isPolymorphic);
            Assert(type->GetTypeHandler()->GetSlotCapacity() <= MaxCachedSlotCount);
            this->content.isPopulated = true;
            this->content.type = type;
            this->content.scriptContext = scriptContext;
            this->content.slotCount = type->GetTypeHandler()->GetSlotCapacity();
            this->content.inlineSlotCount = type->GetTypeHandler()->GetInlineSlotCapacity();
            this->content.ctorHasNoExplicitReturnValue = ctorHasNoExplicitReturnValue;
            this->content.updateAfterCtor = updateAfterCtor;
            Assert(IsConsistent());
        }

        void PopulateForSkipDefaultNewObject(ScriptContext* scriptContext)
        {TRACE_IT(48450);
            Assert(IsConsistent());
            Assert(!this->content.isPopulated);
            this->content.isPopulated = true;
            this->guard.value = CtorCacheGuardValues::Special;
            this->content.scriptContext = scriptContext;
            this->content.skipDefaultNewObject = true;
            Assert(IsConsistent());
        }

        bool TryUpdateAfterConstructor(DynamicType* type, ScriptContext* scriptContext)
        {TRACE_IT(48451);
            Assert(scriptContext == type->GetScriptContext());
            Assert(type->GetTypeHandler()->GetMayBecomeShared());
            Assert(IsConsistent());
            Assert(this->content.isPopulated);
            Assert(this->content.scriptContext == scriptContext);
            Assert(!this->content.typeUpdatePending);
            Assert(this->content.ctorHasNoExplicitReturnValue);

            if (type->GetTypeHandler()->GetSlotCapacity() > MaxCachedSlotCount)
            {TRACE_IT(48452);
                return false;
            }

            if (type->GetIsShared())
            {TRACE_IT(48453);
                this->content.type = type;
                this->content.typeIsFinal = true;
                this->content.pendingType = nullptr;
            }
            else
            {
                AssertMsg(false, "No one calls this part of the code?");
                this->guard.value = CtorCacheGuardValues::Special;
                this->content.pendingType = type;
                this->content.typeUpdatePending = true;
            }
            this->content.slotCount = type->GetTypeHandler()->GetSlotCapacity();
            this->content.inlineSlotCount = type->GetTypeHandler()->GetInlineSlotCapacity();
            Assert(IsConsistent());
            return true;
        }

        void UpdateInlineSlotCount()
        {TRACE_IT(48454);
            Assert(IsConsistent());
            Assert(this->content.isPopulated);
            Assert(IsEnabled() || NeedsTypeUpdate());
            DynamicType* type = this->content.typeUpdatePending ? this->content.pendingType : this->content.type;
            DynamicTypeHandler* typeHandler = type->GetTypeHandler();
            // Inline slot capacity should never grow as a result of shrinking.
            Assert(typeHandler->GetInlineSlotCapacity() <= this->content.inlineSlotCount);
            // Slot capacity should never grow as a result of shrinking.
            Assert(typeHandler->GetSlotCapacity() <= this->content.slotCount);
            this->content.slotCount = typeHandler->GetSlotCapacity();
            this->content.inlineSlotCount = typeHandler->GetInlineSlotCapacity();
            Assert(IsConsistent());
        }

        void EnableAfterTypeUpdate()
        {TRACE_IT(48455);
            Assert(IsConsistent());
            Assert(this->content.isPopulated);
            Assert(!IsEnabled());
            Assert(this->guard.value == CtorCacheGuardValues::Special);
            Assert(this->content.typeUpdatePending);
            Assert(this->content.slotCount == this->content.pendingType->GetTypeHandler()->GetSlotCapacity());
            Assert(this->content.inlineSlotCount == this->content.pendingType->GetTypeHandler()->GetInlineSlotCapacity());
            Assert(this->content.pendingType->GetIsShared());
            this->content.type = this->content.pendingType;
            this->content.typeIsFinal = true;
            this->content.pendingType = nullptr;
            this->content.typeUpdatePending = false;
            Assert(IsConsistent());
        }

        intptr_t GetRawGuardValue() const
        {TRACE_IT(48456);
            return static_cast<intptr_t>(this->guard.value);
        }

        DynamicType* GetGuardValueAsType() const
        {TRACE_IT(48457);
            return reinterpret_cast<DynamicType*>(this->guard.value & ~CtorCacheGuardValues::TagFlag);
        }

        DynamicType* GetType() const
        {TRACE_IT(48458);
            Assert(static_cast<intptr_t>(this->guard.value & CtorCacheGuardValues::TagFlag) == 0);
            return this->content.type;
        }

        DynamicType* GetPendingType() const
        {TRACE_IT(48459);
            return this->content.pendingType;
        }

        ScriptContext* GetScriptContext() const
        {TRACE_IT(48460);
            return this->content.scriptContext;
        }

        int GetSlotCount() const
        {TRACE_IT(48461);
            return this->content.slotCount;
        }

        int16 GetInlineSlotCount() const
        {TRACE_IT(48462);
            return this->content.inlineSlotCount;
        }

        static bool IsDefault(const ConstructorCache* constructorCache)
        {TRACE_IT(48463);
            return constructorCache == &ConstructorCache::DefaultInstance;
        }

        bool IsDefault() const
        {TRACE_IT(48464);
            return IsDefault(this);
        }

        bool IsPopulated() const
        {TRACE_IT(48465);
            Assert(IsConsistent());
            return this->content.isPopulated;
        }

        bool IsEmpty() const
        {TRACE_IT(48466);
            Assert(IsConsistent());
            return !this->content.isPopulated;
        }

        bool IsPolymorphic() const
        {TRACE_IT(48467);
            Assert(IsConsistent());
            return this->content.isPolymorphic;
        }

        bool GetSkipDefaultNewObject() const
        {TRACE_IT(48468);
            return this->content.skipDefaultNewObject;
        }

        bool GetCtorHasNoExplicitReturnValue() const
        {TRACE_IT(48469);
            return this->content.ctorHasNoExplicitReturnValue;
        }

        bool GetUpdateCacheAfterCtor() const
        {TRACE_IT(48470);
            return this->content.updateAfterCtor;
        }

        bool GetTypeUpdatePending() const
        {TRACE_IT(48471);
            return this->content.typeUpdatePending;
        }

        bool IsEnabled() const
        {TRACE_IT(48472);
            return GetGuardValueAsType() != nullptr;
        }

        bool IsInvalidated() const
        {TRACE_IT(48473);
            return this->guard.value == CtorCacheGuardValues::Invalid && this->content.isPopulated;
        }

        bool NeedsTypeUpdate() const
        {TRACE_IT(48474);
            return this->guard.value == CtorCacheGuardValues::Special && this->content.typeUpdatePending;
        }

        uint8 CallCount() const
        {TRACE_IT(48475);
            return content.callCount;
        }

        void IncCallCount()
        {TRACE_IT(48476);
            ++content.callCount;
            Assert(content.callCount != 0);
        }

        bool NeedsUpdateAfterCtor() const
        {TRACE_IT(48477);
            return this->content.updateAfterCtor;
        }

        bool IsNormal() const
        {TRACE_IT(48478);
            return this->guard.value != CtorCacheGuardValues::Invalid && static_cast<intptr_t>(this->guard.value & CtorCacheGuardValues::TagFlag) == 0;
        }

        bool SkipDefaultNewObject() const
        {TRACE_IT(48479);
            return this->guard.value == CtorCacheGuardValues::Special && this->content.skipDefaultNewObject;
        }

        bool IsSetUpForJit() const
        {TRACE_IT(48480);
            return GetRawGuardValue() != NULL && !IsPolymorphic() && !NeedsUpdateAfterCtor() && (IsNormal() || SkipDefaultNewObject());
        }

        void ClearUpdateAfterCtor()
        {TRACE_IT(48481);
            Assert(IsConsistent());
            Assert(this->content.isPopulated);
            Assert(this->content.updateAfterCtor);
            this->content.updateAfterCtor = false;
            Assert(IsConsistent());
        }

        static ConstructorCache* EnsureValidInstance(ConstructorCache* currentCache, ScriptContext* scriptContext);

        const void* GetAddressOfGuardValue() const
        {TRACE_IT(48482);
            return reinterpret_cast<const void*>(&this->guard.value);
        }

        static uint32 GetOffsetOfUpdateAfterCtor()
        {TRACE_IT(48483);
            return offsetof(ConstructorCache, content.updateAfterCtor);
        }

        void InvalidateAsGuard()
        {TRACE_IT(48484);
            Assert(!IsDefault(this));
            this->guard.value = CtorCacheGuardValues::Invalid;
            // Make sure we don't leak the types.
            Assert(this->content.type == nullptr);
            Assert(this->content.pendingType == nullptr);
            Assert(IsInvalidated());
            Assert(IsConsistent());
        }

#if DBG
        bool IsConsistent() const
        {TRACE_IT(48485);
            return this->guard.value == CtorCacheGuardValues::Invalid ||
                (this->content.isPopulated && (
                    (this->guard.value == CtorCacheGuardValues::Special && !this->content.updateAfterCtor && this->content.skipDefaultNewObject && !this->content.typeUpdatePending && this->content.slotCount == 0 && this->content.inlineSlotCount == 0 && this->content.pendingType == nullptr) ||
                    (this->guard.value == CtorCacheGuardValues::Special && !this->content.updateAfterCtor && this->content.typeUpdatePending && !this->content.skipDefaultNewObject && this->content.pendingType != nullptr) ||
                    ((this->guard.value & CtorCacheGuardValues::TagFlag) == CtorCacheGuardValues::Invalid && !this->content.skipDefaultNewObject && !this->content.typeUpdatePending && this->content.pendingType == nullptr)));
        }
#endif

#if DBG_DUMP
        void Dump() const;
#endif

    private:
        void InvalidateOnPrototypeChange();
    };

    // Caches the result of an instanceof operator over a type and a function
    struct IsInstInlineCache
    {
        Type * type;                    // The type of object operand an inline cache caches a result for
        JavascriptFunction * function;  // The function operand an inline cache caches a result for
        JavascriptBoolean * result;     // The result of doing (object instanceof function) where object->type == this->type
        IsInstInlineCache * next;       // Used to link together caches that have the same function operand

    public:
        bool IsEmpty() const {TRACE_IT(48486); return type == nullptr; }
        bool TryGetResult(Var instance, JavascriptFunction * function, JavascriptBoolean ** result);
        void Cache(Type * instanceType, JavascriptFunction * function, JavascriptBoolean * result, ScriptContext * scriptContext);
        void Unregister(ScriptContext * scriptContext);

        static uint32 OffsetOfFunction();
        static uint32 OffsetOfResult();
        static uint32 OffsetOfType();

    private:
        void Set(Type * instanceType, JavascriptFunction * function, JavascriptBoolean * result);
        void Clear();
    };

    // Two-entry Type-indexed circular cache
    //   cache IsConcatSpreadable() result unless user-defined [@@isConcatSpreadable] exists
    class IsConcatSpreadableCache
    {
        Type *type0, *type1;
        int lastAccess;
        BOOL result0, result1;

    public:
        IsConcatSpreadableCache() :
            type0(nullptr),
            type1(nullptr),
            result0(FALSE),
            result1(FALSE),
            lastAccess(1)
        {TRACE_IT(48487);
        }

        bool TryGetIsConcatSpreadable(Type *type, _Out_ BOOL *result)
        {TRACE_IT(48488);
            Assert(type != nullptr);
            Assert(result != nullptr);

            *result = FALSE;
            if (type0 == type)
            {TRACE_IT(48489);
                *result = result0;
                lastAccess = 0;
                return true;
            }

            if (type1 == type)
            {TRACE_IT(48490);
                *result = result1;
                lastAccess = 1;
                return true;
            }

            return false;
        }

        void CacheIsConcatSpreadable(Type *type, BOOL result)
        {TRACE_IT(48491);
            Assert(type != nullptr);

            if (lastAccess == 0)
            {TRACE_IT(48492);
                type1 = type;
                result1 = result;
                lastAccess = 1;
            }
            else
            {TRACE_IT(48493);
                type0 = type;
                result0 = result;
                lastAccess = 0;
            }
        }

        void Invalidate()
        {TRACE_IT(48494);
            type0 = nullptr;
            type1 = nullptr;
        }
    };

#if defined(_M_IX86_OR_ARM32)
    CompileAssert(sizeof(IsInstInlineCache) == 0x10);
#else
    CompileAssert(sizeof(IsInstInlineCache) == 0x20);
#endif
}
