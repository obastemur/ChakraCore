//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class ForInObjectEnumerator
    {
    private:
        JavascriptStaticEnumerator enumerator;
        struct ShadowData
        {
            ShadowData(RecyclableObject * initObject, RecyclableObject * firstPrototype, Recycler * recycler);
            Field(RecyclableObject *) currentObject;
            Field(RecyclableObject *) firstPrototype;
            Field(BVSparse<Recycler>) propertyIds;
            typedef SListBase<Js::PropertyRecord const *, Recycler> _PropertyStringsListType;
            Field(_PropertyStringsListType) newPropertyStrings;
        } *shadowData;

        // States
        bool canUseJitFastPath;
        bool enumeratingPrototype;

        BOOL TestAndSetEnumerated(PropertyId propertyId);
        BOOL InitializeCurrentEnumerator(RecyclableObject * object, ForInCache * forInCache = nullptr);
        BOOL InitializeCurrentEnumerator(RecyclableObject * object, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache);

    public:
        ForInObjectEnumerator(RecyclableObject* currentObject, ScriptContext * requestContext, bool enumSymbols = false);
        ~ForInObjectEnumerator() {TRACE_IT(55296); Clear(); }

        ScriptContext * GetScriptContext() const {TRACE_IT(55297); return enumerator.GetScriptContext(); }
        void Initialize(RecyclableObject* currentObject, ScriptContext * requestContext, bool enumSymbols = false, ForInCache * forInCache = nullptr);
        void Clear();
        Var MoveAndGetNext(PropertyId& propertyId);

        static RecyclableObject* GetFirstPrototypeWithEnumerableProperties(RecyclableObject* object, RecyclableObject** pFirstPrototype = nullptr);


        static uint32 GetOffsetOfCanUseJitFastPath() {TRACE_IT(55298); return offsetof(ForInObjectEnumerator, canUseJitFastPath); }
        static uint32 GetOffsetOfEnumeratorScriptContext() {TRACE_IT(55299); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfScriptContext(); }
        static uint32 GetOffsetOfEnumeratorObject() {TRACE_IT(55300); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfObject(); }
        static uint32 GetOffsetOfEnumeratorInitialType() {TRACE_IT(55301); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfInitialType(); }
        static uint32 GetOffsetOfEnumeratorInitialPropertyCount() {TRACE_IT(55302); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfInitialPropertyCount(); }
        static uint32 GetOffsetOfEnumeratorEnumeratedCount() {TRACE_IT(55303); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfEnumeratedCount(); }
        static uint32 GetOffsetOfEnumeratorCachedData() {TRACE_IT(55304); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfCachedData(); }
        static uint32 GetOffsetOfEnumeratorObjectIndex() {TRACE_IT(55305); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfObjectIndex(); }
        static uint32 GetOffsetOfEnumeratorFlags() {TRACE_IT(55306); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfFlags(); }

        static uint32 GetOffsetOfEnumeratorCurrentEnumerator() {TRACE_IT(55307); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfCurrentEnumerator(); }
        static uint32 GetOffsetOfEnumeratorPrefixEnumerator() {TRACE_IT(55308); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfPrefixEnumerator(); }
        static uint32 GetOffsetOfEnumeratorArrayEnumerator() {TRACE_IT(55309); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfArrayEnumerator(); }

        static uint32 GetOffsetOfShadowData() {TRACE_IT(55310); return offsetof(ForInObjectEnumerator, shadowData); }
        static uint32 GetOffsetOfStates()
        {
            CompileAssert(offsetof(ForInObjectEnumerator, enumeratingPrototype) == offsetof(ForInObjectEnumerator, canUseJitFastPath) + 1);
            return offsetof(ForInObjectEnumerator, canUseJitFastPath);
        }
    };
}
