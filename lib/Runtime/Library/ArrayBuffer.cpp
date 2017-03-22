//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    bool ArrayBufferBase::Is(Var value)
    {LOGMEIN("ArrayBuffer.cpp] 9\n");
        return ArrayBuffer::Is(value) || SharedArrayBuffer::Is(value);
    }

    ArrayBufferBase* ArrayBufferBase::FromVar(Var value)
    {LOGMEIN("ArrayBuffer.cpp] 14\n");
        Assert(ArrayBufferBase::Is(value));
        return static_cast<ArrayBuffer *> (value);
    }

    ArrayBuffer* ArrayBuffer::NewFromDetachedState(DetachedStateBase* state, JavascriptLibrary *library)
    {LOGMEIN("ArrayBuffer.cpp] 20\n");
        ArrayBufferDetachedStateBase* arrayBufferState = (ArrayBufferDetachedStateBase *)state;
        ArrayBuffer *toReturn = nullptr;

        switch (arrayBufferState->allocationType)
        {LOGMEIN("ArrayBuffer.cpp] 25\n");
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
    {LOGMEIN("ArrayBuffer.cpp] 41\n");
        if (parent == nullptr)
        {LOGMEIN("ArrayBuffer.cpp] 43\n");
            return;
        }

        switch (JavascriptOperators::GetTypeId(parent))
        {LOGMEIN("ArrayBuffer.cpp] 48\n");
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
    {LOGMEIN("ArrayBuffer.cpp] 76\n");
        Assert(!this->isDetached);

        AutoPtr<ArrayBufferDetachedStateBase> arrayBufferState(this->CreateDetachedState(this->buffer, this->bufferLength));

        this->buffer = nullptr;
        this->bufferLength = 0;
        this->isDetached = true;

        if (this->primaryParent != nullptr && this->primaryParent->Get() == nullptr)
        {LOGMEIN("ArrayBuffer.cpp] 86\n");
            this->primaryParent = nullptr;
        }

        if (this->primaryParent != nullptr)
        {LOGMEIN("ArrayBuffer.cpp] 91\n");
            this->ClearParentsLength(this->primaryParent->Get());
        }

        if (this->otherParents != nullptr)
        {LOGMEIN("ArrayBuffer.cpp] 96\n");
            this->otherParents->Map([&](RecyclerWeakReference<ArrayBufferParent>* item)
            {
                this->ClearParentsLength(item->Get());
            });
        }

        return arrayBufferState.Detach();
    }

    void ArrayBuffer::AddParent(ArrayBufferParent* parent)
    {LOGMEIN("ArrayBuffer.cpp] 107\n");
        if (this->primaryParent == nullptr || this->primaryParent->Get() == nullptr)
        {LOGMEIN("ArrayBuffer.cpp] 109\n");
            this->primaryParent = this->GetRecycler()->CreateWeakReferenceHandle(parent);
        }
        else
        {
            if (this->otherParents == nullptr)
            {LOGMEIN("ArrayBuffer.cpp] 115\n");
                this->otherParents = RecyclerNew(this->GetRecycler(), OtherParents, this->GetRecycler());
            }

            if (this->otherParents->increasedCount >= ParentsCleanupThreshold)
            {LOGMEIN("ArrayBuffer.cpp] 120\n");
                auto iter = this->otherParents->GetEditingIterator();
                while (iter.Next())
                {LOGMEIN("ArrayBuffer.cpp] 123\n");
                    if (iter.Data()->Get() == nullptr)
                    {LOGMEIN("ArrayBuffer.cpp] 125\n");
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
    {LOGMEIN("ArrayBuffer.cpp] 139\n");
        if (JavascriptOperators::IsUndefined(value))
        {LOGMEIN("ArrayBuffer.cpp] 141\n");
            return 0;
        }

        if (TaggedInt::Is(value))
        {LOGMEIN("ArrayBuffer.cpp] 146\n");
            int64 index = TaggedInt::ToInt64(value);
            if (index < 0 || index >(int64)MaxAllowedLength)
            {LOGMEIN("ArrayBuffer.cpp] 149\n");
                JavascriptError::ThrowRangeError(scriptContext, errorCode);
            }

            return  (uint32)index;
        }

        // Slower path

        double d = JavascriptConversion::ToInteger(value, scriptContext);
        if (d < 0.0 || d >(double)MaxAllowedLength)
        {LOGMEIN("ArrayBuffer.cpp] 160\n");
            JavascriptError::ThrowRangeError(scriptContext, errorCode);
        }

        if (checkSameValueZero)
        {LOGMEIN("ArrayBuffer.cpp] 165\n");
            Var integerIndex = JavascriptNumber::ToVarNoCheck(d, scriptContext);
            Var index = JavascriptNumber::ToVar(JavascriptConversion::ToLength(integerIndex, scriptContext), scriptContext);
            if (!JavascriptConversion::SameValueZero(integerIndex, index))
            {LOGMEIN("ArrayBuffer.cpp] 169\n");
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
        {LOGMEIN("ArrayBuffer.cpp] 191\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("ArrayBuffer"));
        }

        uint32 byteLength = 0;
        if (args.Info.Count > 1)
        {LOGMEIN("ArrayBuffer.cpp] 197\n");
            byteLength = ToIndex(args[1], JSERR_ArrayLengthConstructIncorrect, scriptContext, MaxArrayBufferLength);
        }

        RecyclableObject* newArr = scriptContext->GetLibrary()->CreateArrayBuffer(byteLength);
        Assert(ArrayBuffer::Is(newArr));
        if (byteLength > 0 && !ArrayBuffer::FromVar(newArr)->GetByteLength())
        {LOGMEIN("ArrayBuffer.cpp] 204\n");
            JavascriptError::ThrowRangeError(scriptContext, JSERR_FunctionArgument_Invalid);
        }
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {LOGMEIN("ArrayBuffer.cpp] 209\n");
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
        {LOGMEIN("ArrayBuffer.cpp] 229\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        ArrayBuffer* arrayBuffer = ArrayBuffer::FromVar(args[0]);
        if (arrayBuffer->IsDetached())
        {LOGMEIN("ArrayBuffer.cpp] 235\n");
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
        {LOGMEIN("ArrayBuffer.cpp] 257\n");
            arg = args[1];
        }

        // Only DataView or any TypedArray objects have [[ViewedArrayBuffer]] internal slots
        if (DataView::Is(arg) || TypedArrayBase::Is(arg))
        {LOGMEIN("ArrayBuffer.cpp] 263\n");
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
        {LOGMEIN("ArrayBuffer.cpp] 284\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        ArrayBuffer* arrayBuffer = ArrayBuffer::FromVar(args[1]);

        if (arrayBuffer->IsDetached())
        {LOGMEIN("ArrayBuffer.cpp] 291\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("ArrayBuffer.transfer"));
        }

        uint32 newBufferLength = arrayBuffer->bufferLength;
        if (args.Info.Count >= 3)
        {LOGMEIN("ArrayBuffer.cpp] 297\n");
            newBufferLength = ToIndex(args[2], JSERR_ArrayLengthConstructIncorrect, scriptContext, MaxArrayBufferLength);
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
        {LOGMEIN("ArrayBuffer.cpp] 318\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        JavascriptLibrary* library = scriptContext->GetLibrary();
        ArrayBuffer* arrayBuffer = ArrayBuffer::FromVar(args[0]);

        if (arrayBuffer->IsDetached()) // 24.1.4.3: 5. If IsDetachedBuffer(O) is true, then throw a TypeError exception.
        {LOGMEIN("ArrayBuffer.cpp] 326\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("ArrayBuffer.prototype.slice"));
        }

        int64 len = arrayBuffer->bufferLength;
        int64 start = 0, end = 0;
        int64 newLen;

        // If no start or end arguments, use the entire length
        if (args.Info.Count < 2)
        {LOGMEIN("ArrayBuffer.cpp] 336\n");
            newLen = len;
        }
        else
        {
            start = JavascriptArray::GetIndexFromVar(args[1], len, scriptContext);

            // If no end argument, use length as the end
            if (args.Info.Count < 3 || args[2] == library->GetUndefined())
            {LOGMEIN("ArrayBuffer.cpp] 345\n");
                end = len;
            }
            else
            {
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
        {LOGMEIN("ArrayBuffer.cpp] 366\n");
            Var constructorVar = JavascriptOperators::SpeciesConstructor(arrayBuffer, scriptContext->GetLibrary()->GetArrayBufferConstructor(), scriptContext);

            JavascriptFunction* constructor = JavascriptFunction::FromVar(constructorVar);

            Js::Var constructorArgs[] = {constructor, JavascriptNumber::ToVar(byteLength, scriptContext)};
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            Js::Var newVar = JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext);

            if (!ArrayBuffer::Is(newVar)) // 24.1.4.3: 19.If new does not have an [[ArrayBufferData]] internal slot throw a TypeError exception.
            {LOGMEIN("ArrayBuffer.cpp] 376\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
            }

            newBuffer = ArrayBuffer::FromVar(newVar);

            if (newBuffer->IsDetached()) // 24.1.4.3: 21. If IsDetachedBuffer(new) is true, then throw a TypeError exception.
            {LOGMEIN("ArrayBuffer.cpp] 383\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("ArrayBuffer.prototype.slice"));
            }

            if (newBuffer == arrayBuffer) // 24.1.4.3: 22. If SameValue(new, O) is true, then throw a TypeError exception.
            {LOGMEIN("ArrayBuffer.cpp] 388\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
            }

            if (newBuffer->bufferLength < byteLength) // 24.1.4.3: 23.If the value of new's [[ArrayBufferByteLength]] internal slot < newLen, then throw a TypeError exception.
            {LOGMEIN("ArrayBuffer.cpp] 393\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_ArgumentOutOfRange, _u("ArrayBuffer.prototype.slice"));
            }
        }
        else
        {
            newBuffer = library->CreateArrayBuffer(byteLength);
        }

        Assert(newBuffer);
        Assert(newBuffer->bufferLength >= byteLength);

        if (arrayBuffer->IsDetached()) // 24.1.4.3: 24. NOTE: Side-effects of the above steps may have detached O. 25. If IsDetachedBuffer(O) is true, then throw a TypeError exception.
        {LOGMEIN("ArrayBuffer.cpp] 406\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("ArrayBuffer.prototype.slice"));
        }

        // Don't bother doing memcpy if we aren't copying any elements
        if (byteLength > 0)
        {LOGMEIN("ArrayBuffer.cpp] 412\n");
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
    {LOGMEIN("ArrayBuffer.cpp] 438\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_ArrayBuffer;
    }

    template <class Allocator>
    ArrayBuffer::ArrayBuffer(uint32 length, DynamicType * type, Allocator allocator) :
        ArrayBufferBase(type), isDetached(false)
    {LOGMEIN("ArrayBuffer.cpp] 445\n");
        buffer = nullptr;
        bufferLength = 0;
        if (length > MaxArrayBufferLength)
        {LOGMEIN("ArrayBuffer.cpp] 449\n");
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_FunctionArgument_Invalid);
        }
        else if (length > 0)
        {LOGMEIN("ArrayBuffer.cpp] 453\n");
            Recycler* recycler = GetType()->GetLibrary()->GetRecycler();
            if (recycler->ReportExternalMemoryAllocation(length))
            {LOGMEIN("ArrayBuffer.cpp] 456\n");
                buffer = (BYTE*)allocator(length);
                if (buffer == nullptr)
                {LOGMEIN("ArrayBuffer.cpp] 459\n");
                    recycler->ReportExternalMemoryFree(length);
                }
            }

            if (buffer == nullptr)
            {LOGMEIN("ArrayBuffer.cpp] 465\n");
                recycler->CollectNow<CollectOnTypedArrayAllocation>();

                if (recycler->ReportExternalMemoryAllocation(length))
                {LOGMEIN("ArrayBuffer.cpp] 469\n");
                    buffer = (BYTE*)allocator(length);
                    if (buffer == nullptr)
                    {LOGMEIN("ArrayBuffer.cpp] 472\n");
                        recycler->ReportExternalMemoryFailure(length);
                    }
                }
                else
                {
                    JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                }
            }

            if (buffer != nullptr)
            {LOGMEIN("ArrayBuffer.cpp] 483\n");
                bufferLength = length;
                ZeroMemory(buffer, bufferLength);
            }
        }
    }

    ArrayBuffer::ArrayBuffer(byte* buffer, uint32 length, DynamicType * type) :
        buffer(buffer), bufferLength(length), ArrayBufferBase(type), isDetached(false)
    {LOGMEIN("ArrayBuffer.cpp] 492\n");
        if (length > MaxArrayBufferLength)
        {LOGMEIN("ArrayBuffer.cpp] 494\n");
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_FunctionArgument_Invalid);
        }
    }

    BOOL ArrayBuffer::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ArrayBuffer.cpp] 500\n");
        stringBuilder->AppendCppLiteral(_u("Object, (ArrayBuffer)"));
        return TRUE;
    }

    BOOL ArrayBuffer::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ArrayBuffer.cpp] 506\n");
        stringBuilder->AppendCppLiteral(_u("[object ArrayBuffer]"));
        return TRUE;
    }

#if ENABLE_TTD
    void ArrayBufferParent::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("ArrayBuffer.cpp] 513\n");
        extractor->MarkVisitVar(this->arrayBuffer);
    }

    void ArrayBufferParent::ProcessCorePaths()
    {LOGMEIN("ArrayBuffer.cpp] 518\n");
        this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->arrayBuffer, _u("!buffer"));
    }
#endif

    JavascriptArrayBuffer::JavascriptArrayBuffer(uint32 length, DynamicType * type) :
        ArrayBuffer(length, type, IsValidVirtualBufferLength(length) ? AsmJsVirtualAllocator : malloc)
    {LOGMEIN("ArrayBuffer.cpp] 525\n");
    }
    JavascriptArrayBuffer::JavascriptArrayBuffer(byte* buffer, uint32 length, DynamicType * type) :
        ArrayBuffer(buffer, length, type)
    {LOGMEIN("ArrayBuffer.cpp] 529\n");
    }

    JavascriptArrayBuffer::JavascriptArrayBuffer(DynamicType * type) : ArrayBuffer(0, type, malloc)
    {LOGMEIN("ArrayBuffer.cpp] 533\n");
    }

    JavascriptArrayBuffer* JavascriptArrayBuffer::Create(uint32 length, DynamicType * type)
    {LOGMEIN("ArrayBuffer.cpp] 537\n");
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        JavascriptArrayBuffer* result = RecyclerNewFinalized(recycler, JavascriptArrayBuffer, length, type);
        Assert(result);
        recycler->AddExternalMemoryUsage(length);
        return result;
    }

    JavascriptArrayBuffer* JavascriptArrayBuffer::Create(byte* buffer, uint32 length, DynamicType * type)
    {LOGMEIN("ArrayBuffer.cpp] 546\n");
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        JavascriptArrayBuffer* result = RecyclerNewFinalized(recycler, JavascriptArrayBuffer, buffer, length, type);
        Assert(result);
        recycler->AddExternalMemoryUsage(length);
        return result;
    }

    ArrayBufferDetachedStateBase* JavascriptArrayBuffer::CreateDetachedState(BYTE* buffer, uint32 bufferLength)
    {LOGMEIN("ArrayBuffer.cpp] 555\n");
#if ENABLE_FAST_ARRAYBUFFER
        if (IsValidVirtualBufferLength(bufferLength))
        {LOGMEIN("ArrayBuffer.cpp] 558\n");
            return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, FreeMemAlloc, ArrayBufferAllocationType::MemAlloc);
        }
        else
        {
            return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, free, ArrayBufferAllocationType::Heap);
        }
#else
        return HeapNew(ArrayBufferDetachedState<FreeFn>, buffer, bufferLength, free, ArrayBufferAllocationType::Heap);
#endif
    }

    bool JavascriptArrayBuffer::IsValidAsmJsBufferLengthAlgo(uint length, bool forceCheck)
    {LOGMEIN("ArrayBuffer.cpp] 571\n");
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
    {LOGMEIN("ArrayBuffer.cpp] 593\n");
        return IsValidAsmJsBufferLengthAlgo(length, forceCheck);
    }

    bool JavascriptArrayBuffer::IsValidVirtualBufferLength(uint length)
    {LOGMEIN("ArrayBuffer.cpp] 598\n");
#if ENABLE_FAST_ARRAYBUFFER
        return !PHASE_OFF1(Js::TypedArrayVirtualPhase) && IsValidAsmJsBufferLengthAlgo(length, true);
#else
        return false;
#endif
    }

    void JavascriptArrayBuffer::Finalize(bool isShutdown)
    {LOGMEIN("ArrayBuffer.cpp] 607\n");
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
        {LOGMEIN("ArrayBuffer.cpp] 620\n");
            FreeMemAlloc(buffer);
        }
        else
        {
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
    {LOGMEIN("ArrayBuffer.cpp] 638\n");
        /* See JavascriptArrayBuffer::Finalize */
    }

    // Copy memory from src to dst, truncate if dst smaller, zero extra memory
    // if dst larger
    static void MemCpyZero(__bcount(dstSize) BYTE* dst, size_t dstSize,
        __in_bcount(count) const BYTE* src, size_t count)
    {
        js_memcpy_s(dst, dstSize, src, min(dstSize, count));
        if (dstSize > count)
        {LOGMEIN("ArrayBuffer.cpp] 649\n");
            ZeroMemory(dst + count, dstSize - count);
        }
    }

    // Same as realloc but zero newly allocated portion if newSize > oldSize
    static BYTE* ReallocZero(BYTE* ptr, size_t oldSize, size_t newSize)
    {LOGMEIN("ArrayBuffer.cpp] 656\n");
        BYTE* ptrNew = (BYTE*)realloc(ptr, newSize);
        if (ptrNew && newSize > oldSize)
        {LOGMEIN("ArrayBuffer.cpp] 659\n");
            ZeroMemory(ptrNew + oldSize, newSize - oldSize);
        }
        return ptrNew;
    }

    ArrayBuffer * JavascriptArrayBuffer::TransferInternal(uint32 newBufferLength)
    {LOGMEIN("ArrayBuffer.cpp] 666\n");
        ArrayBuffer* newArrayBuffer;
        Recycler* recycler = this->GetRecycler();

        if (this->bufferLength > 0)
        {LOGMEIN("ArrayBuffer.cpp] 671\n");
            ReportDifferentialAllocation(newBufferLength);
        }

        if (newBufferLength == 0 || this->bufferLength == 0)
        {LOGMEIN("ArrayBuffer.cpp] 676\n");
            newArrayBuffer = GetLibrary()->CreateArrayBuffer(newBufferLength);
            if (newBufferLength > 0 && !newArrayBuffer->GetByteLength())
            {LOGMEIN("ArrayBuffer.cpp] 679\n");
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }
        }
        else
        {
            BYTE * newBuffer = nullptr;
            if (IsValidVirtualBufferLength(this->bufferLength))
            {LOGMEIN("ArrayBuffer.cpp] 687\n");
                if (IsValidVirtualBufferLength(newBufferLength))
                {LOGMEIN("ArrayBuffer.cpp] 689\n");
                    // we are transferring between an optimized buffer using a length that can be optimized
                    if (newBufferLength < this->bufferLength)
                    {LOGMEIN("ArrayBuffer.cpp] 692\n");
#pragma prefast(suppress:6250, "Calling 'VirtualFree' without the MEM_RELEASE flag might free memory but not address descriptors (VADs).")
                        VirtualFree(this->buffer + newBufferLength, this->bufferLength - newBufferLength, MEM_DECOMMIT);
                    }
                    else if (newBufferLength > this->bufferLength)
                    {LOGMEIN("ArrayBuffer.cpp] 697\n");
                        LPVOID newMem = VirtualAlloc(this->buffer + this->bufferLength, newBufferLength - this->bufferLength, MEM_COMMIT, PAGE_READWRITE);
                        if (!newMem)
                        {LOGMEIN("ArrayBuffer.cpp] 700\n");
                            recycler->ReportExternalMemoryFailure(newBufferLength - this->bufferLength);
                            JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                        }
                    }
                    newBuffer = this->buffer;
                }
                else
                {
                    // we are transferring from an optimized buffer, but the new length isn't compatible, so start over and copy to new memory
                    newBuffer = (BYTE*)malloc(newBufferLength);
                    if (!newBuffer)
                    {LOGMEIN("ArrayBuffer.cpp] 712\n");
                        recycler->ReportExternalMemoryFailure(newBufferLength - this->bufferLength);
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                    MemCpyZero(newBuffer, newBufferLength, this->buffer, this->bufferLength);
                }
            }
            else
            {
                if (IsValidVirtualBufferLength(newBufferLength))
                {LOGMEIN("ArrayBuffer.cpp] 722\n");
                    // we are transferring from an unoptimized buffer, but new length can be optimized, so move to that
                    newBuffer = (BYTE*)AsmJsVirtualAllocator(newBufferLength);
                    if (!newBuffer)
                    {LOGMEIN("ArrayBuffer.cpp] 726\n");
                        recycler->ReportExternalMemoryFailure(newBufferLength - this->bufferLength);
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                    MemCpyZero(newBuffer, newBufferLength, this->buffer, this->bufferLength);
                }
                else if (newBufferLength != this->bufferLength)
                {LOGMEIN("ArrayBuffer.cpp] 733\n");
                    // both sides will just be regular ArrayBuffer, so realloc
                    newBuffer = ReallocZero(this->buffer, this->bufferLength, newBufferLength);
                    if (!newBuffer)
                    {LOGMEIN("ArrayBuffer.cpp] 737\n");
                        recycler->ReportExternalMemoryFailure(newBufferLength - this->bufferLength);
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                }
                else
                {
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
    {LOGMEIN("ArrayBuffer.cpp] 757\n");
        Recycler* recycler = this->GetRecycler();

        // Report differential external memory allocation.
        // If current bufferLength == 0, new ArrayBuffer creation records the allocation
        // so no need to do it here.
        if (newBufferLength != this->bufferLength)
        {LOGMEIN("ArrayBuffer.cpp] 764\n");
            // Expanding buffer
            if (newBufferLength > this->bufferLength)
            {LOGMEIN("ArrayBuffer.cpp] 767\n");
                if (!recycler->ReportExternalMemoryAllocation(newBufferLength - this->bufferLength))
                {LOGMEIN("ArrayBuffer.cpp] 769\n");
                    recycler->CollectNow<CollectOnTypedArrayAllocation>();
                    if (!recycler->ReportExternalMemoryAllocation(newBufferLength - this->bufferLength))
                    {LOGMEIN("ArrayBuffer.cpp] 772\n");
                        JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                    }
                }
            }
            // Contracting buffer
            else
            {
                recycler->ReportExternalMemoryFree(this->bufferLength - newBufferLength);
            }
        }
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptArrayBuffer::GetSnapTag_TTD() const
    {LOGMEIN("ArrayBuffer.cpp] 787\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapArrayBufferObject;
    }

    void JavascriptArrayBuffer::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ArrayBuffer.cpp] 792\n");
        TTD::NSSnapObjects::SnapArrayBufferInfo* sabi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapArrayBufferInfo>();

        sabi->Length = this->GetByteLength();
        if (sabi->Length == 0)
        {LOGMEIN("ArrayBuffer.cpp] 797\n");
            sabi->Buff = nullptr;
        }
        else
        {
            sabi->Buff = alloc.SlabAllocateArray<byte>(sabi->Length);
            memcpy(sabi->Buff, this->GetBuffer(), sabi->Length);
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayBufferInfo*, TTD::NSSnapObjects::SnapObjectType::SnapArrayBufferObject>(objData, sabi);
    }
#endif



    WebAssemblyArrayBuffer::WebAssemblyArrayBuffer(uint32 length, DynamicType * type) :
#ifndef ENABLE_FAST_ARRAYBUFFER
        // Treat as a normal JavascriptArrayBuffer
        JavascriptArrayBuffer(length, type) {LOGMEIN("ArrayBuffer.cpp] 815\n");}
#else
        JavascriptArrayBuffer(length, type, WasmVirtualAllocator)
    {LOGMEIN("ArrayBuffer.cpp] 818\n");
        // Make sure we always have a buffer even if the length is 0
        if (buffer == nullptr)
        {LOGMEIN("ArrayBuffer.cpp] 821\n");
            // We want to allocate an empty buffer using virtual memory
            Assert(length == 0);
            buffer = (BYTE*)WasmVirtualAllocator(0);
            if (buffer == nullptr)
            {LOGMEIN("ArrayBuffer.cpp] 826\n");
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }
        }
    }
#endif

    WebAssemblyArrayBuffer::WebAssemblyArrayBuffer(byte* buffer, uint32 length, DynamicType * type):
        JavascriptArrayBuffer(buffer, length, type)
    {LOGMEIN("ArrayBuffer.cpp] 835\n");

    }

    WebAssemblyArrayBuffer* WebAssemblyArrayBuffer::Create(byte* buffer, uint32 length, DynamicType * type)
    {LOGMEIN("ArrayBuffer.cpp] 840\n");
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        WebAssemblyArrayBuffer* result;
        if (buffer)
        {LOGMEIN("ArrayBuffer.cpp] 844\n");
            result = RecyclerNewFinalized(recycler, WebAssemblyArrayBuffer, buffer, length, type);
        }
        else
        {
            result = RecyclerNewFinalized(recycler, WebAssemblyArrayBuffer, length, type);
        }
        Assert(result);
        recycler->AddExternalMemoryUsage(length);
        return result;
    }

    bool WebAssemblyArrayBuffer::IsValidVirtualBufferLength(uint length)
    {LOGMEIN("ArrayBuffer.cpp] 857\n");
#if ENABLE_FAST_ARRAYBUFFER
        return true;
#else
        return false;
#endif
    }

    ArrayBuffer * WebAssemblyArrayBuffer::TransferInternal(uint32 newBufferLength)
    {LOGMEIN("ArrayBuffer.cpp] 866\n");
#if ENABLE_FAST_ARRAYBUFFER
        ReportDifferentialAllocation(newBufferLength);
        Assert(this->buffer);

        AssertMsg(newBufferLength > this->bufferLength, "The only supported scenario in WebAssembly is to grow the memory");
        if (newBufferLength > this->bufferLength)
        {LOGMEIN("ArrayBuffer.cpp] 873\n");
            LPVOID newMem = VirtualAlloc(this->buffer + this->bufferLength, newBufferLength - this->bufferLength, MEM_COMMIT, PAGE_READWRITE);
            if (!newMem)
            {LOGMEIN("ArrayBuffer.cpp] 876\n");
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
    {LOGMEIN("ArrayBuffer.cpp] 895\n");
    }

    ProjectionArrayBuffer::ProjectionArrayBuffer(byte* buffer, uint32 length, DynamicType * type) :
        ArrayBuffer(buffer, length, type)
    {LOGMEIN("ArrayBuffer.cpp] 900\n");
    }

    ProjectionArrayBuffer* ProjectionArrayBuffer::Create(uint32 length, DynamicType * type)
    {LOGMEIN("ArrayBuffer.cpp] 904\n");
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        recycler->AddExternalMemoryUsage(length);
        return RecyclerNewFinalized(recycler, ProjectionArrayBuffer, length, type);
    }

    ProjectionArrayBuffer* ProjectionArrayBuffer::Create(byte* buffer, uint32 length, DynamicType * type)
    {LOGMEIN("ArrayBuffer.cpp] 911\n");
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        // This is user passed [in] buffer, user should AddExternalMemoryUsage before calling jscript, but
        // I don't see we ask everyone to do this. Let's add the memory pressure here as well.
        recycler->AddExternalMemoryUsage(length);
        return RecyclerNewFinalized(recycler, ProjectionArrayBuffer, buffer, length, type);
    }

    void ProjectionArrayBuffer::Dispose(bool isShutdown)
    {LOGMEIN("ArrayBuffer.cpp] 920\n");
        CoTaskMemFree(buffer);
    }

    ArrayBuffer * ProjectionArrayBuffer::TransferInternal(uint32 newBufferLength)
    {LOGMEIN("ArrayBuffer.cpp] 925\n");
        ArrayBuffer* newArrayBuffer;
        if (newBufferLength == 0 || this->bufferLength == 0)
        {LOGMEIN("ArrayBuffer.cpp] 928\n");
            newArrayBuffer = GetLibrary()->CreateProjectionArraybuffer(newBufferLength);
        }
        else
        {
            BYTE * newBuffer = (BYTE*)CoTaskMemRealloc(this->buffer, newBufferLength);
            if (!newBuffer)
            {LOGMEIN("ArrayBuffer.cpp] 935\n");
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
    {LOGMEIN("ArrayBuffer.cpp] 949\n");
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ExternalArrayBuffer::GetSnapTag_TTD() const
    {LOGMEIN("ArrayBuffer.cpp] 954\n");
        //We re-map ExternalArrayBuffers to regular buffers since the 'real' host will be gone when we replay
        return TTD::NSSnapObjects::SnapObjectType::SnapArrayBufferObject;
    }

    void ExternalArrayBuffer::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ArrayBuffer.cpp] 960\n");
        TTD::NSSnapObjects::SnapArrayBufferInfo* sabi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapArrayBufferInfo>();

        sabi->Length = this->GetByteLength();
        if(sabi->Length == 0)
        {LOGMEIN("ArrayBuffer.cpp] 965\n");
            sabi->Buff = nullptr;
        }
        else
        {
            sabi->Buff = alloc.SlabAllocateArray<byte>(sabi->Length);
            memcpy(sabi->Buff, this->GetBuffer(), sabi->Length);
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayBufferInfo*, TTD::NSSnapObjects::SnapObjectType::SnapArrayBufferObject>(objData, sabi);
    }
#endif
}
