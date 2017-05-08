//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    bool ArrayBufferBase::Is(Var value)
    {TRACE_IT(54242);
        return ArrayBuffer::Is(value) || SharedArrayBuffer::Is(value);
    }

    ArrayBufferBase* ArrayBufferBase::FromVar(Var value)
    {TRACE_IT(54243);
        Assert(ArrayBufferBase::Is(value));
        return static_cast<ArrayBuffer *> (value);
    }

    ArrayBuffer* ArrayBuffer::NewFromDetachedState(DetachedStateBase* state, JavascriptLibrary *library)
    {TRACE_IT(54244);
        ArrayBufferDetachedStateBase* arrayBufferState = (ArrayBufferDetachedStateBase *)state;
        ArrayBuffer *toReturn = nullptr;

        switch (arrayBufferState->allocationType)
        {
        case ArrayBufferAllocationType::CoTask:
            toReturn = library->CreateProjectionArraybuffer(arrayBufferState->buffer, arrayBufferState->bufferLength);
            break;
        case ArrayBufferAllocationType::Heap:
        case ArrayBufferAllocationType::MemAlloc:
            toReturn = library->CreateArrayBuffer(arrayBufferState->buffer, arrayBufferState->bufferLength);
            break;
        default:
            AssertMsg(false, "Unknown allocationType of ArrayBufferDetachedStateBase ");
        }

        return toReturn;
    }

    void ArrayBuffer::ClearParentsLength(ArrayBufferParent* parent)
    {TRACE_IT(54245);
        if (parent == nullptr)
        {TRACE_IT(54246);
            return;
        }

        switch (JavascriptOperators::GetTypeId(parent))
        {
        case TypeIds_Int8Array:
        case TypeIds_Uint8Array:
        case TypeIds_Uint8ClampedArray:
        case TypeIds_Int16Array:
        case TypeIds_Uint16Array:
        case TypeIds_Int32Array:
        case TypeIds_Uint32Array:
        case TypeIds_Float32Array:
        case TypeIds_Float64Array:
        case TypeIds_Int64Array:
        case TypeIds_Uint64Array:
        case TypeIds_CharArray:
        case TypeIds_BoolArray:
            TypedArrayBase::FromVar(parent)->length = 0;
            break;

        case TypeIds_DataView:
            DataView::FromVar(parent)->length = 0;
            break;

        default:
            AssertMsg(false, "We need an explicit case for any parent of ArrayBuffer.");
            break;
        }
    }

    ArrayBufferDetachedStateBase* ArrayBuffer::DetachAndGetState()
    {TRACE_IT(54247);
        Assert(!this->isDetached);

        AutoPtr<ArrayBufferDetachedStateBase> arrayBufferState(this->CreateDetachedState(this->buffer, this->bufferLength));

        this->buffer = nullptr;
        this->bufferLength = 0;
        this->isDetached = true;

        if (this->primaryParent != nullptr && this->primaryParent->Get() == nullptr)
        {TRACE_IT(54248);
            this->primaryParent = nullptr;
        }

        if (this->primaryParent != nullptr)
        {TRACE_IT(54249);
            this->ClearParentsLength(this->primaryParent->Get());
        }

        if (this->otherParents != nullptr)
        {TRACE_IT(54250);
            this->otherParents->Map([&](RecyclerWeakReference<ArrayBufferParent>* item)
            {
                this->ClearParentsLength(item->Get());
            });
        }

        return arrayBufferState.Detach();
    }

    void ArrayBuffer::AddParent(ArrayBufferParent* parent)
    {TRACE_IT(54251);
        if (this->primaryParent == nullptr || this->primaryParent->Get() == nullptr)
        {TRACE_IT(54252);
            this->primaryParent = this->GetRecycler()->CreateWeakReferenceHandle(parent);
        }
        else
        {TRACE_IT(54253);
            if (this->otherParents == nullptr)
            {TRACE_IT(54254);
                this->otherParents = RecyclerNew(this->GetRecycler(), OtherParents, this->GetRecycler());
            }

            if (this->otherParents->increasedCount >= ParentsCleanupThreshold)
            {TRACE_IT(54255);
                auto iter = this->otherParents->GetEditingIterator();
                while (iter.Next())
                {TRACE_IT(54256);
                    if (iter.Data()->Get() == nullptr)
                    {TRACE_IT(54257);
                        iter.RemoveCurrent();
                    }
                }

                this->otherParents->increasedCount = 0;
            }

            this->otherParents->PrependNode(this->GetRecycler()->CreateWeakReferenceHandle(parent));
            this->otherParents->increasedCount++;
        }
    }

    uint32 ArrayBuffer::ToIndex(Var value, int32 errorCode, ScriptContext *scriptContext, uint32 MaxAllowedLength, bool checkSameValueZero)
    {TRACE_IT(54258);
        if (JavascriptOperators::IsUndefined(value))
        {TRACE_IT(54259);
            return 0;
        }

        if (TaggedInt::Is(value))
        {TRACE_IT(54260);
            int64 index = TaggedInt::ToInt64(value);
            if (index < 0 || index >(int64)MaxAllowedLength)
            {TRACE_IT(54261);
                JavascriptError::ThrowRangeError(scriptContext, errorCode);
            }

            return  (uint32)index;
        }

        // Slower path

        double d = JavascriptConversion::ToInteger(value, scriptContext);
        if (d < 0.0 || d >(double)MaxAllowedLength)
        {TRACE_IT(54262);
            JavascriptError::ThrowRangeError(scriptContext, errorCode);
        }

        if (checkSameValueZero)
        {TRACE_IT(54263);
            Var integerIndex = JavascriptNumber::ToVarNoCheck(d, scriptContext);
            Var index = JavascriptNumber::ToVar(JavascriptConversion::ToLength(integerIndex, scriptContext), scriptContext);
            if (!JavascriptConversion::SameValueZero(integerIndex, index))
            {TRACE_IT(54264);
                JavascriptError::ThrowRangeError(scriptContext, errorCode);
            }
        }

        return (uint32)d;
    }

    Var ArrayBuffer::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

        if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
        {TRACE_IT(54265);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("ArrayBuffer"));
        }

        uint32 byteLength = 0;
        if (args.Info.Count > 1)
        {TRACE_IT(54266);
            byteLength = ToIndex(args[1], JSERR_ArrayLengthConstructIncorrect, scriptContext, MaxArrayBufferLength);
        }

        RecyclableObject* newArr = scriptContext->GetLibrary()->CreateArrayBuffer(byteLength);
        Assert(ArrayBuffer::Is(newArr));
        if (byteLength > 0 && !ArrayBuffer::FromVar(newArr)->GetByteLength())
        {TRACE_IT(54267);
            JavascriptError::ThrowRangeError(scriptContext, JSERR_FunctionArgument_Invalid);
        }
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {TRACE_IT(54268);
            newArr = Js::JavascriptProxy::AutoProxyWrapper(newArr);
        }
#endif
        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), newArr, nullptr, scriptContext) :
            newArr;
    }

    // ArrayBuffer.prototype.byteLength as described in ES6 draft #20 section 24.1.4.1
    Var ArrayBuffer::EntryGetterByteLength(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !ArrayBuffer::Is(args[0]))
        {TRACE_IT(54269);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        ArrayBuffer* arrayBuffer = ArrayBuffer::FromVar(args[0]);
        if (arrayBuffer->IsDetached())
        {TRACE_IT(54270);
            return JavascriptNumber::ToVar(0, scriptContext);
        }
        return JavascriptNumber::ToVar(arrayBuffer->GetByteLength(), scriptContext);
    }

    // ArrayBuffer.isView as described in ES6 draft #20 section 24.1.3.1
    Var ArrayBuffer::EntryIsView(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        JavascriptLibrary* library = function->GetScriptContext()->GetLibrary();

        Var arg = library->GetUndefined();

        if (args.Info.Count > 1)
        {TRACE_IT(54271);
            arg = args[1];
        }

        // Only DataView or any TypedArray objects have [[ViewedArrayBuffer]] internal slots
        if (DataView::Is(arg) || TypedArrayBase::Is(arg))
        {TRACE_IT(54272);
            return library->GetTrue();
        }

        return library->GetFalse();
    }

    // ArrayBuffer.transfer as described in Luke Wagner's proposal: https://gist.github.com/lukewagner/2735af7eea411e18cf20
    Var ArrayBuffer::EntryTransfer(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ScriptContext* scriptContext = function->GetScriptContext();

        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        Assert(!(callInfo.Flags & CallFlags_New));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ArrayBuffer_Constructor_transfer);

        if (args.Info.Count < 2 || !ArrayBuffer::Is(args[1]))
        {TRACE_IT(54273);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        ArrayBuffer* arrayBuffer = ArrayBuffer::FromVar(args[1]);

        if (arrayBuffer->IsDetached())
        {TRACE_IT(54274);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("ArrayBuffer.transfer"));
        }

        uint32 newBufferLength = arrayBuffer->bufferLength;
        if (args.Info.Count >= 3)
        {TRACE_IT(54275);
            newBufferLength = ToIndex(args[2], JSERR_ArrayLengthConstructIncorrect, scriptContext, MaxArrayBufferLength);

            // ToIndex above can call user script (valueOf) which can detach the buffer
            if (arrayBuffer->IsDetached())
            {TRACE_IT(54276);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("ArrayBuffer.transfer"));
            }
        }

        return arrayBuffer->TransferInternal(newBufferLength);
    }

    // ArrayBuffer.prototype.slice as described in ES6 draft #19 section 24.1.4.3.
    Var ArrayBuffer::EntrySlice(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ScriptContext* scriptContext = function->GetScriptContext();

        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Assert(!(callInfo.Flags & CallFlags_New));

        if (!ArrayBuffer::Is(args[0]))
        {TRACE_IT(54277);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        JavascriptLibrary* library = scriptContext->GetLibrary();
        ArrayBuffer* arrayBuffer = ArrayBuffer::FromVar(args[0]);

        if (arrayBuffer->IsDetached()) // 24.1.4.3: 5. If IsDetachedBuffer(O) is true, then throw a TypeError exception.
        {TRACE_IT(54278);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("ArrayBuffer.prototype.slice"));
        }

        int64 len = arrayBuffer->bufferLength;
        int64 start = 0, end = 0;
        int64 newLen;

        // If no start or end arguments, use the entire length
        if (args.Info.Count < 2)
        {TRACE_IT(54279);
            newLen = len;
        }
        else
        {TRACE_IT(54280);
            start = JavascriptArray::GetIndexFromVar(args[1], len, scriptContext);

            // If no end argument, use length as the end
            if (args.Info.Count < 3 || args[2] == library->GetUndefined())
            {TRACE_IT(54281);
                end = len;
            }
            else
            {TRACE_IT(54282);
                end = JavascriptArray::GetIndexFromVar(args[2], len, scriptContext);
            }

            newLen = end > start ? end - start : 0;
        }

        // We can't have allocated an ArrayBuffer with byteLength > MaxArrayBufferLength.
        // start and end are clamped to valid indices, so the new length also cannot exceed MaxArrayBufferLength.
        // Therefore, should be safe to cast down newLen to uint32.
        // TODO: If we ever support allocating ArrayBuffer with byteLength > MaxArrayBufferLength we may need to review this math.
        Assert(newLen < MaxArrayBufferLength);
        uint32 byteLength = static_cast<uint32>(newLen);

        ArrayBuffer* newBuffer = nullptr;

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {TRACE_IT(54283);
            Var constructorVar = JavascriptOperators::SpeciesConstructor(arrayBuffer, scriptContext->GetLibrary()->GetArrayBufferConstructor(), scriptContext);

            JavascriptFunction* constructor = JavascriptFunction::FromVar(constructorVar);

            Js::Var constructorArgs[] = {constructor, JavascriptNumber::ToVar(byteLength, scriptContext)};
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            Js::Var newVar = JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext);

            if (!ArrayBuffer::Is(newVar)) // 24.1.4.3: 19.If new does not have an [[ArrayBufferData]] internal slot throw a TypeError exception.
            {TRACE_IT(54284);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
            }

            newBuffer = ArrayBuffer::FromVar(newVar);

            if (newBuffer->IsDetached()) // 24.1.4.3: 21. If IsDetachedBuffer(new) is true, then throw a TypeError exception.
            {TRACE_IT(54285);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("ArrayBuffer.prototype.slice"));
            }

            if (newBuffer == arrayBuffer) // 24.1.4.3: 22. If SameValue(new, O) is true, then throw a TypeError exception.
            {TRACE_IT(54286);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
            }

            if (newBuffer->bufferLength < byteLength) // 24.1.4.3: 23.If the value of new's [[ArrayBufferByteLength]] internal slot < newLen, then throw a TypeError exception.
            {TRACE_IT(54287);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_ArgumentOutOfRange, _u("ArrayBuffer.prototype.slice"));
            }
        }
        else
        {TRACE_IT(54288);
            newBuffer = library->CreateArrayBuffer(byteLength);
        }

        Assert(newBuffer);
        Assert(newBuffer->bufferLength >= byteLength);

        if (arrayBuffer->IsDetached()) // 24.1.4.3: 24. NOTE: Side-effects of the above steps may have detached O. 25. If IsDetachedBuffer(O) is true, then throw a TypeError exception.
        {TRACE_IT(54289);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("ArrayBuffer.prototype.slice"));
        }

        // Don't bother doing memcpy if we aren't copying any elements
        if (byteLength > 0)
        {TRACE_IT(54290);
            AssertMsg(arrayBuffer->buffer != nullptr, "buffer must not be null when we copy from it");

            js_memcpy_s(newBuffer->buffer, byteLength, arrayBuffer->buffer + start, byteLength);
        }

        return newBuffer;
    }

    Var ArrayBuffer::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

    ArrayBuffer* ArrayBuffer::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "var must be an ArrayBuffer");

        return static_cast<ArrayBuffer *>(RecyclableObject::FromVar(aValue));
    }

    bool  ArrayBuffer::Is(Var aValue)
    {TRACE_IT(54291);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_ArrayBuffer;
    }

    template <class Allocator>
    ArrayBuffer::ArrayBuffer(uint32 length, DynamicType * type, Allocator allocator) :
        ArrayBufferBase(type), isDetached(false)
    {TRACE_IT(54292);
        buffer = nullptr;
        bufferLength = 0;
        if (length > MaxArrayBufferLength)
        {TRACE_IT(54293);
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_FunctionArgument_Invalid);
        }
        else if (length > 0)
        {TRACE_IT(54294);
            Recycler* recycler = GetType()->GetLibrary()->GetRecycler();
            if (recycler->ReportExternalMemoryAllocation(length))
            {TRACE_IT(54295);
                buffer = (BYTE*)allocator(length);
                if (buffer == nullptr)
                {TRACE_IT(54296);
                    recycler->ReportExternalMemoryFree(length);
                }
            }

            if (buffer == nullptr)
            {TRACE_IT(54297);
                recycler->CollectNow<CollectOnTypedArrayAllocation>();

                if (recycler->ReportExternalMemoryAllocation(length))
                {TRACE_IT(54298);
                    buffer = (BYTE*)allocator(length);
                    if (buffer == nullptr)
                    {TRACE_IT(54299);
                        recycler->ReportExternalMemoryFailure(length);
                    }
                }
                else
                {TRACE_IT(54300);
                    JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                }
            }

            if (buffer != nullptr)
            {TRACE_IT(54301);
                bufferLength = length;
                ZeroMemory(buffer, bufferLength);
            }
        }
    }

    ArrayBuffer::ArrayBuffer(byte* buffer, uint32 length, DynamicType * type) :
        buffer(buffer), bufferLength(length), ArrayBufferBase(type), isDetached(false)
    {TRACE_IT(54302);
        if (length > MaxArrayBufferLength)
        {TRACE_IT(54303);
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_FunctionArgument_Invalid);
        }
    }

    BOOL ArrayBuffer::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(54304);
        stringBuilder->AppendCppLiteral(_u("Object, (ArrayBuffer)"));
        return TRUE;
    }

    BOOL ArrayBuffer::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(54305);
        stringBuilder->AppendCppLiteral(_u("[object ArrayBuffer]"));
        return TRUE;
    }

#if ENABLE_TTD
    void ArrayBufferParent::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(54306);
        extractor->MarkVisitVar(this->arrayBuffer);
    }

    void ArrayBufferParent::ProcessCorePaths()
    {TRACE_IT(54307);
        this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->arrayBuffer, _u("!buffer"));
    }
#endif

    JavascriptArrayBuffer::JavascriptArrayBuffer(uint32 length, DynamicType * type) :
        ArrayBuffer(length, type, IsValidVirtualBufferLength(length) ? AsmJsVirtualAllocator : malloc)
    {TRACE_IT(54308);
    }
    JavascriptArrayBuffer::JavascriptArrayBuffer(byte* buffer, uint32 length, DynamicType * type) :
        ArrayBuffer(buffer, length, type)
    {TRACE_IT(54309);
    }

    JavascriptArrayBuffer::JavascriptArrayBuffer(DynamicType * type) : ArrayBuffer(0, type, malloc)
    {TRACE_IT(54310);
    }

    JavascriptArrayBuffer* JavascriptArrayBuffer::Create(uint32 length, DynamicType * type)
    {TRACE_IT(54311);
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        JavascriptArrayBuffer* result = RecyclerNewFinalized(recycler, JavascriptArrayBuffer, length, type);
        Assert(result);
        recycler->AddExternalMemoryUsage(length);
        return result;
    }

    JavascriptArrayBuffer* JavascriptArrayBuffer::Create(byte* buffer, uint32 length, DynamicType * type)
    {TRACE_IT(54312);
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        JavascriptArrayBuffer* result = RecyclerNewFinalized(recycler, JavascriptArrayBuffer, buffer, length, type);
        Assert(result);
        recycler->AddExternalMemoryUsage(length);
        return result;
    }

    ArrayBufferDetachedStateBase* JavascriptArrayBuffer::CreateDetachedState(BYTE* buffer, uint32 bufferLength)
    {TRACE_IT(54313);
#if ENABLE_FAST_ARRAYBUFFER
        if (IsValidVirtualBufferLength(bufferLength))
        {TRACE_IT(54314);
            return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, FreeMemAlloc, ArrayBufferAllocationType::MemAlloc);
        }
        else
        {TRACE_IT(54315);
            return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, free, ArrayBufferAllocationType::Heap);
        }
#else
        return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, free, ArrayBufferAllocationType::Heap);
#endif
    }

    bool JavascriptArrayBuffer::IsValidAsmJsBufferLengthAlgo(uint length, bool forceCheck)
    {TRACE_IT(54316);
        /*
        1. length >= 2^16
        2. length is power of 2 or (length > 2^24 and length is multiple of 2^24)
        3. length is a multiple of 4K
        */
        const bool isLongEnough = length >= 0x10000;
        const bool isPow2 = ::Math::IsPow2(length);
        // No need to check for length > 2^24, because it already has to be non zero length
        const bool isMultipleOf2e24 = (length & 0xFFFFFF) == 0;
        const bool isPageSizeMultiple = (length % AutoSystemInfo::PageSize) == 0;
        return (
#ifndef ENABLE_FAST_ARRAYBUFFER
            forceCheck &&
#endif
            isLongEnough &&
            (isPow2 || isMultipleOf2e24) &&
            isPageSizeMultiple
        );
    }

    bool JavascriptArrayBuffer::IsValidAsmJsBufferLength(uint length, bool forceCheck)
    {TRACE_IT(54317);
        return IsValidAsmJsBufferLengthAlgo(length, forceCheck);
    }

    bool JavascriptArrayBuffer::IsValidVirtualBufferLength(uint length)
    {TRACE_IT(54318);
#if ENABLE_FAST_ARRAYBUFFER
        return !PHASE_OFF1(Js::TypedArrayVirtualPhase) && IsValidAsmJsBufferLengthAlgo(length, true);
#else
        return false;
#endif
    }

    void JavascriptArrayBuffer::Finalize(bool isShutdown)
    {TRACE_IT(54319);
        // In debugger scenario, ScriptAuthor can create scriptContext and delete scriptContext
        // explicitly. So for the builtin, while javascriptLibrary is still alive fine, the
        // matching scriptContext might have been deleted and the javascriptLibrary->scriptContext
        // field reset (but javascriptLibrary is still alive).
        // Use the recycler field off library instead of scriptcontext to avoid av.

        // Recycler may not be available at Dispose. We need to
        // free the memory and report that it has been freed at the same
        // time. Otherwise, AllocationPolicyManager is unable to provide correct feedback
#if ENABLE_FAST_ARRAYBUFFER
        //AsmJS Virtual Free
        if (buffer && IsValidVirtualBufferLength(this->bufferLength))
        {TRACE_IT(54320);
            FreeMemAlloc(buffer);
        }
        else
        {TRACE_IT(54321);
            free(buffer);
        }
#else
        free(buffer);
#endif
        Recycler* recycler = GetType()->GetLibrary()->GetRecycler();
        recycler->ReportExternalMemoryFree(bufferLength);

        buffer = nullptr;
        bufferLength = 0;
    }

    void JavascriptArrayBuffer::Dispose(bool isShutdown)
    {TRACE_IT(54322);
        /* See JavascriptArrayBuffer::Finalize */
    }

    // Copy memory from src to dst, truncate if dst smaller, zero extra memory
    // if dst larger
    static void MemCpyZero(__bcount(dstSize) BYTE* dst, size_t dstSize,
        __in_bcount(count) const BYTE* src, size_t count)
    {
        js_memcpy_s(dst, dstSize, src, min(dstSize, count));
        if (dstSize > count)
        {TRACE_IT(54323);
            ZeroMemory(dst + count, dstSize - count);
        }
    }

    // Same as realloc but zero newly allocated portion if newSize > oldSize
    static BYTE* ReallocZero(BYTE* ptr, size_t oldSize, size_t newSize)
    {TRACE_IT(54324);
        BYTE* ptrNew = (BYTE*)realloc(ptr, newSize);
        if (ptrNew && newSize > oldSize)
        {TRACE_IT(54325);
            ZeroMemory(ptrNew + oldSize, newSize - oldSize);
        }
        return ptrNew;
    }

    ArrayBuffer * JavascriptArrayBuffer::TransferInternal(uint32 newBufferLength)
    {TRACE_IT(54326);
        ArrayBuffer* newArrayBuffer;
        Recycler* recycler = this->GetRecycler();

        if (this->bufferLength > 0)
        {TRACE_IT(54327);
            ReportDifferentialAllocation(newBufferLength);
        }

        if (newBufferLength == 0 || this->bufferLength == 0)
        {TRACE_IT(54328);
            newArrayBuffer = GetLibrary()->CreateArrayBuffer(newBufferLength);
            if (newBufferLength > 0 && !newArrayBuffer->GetByteLength())
            {TRACE_IT(54329);
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }
        }
        else
        {TRACE_IT(54330);
            BYTE * newBuffer = nullptr;
            if (IsValidVirtualBufferLength(this->bufferLength))
            {TRACE_IT(54331);
                if (IsValidVirtualBufferLength(newBufferLength))
                {TRACE_IT(54332);
                    // we are transferring between an optimized buffer using a length that can be optimized
                    if (newBufferLength < this->bufferLength)
                    {TRACE_IT(54333);
#pragma prefast(suppress:6250, "Calling 'VirtualFree' without the MEM_RELEASE flag might free memory but not address descriptors (VADs).")
                        VirtualFree(this->buffer + newBufferLength, this->bufferLength - newBufferLength, MEM_DECOMMIT);
                    }
                    else if (newBufferLength > this->bufferLength)
                    {TRACE_IT(54334);
                        LPVOID newMem = VirtualAlloc(this->buffer + this->bufferLength, newBufferLength - this->bufferLength, MEM_COMMIT, PAGE_READWRITE);
                        if (!newMem)
                        {TRACE_IT(54335);
                            recycler->ReportExternalMemoryFailure(newBufferLength - this->bufferLength);
                            JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                        }
                    }
                    newBuffer = this->buffer;
                }
                else
                {TRACE_IT(54336);
                    // we are transferring from an optimized buffer, but the new length isn't compatible, so start over and copy to new memory
                    newBuffer = (BYTE*)malloc(newBufferLength);
                    if (!newBuffer)
                    {TRACE_IT(54337);
                        recycler->ReportExternalMemoryFailure(newBufferLength - this->bufferLength);
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                    MemCpyZero(newBuffer, newBufferLength, this->buffer, this->bufferLength);
                }
            }
            else
            {TRACE_IT(54338);
                if (IsValidVirtualBufferLength(newBufferLength))
                {TRACE_IT(54339);
                    // we are transferring from an unoptimized buffer, but new length can be optimized, so move to that
                    newBuffer = (BYTE*)AsmJsVirtualAllocator(newBufferLength);
                    if (!newBuffer)
                    {TRACE_IT(54340);
                        recycler->ReportExternalMemoryFailure(newBufferLength - this->bufferLength);
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                    MemCpyZero(newBuffer, newBufferLength, this->buffer, this->bufferLength);
                }
                else if (newBufferLength != this->bufferLength)
                {TRACE_IT(54341);
                    // both sides will just be regular ArrayBuffer, so realloc
                    newBuffer = ReallocZero(this->buffer, this->bufferLength, newBufferLength);
                    if (!newBuffer)
                    {TRACE_IT(54342);
                        recycler->ReportExternalMemoryFailure(newBufferLength - this->bufferLength);
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                }
                else
                {TRACE_IT(54343);
                    newBuffer = this->buffer;
                }
            }
            newArrayBuffer = GetLibrary()->CreateArrayBuffer(newBuffer, newBufferLength);

        }
        AutoDiscardPTR<Js::ArrayBufferDetachedStateBase> state(DetachAndGetState());
        state->MarkAsClaimed();

        return newArrayBuffer;
    }

    void JavascriptArrayBuffer::ReportDifferentialAllocation(uint32 newBufferLength)
    {TRACE_IT(54344);
        Recycler* recycler = this->GetRecycler();

        // Report differential external memory allocation.
        // If current bufferLength == 0, new ArrayBuffer creation records the allocation
        // so no need to do it here.
        if (newBufferLength != this->bufferLength)
        {TRACE_IT(54345);
            // Expanding buffer
            if (newBufferLength > this->bufferLength)
            {TRACE_IT(54346);
                if (!recycler->ReportExternalMemoryAllocation(newBufferLength - this->bufferLength))
                {TRACE_IT(54347);
                    recycler->CollectNow<CollectOnTypedArrayAllocation>();
                    if (!recycler->ReportExternalMemoryAllocation(newBufferLength - this->bufferLength))
                    {TRACE_IT(54348);
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                }
            }
            // Contracting buffer
            else
            {TRACE_IT(54349);
                recycler->ReportExternalMemoryFree(this->bufferLength - newBufferLength);
            }
        }
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptArrayBuffer::GetSnapTag_TTD() const
    {TRACE_IT(54350);
        return TTD::NSSnapObjects::SnapObjectType::SnapArrayBufferObject;
    }

    void JavascriptArrayBuffer::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(54351);
        TTD::NSSnapObjects::SnapArrayBufferInfo* sabi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapArrayBufferInfo>();

        sabi->Length = this->GetByteLength();
        if (sabi->Length == 0)
        {TRACE_IT(54352);
            sabi->Buff = nullptr;
        }
        else
        {TRACE_IT(54353);
            sabi->Buff = alloc.SlabAllocateArray<byte>(sabi->Length);
            memcpy(sabi->Buff, this->GetBuffer(), sabi->Length);
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayBufferInfo*, TTD::NSSnapObjects::SnapObjectType::SnapArrayBufferObject>(objData, sabi);
    }
#endif



    WebAssemblyArrayBuffer::WebAssemblyArrayBuffer(uint32 length, DynamicType * type) :
#ifndef ENABLE_FAST_ARRAYBUFFER
        // Treat as a normal JavascriptArrayBuffer
        JavascriptArrayBuffer(length, type) {TRACE_IT(54354);}
#else
        JavascriptArrayBuffer(length, type, WasmVirtualAllocator)
    {TRACE_IT(54355);
        // Make sure we always have a buffer even if the length is 0
        if (buffer == nullptr)
        {TRACE_IT(54356);
            // We want to allocate an empty buffer using virtual memory
            Assert(length == 0);
            buffer = (BYTE*)WasmVirtualAllocator(0);
            if (buffer == nullptr)
            {TRACE_IT(54357);
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }
        }
    }
#endif

    WebAssemblyArrayBuffer::WebAssemblyArrayBuffer(byte* buffer, uint32 length, DynamicType * type):
        JavascriptArrayBuffer(buffer, length, type)
    {TRACE_IT(54358);

    }

    WebAssemblyArrayBuffer* WebAssemblyArrayBuffer::Create(byte* buffer, uint32 length, DynamicType * type)
    {TRACE_IT(54359);
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        WebAssemblyArrayBuffer* result;
        if (buffer)
        {TRACE_IT(54360);
            result = RecyclerNewFinalized(recycler, WebAssemblyArrayBuffer, buffer, length, type);
        }
        else
        {TRACE_IT(54361);
            result = RecyclerNewFinalized(recycler, WebAssemblyArrayBuffer, length, type);
        }
        Assert(result);
        recycler->AddExternalMemoryUsage(length);
        return result;
    }

    bool WebAssemblyArrayBuffer::IsValidVirtualBufferLength(uint length)
    {TRACE_IT(54362);
#if ENABLE_FAST_ARRAYBUFFER
        return true;
#else
        return false;
#endif
    }

    ArrayBuffer * WebAssemblyArrayBuffer::TransferInternal(uint32 newBufferLength)
    {TRACE_IT(54363);
#if ENABLE_FAST_ARRAYBUFFER
        ReportDifferentialAllocation(newBufferLength);
        Assert(this->buffer);

        AssertMsg(newBufferLength > this->bufferLength, "The only supported scenario in WebAssembly is to grow the memory");
        if (newBufferLength > this->bufferLength)
        {TRACE_IT(54364);
            LPVOID newMem = VirtualAlloc(this->buffer + this->bufferLength, newBufferLength - this->bufferLength, MEM_COMMIT, PAGE_READWRITE);
            if (!newMem)
            {TRACE_IT(54365);
                Recycler* recycler = this->GetRecycler();
                recycler->ReportExternalMemoryFailure(newBufferLength);
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }
        }
        ArrayBuffer* newArrayBuffer = GetLibrary()->CreateWebAssemblyArrayBuffer(this->buffer, newBufferLength);

        AutoDiscardPTR<Js::ArrayBufferDetachedStateBase> state(DetachAndGetState());
        state->MarkAsClaimed();

        return newArrayBuffer;
#else
        return JavascriptArrayBuffer::TransferInternal(newBufferLength);
#endif
    }

    ProjectionArrayBuffer::ProjectionArrayBuffer(uint32 length, DynamicType * type) :
        ArrayBuffer(length, type, CoTaskMemAlloc)
    {TRACE_IT(54366);
    }

    ProjectionArrayBuffer::ProjectionArrayBuffer(byte* buffer, uint32 length, DynamicType * type) :
        ArrayBuffer(buffer, length, type)
    {TRACE_IT(54367);
    }

    ProjectionArrayBuffer* ProjectionArrayBuffer::Create(uint32 length, DynamicType * type)
    {TRACE_IT(54368);
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        recycler->AddExternalMemoryUsage(length);
        return RecyclerNewFinalized(recycler, ProjectionArrayBuffer, length, type);
    }

    ProjectionArrayBuffer* ProjectionArrayBuffer::Create(byte* buffer, uint32 length, DynamicType * type)
    {TRACE_IT(54369);
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        // This is user passed [in] buffer, user should AddExternalMemoryUsage before calling jscript, but
        // I don't see we ask everyone to do this. Let's add the memory pressure here as well.
        recycler->AddExternalMemoryUsage(length);
        return RecyclerNewFinalized(recycler, ProjectionArrayBuffer, buffer, length, type);
    }

    void ProjectionArrayBuffer::Dispose(bool isShutdown)
    {TRACE_IT(54370);
        CoTaskMemFree(buffer);
    }

    ArrayBuffer * ProjectionArrayBuffer::TransferInternal(uint32 newBufferLength)
    {TRACE_IT(54371);
        ArrayBuffer* newArrayBuffer;
        if (newBufferLength == 0 || this->bufferLength == 0)
        {TRACE_IT(54372);
            newArrayBuffer = GetLibrary()->CreateProjectionArraybuffer(newBufferLength);
        }
        else
        {TRACE_IT(54373);
            BYTE * newBuffer = (BYTE*)CoTaskMemRealloc(this->buffer, newBufferLength);
            if (!newBuffer)
            {TRACE_IT(54374);
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }
            newArrayBuffer = GetLibrary()->CreateProjectionArraybuffer(newBuffer, newBufferLength);
        }

        AutoDiscardPTR<Js::ArrayBufferDetachedStateBase> state(DetachAndGetState());
        state->MarkAsClaimed();

        return newArrayBuffer;
    }

    ExternalArrayBuffer::ExternalArrayBuffer(byte *buffer, uint32 length, DynamicType *type)
        : ArrayBuffer(buffer, length, type)
    {TRACE_IT(54375);
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ExternalArrayBuffer::GetSnapTag_TTD() const
    {TRACE_IT(54376);
        //We re-map ExternalArrayBuffers to regular buffers since the 'real' host will be gone when we replay
        return TTD::NSSnapObjects::SnapObjectType::SnapArrayBufferObject;
    }

    void ExternalArrayBuffer::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(54377);
        TTD::NSSnapObjects::SnapArrayBufferInfo* sabi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapArrayBufferInfo>();

        sabi->Length = this->GetByteLength();
        if(sabi->Length == 0)
        {TRACE_IT(54378);
            sabi->Buff = nullptr;
        }
        else
        {TRACE_IT(54379);
            sabi->Buff = alloc.SlabAllocateArray<byte>(sabi->Length);
            memcpy(sabi->Buff, this->GetBuffer(), sabi->Length);
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayBufferInfo*, TTD::NSSnapObjects::SnapObjectType::SnapArrayBufferObject>(objData, sabi);
    }
#endif
}
