//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//  Implements ArrayBuffer according to Khronos spec.
//----------------------------------------------------------------------------

#pragma once
namespace Js
{
    class ArrayBufferParent;
    class ArrayBuffer;
    class SharedArrayBuffer;
    class ArrayBufferBase : public DynamicObject
    {
    protected:
#if ENABLE_FAST_ARRAYBUFFER
#define MAX_ASMJS_ARRAYBUFFER_LENGTH 0x100000000 // 4GB
#define MAX_WASM__ARRAYBUFFER_LENGTH 0x200000000 // 8GB
        typedef void*(*AllocWrapperType)(size_t);
#define AsmJsVirtualAllocator ((AllocWrapperType)Js::ArrayBuffer::AllocWrapper<MAX_ASMJS_ARRAYBUFFER_LENGTH>)
#define WasmVirtualAllocator ((AllocWrapperType)Js::ArrayBuffer::AllocWrapper<MAX_WASM__ARRAYBUFFER_LENGTH>)
        template<size_t MaxVirtualSize = MAX_ASMJS_ARRAYBUFFER_LENGTH>
        static void* __cdecl AllocWrapper(DECLSPEC_GUARD_OVERFLOW size_t length)
        {TRACE_IT(54380);
            LPVOID address = VirtualAlloc(nullptr, MaxVirtualSize, MEM_RESERVE, PAGE_NOACCESS);
            //throw out of memory
            if (!address)
            {TRACE_IT(54381);
                return nullptr;
            }

            if (length == 0)
            {TRACE_IT(54382);
                return address;
            }

            LPVOID arrayAddress = VirtualAlloc(address, length, MEM_COMMIT, PAGE_READWRITE);
            if (!arrayAddress)
            {
                VirtualFree(address, 0, MEM_RELEASE);
                return nullptr;
            }
            return arrayAddress;
        }

        static void FreeMemAlloc(Var ptr)
        {TRACE_IT(54383);
            BOOL fSuccess = VirtualFree((LPVOID)ptr, 0, MEM_RELEASE);
            Assert(fSuccess);
        }
#else
        static void* __cdecl AllocWrapper(DECLSPEC_GUARD_OVERFLOW size_t length)
        {TRACE_IT(54384);
            // This allocator should never be used
            Js::Throw::FatalInternalError();
        }
#define AsmJsVirtualAllocator Js::ArrayBuffer::AllocWrapper
#define WasmVirtualAllocator Js::ArrayBuffer::AllocWrapper
#endif
    public:
        DEFINE_VTABLE_CTOR_ABSTRACT(ArrayBufferBase, DynamicObject);

        virtual void MarshalToScriptContext(Js::ScriptContext * scriptContext) = 0;

#if ENABLE_TTD
        virtual void MarshalCrossSite_TTDInflate() = 0;
#endif

        ArrayBufferBase(DynamicType *type) : DynamicObject(type) {TRACE_IT(54385); }

        virtual bool IsArrayBuffer() = 0;
        virtual bool IsSharedArrayBuffer() = 0;
        virtual ArrayBuffer * GetAsArrayBuffer() = 0;
        virtual SharedArrayBuffer * GetAsSharedArrayBuffer() {TRACE_IT(54386); return nullptr; }
        virtual void AddParent(ArrayBufferParent* parent) {TRACE_IT(54387); }
        virtual bool IsDetached() {TRACE_IT(54388); return false; }
        virtual uint32 GetByteLength() const = 0;
        virtual BYTE* GetBuffer() const = 0;
        virtual bool IsValidVirtualBufferLength(uint length) {TRACE_IT(54389); return false; }

        static bool Is(Var value);
        static ArrayBufferBase* FromVar(Var value);
    };

    class ArrayBuffer : public ArrayBufferBase
    {
    public:
        // we need to install cross-site thunk on the nested array buffer when marshaling
        // typed array.
        DEFINE_VTABLE_CTOR_ABSTRACT(ArrayBuffer, ArrayBufferBase);
    private:
        void ClearParentsLength(ArrayBufferParent* parent);
    public:
        template <typename FreeFN>
        class ArrayBufferDetachedState : public ArrayBufferDetachedStateBase
        {
        public:
            FreeFN* freeFunction;

            ArrayBufferDetachedState(BYTE* buffer, uint32 bufferLength, FreeFN* freeFunction, ArrayBufferAllocationType allocationType)
                : ArrayBufferDetachedStateBase(TypeIds_ArrayBuffer, buffer, bufferLength, allocationType),
                freeFunction(freeFunction)
            {TRACE_IT(54390);}

            virtual void ClearSelfOnly() override
            {
                HeapDelete(this);
            }

            virtual void DiscardState() override
            {
                if (this->buffer != nullptr)
                {TRACE_IT(54391);
                    freeFunction(this->buffer);
                    this->buffer = nullptr;
                }
                this->bufferLength = 0;
            }

            virtual void Discard() override
            {
                ClearSelfOnly();
            }
        };

        template <typename Allocator>
        ArrayBuffer(DECLSPEC_GUARD_OVERFLOW uint32 length, DynamicType * type, Allocator allocator);

        ArrayBuffer(byte* buffer, DECLSPEC_GUARD_OVERFLOW uint32 length, DynamicType * type);

        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo Slice;
            static FunctionInfo IsView;
            static FunctionInfo GetterByteLength;
            static FunctionInfo GetterSymbolSpecies;
            static FunctionInfo Transfer;
        };

        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySlice(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIsView(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetterByteLength(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryTransfer(RecyclableObject* function, CallInfo callInfo, ...);

        static bool Is(Var aValue);
        static ArrayBuffer* NewFromDetachedState(DetachedStateBase* state, JavascriptLibrary *library);
        static ArrayBuffer* FromVar(Var aValue);

        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;

        virtual ArrayBufferDetachedStateBase* DetachAndGetState();
        virtual bool IsDetached() override { return this->isDetached; }
        virtual uint32 GetByteLength() const override { return bufferLength; }
        virtual BYTE* GetBuffer() const override { return buffer; }

        static int GetByteLengthOffset() {TRACE_IT(54392); return offsetof(ArrayBuffer, bufferLength); }
        static int GetIsDetachedOffset() {TRACE_IT(54393); return offsetof(ArrayBuffer, isDetached); }
        static int GetBufferOffset() {TRACE_IT(54394); return offsetof(ArrayBuffer, buffer); }

        virtual void AddParent(ArrayBufferParent* parent) override;
#if _WIN64
        //maximum 2G -1  for amd64
        static const uint32 MaxArrayBufferLength = 0x7FFFFFFF;
#else
        // maximum 1G to avoid arithmetic overflow.
        static const uint32 MaxArrayBufferLength = 1 << 30;
#endif
        static const uint32 ParentsCleanupThreshold = 1000;

        virtual bool IsValidAsmJsBufferLength(uint length, bool forceCheck = false) {TRACE_IT(54395); return false; }
        virtual bool IsArrayBuffer() override { return true; }
        virtual bool IsSharedArrayBuffer() override { return false; }
        virtual ArrayBuffer * GetAsArrayBuffer() override { return ArrayBuffer::FromVar(this); }

        static uint32 ToIndex(Var value, int32 errorCode, ScriptContext *scriptContext, uint32 MaxAllowedLength, bool checkSameValueZero = true);

        virtual ArrayBuffer * TransferInternal(DECLSPEC_GUARD_OVERFLOW uint32 newBufferLength) = 0;
    protected:

        typedef void __cdecl FreeFn(void* ptr);
        virtual ArrayBufferDetachedStateBase* CreateDetachedState(BYTE* buffer, DECLSPEC_GUARD_OVERFLOW uint32 bufferLength) = 0;

        static uint32 GetIndexFromVar(Js::Var arg, uint32 length, ScriptContext* scriptContext);

        //In most cases, the ArrayBuffer will only have one parent
        Field(RecyclerWeakReference<ArrayBufferParent>*) primaryParent;

        struct OtherParents :public SList<RecyclerWeakReference<ArrayBufferParent>*, Recycler>
        {
            OtherParents(Recycler* recycler)
                :SList<RecyclerWeakReference<ArrayBufferParent>*, Recycler>(recycler), increasedCount(0)
            {TRACE_IT(54396);
            }
            Field(uint) increasedCount;
        };

        Field(OtherParents*) otherParents;

        FieldNoBarrier(BYTE*) buffer;             // Points to a heap allocated RGBA buffer, can be null
        Field(uint32) bufferLength;       // Number of bytes allocated

        // When an ArrayBuffer is detached, the TypedArray and DataView objects pointing to it must be made aware,
        // for this purpose the ArrayBuffer needs to hold WeakReferences to them
        Field(bool) isDetached;
    };

    class ArrayBufferParent : public ArrayObject
    {
        friend ArrayBuffer;
        friend ArrayBufferBase;

    private:
        Field(ArrayBufferBase*) arrayBuffer;

    protected:
        DEFINE_VTABLE_CTOR_ABSTRACT(ArrayBufferParent, ArrayObject);

        ArrayBufferParent(DynamicType * type, uint32 length, ArrayBufferBase* arrayBuffer)
            : ArrayObject(type, /*initSlots*/true, length),
            arrayBuffer(arrayBuffer)
        {TRACE_IT(54397);
            arrayBuffer->AddParent(this);
        }

    public:
        ArrayBufferBase* GetArrayBuffer() const
        {TRACE_IT(54398);
            return this->arrayBuffer;
        }

#if ENABLE_TTD
    public:
        virtual void MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor) override;

        virtual void ProcessCorePaths() override;
#endif
    };

    // Normally we use malloc/free; for ArrayBuffer created from projection we need to use different allocator.
    class JavascriptArrayBuffer : public ArrayBuffer
    {
    protected:
        DEFINE_VTABLE_CTOR(JavascriptArrayBuffer, ArrayBuffer);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptArrayBuffer);

    public:
        static JavascriptArrayBuffer* Create(DECLSPEC_GUARD_OVERFLOW uint32 length, DynamicType * type);
        static JavascriptArrayBuffer* Create(byte* buffer, DECLSPEC_GUARD_OVERFLOW uint32 length, DynamicType * type);
        virtual void Dispose(bool isShutdown) override;
        virtual void Finalize(bool isShutdown) override;

        static bool IsValidAsmJsBufferLengthAlgo(uint length, bool forceCheck);
        virtual bool IsValidAsmJsBufferLength(uint length, bool forceCheck = false) override;
        virtual bool IsValidVirtualBufferLength(uint length) override;

        virtual ArrayBuffer * TransferInternal(DECLSPEC_GUARD_OVERFLOW uint32 newBufferLength) override;

        void ReportDifferentialAllocation(uint32 newBufferLength);

    protected:
        JavascriptArrayBuffer(DynamicType * type);
        virtual ArrayBufferDetachedStateBase* CreateDetachedState(BYTE* buffer, DECLSPEC_GUARD_OVERFLOW uint32 bufferLength) override;

        template<typename Allocator>
        JavascriptArrayBuffer(uint32 length, DynamicType * type, Allocator allocator): ArrayBuffer(length, type, allocator){TRACE_IT(54399);}
        JavascriptArrayBuffer(uint32 length, DynamicType * type);
        JavascriptArrayBuffer(byte* buffer, uint32 length, DynamicType * type);

#if ENABLE_TTD
    public:
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };

    class WebAssemblyArrayBuffer : public JavascriptArrayBuffer
    {
        WebAssemblyArrayBuffer(uint32 length, DynamicType * type);
        WebAssemblyArrayBuffer(byte* buffer, uint32 length, DynamicType * type);
    protected:
        DEFINE_VTABLE_CTOR(WebAssemblyArrayBuffer, JavascriptArrayBuffer);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(WebAssemblyArrayBuffer);
    public:
        static WebAssemblyArrayBuffer* Create(byte* buffer, DECLSPEC_GUARD_OVERFLOW uint32 length, DynamicType * type);
        virtual bool IsValidVirtualBufferLength(uint length) override;
        virtual ArrayBuffer * TransferInternal(DECLSPEC_GUARD_OVERFLOW uint32 newBufferLength) override;
    };

    // the memory must be allocated via CoTaskMemAlloc.
    class ProjectionArrayBuffer : public ArrayBuffer
    {
    protected:
        DEFINE_VTABLE_CTOR(ProjectionArrayBuffer, ArrayBuffer);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(ProjectionArrayBuffer);

        typedef void __stdcall FreeFn(LPVOID ptr);
        virtual ArrayBufferDetachedStateBase* CreateDetachedState(BYTE* buffer, DECLSPEC_GUARD_OVERFLOW uint32 bufferLength) override
        {
            return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, CoTaskMemFree, ArrayBufferAllocationType::CoTask);
        }

    public:
        // Create constructor. script engine creates a buffer allocated via CoTaskMemAlloc.
        static ProjectionArrayBuffer* Create(DECLSPEC_GUARD_OVERFLOW uint32 length, DynamicType * type);
        // take over ownership. a CoTaskMemAlloc'ed buffer passed in via projection.
        static ProjectionArrayBuffer* Create(byte* buffer, DECLSPEC_GUARD_OVERFLOW uint32 length, DynamicType * type);
        virtual void Dispose(bool isShutdown) override;
        virtual void Finalize(bool isShutdown) override {};
        virtual ArrayBuffer * TransferInternal(DECLSPEC_GUARD_OVERFLOW uint32 newBufferLength) override;
    private:
        ProjectionArrayBuffer(uint32 length, DynamicType * type);
        ProjectionArrayBuffer(byte* buffer, uint32 length, DynamicType * type);
    };

    // non-owning ArrayBuffer used for wrapping external data
    class ExternalArrayBuffer : public ArrayBuffer
    {
    protected:
        DEFINE_VTABLE_CTOR(ExternalArrayBuffer, ArrayBuffer);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(ExternalArrayBuffer);
    public:
        ExternalArrayBuffer(byte *buffer, DECLSPEC_GUARD_OVERFLOW uint32 length, DynamicType *type);
        virtual ArrayBuffer * TransferInternal(DECLSPEC_GUARD_OVERFLOW uint32 newBufferLength) override { Assert(UNREACHED); Throw::InternalError(); };
    protected:
        virtual ArrayBufferDetachedStateBase* CreateDetachedState(BYTE* buffer, DECLSPEC_GUARD_OVERFLOW uint32 bufferLength) override { Assert(UNREACHED); Throw::InternalError(); };

#if ENABLE_TTD
    public:
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };
}
