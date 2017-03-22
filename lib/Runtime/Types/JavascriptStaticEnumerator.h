//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    enum class EnumeratorFlags : byte
    {
        None                = 0x0,
        EnumNonEnumerable   = 0x1,
        EnumSymbols         = 0x2,
        SnapShotSemantics   = 0x4,
        UseCache            = 0x8
    };
    ENUM_CLASS_HELPERS(EnumeratorFlags, byte);

    class JavascriptStaticEnumerator 
    {
    protected:
        friend class ForInObjectEnumerator;
        Field(DynamicObjectPropertyEnumerator) propertyEnumerator;
        Field(JavascriptEnumerator*) currentEnumerator;
        Field(JavascriptEnumerator*) prefixEnumerator;
        Field(JavascriptEnumerator*) arrayEnumerator;

        Var MoveAndGetNextFromEnumerator(PropertyId& propertyId, PropertyAttributes* attributes);
    public:
        JavascriptStaticEnumerator() {LOGMEIN("JavascriptStaticEnumerator.h] 29\n"); Clear(EnumeratorFlags::None, nullptr); }
        bool Initialize(JavascriptEnumerator * prefixEnumerator, ArrayObject * arrayToEnumerate, DynamicObject* objectToEnumerate, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache);
        bool IsNullEnumerator() const;
        bool CanUseJITFastPath() const;
        ScriptContext * GetScriptContext() const {LOGMEIN("JavascriptStaticEnumerator.h] 33\n"); return propertyEnumerator.GetScriptContext(); }
        EnumeratorFlags GetFlags() const {LOGMEIN("JavascriptStaticEnumerator.h] 34\n"); return propertyEnumerator.GetFlags(); }

        void Clear(EnumeratorFlags flags, ScriptContext * requestContext);
        void Reset();
        uint32 GetCurrentItemIndex();
        Var MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr);

        static uint32 GetOffsetOfCurrentEnumerator() {LOGMEIN("JavascriptStaticEnumerator.h] 41\n"); return offsetof(JavascriptStaticEnumerator, currentEnumerator); }
        static uint32 GetOffsetOfPrefixEnumerator() {LOGMEIN("JavascriptStaticEnumerator.h] 42\n"); return offsetof(JavascriptStaticEnumerator, prefixEnumerator); }
        static uint32 GetOffsetOfArrayEnumerator() {LOGMEIN("JavascriptStaticEnumerator.h] 43\n"); return offsetof(JavascriptStaticEnumerator, arrayEnumerator); }
        static uint32 GetOffsetOfScriptContext() {LOGMEIN("JavascriptStaticEnumerator.h] 44\n"); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfScriptContext(); }
        static uint32 GetOffsetOfInitialType() {LOGMEIN("JavascriptStaticEnumerator.h] 45\n"); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfInitialType(); }
        static uint32 GetOffsetOfObject() {LOGMEIN("JavascriptStaticEnumerator.h] 46\n"); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfObject(); }
        static uint32 GetOffsetOfObjectIndex() {LOGMEIN("JavascriptStaticEnumerator.h] 47\n"); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfObjectIndex(); }
        static uint32 GetOffsetOfInitialPropertyCount() {LOGMEIN("JavascriptStaticEnumerator.h] 48\n"); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfInitialPropertyCount(); }
        static uint32 GetOffsetOfEnumeratedCount() {LOGMEIN("JavascriptStaticEnumerator.h] 49\n"); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfEnumeratedCount(); }
        static uint32 GetOffsetOfCachedData() {LOGMEIN("JavascriptStaticEnumerator.h] 50\n"); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfCachedData(); }
        static uint32 GetOffsetOfFlags() {LOGMEIN("JavascriptStaticEnumerator.h] 51\n"); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfFlags(); }


    };

    

} // namespace Js
