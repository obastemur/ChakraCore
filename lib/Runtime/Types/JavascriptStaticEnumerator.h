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
        JavascriptStaticEnumerator() {TRACE_IT(66359); Clear(EnumeratorFlags::None, nullptr); }
        bool Initialize(JavascriptEnumerator * prefixEnumerator, ArrayObject * arrayToEnumerate, DynamicObject* objectToEnumerate, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache);
        bool IsNullEnumerator() const;
        bool CanUseJITFastPath() const;
        ScriptContext * GetScriptContext() const {TRACE_IT(66360); return propertyEnumerator.GetScriptContext(); }
        EnumeratorFlags GetFlags() const {TRACE_IT(66361); return propertyEnumerator.GetFlags(); }

        void Clear(EnumeratorFlags flags, ScriptContext * requestContext);
        void Reset();
        uint32 GetCurrentItemIndex();
        Var MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr);

        static uint32 GetOffsetOfCurrentEnumerator() {TRACE_IT(66362); return offsetof(JavascriptStaticEnumerator, currentEnumerator); }
        static uint32 GetOffsetOfPrefixEnumerator() {TRACE_IT(66363); return offsetof(JavascriptStaticEnumerator, prefixEnumerator); }
        static uint32 GetOffsetOfArrayEnumerator() {TRACE_IT(66364); return offsetof(JavascriptStaticEnumerator, arrayEnumerator); }
        static uint32 GetOffsetOfScriptContext() {TRACE_IT(66365); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfScriptContext(); }
        static uint32 GetOffsetOfInitialType() {TRACE_IT(66366); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfInitialType(); }
        static uint32 GetOffsetOfObject() {TRACE_IT(66367); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfObject(); }
        static uint32 GetOffsetOfObjectIndex() {TRACE_IT(66368); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfObjectIndex(); }
        static uint32 GetOffsetOfInitialPropertyCount() {TRACE_IT(66369); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfInitialPropertyCount(); }
        static uint32 GetOffsetOfEnumeratedCount() {TRACE_IT(66370); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfEnumeratedCount(); }
        static uint32 GetOffsetOfCachedData() {TRACE_IT(66371); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfCachedData(); }
        static uint32 GetOffsetOfFlags() {TRACE_IT(66372); return offsetof(JavascriptStaticEnumerator, propertyEnumerator) + DynamicObjectPropertyEnumerator::GetOffsetOfFlags(); }


    };

    

} // namespace Js
