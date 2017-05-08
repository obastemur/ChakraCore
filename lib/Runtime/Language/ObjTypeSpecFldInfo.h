//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#if ENABLE_NATIVE_CODEGEN

// forward declaration
class JITTimeConstructorCache;

namespace Js
{

#define InitialObjTypeSpecFldInfoFlagValue 0x01

    struct FixedFieldInfo
    {
        Field(Var) fieldValue;
        Field(Type*) type;
        Field(bool) nextHasSameFixedField; // set to true if the next entry in the FixedFieldInfo array on ObjTypeSpecFldInfo has the same type
    };

    // Union with uint16 flags for fast default initialization
    union ObjTypeSpecFldInfoFlags
    {
        struct
        {
            Field(bool) falseReferencePreventionBit : 1;
            Field(bool) isPolymorphic : 1;
            Field(bool) isRootObjectNonConfigurableField : 1;
            Field(bool) isRootObjectNonConfigurableFieldLoad : 1;
            Field(bool) usesAuxSlot : 1;
            Field(bool) isLocal : 1;
            Field(bool) isLoadedFromProto : 1;
            Field(bool) usesAccessor : 1;
            Field(bool) hasFixedValue : 1;
            Field(bool) keepFieldValue : 1;
            Field(bool) isBeingStored : 1;
            Field(bool) isBeingAdded : 1;
            Field(bool) doesntHaveEquivalence : 1;
            Field(bool) isBuiltIn : 1;
        };
        struct
        {
            Field(uint16) flags;
        };
        ObjTypeSpecFldInfoFlags(uint16 flags) : flags(flags) {TRACE_IT(52003); }
    };

    class ObjTypeSpecFldInfo
    {
    private:
        Field(DynamicObject*) protoObject;
        Field(PropertyGuard*) propertyGuard;
        Field(EquivalentTypeSet*) typeSet;
        Field(Type*) initialType;
        Field(JITTimeConstructorCache*) ctorCache;
        Field(FixedFieldInfo*) fixedFieldInfoArray;

        Field(PropertyId) propertyId;
        Field(Js::TypeId) typeId;
        Field(uint) id;

        Field(ObjTypeSpecFldInfoFlags) flags;
        Field(uint16) slotIndex;

        Field(uint16) fixedFieldCount; // currently used only for fields that are functions

    public:
        ObjTypeSpecFldInfo() :
            id(0), typeId(TypeIds_Limit), typeSet(nullptr), initialType(nullptr), flags(InitialObjTypeSpecFldInfoFlagValue),
            slotIndex(Constants::NoSlot), propertyId(Constants::NoProperty), protoObject(nullptr), propertyGuard(nullptr),
            ctorCache(nullptr), fixedFieldInfoArray(nullptr) {TRACE_IT(52004);}

        ObjTypeSpecFldInfo(uint id, TypeId typeId, Type* initialType,
            bool usesAuxSlot, bool isLoadedFromProto, bool usesAccessor, bool isFieldValueFixed, bool keepFieldValue, bool isBuiltIn,
            uint16 slotIndex, PropertyId propertyId, DynamicObject* protoObject, PropertyGuard* propertyGuard,
            JITTimeConstructorCache* ctorCache, FixedFieldInfo* fixedFieldInfoArray) :
            id(id), typeId(typeId), typeSet(nullptr), initialType(initialType), flags(InitialObjTypeSpecFldInfoFlagValue),
            slotIndex(slotIndex), propertyId(propertyId), protoObject(protoObject), propertyGuard(propertyGuard),
            ctorCache(ctorCache), fixedFieldInfoArray(fixedFieldInfoArray)
        {TRACE_IT(52005);
            this->flags.isPolymorphic = false;
            this->flags.usesAuxSlot = usesAuxSlot;
            this->flags.isLocal = !isLoadedFromProto && !usesAccessor;
            this->flags.isLoadedFromProto = isLoadedFromProto;
            this->flags.usesAccessor = usesAccessor;
            this->flags.hasFixedValue = isFieldValueFixed;
            this->flags.keepFieldValue = keepFieldValue;
            this->flags.isBeingAdded = initialType != nullptr;
            this->flags.doesntHaveEquivalence = true; // doesn't mean anything for data from a monomorphic cache
            this->flags.isBuiltIn = isBuiltIn;
            this->fixedFieldCount = 1;
        }

        ObjTypeSpecFldInfo(uint id, TypeId typeId, Type* initialType, EquivalentTypeSet* typeSet,
            bool usesAuxSlot, bool isLoadedFromProto, bool usesAccessor, bool isFieldValueFixed, bool keepFieldValue, bool doesntHaveEquivalence, bool isPolymorphic,
            uint16 slotIndex, PropertyId propertyId, DynamicObject* protoObject, PropertyGuard* propertyGuard,
            JITTimeConstructorCache* ctorCache, FixedFieldInfo* fixedFieldInfoArray, uint16 fixedFieldCount) :
            id(id), typeId(typeId), typeSet(typeSet), initialType(initialType), flags(InitialObjTypeSpecFldInfoFlagValue),
            slotIndex(slotIndex), propertyId(propertyId), protoObject(protoObject), propertyGuard(propertyGuard),
            ctorCache(ctorCache), fixedFieldInfoArray(fixedFieldInfoArray)
        {TRACE_IT(52006);
            this->flags.isPolymorphic = isPolymorphic;
            this->flags.usesAuxSlot = usesAuxSlot;
            this->flags.isLocal = !isLoadedFromProto && !usesAccessor;
            this->flags.isLoadedFromProto = isLoadedFromProto;
            this->flags.usesAccessor = usesAccessor;
            this->flags.hasFixedValue = isFieldValueFixed;
            this->flags.keepFieldValue = keepFieldValue;
            this->flags.isBeingAdded = initialType != nullptr;
            this->flags.doesntHaveEquivalence = doesntHaveEquivalence;
            this->flags.isBuiltIn = false;
            this->fixedFieldCount = fixedFieldCount;
        }

        static ObjTypeSpecFldInfo* CreateFrom(uint id, InlineCache* cache, uint cacheId,
            EntryPointInfo *entryPoint, FunctionBody* const topFunctionBody, FunctionBody *const functionBody, FieldAccessStatsPtr inlineCacheStats);

        static ObjTypeSpecFldInfo* CreateFrom(uint id, PolymorphicInlineCache* cache, uint cacheId,
            EntryPointInfo *entryPoint, FunctionBody* const topFunctionBody, FunctionBody *const functionBody, FieldAccessStatsPtr inlineCacheStats);

        uint GetObjTypeSpecFldId() const
        {TRACE_IT(52007);
            return this->id;
        }

        bool IsMono() const
        {TRACE_IT(52008);
            return !this->flags.isPolymorphic;
        }

        bool IsPoly() const
        {TRACE_IT(52009);
            return this->flags.isPolymorphic;
        }

        bool UsesAuxSlot() const
        {TRACE_IT(52010);
            return this->flags.usesAuxSlot;
        }

        bool IsBuiltin() const
        {TRACE_IT(52011);
            return this->flags.isBuiltIn;
        }

        void SetUsesAuxSlot(bool value)
        {TRACE_IT(52012);
            this->flags.usesAuxSlot = value;
        }

        bool IsLoadedFromProto() const
        {TRACE_IT(52013);
            return this->flags.isLoadedFromProto;
        }

        bool IsLocal() const
        {TRACE_IT(52014);
            return this->flags.isLocal;
        }

        bool UsesAccessor() const
        {TRACE_IT(52015);
            return this->flags.usesAccessor;
        }

        bool HasFixedValue() const
        {TRACE_IT(52016);
            return this->flags.hasFixedValue;
        }

        void SetHasFixedValue(bool value)
        {TRACE_IT(52017);
            this->flags.hasFixedValue = value;
        }

        bool IsBeingStored() const
        {TRACE_IT(52018);
            return this->flags.isBeingStored;
        }

        void SetIsBeingStored(bool value)
        {TRACE_IT(52019);
            this->flags.isBeingStored = value;
        }

        bool IsBeingAdded() const
        {TRACE_IT(52020);
            return this->flags.isBeingAdded;
        }

        bool IsRootObjectNonConfigurableField() const
        {TRACE_IT(52021);
            return this->flags.isRootObjectNonConfigurableField;
        }

        bool IsRootObjectNonConfigurableFieldLoad() const
        {TRACE_IT(52022);
            return this->flags.isRootObjectNonConfigurableField && this->flags.isRootObjectNonConfigurableFieldLoad;
        }

        void SetRootObjectNonConfigurableField(bool isLoad)
        {TRACE_IT(52023);
            this->flags.isRootObjectNonConfigurableField = true;
            this->flags.isRootObjectNonConfigurableFieldLoad = isLoad;
        }

        bool DoesntHaveEquivalence() const
        {TRACE_IT(52024);
            return this->flags.doesntHaveEquivalence;
        }

        void ClearFlags()
        {TRACE_IT(52025);
            this->flags = 0;
        }

        void SetFlags(uint16 flags)
        {TRACE_IT(52026);
            this->flags = flags | 0x01;
        }

        uint16 GetFlags() const
        {TRACE_IT(52027);
            return this->flags.flags;
        }

        uint16 GetSlotIndex() const
        {TRACE_IT(52028);
            return this->slotIndex;
        }

        void SetSlotIndex(uint16 index)
        {TRACE_IT(52029);
            this->slotIndex = index;
        }

        PropertyId GetPropertyId() const
        {TRACE_IT(52030);
            return this->propertyId;
        }

        Js::DynamicObject* GetProtoObject() const
        {TRACE_IT(52031);
            Assert(IsLoadedFromProto());
            return this->protoObject;
        }

        Var GetFieldValue() const
        {TRACE_IT(52032);
            Assert(IsMono() || (IsPoly() && !DoesntHaveEquivalence()));
            return this->fixedFieldInfoArray[0].fieldValue;
        }

        Var GetFieldValue(uint i) const
        {TRACE_IT(52033);
            Assert(IsPoly());
            return this->fixedFieldInfoArray[i].fieldValue;
        }

        void SetFieldValue(Var value)
        {TRACE_IT(52034);
            Assert(IsMono() || (IsPoly() && !DoesntHaveEquivalence()));
            this->fixedFieldInfoArray[0].fieldValue = value;
        }

        Var GetFieldValueAsFixedDataIfAvailable() const;

        Js::JavascriptFunction* GetFieldValueAsFixedFunction() const;
        Js::JavascriptFunction* GetFieldValueAsFixedFunction(uint i) const;

        Js::JavascriptFunction* GetFieldValueAsFunction() const;

        Js::JavascriptFunction* GetFieldValueAsFunctionIfAvailable() const;

        Js::JavascriptFunction* GetFieldValueAsFixedFunctionIfAvailable() const;
        Js::JavascriptFunction* GetFieldValueAsFixedFunctionIfAvailable(uint i) const;

        bool GetKeepFieldValue() const
        {TRACE_IT(52035);
            return this->flags.keepFieldValue;
        }

        JITTimeConstructorCache* GetCtorCache() const
        {TRACE_IT(52036);
            return this->ctorCache;
        }

        Js::PropertyGuard* GetPropertyGuard() const
        {TRACE_IT(52037);
            return this->propertyGuard;
        }

        bool IsObjTypeSpecCandidate() const
        {TRACE_IT(52038);
            return true;
        }

        bool IsMonoObjTypeSpecCandidate() const
        {TRACE_IT(52039);
            return IsObjTypeSpecCandidate() && IsMono();
        }

        bool IsPolyObjTypeSpecCandidate() const
        {TRACE_IT(52040);
            return IsObjTypeSpecCandidate() && IsPoly();
        }

        Js::TypeId GetTypeId() const
        {TRACE_IT(52041);
            Assert(typeId != TypeIds_Limit);
            return this->typeId;
        }

        Js::TypeId GetTypeId(uint i) const
        {TRACE_IT(52042);
            Assert(IsPoly());
            return this->fixedFieldInfoArray[i].type->GetTypeId();
        }

        Js::Type * GetType() const
        {TRACE_IT(52043);
            Assert(IsObjTypeSpecCandidate() && IsMono());
            return this->fixedFieldInfoArray[0].type;
        }

        Js::Type * GetType(uint i) const
        {TRACE_IT(52044);
            Assert(IsPoly());
            return this->fixedFieldInfoArray[i].type;
        }

        bool HasInitialType() const
        {TRACE_IT(52045);
            return IsObjTypeSpecCandidate() && IsMono() && !IsLoadedFromProto() && this->initialType != nullptr;
        }

        Js::Type * GetInitialType() const
        {TRACE_IT(52046);
            Assert(IsObjTypeSpecCandidate() && IsMono() && !IsLoadedFromProto());
            return this->initialType;
        }

        Js::EquivalentTypeSet * GetEquivalentTypeSet() const
        {TRACE_IT(52047);
            Assert(IsObjTypeSpecCandidate());
            return this->typeSet;
        }

        JITTypeHolder GetFirstEquivalentType() const;

        Js::FixedFieldInfo* GetFixedFieldInfoArray()
        {TRACE_IT(52048);
            return this->fixedFieldInfoArray;
        }

        uint16 GetFixedFieldCount() const
        {TRACE_IT(52049);
            return this->fixedFieldCount;
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        const char16 *GetCacheLayoutString() const;
#endif
    };

    class ObjTypeSpecFldInfoArray
    {
    private:
        Field(Field(ObjTypeSpecFldInfo*)*) infoArray;
#if DBG
        Field(uint) infoCount;
#endif
    public:
        ObjTypeSpecFldInfoArray();

    private:
        void EnsureArray(Recycler *const recycler, FunctionBody *const functionBody);

    public:
        ObjTypeSpecFldInfo* GetInfo(FunctionBody *const functionBody, const uint index) const;
        ObjTypeSpecFldInfo* GetInfo(const uint index) const;
        Field(ObjTypeSpecFldInfo*)* GetInfoArray() const {TRACE_IT(52050); return infoArray; }

        void SetInfo(Recycler *const recycler, FunctionBody *const functionBody,
            const uint index, ObjTypeSpecFldInfo* info);

        void Reset();

        template <class Fn>
        void Map(Fn fn, uint count) const
        {TRACE_IT(52051);
            if (this->infoArray != nullptr)
            {TRACE_IT(52052);
                for (uint i = 0; i < count; i++)
                {TRACE_IT(52053);
                    ObjTypeSpecFldInfo* info = this->infoArray[i];

                    if (info != nullptr)
                    {TRACE_IT(52054);
                        fn(info);
                    }
                }
            }
        };

        PREVENT_COPY(ObjTypeSpecFldInfoArray)
    };
}
#endif // ENABLE_NATIVE_CODEGEN

