//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class DynamicObjectPropertyEnumerator
    {
    private:
        Field(ScriptContext *) scriptContext;
        Field(DynamicObject *) object;
        Field(DynamicType *) initialType;              // for snapshot enumeration
        Field(BigPropertyIndex) objectIndex;
        Field(BigPropertyIndex) initialPropertyCount;
        Field(int) enumeratedCount;

        Field(EnumeratorFlags) flags;

        struct CachedData
        {
            Field(ScriptContext *) scriptContext;
            Field(Field(PropertyString*)*) strings;
            Field(BigPropertyIndex *) indexes;
            Field(PropertyAttributes *) attributes;
            Field(int) cachedCount;
            Field(int) propertyCount;
            Field(bool) completed;
            Field(bool) enumNonEnumerable;
            Field(bool) enumSymbols;
        };
        Field(CachedData *) cachedData;

        DynamicType * GetTypeToEnumerate() const;
        JavascriptString * MoveAndGetNextWithCache(PropertyId& propertyId, PropertyAttributes* attributes);
        JavascriptString * MoveAndGetNextNoCache(PropertyId& propertyId, PropertyAttributes * attributes);

        void Initialize(DynamicType * type, CachedData * data, Js::BigPropertyIndex initialPropertyCount);
    public:
        DynamicObject * GetObject() const {TRACE_IT(66011); return object; }
        EnumeratorFlags GetFlags() const {TRACE_IT(66012); return flags; }
        bool GetEnumNonEnumerable() const;
        bool GetEnumSymbols() const;
        bool GetSnapShotSemantics() const;
        bool GetUseCache() const;
        ScriptContext * GetScriptContext() const {TRACE_IT(66013); return scriptContext; }

        bool Initialize(DynamicObject * object, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache);
        bool IsNullEnumerator() const;
        void Reset();
        void Clear(EnumeratorFlags flags, ScriptContext * requestContext);
        Var MoveAndGetNext(PropertyId& propertyId, PropertyAttributes * attributes);

        bool CanUseJITFastPath() const;
        static uint32 GetOffsetOfScriptContext() {TRACE_IT(66014); return offsetof(DynamicObjectPropertyEnumerator, scriptContext); }
        static uint32 GetOffsetOfInitialType() {TRACE_IT(66015); return offsetof(DynamicObjectPropertyEnumerator, initialType); }
        static uint32 GetOffsetOfObject() {TRACE_IT(66016); return offsetof(DynamicObjectPropertyEnumerator, object); }
        static uint32 GetOffsetOfObjectIndex() {TRACE_IT(66017); return offsetof(DynamicObjectPropertyEnumerator, objectIndex); }
        static uint32 GetOffsetOfInitialPropertyCount() {TRACE_IT(66018); return offsetof(DynamicObjectPropertyEnumerator, initialPropertyCount); }
        static uint32 GetOffsetOfEnumeratedCount() {TRACE_IT(66019); return offsetof(DynamicObjectPropertyEnumerator, enumeratedCount); }
        static uint32 GetOffsetOfCachedData() {TRACE_IT(66020); return offsetof(DynamicObjectPropertyEnumerator, cachedData); }
        static uint32 GetOffsetOfFlags() {TRACE_IT(66021); return offsetof(DynamicObjectPropertyEnumerator, flags);
        }
        static uint32 GetOffsetOfCachedDataStrings() {TRACE_IT(66022); return offsetof(CachedData, strings); }
        static uint32 GetOffsetOfCachedDataIndexes() {TRACE_IT(66023); return offsetof(CachedData, indexes); }
        static uint32 GetOffsetOfCachedDataPropertyCount() {TRACE_IT(66024); return offsetof(CachedData, propertyCount); }
        static uint32 GetOffsetOfCachedDataCachedCount() {TRACE_IT(66025); return offsetof(CachedData, cachedCount); }
        static uint32 GetOffsetOfCachedDataPropertyAttributes() {TRACE_IT(66026); return offsetof(CachedData, attributes); }
        static uint32 GetOffsetOfCachedDataCompleted() {TRACE_IT(66027); return offsetof(CachedData, completed); }
        static uint32 GetOffsetOfCachedDataEnumNonEnumerable() {TRACE_IT(66028); return offsetof(CachedData, enumNonEnumerable); }
    };
};