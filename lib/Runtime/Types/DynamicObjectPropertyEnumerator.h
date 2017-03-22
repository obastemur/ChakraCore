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
        DynamicObject * GetObject() const {LOGMEIN("DynamicObjectPropertyEnumerator.h] 40\n"); return object; }
        EnumeratorFlags GetFlags() const {LOGMEIN("DynamicObjectPropertyEnumerator.h] 41\n"); return flags; }
        bool GetEnumNonEnumerable() const;
        bool GetEnumSymbols() const;
        bool GetSnapShotSemantics() const;
        bool GetUseCache() const;
        ScriptContext * GetScriptContext() const {LOGMEIN("DynamicObjectPropertyEnumerator.h] 46\n"); return scriptContext; }

        bool Initialize(DynamicObject * object, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache);
        bool IsNullEnumerator() const;
        void Reset();
        void Clear(EnumeratorFlags flags, ScriptContext * requestContext);
        Var MoveAndGetNext(PropertyId& propertyId, PropertyAttributes * attributes);

        bool CanUseJITFastPath() const;
        static uint32 GetOffsetOfScriptContext() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 55\n"); return offsetof(DynamicObjectPropertyEnumerator, scriptContext); }
        static uint32 GetOffsetOfInitialType() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 56\n"); return offsetof(DynamicObjectPropertyEnumerator, initialType); }
        static uint32 GetOffsetOfObject() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 57\n"); return offsetof(DynamicObjectPropertyEnumerator, object); }
        static uint32 GetOffsetOfObjectIndex() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 58\n"); return offsetof(DynamicObjectPropertyEnumerator, objectIndex); }
        static uint32 GetOffsetOfInitialPropertyCount() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 59\n"); return offsetof(DynamicObjectPropertyEnumerator, initialPropertyCount); }
        static uint32 GetOffsetOfEnumeratedCount() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 60\n"); return offsetof(DynamicObjectPropertyEnumerator, enumeratedCount); }
        static uint32 GetOffsetOfCachedData() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 61\n"); return offsetof(DynamicObjectPropertyEnumerator, cachedData); }
        static uint32 GetOffsetOfFlags() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 62\n"); return offsetof(DynamicObjectPropertyEnumerator, flags);
        }
        static uint32 GetOffsetOfCachedDataStrings() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 64\n"); return offsetof(CachedData, strings); }
        static uint32 GetOffsetOfCachedDataIndexes() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 65\n"); return offsetof(CachedData, indexes); }
        static uint32 GetOffsetOfCachedDataPropertyCount() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 66\n"); return offsetof(CachedData, propertyCount); }
        static uint32 GetOffsetOfCachedDataCachedCount() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 67\n"); return offsetof(CachedData, cachedCount); }
        static uint32 GetOffsetOfCachedDataPropertyAttributes() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 68\n"); return offsetof(CachedData, attributes); }
        static uint32 GetOffsetOfCachedDataCompleted() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 69\n"); return offsetof(CachedData, completed); }
        static uint32 GetOffsetOfCachedDataEnumNonEnumerable() {LOGMEIN("DynamicObjectPropertyEnumerator.h] 70\n"); return offsetof(CachedData, enumNonEnumerable); }
    };
};