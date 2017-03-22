//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class ScriptFunctionBase : public JavascriptFunction
    {
    protected:
        ScriptFunctionBase(DynamicType * type);
        ScriptFunctionBase(DynamicType * type, FunctionInfo * functionInfo);

        DEFINE_VTABLE_CTOR(ScriptFunctionBase, JavascriptFunction);

    public:
        static bool Is(Var func);
        static ScriptFunctionBase * FromVar(Var func);

        virtual Var  GetHomeObj() const = 0;
        virtual void SetHomeObj(Var homeObj) = 0;
        virtual void SetComputedNameVar(Var computedNameVar) = 0;
        virtual Var GetComputedNameVar() const = 0;
        virtual bool IsAnonymousFunction() const = 0;
    };

    class ScriptFunction : public ScriptFunctionBase
    {
    private:
        Field(FrameDisplay*) environment;  // Optional environment, for closures
        Field(ActivationObjectEx *) cachedScopeObj;
        Field(Var) homeObj;
        Field(Var) computedNameVar;
        Field(bool) hasInlineCaches;
        Field(bool) hasSuperReference;
        Field(bool) isActiveScript;

        Var FormatToString(JavascriptString* inputString);
    protected:
        ScriptFunction(DynamicType * type);

        DEFINE_VTABLE_CTOR(ScriptFunction, ScriptFunctionBase);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(ScriptFunction);
    public:
        ScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType);
        static bool Is(Var func);
        static ScriptFunction * FromVar(Var func);
        static ScriptFunction * OP_NewScFunc(FrameDisplay *environment, FunctionInfoPtrPtr infoRef);

        ProxyEntryPointInfo* GetEntryPointInfo() const;
        FunctionEntryPointInfo* GetFunctionEntryPointInfo() const
        {LOGMEIN("ScriptFunction.h] 52\n");
            Assert(this->GetFunctionProxy()->IsDeferred() == FALSE);
            return (FunctionEntryPointInfo*) this->GetEntryPointInfo();
        }

        FunctionProxy * GetFunctionProxy() const;
        ScriptFunctionType * GetScriptFunctionType() const;

        uint32 GetFrameHeight(FunctionEntryPointInfo* entryPointInfo) const;
        FrameDisplay* GetEnvironment() const {LOGMEIN("ScriptFunction.h] 61\n"); return environment; }
        void SetEnvironment(FrameDisplay * environment);
        ActivationObjectEx *GetCachedScope() const {LOGMEIN("ScriptFunction.h] 63\n"); return cachedScopeObj; }
        void SetCachedScope(ActivationObjectEx *obj) {LOGMEIN("ScriptFunction.h] 64\n"); cachedScopeObj = obj; }
        void InvalidateCachedScopeChain();

        static uint32 GetOffsetOfEnvironment() {LOGMEIN("ScriptFunction.h] 67\n"); return offsetof(ScriptFunction, environment); }
        static uint32 GetOffsetOfCachedScopeObj() {LOGMEIN("ScriptFunction.h] 68\n"); return offsetof(ScriptFunction, cachedScopeObj); };
        static uint32 GetOffsetOfHasInlineCaches() {LOGMEIN("ScriptFunction.h] 69\n"); return offsetof(ScriptFunction, hasInlineCaches); };
        static uint32 GetOffsetOfHomeObj() {LOGMEIN("ScriptFunction.h] 70\n"); return  offsetof(ScriptFunction, homeObj); }

        void ChangeEntryPoint(ProxyEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint);
        JavascriptMethod UpdateThunkEntryPoint(FunctionEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint);
        bool IsNewEntryPointAvailable();
        JavascriptMethod UpdateUndeferredBody(FunctionBody* newFunctionInfo);

        virtual ScriptFunctionType * DuplicateType() override;

        virtual Var GetSourceString() const;
        virtual Var EnsureSourceString();

        bool GetHasInlineCaches() {LOGMEIN("ScriptFunction.h] 82\n"); return hasInlineCaches; }
        void SetHasInlineCaches(bool has) {LOGMEIN("ScriptFunction.h] 83\n"); hasInlineCaches = has; }

        bool HasSuperReference() {LOGMEIN("ScriptFunction.h] 85\n"); return hasSuperReference; }
        void SetHasSuperReference(bool has) {LOGMEIN("ScriptFunction.h] 86\n"); hasSuperReference = has; }

        void SetIsActiveScript(bool is) {LOGMEIN("ScriptFunction.h] 88\n"); isActiveScript = is; }

        virtual Var GetHomeObj() const override { return homeObj; }
        virtual void SetHomeObj(Var homeObj) override { this->homeObj = homeObj; }
        virtual void SetComputedNameVar(Var computedNameVar) override { this->computedNameVar = computedNameVar; }
        bool GetSymbolName(const char16** symbolName, charcount_t *length) const;
        virtual Var GetComputedNameVar() const override { return this->computedNameVar; }
        virtual JavascriptString* GetDisplayNameImpl() const;
        JavascriptString* GetComputedName() const;
        virtual bool IsAnonymousFunction() const override;

        virtual JavascriptFunction* GetRealFunctionObject() {LOGMEIN("ScriptFunction.h] 99\n"); return this; }

        bool HasFunctionBody();
#if ENABLE_TTD
    public:
        virtual void MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor) override;

        virtual void ProcessCorePaths() override;

        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };

    class AsmJsScriptFunction : public ScriptFunction
    {
    public:
        AsmJsScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType);

        static bool Is(Var func);
        static bool IsWasmScriptFunction(Var func);
        static AsmJsScriptFunction* FromVar(Var func);

        void SetModuleMemory(Field(Var)* mem) {LOGMEIN("ScriptFunction.h] 122\n"); m_moduleMemory = mem; }
        Field(Var)* GetModuleMemory() const {LOGMEIN("ScriptFunction.h] 123\n"); return m_moduleMemory; }

#ifdef ENABLE_WASM
        void SetSignature(Wasm::WasmSignature * sig) {LOGMEIN("ScriptFunction.h] 126\n"); m_signature = sig; }
        Wasm::WasmSignature * GetSignature() const {LOGMEIN("ScriptFunction.h] 127\n"); return m_signature; }
        static uint32 GetOffsetOfSignature() {LOGMEIN("ScriptFunction.h] 128\n"); return offsetof(AsmJsScriptFunction, m_signature); }
#endif
        static uint32 GetOffsetOfModuleMemory() {LOGMEIN("ScriptFunction.h] 130\n"); return offsetof(AsmJsScriptFunction, m_moduleMemory); }
    protected:
        AsmJsScriptFunction(DynamicType * type);
        DEFINE_VTABLE_CTOR(AsmJsScriptFunction, ScriptFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(AsmJsScriptFunction);

    private:
        Field(Field(Var)*) m_moduleMemory;
        Field(Wasm::WasmSignature *) m_signature;
    };

    class ScriptFunctionWithInlineCache : public ScriptFunction
    {
    private:
        Field(Field(void*)*) m_inlineCaches;
        Field(bool) hasOwnInlineCaches;

#if DBG
#define InlineCacheTypeNone         0x00
#define InlineCacheTypeInlineCache  0x01
#define InlineCacheTypeIsInst       0x02
        Field(byte *) m_inlineCacheTypes;
#endif
        Field(uint) inlineCacheCount;
        Field(uint) rootObjectLoadInlineCacheStart;
        Field(uint) rootObjectLoadMethodInlineCacheStart;
        Field(uint) rootObjectStoreInlineCacheStart;
        Field(uint) isInstInlineCacheCount;

    protected:
        ScriptFunctionWithInlineCache(DynamicType * type);

        DEFINE_VTABLE_CTOR(ScriptFunctionWithInlineCache, ScriptFunction);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(ScriptFunctionWithInlineCache);

    public:
        ScriptFunctionWithInlineCache(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType);
        static bool Is(Var func);
        static ScriptFunctionWithInlineCache * FromVar(Var func);
        void CreateInlineCache();
        void AllocateInlineCache();
        void ClearInlineCacheOnFunctionObject();
        void ClearBorrowedInlineCacheOnFunctionObject();
        InlineCache * GetInlineCache(uint index);
        uint GetInlineCacheCount() {LOGMEIN("ScriptFunction.h] 174\n"); return inlineCacheCount; }
        Field(void*)* GetInlineCaches() {LOGMEIN("ScriptFunction.h] 175\n"); return m_inlineCaches; }
        bool GetHasOwnInlineCaches() {LOGMEIN("ScriptFunction.h] 176\n"); return hasOwnInlineCaches; }
        void SetInlineCachesFromFunctionBody();
        static uint32 GetOffsetOfInlineCaches() {LOGMEIN("ScriptFunction.h] 178\n"); return offsetof(ScriptFunctionWithInlineCache, m_inlineCaches); };
        template<bool isShutdown>
        void FreeOwnInlineCaches();
        virtual void Finalize(bool isShutdown) override;
    };
} // namespace Js
