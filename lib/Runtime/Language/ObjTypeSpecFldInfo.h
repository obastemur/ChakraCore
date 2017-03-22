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
        ObjTypeSpecFldInfoFlags(uint16 flags) : flags(flags) {LOGMEIN("ObjTypeSpecFldInfo.h] 48\n"); }
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
            ctorCache(nullptr), fixedFieldInfoArray(nullptr) {LOGMEIN("ObjTypeSpecFldInfo.h] 74\n");}

        ObjTypeSpecFldInfo(uint id, TypeId typeId, Type* initialType,
            bool usesAuxSlot, bool isLoadedFromProto, bool usesAccessor, bool isFieldValueFixed, bool keepFieldValue, bool isBuiltIn,
            uint16 slotIndex, PropertyId propertyId, DynamicObject* protoObject, PropertyGuard* propertyGuard,
            JITTimeConstructorCache* ctorCache, FixedFieldInfo* fixedFieldInfoArray) :
            id(id), typeId(typeId), typeSet(nullptr), initialType(initialType), flags(InitialObjTypeSpecFldInfoFlagValue),
            slotIndex(slotIndex), propertyId(propertyId), protoObject(protoObject), propertyGuard(propertyGuard),
            ctorCache(ctorCache), fixedFieldInfoArray(fixedFieldInfoArray)
        {LOGMEIN("ObjTypeSpecFldInfo.h] 83\n");
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
        {LOGMEIN("ObjTypeSpecFldInfo.h] 104\n");
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
        {LOGMEIN("ObjTypeSpecFldInfo.h] 125\n");
            return this->id;
        }

        bool IsMono() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 130\n");
            return !this->flags.isPolymorphic;
        }

        bool IsPoly() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 135\n");
            return this->flags.isPolymorphic;
        }

        bool UsesAuxSlot() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 140\n");
            return this->flags.usesAuxSlot;
        }

        bool IsBuiltin() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 145\n");
            return this->flags.isBuiltIn;
        }

        void SetUsesAuxSlot(bool value)
        {LOGMEIN("ObjTypeSpecFldInfo.h] 150\n");
            this->flags.usesAuxSlot = value;
        }

        bool IsLoadedFromProto() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 155\n");
            return this->flags.isLoadedFromProto;
        }

        bool IsLocal() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 160\n");
            return this->flags.isLocal;
        }

        bool UsesAccessor() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 165\n");
            return this->flags.usesAccessor;
        }

        bool HasFixedValue() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 170\n");
            return this->flags.hasFixedValue;
        }

        void SetHasFixedValue(bool value)
        {LOGMEIN("ObjTypeSpecFldInfo.h] 175\n");
            this->flags.hasFixedValue = value;
        }

        bool IsBeingStored() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 180\n");
            return this->flags.isBeingStored;
        }

        void SetIsBeingStored(bool value)
        {LOGMEIN("ObjTypeSpecFldInfo.h] 185\n");
            this->flags.isBeingStored = value;
        }

        bool IsBeingAdded() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 190\n");
            return this->flags.isBeingAdded;
        }

        bool IsRootObjectNonConfigurableField() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 195\n");
            return this->flags.isRootObjectNonConfigurableField;
        }

        bool IsRootObjectNonConfigurableFieldLoad() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 200\n");
            return this->flags.isRootObjectNonConfigurableField && this->flags.isRootObjectNonConfigurableFieldLoad;
        }

        void SetRootObjectNonConfigurableField(bool isLoad)
        {LOGMEIN("ObjTypeSpecFldInfo.h] 205\n");
            this->flags.isRootObjectNonConfigurableField = true;
            this->flags.isRootObjectNonConfigurableFieldLoad = isLoad;
        }

        bool DoesntHaveEquivalence() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 211\n");
            return this->flags.doesntHaveEquivalence;
        }

        void ClearFlags()
        {LOGMEIN("ObjTypeSpecFldInfo.h] 216\n");
            this->flags = 0;
        }

        void SetFlags(uint16 flags)
        {LOGMEIN("ObjTypeSpecFldInfo.h] 221\n");
            this->flags = flags | 0x01;
        }

        uint16 GetFlags() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 226\n");
            return this->flags.flags;
        }

        uint16 GetSlotIndex() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 231\n");
            return this->slotIndex;
        }

        void SetSlotIndex(uint16 index)
        {LOGMEIN("ObjTypeSpecFldInfo.h] 236\n");
            this->slotIndex = index;
        }

        PropertyId GetPropertyId() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 241\n");
            return this->propertyId;
        }

        Js::DynamicObject* GetProtoObject() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 246\n");
            Assert(IsLoadedFromProto());
            return this->protoObject;
        }

        Var GetFieldValue() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 252\n");
            Assert(IsMono() || (IsPoly() && !DoesntHaveEquivalence()));
            return this->fixedFieldInfoArray[0].fieldValue;
        }

        Var GetFieldValue(uint i) const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 258\n");
            Assert(IsPoly());
            return this->fixedFieldInfoArray[i].fieldValue;
        }

        void SetFieldValue(Var value)
        {LOGMEIN("ObjTypeSpecFldInfo.h] 264\n");
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
        {LOGMEIN("ObjTypeSpecFldInfo.h] 282\n");
            return this->flags.keepFieldValue;
        }

        JITTimeConstructorCache* GetCtorCache() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 287\n");
            return this->ctorCache;
        }

        Js::PropertyGuard* GetPropertyGuard() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 292\n");
            return this->propertyGuard;
        }

        bool IsObjTypeSpecCandidate() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 297\n");
            return true;
        }

        bool IsMonoObjTypeSpecCandidate() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 302\n");
            return IsObjTypeSpecCandidate() && IsMono();
        }

        bool IsPolyObjTypeSpecCandidate() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 307\n");
            return IsObjTypeSpecCandidate() && IsPoly();
        }

        Js::TypeId GetTypeId() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 312\n");
            Assert(typeId != TypeIds_Limit);
            return this->typeId;
        }

        Js::TypeId GetTypeId(uint i) const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 318\n");
            Assert(IsPoly());
            return this->fixedFieldInfoArray[i].type->GetTypeId();
        }

        Js::Type * GetType() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 324\n");
            Assert(IsObjTypeSpecCandidate() && IsMono());
            return this->fixedFieldInfoArray[0].type;
        }

        Js::Type * GetType(uint i) const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 330\n");
            Assert(IsPoly());
            return this->fixedFieldInfoArray[i].type;
        }

        bool HasInitialType() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 336\n");
            return IsObjTypeSpecCandidate() && IsMono() && !IsLoadedFromProto() && this->initialType != nullptr;
        }

        Js::Type * GetInitialType() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 341\n");
            Assert(IsObjTypeSpecCandidate() && IsMono() && !IsLoadedFromProto());
            return this->initialType;
        }

        Js::EquivalentTypeSet * GetEquivalentTypeSet() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 347\n");
            Assert(IsObjTypeSpecCandidate());
            return this->typeSet;
        }

        JITTypeHolder GetFirstEquivalentType() const;

        Js::FixedFieldInfo* GetFixedFieldInfoArray()
        {LOGMEIN("ObjTypeSpecFldInfo.h] 355\n");
            return this->fixedFieldInfoArray;
        }

        uint16 GetFixedFieldCount() const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 360\n");
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
        Field(ObjTypeSpecFldInfo*)* GetInfoArray() const {LOGMEIN("ObjTypeSpecFldInfo.h] 385\n"); return infoArray; }

        void SetInfo(Recycler *const recycler, FunctionBody *const functionBody,
            const uint index, ObjTypeSpecFldInfo* info);

        void Reset();

        template <class Fn>
        void Map(Fn fn, uint count) const
        {LOGMEIN("ObjTypeSpecFldInfo.h] 394\n");
            if (this->infoArray != nullptr)
            {LOGMEIN("ObjTypeSpecFldInfo.h] 396\n");
                for (uint i = 0; i < count; i++)
                {LOGMEIN("ObjTypeSpecFldInfo.h] 398\n");
                    ObjTypeSpecFldInfo* info = this->infoArray[i];

                    if (info != nullptr)
                    {LOGMEIN("ObjTypeSpecFldInfo.h] 402\n");
                        fn(info);
                    }
                }
            }
        };

        PREVENT_COPY(ObjTypeSpecFldInfoArray)
    };
}
#endif // ENABLE_NATIVE_CODEGEN

