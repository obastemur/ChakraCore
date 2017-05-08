//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
    class FunctionProxy;
    class FunctionBody;
    class ParseableFunctionInfo;
    class DeferDeserializeFunctionInfo;

    class FunctionInfo: public FinalizableObject
    {
        friend class RemoteFunctionBody;
    protected:
        DEFINE_VTABLE_CTOR_NOBASE(FunctionInfo);
    public:

        enum Attributes : uint32
        {
            None                           = 0x00000,
            ErrorOnNew                     = 0x00001,
            SkipDefaultNewObject           = 0x00002,
            DoNotProfile                   = 0x00004,
            HasNoSideEffect                = 0x00008, // calling function doesn't cause an implicit flags to be set,
                                                      // the callee will detect and set implicit flags on its individual operations
            NeedCrossSiteSecurityCheck     = 0x00010,
            DeferredDeserialize            = 0x00020, // The function represents something that needs to be deserialized on use
            DeferredParse                  = 0x00040, // The function represents something that needs to be parsed on use
            CanBeHoisted                   = 0x00080, // The function return value won't be changed in a loop so the evaluation can be hoisted.
            SuperReference                 = 0x00100,
            ClassMethod                    = 0x00200, // The function is a class method
            ClassConstructor               = 0x00400, // The function is a class constructor
            Lambda                         = 0x01000,
            CapturesThis                   = 0x02000, // Only lambdas will set this; denotes whether the lambda referred to this, used by debugger
            Generator                      = 0x04000,
            BuiltInInlinableAsLdFldInlinee = 0x08000,
            Async                          = 0x10000,
            Module                         = 0x20000, // The function is the function body wrapper for a module
            EnclosedByGlobalFunc           = 0x40000,
            CanDefer                       = 0x80000,
            AllowDirectSuper               = 0x100000,
            BaseConstructorKind            = 0x200000
        };
        FunctionInfo(JavascriptMethod entryPoint, Attributes attributes = None, LocalFunctionId functionId = Js::Constants::NoFunctionId, FunctionProxy* functionBodyImpl = nullptr);
        FunctionInfo(JavascriptMethod entryPoint, _no_write_barrier_tag, Attributes attributes = None, LocalFunctionId functionId = Js::Constants::NoFunctionId, FunctionProxy* functionBodyImpl = nullptr);
        FunctionInfo(FunctionInfo& that); // Todo: (leish)(swb) find a way to prevent non-static initializer calling this ctor

        static bool Is(void *ptr);
        static DWORD GetFunctionBodyImplOffset() {TRACE_IT(35733); return offsetof(FunctionInfo, functionBodyImpl); }
        static BYTE GetOffsetOfFunctionProxy()
        {
            CompileAssert(offsetof(FunctionInfo, functionBodyImpl) <= UCHAR_MAX);
            return offsetof(FunctionInfo, functionBodyImpl);
        }
        static DWORD GetAttributesOffset() {TRACE_IT(35734); return offsetof(FunctionInfo, attributes); }

        void VerifyOriginalEntryPoint() const;
        JavascriptMethod GetOriginalEntryPoint() const;
        JavascriptMethod GetOriginalEntryPoint_Unchecked() const;
        void SetOriginalEntryPoint(const JavascriptMethod originalEntryPoint);

        bool IsAsync() const {TRACE_IT(35735); return ((this->attributes & Async) != 0); }
        bool IsDeferred() const {TRACE_IT(35736); return ((this->attributes & (DeferredDeserialize | DeferredParse)) != 0); }
        static bool IsLambda(Attributes attributes) {TRACE_IT(35737); return ((attributes & Lambda) != 0); }
        bool IsLambda() const {TRACE_IT(35738); return IsLambda(this->attributes); }
        bool IsConstructor() const {TRACE_IT(35739); return ((this->attributes & ErrorOnNew) == 0); }

        static bool IsGenerator(Attributes attributes) {TRACE_IT(35740); return ((attributes & Generator) != 0); }
        bool IsGenerator() const {TRACE_IT(35741); return IsGenerator(this->attributes); }

        bool IsClassConstructor() const {TRACE_IT(35742); return ((this->attributes & ClassConstructor) != 0); }
        bool IsClassMethod() const {TRACE_IT(35743); return ((this->attributes & ClassMethod) != 0); }
        bool IsModule() const {TRACE_IT(35744); return ((this->attributes & Module) != 0); }
        bool HasSuperReference() const {TRACE_IT(35745); return ((this->attributes & SuperReference) != 0); }
        bool CanBeDeferred() const {TRACE_IT(35746); return ((this->attributes & CanDefer) != 0); }
        static bool IsCoroutine(Attributes attributes) {TRACE_IT(35747); return ((attributes & (Async | Generator)) != 0); }
        bool IsCoroutine() const {TRACE_IT(35748); return IsCoroutine(this->attributes); }


        BOOL HasBody() const {TRACE_IT(35749); return functionBodyImpl != NULL; }
        BOOL HasParseableInfo() const {TRACE_IT(35750); return this->HasBody() && !this->IsDeferredDeserializeFunction(); }

        FunctionProxy * GetFunctionProxy() const
        {TRACE_IT(35751);
            return functionBodyImpl;
        }
        void SetFunctionProxy(FunctionProxy * proxy)
        {TRACE_IT(35752);
            functionBodyImpl = proxy;
        }
        ParseableFunctionInfo* GetParseableFunctionInfo() const
        {TRACE_IT(35753);
            Assert(functionBodyImpl == nullptr || !IsDeferredDeserializeFunction());
            return (ParseableFunctionInfo*)GetFunctionProxy();
        }
        void SetParseableFunctionInfo(ParseableFunctionInfo* func)
        {TRACE_IT(35754);
            Assert(functionBodyImpl == nullptr || !IsDeferredDeserializeFunction());
            SetFunctionProxy((FunctionProxy*)func);
        }
        DeferDeserializeFunctionInfo* GetDeferDeserializeFunctionInfo() const
        {TRACE_IT(35755);
            Assert(functionBodyImpl == nullptr || IsDeferredDeserializeFunction());
            return (DeferDeserializeFunctionInfo*)PointerValue(functionBodyImpl);
        }
        FunctionBody * GetFunctionBody() const;

        Attributes GetAttributes() const {TRACE_IT(35756); return attributes; }
        static Attributes GetAttributes(Js::RecyclableObject * function);
        void SetAttributes(Attributes attr) {TRACE_IT(35757); attributes = attr; }

        LocalFunctionId GetLocalFunctionId() const {TRACE_IT(35758); return functionId; }
        void SetLocalFunctionId(LocalFunctionId functionId) {TRACE_IT(35759); this->functionId = functionId; }

        uint GetCompileCount() const {TRACE_IT(35760); return compileCount; }
        void SetCompileCount(uint count) {TRACE_IT(35761); compileCount = count; }

        virtual void Finalize(bool isShutdown) override
        {
        }

        virtual void Dispose(bool isShutdown) override
        {
        }

        virtual void Mark(Recycler *recycler) override { AssertMsg(false, "Mark called on object that isn't TrackableObject"); }

        BOOL IsDeferredDeserializeFunction() const {TRACE_IT(35762); return ((this->attributes & DeferredDeserialize) == DeferredDeserialize); }
        BOOL IsDeferredParseFunction() const {TRACE_IT(35763); return ((this->attributes & DeferredParse) == DeferredParse); }
        void SetCapturesThis() {TRACE_IT(35764); attributes = (Attributes)(attributes | Attributes::CapturesThis); }
        bool GetCapturesThis() const {TRACE_IT(35765); return (attributes & Attributes::CapturesThis) != 0; }
        void SetEnclosedByGlobalFunc() {TRACE_IT(35766); attributes = (Attributes)(attributes | Attributes::EnclosedByGlobalFunc ); }
        bool GetEnclosedByGlobalFunc() const {TRACE_IT(35767); return (attributes & Attributes::EnclosedByGlobalFunc) != 0; }
        void SetAllowDirectSuper() {TRACE_IT(35768); attributes = (Attributes)(attributes | Attributes::AllowDirectSuper); }
        bool GetAllowDirectSuper() const {TRACE_IT(35769); return (attributes & Attributes::AllowDirectSuper) != 0; }
        void SetBaseConstructorKind() {TRACE_IT(35770); attributes = (Attributes)(attributes | Attributes::BaseConstructorKind); }
        bool GetBaseConstructorKind() const {TRACE_IT(35771); return (attributes & Attributes::BaseConstructorKind) != 0; }

    protected:
        FieldNoBarrier(JavascriptMethod) originalEntryPoint;
        FieldWithBarrier(FunctionProxy *) functionBodyImpl;     // Implementation of the function- null if the function doesn't have a body
        Field(LocalFunctionId) functionId;        // Per host source context (source file) function Id
        Field(uint) compileCount;
        Field(Attributes) attributes;
    };

    // Helper FunctionInfo for builtins that we don't want to profile (script profiler).
    class NoProfileFunctionInfo : public FunctionInfo
    {
    public:
        NoProfileFunctionInfo(JavascriptMethod entryPoint)
            : FunctionInfo(entryPoint, Attributes::DoNotProfile)
        {TRACE_IT(35772);}

        NoProfileFunctionInfo(JavascriptMethod entryPoint, _no_write_barrier_tag)
            : FunctionInfo(FORCE_NO_WRITE_BARRIER_TAG(entryPoint), Attributes::DoNotProfile)
        {TRACE_IT(35773);}
    };
};
