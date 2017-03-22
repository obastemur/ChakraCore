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
        ~ForInObjectEnumerator() {LOGMEIN("ForInObjectEnumerator.h] 32\n"); Clear(); }

        ScriptContext * GetScriptContext() const {LOGMEIN("ForInObjectEnumerator.h] 34\n"); return enumerator.GetScriptContext(); }
        void Initialize(RecyclableObject* currentObject, ScriptContext * requestContext, bool enumSymbols = false, ForInCache * forInCache = nullptr);
        void Clear();
        Var MoveAndGetNext(PropertyId& propertyId);

        static RecyclableObject* GetFirstPrototypeWithEnumerableProperties(RecyclableObject* object, RecyclableObject** pFirstPrototype = nullptr);


        static uint32 GetOffsetOfCanUseJitFastPath() {LOGMEIN("ForInObjectEnumerator.h] 42\n"); return offsetof(ForInObjectEnumerator, canUseJitFastPath); }
        static uint32 GetOffsetOfEnumeratorScriptContext() {LOGMEIN("ForInObjectEnumerator.h] 43\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfScriptContext(); }
        static uint32 GetOffsetOfEnumeratorObject() {LOGMEIN("ForInObjectEnumerator.h] 44\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfObject(); }
        static uint32 GetOffsetOfEnumeratorInitialType() {LOGMEIN("ForInObjectEnumerator.h] 45\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfInitialType(); }
        static uint32 GetOffsetOfEnumeratorInitialPropertyCount() {LOGMEIN("ForInObjectEnumerator.h] 46\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfInitialPropertyCount(); }
        static uint32 GetOffsetOfEnumeratorEnumeratedCount() {LOGMEIN("ForInObjectEnumerator.h] 47\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfEnumeratedCount(); }
        static uint32 GetOffsetOfEnumeratorCachedData() {LOGMEIN("ForInObjectEnumerator.h] 48\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfCachedData(); }
        static uint32 GetOffsetOfEnumeratorObjectIndex() {LOGMEIN("ForInObjectEnumerator.h] 49\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfObjectIndex(); }
        static uint32 GetOffsetOfEnumeratorFlags() {LOGMEIN("ForInObjectEnumerator.h] 50\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfFlags(); }

        static uint32 GetOffsetOfEnumeratorCurrentEnumerator() {LOGMEIN("ForInObjectEnumerator.h] 52\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfCurrentEnumerator(); }
        static uint32 GetOffsetOfEnumeratorPrefixEnumerator() {LOGMEIN("ForInObjectEnumerator.h] 53\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfPrefixEnumerator(); }
        static uint32 GetOffsetOfEnumeratorArrayEnumerator() {LOGMEIN("ForInObjectEnumerator.h] 54\n"); return offsetof(ForInObjectEnumerator, enumerator) + JavascriptStaticEnumerator::GetOffsetOfArrayEnumerator(); }

        static uint32 GetOffsetOfShadowData() {LOGMEIN("ForInObjectEnumerator.h] 56\n"); return offsetof(ForInObjectEnumerator, shadowData); }
        static uint32 GetOffsetOfStates()
        {
            CompileAssert(offsetof(ForInObjectEnumerator, enumeratingPrototype) == offsetof(ForInObjectEnumerator, canUseJitFastPath) + 1);
            return offsetof(ForInObjectEnumerator, canUseJitFastPath);
        }
    };
}
