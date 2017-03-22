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
        {LOGMEIN("ActivationObject.h] 21\n");}

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
        BlockActivationObject(DynamicType * type) : ActivationObject(type) {LOGMEIN("ActivationObject.h] 52\n");}

        virtual BOOL EnsureProperty(PropertyId propertyId) override;
        virtual BOOL EnsureNoRedeclProperty(PropertyId propertyId) override;
        virtual BOOL InitPropertyScoped(PropertyId propertyId, Var value) override;
        virtual BOOL InitFuncScoped(PropertyId propertyId, Var value) override;
        static bool Is(void* instance)
        {LOGMEIN("ActivationObject.h] 59\n");
            return VirtualTableInfo<Js::BlockActivationObject>::HasVirtualTable(instance);
        }
        static BlockActivationObject* FromVar(Var value)
        {LOGMEIN("ActivationObject.h] 63\n");
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
        PseudoActivationObject(DynamicType * type) : ActivationObject(type) {LOGMEIN("ActivationObject.h] 84\n");}

        virtual BOOL EnsureProperty(PropertyId propertyId) override;
        virtual BOOL EnsureNoRedeclProperty(PropertyId propertyId) override;
        virtual BOOL InitFuncScoped(PropertyId propertyId, Var value) override;
        virtual BOOL InitPropertyScoped(PropertyId propertyId, Var value) override;
        static bool Is(void* instance)
        {LOGMEIN("ActivationObject.h] 91\n");
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
        ConsoleScopeActivationObject(DynamicType * type) : ActivationObject(type) {LOGMEIN("ActivationObject.h] 108\n");}

        // A dummy function to have a different vtable
        virtual void DummyVirtualFunc(void)
        {
            AssertMsg(false, "ConsoleScopeActivationObject::DummyVirtualFunc function should never be called");
        }

        static bool Is(void* instance)
        {LOGMEIN("ActivationObject.h] 117\n");
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
        {LOGMEIN("ActivationObject.h] 144\n");
            if (cachedFuncCount != 0)
            {LOGMEIN("ActivationObject.h] 146\n");
                cache[0].func = nullptr;
            }
        }

        virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext) override;
        virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var *value, PropertyValueInfo *info, ScriptContext *requestContext) override;
        virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext) override;
        virtual void InvalidateCachedScope() override sealed;

        bool IsCommitted() const {LOGMEIN("ActivationObject.h] 156\n"); return committed; }
        void SetCommit(bool set) {LOGMEIN("ActivationObject.h] 157\n"); committed = set; }
        ScriptFunction *GetParentFunc() const {LOGMEIN("ActivationObject.h] 158\n"); return parentFunc; }
        uint GetFirstFuncSlot() const {LOGMEIN("ActivationObject.h] 159\n"); return firstFuncSlot; }
        uint GetLastFuncSlot() const {LOGMEIN("ActivationObject.h] 160\n"); return lastFuncSlot; }
        bool HasCachedFuncs() const {LOGMEIN("ActivationObject.h] 161\n"); return cachedFuncCount != 0 && cache[0].func != nullptr; }

        void SetCachedFunc(uint i, ScriptFunction *func);

        FuncCacheEntry *GetFuncCacheEntry(uint i)
        {LOGMEIN("ActivationObject.h] 166\n");
            Assert(i < cachedFuncCount);
            return &cache[i];
        }

        static uint32 GetOffsetOfCache() {LOGMEIN("ActivationObject.h] 171\n"); return offsetof(ActivationObjectEx, cache); }
        static uint32 GetOffsetOfCommitFlag() {LOGMEIN("ActivationObject.h] 172\n"); return offsetof(ActivationObjectEx, committed); }
        static uint32 GetOffsetOfParentFunc() {LOGMEIN("ActivationObject.h] 173\n"); return offsetof(ActivationObjectEx, parentFunc); }

        static const PropertyId *GetCachedScopeInfo(const PropertyIdArray *propIds);

        // Cached scope info:
        // [0] - cached func count
        // [1] - first func slot
        // [2] - first var slot
        // [3] - literal object reference

        static PropertyId GetCachedFuncCount(const PropertyIdArray *propIds)
        {LOGMEIN("ActivationObject.h] 184\n");
            return ActivationObjectEx::GetCachedScopeInfo(propIds)[0];
        }

        static PropertyId GetFirstFuncSlot(const PropertyIdArray *propIds)
        {LOGMEIN("ActivationObject.h] 189\n");
            return ActivationObjectEx::GetCachedScopeInfo(propIds)[1];
        }

        static PropertyId GetFirstVarSlot(const PropertyIdArray *propIds)
        {LOGMEIN("ActivationObject.h] 194\n");
            return ActivationObjectEx::GetCachedScopeInfo(propIds)[2];
        }

        static PropertyId GetLiteralObjectRef(const PropertyIdArray *propIds)
        {LOGMEIN("ActivationObject.h] 199\n");
            return ActivationObjectEx::GetCachedScopeInfo(propIds)[3];
        }

        static byte ExtraSlotCount() {LOGMEIN("ActivationObject.h] 203\n"); return 4; }

        static bool Is(void* instance)
        {LOGMEIN("ActivationObject.h] 206\n");
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
