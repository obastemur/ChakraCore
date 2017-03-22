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
        {LOGMEIN("TypePath.h] 19\n");
            CompileAssert(BUCKETS_DWORDS * sizeof(DWORD) == PowerOf2_BUCKETS);
            CompileAssert(BUCKETS_DWORDS == 2);
            DWORD* init = bucketsData;
            init[0] = init[1] = 0xffffffff;
        }

        void Add(PropertyId key, byte value)
        {LOGMEIN("TypePath.h] 27\n");
            byte* buckets = reinterpret_cast<byte*>(bucketsData);
            uint32 bucketIndex = key & (PowerOf2_BUCKETS - 1);

            byte i = buckets[bucketIndex];
            buckets[bucketIndex] = value;
            next[value] = i;
        }

        // Template shared with diagnostics
        template <class Data>
        inline bool TryGetValue(PropertyId key, PropertyIndex* index, const Data& data)
        {LOGMEIN("TypePath.h] 39\n");
            byte* buckets = reinterpret_cast<byte*>(bucketsData);
            uint32 bucketIndex = key & (PowerOf2_BUCKETS - 1);

            for (byte i = buckets[bucketIndex] ; i != NIL ; i = next[i])
            {LOGMEIN("TypePath.h] 44\n");
                if (data[i]->GetPropertyId()== key)
                {LOGMEIN("TypePath.h] 46\n");
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
        {LOGMEIN("TypePath.h] 120\n");
        }

        Data * GetData() {LOGMEIN("TypePath.h] 123\n"); return data; }
    public:
        static TypePath* New(Recycler* recycler, uint size = InitialTypePathSize);

        TypePath * Branch(Recycler * alloc, int pathLength, bool couldSeeProto);

        TypePath * Grow(Recycler * alloc);

        const PropertyRecord* GetPropertyIdUnchecked(int index)
        {LOGMEIN("TypePath.h] 132\n");
            Assert(((uint)index) < ((uint)this->GetPathLength()));
            return assignments[index];
        }

        const PropertyRecord* GetPropertyId(int index)
        {LOGMEIN("TypePath.h] 138\n");
            if (((uint)index) < ((uint)this->GetPathLength()))
                return GetPropertyIdUnchecked(index);
            else
                return nullptr;
        }

        int Add(const PropertyRecord * propertyRecord)
        {LOGMEIN("TypePath.h] 146\n");
#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
            Assert(this->GetPathLength() == this->GetMaxInitializedLength());
            this->GetData()->maxInitializedLength++;
#endif
            return AddInternal(propertyRecord);
        }

        uint8 GetPathLength() {LOGMEIN("TypePath.h] 154\n"); return this->GetData()->pathLength; }
        uint8 GetPathSize() {LOGMEIN("TypePath.h] 155\n"); return this->GetData()->pathSize; }

        PropertyIndex Lookup(PropertyId propId,int typePathLength);
        PropertyIndex LookupInline(PropertyId propId,int typePathLength);

    private:
        int AddInternal(const PropertyRecord* propId);

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
        uint8 GetMaxInitializedLength() {LOGMEIN("TypePath.h] 164\n"); return this->GetData()->maxInitializedLength; }
        void SetMaxInitializedLength(int newMaxInitializedLength)
        {LOGMEIN("TypePath.h] 166\n");
            Assert(newMaxInitializedLength >= 0);
            Assert(newMaxInitializedLength <= MaxPathTypeHandlerLength);
            Assert(this->GetMaxInitializedLength() <= newMaxInitializedLength);
            this->GetData()->maxInitializedLength = (uint8)newMaxInitializedLength;
        }

        Var GetSingletonFixedFieldAt(PropertyIndex index, int typePathLength, ScriptContext * requestContext);

        bool HasSingletonInstance() const
        {LOGMEIN("TypePath.h] 176\n");
            return this->singletonInstance != nullptr;
        }

        RecyclerWeakReference<DynamicObject>* GetSingletonInstance() const
        {LOGMEIN("TypePath.h] 181\n");
            return this->singletonInstance;
        }

        void SetSingletonInstance(RecyclerWeakReference<DynamicObject>* instance, int typePathLength)
        {LOGMEIN("TypePath.h] 186\n");
            Assert(this->singletonInstance == nullptr && instance != nullptr);
            Assert(typePathLength >= this->GetMaxInitializedLength());
            this->singletonInstance = instance;
        }

        void ClearSingletonInstance()
        {LOGMEIN("TypePath.h] 193\n");
            this->singletonInstance = nullptr;
        }

        void ClearSingletonInstanceIfSame(DynamicObject* instance)
        {LOGMEIN("TypePath.h] 198\n");
            if (this->singletonInstance != nullptr && this->singletonInstance->Get() == instance)
            {LOGMEIN("TypePath.h] 200\n");
                ClearSingletonInstance();
            }
        }

        void ClearSingletonInstanceIfDifferent(DynamicObject* instance)
        {LOGMEIN("TypePath.h] 206\n");
            if (this->singletonInstance != nullptr && this->singletonInstance->Get() != instance)
            {LOGMEIN("TypePath.h] 208\n");
                ClearSingletonInstance();
            }
        }

        bool GetIsFixedFieldAt(PropertyIndex index, int typePathLength)
        {LOGMEIN("TypePath.h] 214\n");
            Assert(index < this->GetPathLength());
            Assert(index < typePathLength);
            Assert(typePathLength <= this->GetPathLength());

            return this->GetData()->fixedFields.Test(index) != 0;
        }

        bool GetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength)
        {LOGMEIN("TypePath.h] 223\n");
            Assert(index < this->GetPathLength());
            Assert(index < typePathLength);
            Assert(typePathLength <= this->GetPathLength());

            return this->GetData()->usedFixedFields.Test(index) != 0;
        }

        void SetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength)
        {LOGMEIN("TypePath.h] 232\n");
            Assert(index < this->GetMaxInitializedLength());
            Assert(CanHaveFixedFields(typePathLength));
            this->GetData()->usedFixedFields.Set(index);
        }

        void ClearIsFixedFieldAt(PropertyIndex index, int typePathLength)
        {LOGMEIN("TypePath.h] 239\n");
            Assert(index < this->GetMaxInitializedLength());
            Assert(index < typePathLength);
            Assert(typePathLength <= this->GetPathLength());

            this->GetData()->fixedFields.Clear(index);
            this->GetData()->usedFixedFields.Clear(index);
        }

        bool CanHaveFixedFields(int typePathLength)
        {LOGMEIN("TypePath.h] 249\n");
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
        int GetMaxInitializedLength() {LOGMEIN("TypePath.h] 266\n"); Assert(false); return this->pathLength; }

        Var GetSingletonFixedFieldAt(PropertyIndex index, int typePathLength, ScriptContext * requestContext);

        bool HasSingletonInstance() const {LOGMEIN("TypePath.h] 270\n"); Assert(false); return false; }
        RecyclerWeakReference<DynamicObject>* GetSingletonInstance() const {LOGMEIN("TypePath.h] 271\n"); Assert(false); return nullptr; }
        void SetSingletonInstance(RecyclerWeakReference<DynamicObject>* instance, int typePathLength) {LOGMEIN("TypePath.h] 272\n"); Assert(false); }
        void ClearSingletonInstance() {LOGMEIN("TypePath.h] 273\n"); Assert(false); }
        void ClearSingletonInstanceIfSame(RecyclerWeakReference<DynamicObject>* instance) {LOGMEIN("TypePath.h] 274\n"); Assert(false); }
        void ClearSingletonInstanceIfDifferent(RecyclerWeakReference<DynamicObject>* instance) {LOGMEIN("TypePath.h] 275\n"); Assert(false); }

        bool GetIsFixedFieldAt(PropertyIndex index, int typePathLength) {LOGMEIN("TypePath.h] 277\n"); Assert(false); return false; }
        bool GetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength) {LOGMEIN("TypePath.h] 278\n"); Assert(false); return false; }
        void SetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength) {LOGMEIN("TypePath.h] 279\n"); Assert(false); }
        void ClearIsFixedFieldAt(PropertyIndex index, int typePathLength) {LOGMEIN("TypePath.h] 280\n"); Assert(false); }
        bool CanHaveFixedFields(int typePathLength) {LOGMEIN("TypePath.h] 281\n"); Assert(false); return false; }
        void AddBlankFieldAt(PropertyIndex index, int typePathLength) {LOGMEIN("TypePath.h] 282\n"); Assert(false); }
        void AddSingletonInstanceFieldAt(DynamicObject* instance, PropertyIndex index, bool isFixed, int typePathLength) {LOGMEIN("TypePath.h] 283\n"); Assert(false); }
        void AddSingletonInstanceFieldAt(PropertyIndex index, int typePathLength) {LOGMEIN("TypePath.h] 284\n"); Assert(false); }
#if DBG
        bool HasSingletonInstanceOnlyIfNeeded();
#endif
#endif
    };
}

CompileAssert((sizeof(Js::TypePath) % HeapConstants::ObjectGranularity) / sizeof(void *) == TYPE_PATH_ALLOC_GRANULARITY_GAP);