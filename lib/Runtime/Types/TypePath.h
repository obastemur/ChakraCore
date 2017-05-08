//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class TinyDictionary
    {
        static const int PowerOf2_BUCKETS = 8;
        static const int BUCKETS_DWORDS = PowerOf2_BUCKETS / sizeof(DWORD);
        static const byte NIL = 0xff;

        Field(DWORD) bucketsData[BUCKETS_DWORDS];  // use DWORDs to enforce alignment
        Field(byte) next[0];

public:
        TinyDictionary()
        {TRACE_IT(67983);
            CompileAssert(BUCKETS_DWORDS * sizeof(DWORD) == PowerOf2_BUCKETS);
            CompileAssert(BUCKETS_DWORDS == 2);
            DWORD* init = bucketsData;
            init[0] = init[1] = 0xffffffff;
        }

        void Add(PropertyId key, byte value)
        {TRACE_IT(67984);
            byte* buckets = reinterpret_cast<byte*>(bucketsData);
            uint32 bucketIndex = key & (PowerOf2_BUCKETS - 1);

            byte i = buckets[bucketIndex];
            buckets[bucketIndex] = value;
            next[value] = i;
        }

        // Template shared with diagnostics
        template <class Data>
        inline bool TryGetValue(PropertyId key, PropertyIndex* index, const Data& data)
        {TRACE_IT(67985);
            byte* buckets = reinterpret_cast<byte*>(bucketsData);
            uint32 bucketIndex = key & (PowerOf2_BUCKETS - 1);

            for (byte i = buckets[bucketIndex] ; i != NIL ; i = next[i])
            {TRACE_IT(67986);
                if (data[i]->GetPropertyId()== key)
                {TRACE_IT(67987);
                    *index = i;
                    return true;
                }
                Assert(i != next[i]);
            }
            return false;
        }
    };

    class TypePath
    {
        friend class PathTypeHandlerBase;
    public:
        // This is the space between the end of the TypePath and the allocation granularity that can be used for assignments too.
#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
#if defined(_M_X64_OR_ARM64)
#define TYPE_PATH_ALLOC_GRANULARITY_GAP 0
#else
#define TYPE_PATH_ALLOC_GRANULARITY_GAP 2
#endif
#else
#if defined(_M_X64_OR_ARM64)
#define TYPE_PATH_ALLOC_GRANULARITY_GAP 1
#else
#define TYPE_PATH_ALLOC_GRANULARITY_GAP 3
#endif
#endif
        // Although we can allocate 2 more, this will put struct Data into another bucket.  Just waste some slot in that case for 32-bit
        static const uint MaxPathTypeHandlerLength = 128;
        static const uint InitialTypePathSize = 16 + TYPE_PATH_ALLOC_GRANULARITY_GAP;

    private:
        struct Data
        {
            Data(uint8 pathSize) : pathSize(pathSize), pathLength(0)
#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
                , maxInitializedLength(0)
#endif
            {}

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
            Field(BVStatic<MaxPathTypeHandlerLength>) fixedFields;
            Field(BVStatic<MaxPathTypeHandlerLength>) usedFixedFields;

            // We sometimes set up PathTypeHandlers and associate TypePaths before we create any instances
            // that populate the corresponding slots, e.g. for object literals or constructors with only
            // this statements.  This field keeps track of the longest instance associated with the given
            // TypePath.
            Field(uint8) maxInitializedLength;
#endif
            Field(uint8) pathLength;      // Entries in use
            Field(uint8) pathSize;        // Allocated entries

            // This map has to be at the end, because TinyDictionary has a zero size array
            Field(TinyDictionary) map;

            int Add(const PropertyRecord * propertyId, Field(const PropertyRecord *)* assignments);
        };
        Field(Data*) data;

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
        Field(RecyclerWeakReference<DynamicObject>*) singletonInstance;
#endif

        // PropertyRecord assignments are allocated off the end of the structure
        Field(const PropertyRecord *) assignments[];


        TypePath() :
#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
            singletonInstance(nullptr),
#endif
            data(nullptr)
        {TRACE_IT(67988);
        }

        Data * GetData() {TRACE_IT(67989); return data; }
    public:
        static TypePath* New(Recycler* recycler, uint size = InitialTypePathSize);

        TypePath * Branch(Recycler * alloc, int pathLength, bool couldSeeProto);

        TypePath * Grow(Recycler * alloc);

        const PropertyRecord* GetPropertyIdUnchecked(int index)
        {TRACE_IT(67990);
            Assert(((uint)index) < ((uint)this->GetPathLength()));
            return assignments[index];
        }

        const PropertyRecord* GetPropertyId(int index)
        {TRACE_IT(67991);
            if (((uint)index) < ((uint)this->GetPathLength()))
                return GetPropertyIdUnchecked(index);
            else
                return nullptr;
        }

        int Add(const PropertyRecord * propertyRecord)
        {TRACE_IT(67992);
#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
            Assert(this->GetPathLength() == this->GetMaxInitializedLength());
            this->GetData()->maxInitializedLength++;
#endif
            return AddInternal(propertyRecord);
        }

        uint8 GetPathLength() {TRACE_IT(67993); return this->GetData()->pathLength; }
        uint8 GetPathSize() {TRACE_IT(67994); return this->GetData()->pathSize; }

        PropertyIndex Lookup(PropertyId propId,int typePathLength);
        PropertyIndex LookupInline(PropertyId propId,int typePathLength);

    private:
        int AddInternal(const PropertyRecord* propId);

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
        uint8 GetMaxInitializedLength() {TRACE_IT(67995); return this->GetData()->maxInitializedLength; }
        void SetMaxInitializedLength(int newMaxInitializedLength)
        {TRACE_IT(67996);
            Assert(newMaxInitializedLength >= 0);
            Assert(newMaxInitializedLength <= MaxPathTypeHandlerLength);
            Assert(this->GetMaxInitializedLength() <= newMaxInitializedLength);
            this->GetData()->maxInitializedLength = (uint8)newMaxInitializedLength;
        }

        Var GetSingletonFixedFieldAt(PropertyIndex index, int typePathLength, ScriptContext * requestContext);

        bool HasSingletonInstance() const
        {TRACE_IT(67997);
            return this->singletonInstance != nullptr;
        }

        RecyclerWeakReference<DynamicObject>* GetSingletonInstance() const
        {TRACE_IT(67998);
            return this->singletonInstance;
        }

        void SetSingletonInstance(RecyclerWeakReference<DynamicObject>* instance, int typePathLength)
        {TRACE_IT(67999);
            Assert(this->singletonInstance == nullptr && instance != nullptr);
            Assert(typePathLength >= this->GetMaxInitializedLength());
            this->singletonInstance = instance;
        }

        void ClearSingletonInstance()
        {TRACE_IT(68000);
            this->singletonInstance = nullptr;
        }

        void ClearSingletonInstanceIfSame(DynamicObject* instance)
        {TRACE_IT(68001);
            if (this->singletonInstance != nullptr && this->singletonInstance->Get() == instance)
            {TRACE_IT(68002);
                ClearSingletonInstance();
            }
        }

        void ClearSingletonInstanceIfDifferent(DynamicObject* instance)
        {TRACE_IT(68003);
            if (this->singletonInstance != nullptr && this->singletonInstance->Get() != instance)
            {TRACE_IT(68004);
                ClearSingletonInstance();
            }
        }

        bool GetIsFixedFieldAt(PropertyIndex index, int typePathLength)
        {TRACE_IT(68005);
            Assert(index < this->GetPathLength());
            Assert(index < typePathLength);
            Assert(typePathLength <= this->GetPathLength());

            return this->GetData()->fixedFields.Test(index) != 0;
        }

        bool GetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength)
        {TRACE_IT(68006);
            Assert(index < this->GetPathLength());
            Assert(index < typePathLength);
            Assert(typePathLength <= this->GetPathLength());

            return this->GetData()->usedFixedFields.Test(index) != 0;
        }

        void SetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength)
        {TRACE_IT(68007);
            Assert(index < this->GetMaxInitializedLength());
            Assert(CanHaveFixedFields(typePathLength));
            this->GetData()->usedFixedFields.Set(index);
        }

        void ClearIsFixedFieldAt(PropertyIndex index, int typePathLength)
        {TRACE_IT(68008);
            Assert(index < this->GetMaxInitializedLength());
            Assert(index < typePathLength);
            Assert(typePathLength <= this->GetPathLength());

            this->GetData()->fixedFields.Clear(index);
            this->GetData()->usedFixedFields.Clear(index);
        }

        bool CanHaveFixedFields(int typePathLength)
        {TRACE_IT(68009);
            // We only support fixed fields on singleton instances.
            // If the instance in question is a singleton, it must be the tip of the type path.
            return this->singletonInstance != nullptr && typePathLength >= this->GetData()->maxInitializedLength;
        }

        void AddBlankFieldAt(PropertyIndex index, int typePathLength);

        void AddSingletonInstanceFieldAt(DynamicObject* instance, PropertyIndex index, bool isFixed, int typePathLength);

        void AddSingletonInstanceFieldAt(PropertyIndex index, int typePathLength);

#if DBG
        bool HasSingletonInstanceOnlyIfNeeded();
#endif

#else
        int GetMaxInitializedLength() {TRACE_IT(68010); Assert(false); return this->pathLength; }

        Var GetSingletonFixedFieldAt(PropertyIndex index, int typePathLength, ScriptContext * requestContext);

        bool HasSingletonInstance() const {TRACE_IT(68011); Assert(false); return false; }
        RecyclerWeakReference<DynamicObject>* GetSingletonInstance() const {TRACE_IT(68012); Assert(false); return nullptr; }
        void SetSingletonInstance(RecyclerWeakReference<DynamicObject>* instance, int typePathLength) {TRACE_IT(68013); Assert(false); }
        void ClearSingletonInstance() {TRACE_IT(68014); Assert(false); }
        void ClearSingletonInstanceIfSame(RecyclerWeakReference<DynamicObject>* instance) {TRACE_IT(68015); Assert(false); }
        void ClearSingletonInstanceIfDifferent(RecyclerWeakReference<DynamicObject>* instance) {TRACE_IT(68016); Assert(false); }

        bool GetIsFixedFieldAt(PropertyIndex index, int typePathLength) {TRACE_IT(68017); Assert(false); return false; }
        bool GetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength) {TRACE_IT(68018); Assert(false); return false; }
        void SetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength) {TRACE_IT(68019); Assert(false); }
        void ClearIsFixedFieldAt(PropertyIndex index, int typePathLength) {TRACE_IT(68020); Assert(false); }
        bool CanHaveFixedFields(int typePathLength) {TRACE_IT(68021); Assert(false); return false; }
        void AddBlankFieldAt(PropertyIndex index, int typePathLength) {TRACE_IT(68022); Assert(false); }
        void AddSingletonInstanceFieldAt(DynamicObject* instance, PropertyIndex index, bool isFixed, int typePathLength) {TRACE_IT(68023); Assert(false); }
        void AddSingletonInstanceFieldAt(PropertyIndex index, int typePathLength) {TRACE_IT(68024); Assert(false); }
#if DBG
        bool HasSingletonInstanceOnlyIfNeeded();
#endif
#endif
    };
}

CompileAssert((sizeof(Js::TypePath) % HeapConstants::ObjectGranularity) / sizeof(void *) == TYPE_PATH_ALLOC_GRANULARITY_GAP);