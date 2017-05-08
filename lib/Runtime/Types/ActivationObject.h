//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct FuncCacheEntry
    {
        Field(ScriptFunction *) func;
        Field(DynamicType *) type;
    };

    class ActivationObject : public DynamicObject
    {
    protected:
        DEFINE_VTABLE_CTOR(ActivationObject, DynamicObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(ActivationObject);
    public:
        ActivationObject(DynamicType * type) : DynamicObject(type)
        {TRACE_IT(65279);}

        virtual BOOL HasOwnPropertyCheckNoRedecl(PropertyId propertyId) override;
        virtual BOOL SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetInternalProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL EnsureProperty(PropertyId propertyId) override;
        virtual BOOL EnsureNoRedeclProperty(PropertyId propertyId) override;
        virtual BOOL InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags = PropertyOperation_None, PropertyValueInfo* info = NULL) override;
        virtual BOOL InitPropertyScoped(PropertyId propertyId, Var value) override;
        virtual BOOL InitFuncScoped(PropertyId propertyId, Var value) override;
        virtual BOOL DeleteItem(uint32 index, PropertyOperationFlags flags) override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        static bool Is(void* instance);

#if ENABLE_TTD
    public:
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };

    // A block-ActivationObject is a scope for an ES6 block that should only receive block-scoped inits,
    // including function, let, and const.
    class BlockActivationObject : public ActivationObject
    {
    private:
        DEFINE_VTABLE_CTOR(BlockActivationObject, ActivationObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(BlockActivationObject);
    public:
        BlockActivationObject(DynamicType * type) : ActivationObject(type) {TRACE_IT(65280);}

        virtual BOOL EnsureProperty(PropertyId propertyId) override;
        virtual BOOL EnsureNoRedeclProperty(PropertyId propertyId) override;
        virtual BOOL InitPropertyScoped(PropertyId propertyId, Var value) override;
        virtual BOOL InitFuncScoped(PropertyId propertyId, Var value) override;
        static bool Is(void* instance)
        {TRACE_IT(65281);
            return VirtualTableInfo<Js::BlockActivationObject>::HasVirtualTable(instance);
        }
        static BlockActivationObject* FromVar(Var value)
        {TRACE_IT(65282);
            Assert(BlockActivationObject::Is(value));
            return static_cast<BlockActivationObject*>(DynamicObject::FromVar(value));
        }

        BlockActivationObject* Clone(ScriptContext *scriptContext);

#if ENABLE_TTD
    public:
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };

    // A pseudo-ActivationObject is a scope like a "catch" scope that shouldn't receive var inits.
    class PseudoActivationObject : public ActivationObject
    {
    private:
        DEFINE_VTABLE_CTOR(PseudoActivationObject, ActivationObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(PseudoActivationObject);
    public:
        PseudoActivationObject(DynamicType * type) : ActivationObject(type) {TRACE_IT(65283);}

        virtual BOOL EnsureProperty(PropertyId propertyId) override;
        virtual BOOL EnsureNoRedeclProperty(PropertyId propertyId) override;
        virtual BOOL InitFuncScoped(PropertyId propertyId, Var value) override;
        virtual BOOL InitPropertyScoped(PropertyId propertyId, Var value) override;
        static bool Is(void* instance)
        {TRACE_IT(65284);
            return VirtualTableInfo<Js::PseudoActivationObject>::HasVirtualTable(instance);
        }

#if ENABLE_TTD
    public:
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };

    class ConsoleScopeActivationObject : public ActivationObject
    {
    private:
        DEFINE_VTABLE_CTOR(ConsoleScopeActivationObject, ActivationObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(ConsoleScopeActivationObject);
    public:
        ConsoleScopeActivationObject(DynamicType * type) : ActivationObject(type) {TRACE_IT(65285);}

        // A dummy function to have a different vtable
        virtual void DummyVirtualFunc(void)
        {
            AssertMsg(false, "ConsoleScopeActivationObject::DummyVirtualFunc function should never be called");
        }

        static bool Is(void* instance)
        {TRACE_IT(65286);
            return VirtualTableInfo<Js::ConsoleScopeActivationObject>::HasVirtualTable(instance);
        }

#if ENABLE_TTD
    public:
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };

    class ActivationObjectEx : public ActivationObject
    {
    private:
        DEFINE_VTABLE_CTOR(ActivationObjectEx, ActivationObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(ActivationObjectEx);

        void GetPropertyCore(PropertyValueInfo *info, ScriptContext *requestContext);
    public:
        ActivationObjectEx(
            DynamicType * type, ScriptFunction *func, uint cachedFuncCount, uint firstFuncSlot, uint lastFuncSlot)
            : ActivationObject(type),
              parentFunc(func),
              cachedFuncCount(cachedFuncCount),
              firstFuncSlot(firstFuncSlot),
              lastFuncSlot(lastFuncSlot),
              committed(false)
        {TRACE_IT(65287);
            if (cachedFuncCount != 0)
            {TRACE_IT(65288);
                cache[0].func = nullptr;
            }
        }

        virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext) override;
        virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var *value, PropertyValueInfo *info, ScriptContext *requestContext) override;
        virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext) override;
        virtual void InvalidateCachedScope() override sealed;

        bool IsCommitted() const {TRACE_IT(65289); return committed; }
        void SetCommit(bool set) {TRACE_IT(65290); committed = set; }
        ScriptFunction *GetParentFunc() const {TRACE_IT(65291); return parentFunc; }
        uint GetFirstFuncSlot() const {TRACE_IT(65292); return firstFuncSlot; }
        uint GetLastFuncSlot() const {TRACE_IT(65293); return lastFuncSlot; }
        bool HasCachedFuncs() const {TRACE_IT(65294); return cachedFuncCount != 0 && cache[0].func != nullptr; }

        void SetCachedFunc(uint i, ScriptFunction *func);

        FuncCacheEntry *GetFuncCacheEntry(uint i)
        {TRACE_IT(65295);
            Assert(i < cachedFuncCount);
            return &cache[i];
        }

        static uint32 GetOffsetOfCache() {TRACE_IT(65296); return offsetof(ActivationObjectEx, cache); }
        static uint32 GetOffsetOfCommitFlag() {TRACE_IT(65297); return offsetof(ActivationObjectEx, committed); }
        static uint32 GetOffsetOfParentFunc() {TRACE_IT(65298); return offsetof(ActivationObjectEx, parentFunc); }

        static const PropertyId *GetCachedScopeInfo(const PropertyIdArray *propIds);

        // Cached scope info:
        // [0] - cached func count
        // [1] - first func slot
        // [2] - first var slot
        // [3] - literal object reference

        static PropertyId GetCachedFuncCount(const PropertyIdArray *propIds)
        {TRACE_IT(65299);
            return ActivationObjectEx::GetCachedScopeInfo(propIds)[0];
        }

        static PropertyId GetFirstFuncSlot(const PropertyIdArray *propIds)
        {TRACE_IT(65300);
            return ActivationObjectEx::GetCachedScopeInfo(propIds)[1];
        }

        static PropertyId GetFirstVarSlot(const PropertyIdArray *propIds)
        {TRACE_IT(65301);
            return ActivationObjectEx::GetCachedScopeInfo(propIds)[2];
        }

        static PropertyId GetLiteralObjectRef(const PropertyIdArray *propIds)
        {TRACE_IT(65302);
            return ActivationObjectEx::GetCachedScopeInfo(propIds)[3];
        }

        static byte ExtraSlotCount() {TRACE_IT(65303); return 4; }

        static bool Is(void* instance)
        {TRACE_IT(65304);
            return VirtualTableInfo<Js::ActivationObjectEx>::HasVirtualTable(instance);
        }

    private:
        Field(ScriptFunction *) parentFunc;
        Field(uint) cachedFuncCount;
        Field(uint) firstFuncSlot;
        Field(uint) lastFuncSlot;
        Field(bool) committed;
        Field(FuncCacheEntry) cache[1];

#if ENABLE_TTD
    public:
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };
};
