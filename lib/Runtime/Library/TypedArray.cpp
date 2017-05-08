//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Implementation for typed arrays based on ArrayBuffer.
// There is one nested ArrayBuffer for each typed array. Multiple typed array
// can share the same array buffer.
//----------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#define INSTANTIATE_BUILT_IN_ENTRYPOINTS(typeName) \
    template Var typeName::NewInstance(RecyclableObject* function, CallInfo callInfo, ...); \
    template Var typeName::EntrySet(RecyclableObject* function, CallInfo callInfo, ...);

namespace Js
{
    // explicitly instantiate these function for the built in entry point FunctionInfo
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Int8Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint8Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint8ClampedArray)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Int16Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint16Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Int32Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint32Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Float32Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Float64Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Int64Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(Uint64Array)
    INSTANTIATE_BUILT_IN_ENTRYPOINTS(BoolArray)

    template<> BOOL Uint8ClampedArray::Is(Var aValue)
    {TRACE_IT(63785);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint8ClampedArray &&
               ( VirtualTableInfo<Uint8ClampedArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint8ClampedArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint8Array::Is(Var aValue)
    {TRACE_IT(63786);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint8Array &&
              ( VirtualTableInfo<Uint8Array>::HasVirtualTable(aValue) ||
                VirtualTableInfo<CrossSiteObject<Uint8Array>>::HasVirtualTable(aValue)
              );
    }

    template<> BOOL Int8Array::Is(Var aValue)
    {TRACE_IT(63787);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int8Array &&
               ( VirtualTableInfo<Int8Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int8Array>>::HasVirtualTable(aValue)
               );
    }


    template<> BOOL Int16Array::Is(Var aValue)
    {TRACE_IT(63788);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int16Array &&
               ( VirtualTableInfo<Int16Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int16Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint16Array::Is(Var aValue)
    {TRACE_IT(63789);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint16Array &&
              ( VirtualTableInfo<Uint16Array>::HasVirtualTable(aValue) ||
                VirtualTableInfo<CrossSiteObject<Uint16Array>>::HasVirtualTable(aValue)
              );
    }

    template<> BOOL Int32Array::Is(Var aValue)
    {TRACE_IT(63790);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int32Array &&
               ( VirtualTableInfo<Int32Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int32Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint32Array::Is(Var aValue)
    {TRACE_IT(63791);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint32Array &&
               ( VirtualTableInfo<Uint32Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint32Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Float32Array::Is(Var aValue)
    {TRACE_IT(63792);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Float32Array &&
               ( VirtualTableInfo<Float32Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Float32Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Float64Array::Is(Var aValue)
    {TRACE_IT(63793);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Float64Array &&
               ( VirtualTableInfo<Float64Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Float64Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Int64Array::Is(Var aValue)
    {TRACE_IT(63794);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int64Array &&
               ( VirtualTableInfo<Int64Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int64Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint64Array::Is(Var aValue)
    {TRACE_IT(63795);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint64Array &&
               ( VirtualTableInfo<Uint64Array>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint64Array>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL BoolArray::Is(Var aValue)
    {TRACE_IT(63796);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_BoolArray &&
               ( VirtualTableInfo<BoolArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<BoolArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint8ClampedVirtualArray::Is(Var aValue)
    {TRACE_IT(63797);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint8ClampedArray &&
               ( VirtualTableInfo<Uint8ClampedVirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint8ClampedVirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint8VirtualArray::Is(Var aValue)
    {TRACE_IT(63798);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint8Array &&
               ( VirtualTableInfo<Uint8VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint8VirtualArray>>::HasVirtualTable(aValue)
               );
    }


    template<> BOOL Int8VirtualArray::Is(Var aValue)
    {TRACE_IT(63799);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int8Array &&
               ( VirtualTableInfo<Int8VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int8VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Int16VirtualArray::Is(Var aValue)
    {TRACE_IT(63800);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int16Array &&
               ( VirtualTableInfo<Int16VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int16VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint16VirtualArray::Is(Var aValue)
    {TRACE_IT(63801);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint16Array &&
               ( VirtualTableInfo<Uint16VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint16VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Int32VirtualArray::Is(Var aValue)
    {TRACE_IT(63802);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Int32Array &&
               ( VirtualTableInfo<Int32VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Int32VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Uint32VirtualArray::Is(Var aValue)
    {TRACE_IT(63803);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Uint32Array &&
               ( VirtualTableInfo<Uint32VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Uint32VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Float32VirtualArray::Is(Var aValue)
    {TRACE_IT(63804);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Float32Array &&
               ( VirtualTableInfo<Float32VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Float32VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template<> BOOL Float64VirtualArray::Is(Var aValue)
    {TRACE_IT(63805);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Float64Array &&
               ( VirtualTableInfo<Float64VirtualArray>::HasVirtualTable(aValue) ||
                 VirtualTableInfo<CrossSiteObject<Float64VirtualArray>>::HasVirtualTable(aValue)
               );
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    TypedArray<TypeName, clamped, virtualAllocated>* TypedArray<TypeName, clamped, virtualAllocated>::FromVar(Var aValue)
    {TRACE_IT(63806);
        AssertMsg(TypedArray::Is(aValue), "invalid TypedArray");
        return static_cast<TypedArray<TypeName, clamped, virtualAllocated>*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint8ClampedArray* Uint8ClampedArray::FromVar(Var aValue)
    {TRACE_IT(63807);
        AssertMsg(Uint8ClampedArray::Is(aValue), "invalid Uint8ClampedArray");
        return static_cast<Uint8ClampedArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint8Array* Uint8Array::FromVar(Var aValue)
    {TRACE_IT(63808);
        AssertMsg(Uint8Array::Is(aValue), "invalid Uint8Array");
        return static_cast<Uint8Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int8Array* Int8Array::FromVar(Var aValue)
    {TRACE_IT(63809);
        AssertMsg(Int8Array::Is(aValue), "invalid Int8Array");
        return static_cast<Int8Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int16Array* Int16Array::FromVar(Var aValue)
    {TRACE_IT(63810);
        AssertMsg(Int16Array::Is(aValue), "invalid Int16Array");
        return static_cast<Int16Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint16Array* Uint16Array::FromVar(Var aValue)
    {TRACE_IT(63811);
        AssertMsg(Uint16Array::Is(aValue), "invalid Uint16Array");
        return static_cast<Uint16Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int32Array* Int32Array::FromVar(Var aValue)
    {TRACE_IT(63812);
        AssertMsg(Int32Array::Is(aValue), "invalid Int32Array");
        return static_cast<Int32Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint32Array* Uint32Array::FromVar(Var aValue)
    {TRACE_IT(63813);
        AssertMsg(Uint32Array::Is(aValue), "invalid Uint32Array");
        return static_cast<Uint32Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Float32Array* Float32Array::FromVar(Var aValue)
    {TRACE_IT(63814);
        AssertMsg(Float32Array::Is(aValue), "invalid Float32Array");
        return static_cast<Float32Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Float64Array* Float64Array::FromVar(Var aValue)
    {TRACE_IT(63815);
        AssertMsg(Float64Array::Is(aValue), "invalid Float64Array");
        return static_cast<Float64Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int64Array* Int64Array::FromVar(Var aValue)
    {TRACE_IT(63816);
        AssertMsg(Int64Array::Is(aValue), "invalid Int64Array");
        return static_cast<Int64Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint64Array* Uint64Array::FromVar(Var aValue)
    {TRACE_IT(63817);
        AssertMsg(Uint64Array::Is(aValue), "invalid Uint64Array");
        return static_cast<Uint64Array*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int8VirtualArray* Int8VirtualArray::FromVar(Var aValue)
    {TRACE_IT(63818);
        AssertMsg(Int8VirtualArray::Is(aValue), "invalid Int8Array");
        return static_cast<Int8VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint8ClampedVirtualArray* Uint8ClampedVirtualArray::FromVar(Var aValue)
    {TRACE_IT(63819);
        AssertMsg(Uint8ClampedVirtualArray::Is(aValue), "invalid Uint8ClampedArray");
        return static_cast<Uint8ClampedVirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint8VirtualArray* Uint8VirtualArray::FromVar(Var aValue)
    {TRACE_IT(63820);
        AssertMsg(Uint8VirtualArray::Is(aValue), "invalid Uint8Array");
        return static_cast<Uint8VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int16VirtualArray* Int16VirtualArray::FromVar(Var aValue)
    {TRACE_IT(63821);
        AssertMsg(Int16VirtualArray::Is(aValue), "invalid Int16Array");
        return static_cast<Int16VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint16VirtualArray* Uint16VirtualArray::FromVar(Var aValue)
    {TRACE_IT(63822);
        AssertMsg(Uint16VirtualArray::Is(aValue), "invalid Uint16Array");
        return static_cast<Uint16VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Int32VirtualArray* Int32VirtualArray::FromVar(Var aValue)
    {TRACE_IT(63823);
        AssertMsg(Int32VirtualArray::Is(aValue), "invalid Int32Array");
        return static_cast<Int32VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Uint32VirtualArray* Uint32VirtualArray::FromVar(Var aValue)
    {TRACE_IT(63824);
        AssertMsg(Uint32VirtualArray::Is(aValue), "invalid Uint32Array");
        return static_cast<Uint32VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Float32VirtualArray* Float32VirtualArray::FromVar(Var aValue)
    {TRACE_IT(63825);
        AssertMsg(Float32VirtualArray::Is(aValue), "invalid Float32Array");
        return static_cast<Float32VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> Float64VirtualArray* Float64VirtualArray::FromVar(Var aValue)
    {TRACE_IT(63826);
        AssertMsg(Float64VirtualArray::Is(aValue), "invalid Float64Array");
        return static_cast<Float64VirtualArray*>(RecyclableObject::FromVar(aValue));
    }

    template<> BoolArray* BoolArray::FromVar(Var aValue)
    {TRACE_IT(63827);
        AssertMsg(BoolArray::Is(aValue), "invalid BoolArray");
        return static_cast<BoolArray*>(RecyclableObject::FromVar(aValue));
    }

    TypedArrayBase::TypedArrayBase(ArrayBufferBase* arrayBuffer, uint32 offSet, uint mappedLength, uint elementSize, DynamicType* type) :
        ArrayBufferParent(type, mappedLength, arrayBuffer),
        byteOffset(offSet),
        BYTES_PER_ELEMENT(elementSize)
    {TRACE_IT(63828);
    }

    inline JsUtil::List<Var, ArenaAllocator>* IteratorToList(RecyclableObject *iterator, ScriptContext *scriptContext, ArenaAllocator *alloc)
    {TRACE_IT(63829);
        Assert(iterator != nullptr);

        Var nextValue;
        JsUtil::List<Var, ArenaAllocator>* retList = JsUtil::List<Var, ArenaAllocator>::New(alloc);

        while (JavascriptOperators::IteratorStepAndValue(iterator, scriptContext, &nextValue))
        {TRACE_IT(63830);
            retList->Add(nextValue);
        }

        return retList;
    }

    Var TypedArrayBase::CreateNewInstanceFromIterator(RecyclableObject *iterator, ScriptContext *scriptContext, uint32 elementSize, PFNCreateTypedArray pfnCreateTypedArray)
    {TRACE_IT(63831);
        TypedArrayBase *newArr = nullptr;

        DECLARE_TEMP_GUEST_ALLOCATOR(tempAlloc);

        ACQUIRE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext, _u("Runtime"));
        {TRACE_IT(63832);
            JsUtil::List<Var, ArenaAllocator>* tempList = IteratorToList(iterator, scriptContext, tempAlloc);

            uint32 len = tempList->Count();
            uint32 byteLen;

            if (UInt32Math::Mul(len, elementSize, &byteLen))
            {TRACE_IT(63833);
                JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidTypedArrayLength);
            }

            ArrayBufferBase *arrayBuffer = scriptContext->GetLibrary()->CreateArrayBuffer(byteLen);
            newArr = static_cast<TypedArrayBase*>(pfnCreateTypedArray(arrayBuffer, 0, len, scriptContext->GetLibrary()));

            for (uint32 k = 0; k < len; k++)
            {TRACE_IT(63834);
                Var kValue = tempList->Item(k);
                newArr->SetItem(k, kValue);
            }
        }
        RELEASE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext);

        return newArr;
    }

    Var TypedArrayBase::CreateNewInstance(Arguments& args, ScriptContext* scriptContext, uint32 elementSize, PFNCreateTypedArray pfnCreateTypedArray)
    {TRACE_IT(63835);
        uint32 byteLength = 0;
        int32 offset = 0;
        int32 mappedLength = -1;
        uint32 elementCount = 0;
        ArrayBufferBase* arrayBuffer = nullptr;
        TypedArrayBase* typedArraySource = nullptr;
        RecyclableObject* jsArraySource = nullptr;
        bool fromExternalObject = false;

        // Handle first argument - see if that is ArrayBuffer/SharedArrayBuffer
        if (args.Info.Count > 1)
        {TRACE_IT(63836);
            Var firstArgument = args[1];
            if (!Js::JavascriptOperators::IsObject(firstArgument))
            {TRACE_IT(63837);
                elementCount = ArrayBuffer::ToIndex(firstArgument, JSERR_InvalidTypedArrayLength, scriptContext, ArrayBuffer::MaxArrayBufferLength / elementSize);
            }
            else
            {TRACE_IT(63838);
                if (TypedArrayBase::Is(firstArgument))
                {TRACE_IT(63839);
                    // Constructor(TypedArray array)
                    typedArraySource = static_cast<TypedArrayBase*>(firstArgument);
                    if (typedArraySource->IsDetachedBuffer())
                    {TRACE_IT(63840);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("[TypedArray]"));
                    }

                    elementCount = typedArraySource->GetLength();
                    if (elementCount >= ArrayBuffer::MaxArrayBufferLength / elementSize)
                    {TRACE_IT(63841);
                        JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidTypedArrayLength);
                    }
                }
                else if (ArrayBufferBase::Is(firstArgument))
                {TRACE_IT(63842);
                    // Constructor(ArrayBuffer buffer,
                    //  optional uint32 byteOffset, optional uint32 length)
                    arrayBuffer = ArrayBufferBase::FromVar(firstArgument);
                    if (arrayBuffer->IsDetached())
                    {TRACE_IT(63843);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("[TypedArray]"));
                    }
                }
                else
                {TRACE_IT(63844);
                    // Use GetIteratorFunction instead of GetIterator to check if it is the built-in array iterator
                    RecyclableObject* iteratorFn = JavascriptOperators::GetIteratorFunction(firstArgument, scriptContext, true /* optional */);
                    if (iteratorFn != nullptr &&
                        (iteratorFn != scriptContext->GetLibrary()->GetArrayPrototypeValuesFunction() ||
                            !JavascriptArray::Is(firstArgument) || JavascriptLibrary::ArrayIteratorPrototypeHasUserDefinedNext(scriptContext)))
                    {TRACE_IT(63845);
                        Var iterator = CALL_FUNCTION(iteratorFn, CallInfo(Js::CallFlags_Value, 1), RecyclableObject::FromVar(firstArgument));

                        if (!JavascriptOperators::IsObject(iterator))
                        {TRACE_IT(63846);
                            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                        }
                        return CreateNewInstanceFromIterator(RecyclableObject::FromVar(iterator), scriptContext, elementSize, pfnCreateTypedArray);
                    }

                    if (!JavascriptConversion::ToObject(firstArgument, scriptContext, &jsArraySource))
                    {TRACE_IT(63847);
                        // REVIEW: unclear why this JavascriptConversion::ToObject() call is being made.
                        // It ends up calling RecyclableObject::ToObject which at least Proxy objects can
                        // hit with non-trivial behavior.
                        Js::Throw::FatalInternalError();
                    }

                    ArrayBuffer *temp = nullptr;
                    HRESULT hr = scriptContext->GetHostScriptContext()->ArrayBufferFromExternalObject(jsArraySource, &temp);
                    arrayBuffer = static_cast<ArrayBufferBase *> (temp);
                    switch (hr)
                    {
                    case S_OK:
                        // We found an IBuffer
                        fromExternalObject = true;
                        OUTPUT_TRACE(TypedArrayPhase, _u("Projection ArrayBuffer query succeeded with HR=0x%08X\n"), hr);
                        // We have an ArrayBuffer now, so we can skip all the object probing.
                        break;

                    case S_FALSE:
                        // We didn't find an IBuffer - fall through
                        OUTPUT_TRACE(TypedArrayPhase, _u("Projection ArrayBuffer query aborted safely with HR=0x%08X (non-handled type)\n"), hr);
                        break;

                    default:
                        // Any FAILURE HRESULT or unexpected HRESULT
                        OUTPUT_TRACE(TypedArrayPhase, _u("Projection ArrayBuffer query failed with HR=0x%08X\n"), hr);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidTypedArray_Constructor);
                        break;
                    }

                    if (!fromExternalObject)
                    {TRACE_IT(63848);
                        Var lengthVar = JavascriptOperators::OP_GetLength(jsArraySource, scriptContext);
                        elementCount = ArrayBuffer::ToIndex(lengthVar, JSERR_InvalidTypedArrayLength, scriptContext, ArrayBuffer::MaxArrayBufferLength / elementSize);
                    }
                }
            }
        }

        // If we got an ArrayBuffer, we can continue to process arguments.
        if (arrayBuffer != nullptr)
        {TRACE_IT(63849);
            byteLength = arrayBuffer->GetByteLength();
            if (args.Info.Count > 2)
            {TRACE_IT(63850);
                offset = ArrayBuffer::ToIndex(args[2], JSERR_InvalidTypedArrayLength, scriptContext, byteLength, false);

                // we can start the mapping from the end of the incoming buffer, but with a map of 0 length.
                // User can't really do anything useful with the typed array but apparently this is allowed.
                if ((offset % elementSize) != 0)
                {TRACE_IT(63851);
                    JavascriptError::ThrowRangeError(
                        scriptContext, JSERR_InvalidTypedArrayLength);
                }
            }

            if (args.Info.Count > 3 && !JavascriptOperators::IsUndefinedObject(args[3]))
            {TRACE_IT(63852);
                mappedLength = ArrayBuffer::ToIndex(args[3], JSERR_InvalidTypedArrayLength, scriptContext, (byteLength - offset) / elementSize, false);
            }
            else
            {TRACE_IT(63853);
                if ((byteLength - offset) % elementSize != 0)
                {TRACE_IT(63854);
                    JavascriptError::ThrowRangeError(
                        scriptContext, JSERR_InvalidTypedArrayLength);
                }
                mappedLength = (byteLength - offset)/elementSize;
            }
        }
        else
        {TRACE_IT(63855);
            // Null arrayBuffer - could be new constructor or copy constructor.
            byteLength = elementCount * elementSize;
            arrayBuffer = scriptContext->GetLibrary()->CreateArrayBuffer(byteLength);
        }

        if (arrayBuffer->IsDetached())
        {TRACE_IT(63856);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("[TypedArray]"));
        }

        if (mappedLength == -1)
        {TRACE_IT(63857);
            mappedLength = (byteLength - offset)/elementSize;
        }

        // Create and set the array based on the source.
        TypedArrayBase* newArray  = static_cast<TypedArrayBase*>(pfnCreateTypedArray(arrayBuffer, offset, mappedLength, scriptContext->GetLibrary()));
        if (fromExternalObject)
        {
            // No need to copy externally provided buffer
            OUTPUT_TRACE(TypedArrayPhase, _u("Created a TypedArray from an external buffer source\n"));
        }
        else if (typedArraySource)
        {TRACE_IT(63858);
            newArray->Set(typedArraySource, offset);
            OUTPUT_TRACE(TypedArrayPhase, _u("Created a TypedArray from a typed array source\n"));
        }
        else if (jsArraySource)
        {TRACE_IT(63859);
            newArray->SetObjectNoDetachCheck(jsArraySource, newArray->GetLength(), offset);
            OUTPUT_TRACE(TypedArrayPhase, _u("Created a TypedArray from a JavaScript array source\n"));
        }

        return newArray;
    }

    Var TypedArrayBase::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        function->GetScriptContext()->GetThreadContext()->ProbeStack(Js::Constants::MinStackDefault, function->GetScriptContext());

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

        JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidTypedArray_Constructor);
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    Var TypedArray<TypeName, clamped, virtualAllocated>::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        function->GetScriptContext()->GetThreadContext()->ProbeStack(Js::Constants::MinStackDefault, function->GetScriptContext());

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

        if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
        {TRACE_IT(63860);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("[TypedArray]"));
        }

        Var object = TypedArrayBase::CreateNewInstance(args, scriptContext, sizeof(TypeName), TypedArray<TypeName, clamped, virtualAllocated>::Create);

#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {TRACE_IT(63861);
            object = Js::JavascriptProxy::AutoProxyWrapper(object);
        }
#endif
        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), RecyclableObject::FromVar(object), nullptr, scriptContext) :
            object;
    };

    BOOL TypedArrayBase::HasProperty(PropertyId propertyId)
    {TRACE_IT(63862);
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index) && (index < this->GetLength()))
        {TRACE_IT(63863);
            // All the slots within the length of the array are valid.
            return true;
        }
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL TypedArrayBase::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(63864);
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index) && (index < this->GetLength()))
        {TRACE_IT(63865);
            // All the slots within the length of the array are valid.
            return false;
        }
        return DynamicObject::DeleteProperty(propertyId, flags);
    }

    BOOL TypedArrayBase::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {TRACE_IT(63866);
        PropertyRecord const *propertyRecord = nullptr;
        if (JavascriptOperators::ShouldTryDeleteProperty(this, propertyNameString, &propertyRecord))
        {TRACE_IT(63867);
            Assert(propertyRecord);
            return DeleteProperty(propertyRecord->GetPropertyId(), flags);
        }

        return TRUE;
    }

    BOOL TypedArrayBase::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(63868);
        return TypedArrayBase::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL TypedArrayBase::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(63869);
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(63870);
            *value = this->DirectGetItem(index);
            if (JavascriptOperators::GetTypeId(*value) == Js::TypeIds_Undefined)
            {TRACE_IT(63871);
                return false;
            }
            return true;
        }

        return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL TypedArrayBase::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(63872);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    BOOL TypedArrayBase::HasItem(uint32 index)
    {TRACE_IT(63873);
        if (this->IsDetachedBuffer())
        {TRACE_IT(63874);
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_DetachedTypedArray);
        }

        if (index < GetLength())
        {TRACE_IT(63875);
            return true;
        }
        return false;
    }

    BOOL TypedArrayBase::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(63876);
        *value = DirectGetItem(index);
        return true;
    }

    BOOL TypedArrayBase::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(63877);
        *value = DirectGetItem(index);
        return true;
    }

    BOOL TypedArrayBase::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(63878);
        uint32 index;

        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(63879);
            this->DirectSetItem(index, value);
            return true;
        }
        else
        {TRACE_IT(63880);
            return DynamicObject::SetProperty(propertyId, value, flags, info);
        }
    }

    BOOL TypedArrayBase::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(63881);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");


        return DynamicObject::SetProperty(propertyNameString, value, flags, info);
    }

    BOOL TypedArrayBase::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {TRACE_IT(63882);
        return enumerator->Initialize(nullptr, this, this, flags, requestContext, forInCache);
    }

    JavascriptEnumerator * TypedArrayBase::GetIndexEnumerator(EnumeratorFlags flags, ScriptContext * requestContext)
    {TRACE_IT(63883);
        return RecyclerNew(requestContext->GetRecycler(), TypedArrayIndexEnumerator, this, flags, requestContext);
    }

    BOOL TypedArrayBase::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {
        DirectSetItem(index, value);
        return true;
    }

    BOOL TypedArrayBase::IsEnumerable(PropertyId propertyId)
    {TRACE_IT(63884);
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(63885);
            return true;
        }
        return DynamicObject::IsEnumerable(propertyId);
    }

    BOOL TypedArrayBase::IsConfigurable(PropertyId propertyId)
    {TRACE_IT(63886);
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(63887);
            return false;
        }
        return DynamicObject::IsConfigurable(propertyId);
    }


    BOOL TypedArrayBase::InitProperty(Js::PropertyId propertyId, Js::Var value, PropertyOperationFlags flags, Js::PropertyValueInfo* info)
    {TRACE_IT(63888);
        return SetProperty(propertyId, value, flags, info);
    }

    BOOL TypedArrayBase::IsWritable(PropertyId propertyId)
    {TRACE_IT(63889);
        uint32 index = 0;
        if (GetScriptContext()->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(63890);
            return true;
        }
        return DynamicObject::IsWritable(propertyId);
    }


    BOOL TypedArrayBase::SetEnumerable(PropertyId propertyId, BOOL value)
    {TRACE_IT(63891);
        ScriptContext* scriptContext = GetScriptContext();
        uint32 index;

        // all numeric properties are enumerable
        if (scriptContext->IsNumericPropertyId(propertyId, &index) )
        {TRACE_IT(63892);
            if (!value)
            {TRACE_IT(63893);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                    scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        return __super::SetEnumerable(propertyId, value);
    }

    BOOL TypedArrayBase::SetWritable(PropertyId propertyId, BOOL value)
    {TRACE_IT(63894);
        ScriptContext* scriptContext = GetScriptContext();
        uint32 index;

        // default is writable
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(63895);
            if (!value)
            {TRACE_IT(63896);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        return __super::SetWritable(propertyId, value);
    }

    BOOL TypedArrayBase::SetConfigurable(PropertyId propertyId, BOOL value)
    {TRACE_IT(63897);
        ScriptContext* scriptContext = GetScriptContext();
        uint32 index;

        // default is not configurable
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(63898);
            if (value)
            {TRACE_IT(63899);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }

        return __super::SetConfigurable(propertyId, value);
    }

    BOOL TypedArrayBase::SetAttributes(PropertyId propertyId, PropertyAttributes attributes)
    {TRACE_IT(63900);
        ScriptContext* scriptContext = this->GetScriptContext();
        uint32 index;

        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            VerifySetItemAttributes(propertyId, attributes);
            return true;
        }

        return __super::SetAttributes(propertyId, attributes);
    }

    BOOL TypedArrayBase::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(63901);
        ScriptContext* scriptContext = this->GetScriptContext();
        uint32 index;

        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(63902);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
                GetScriptContext()->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
        }

        return __super::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL TypedArrayBase::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(63903);
        ScriptContext* scriptContext = GetScriptContext();

        uint32 index;

        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            VerifySetItemAttributes(propertyId, attributes);
            return SetItem(index, value);
        }

        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL TypedArrayBase::SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {TRACE_IT(63904);
        VerifySetItemAttributes(Constants::NoProperty, attributes);
        return SetItem(index, value);
    }

    BOOL TypedArrayBase::Is(Var aValue)
    {TRACE_IT(63905);
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return Is(typeId);
    }

    BOOL TypedArrayBase::Is(TypeId typeId)
    {TRACE_IT(63906);
        return typeId >= TypeIds_TypedArrayMin && typeId <= TypeIds_TypedArrayMax;
    }

    TypedArrayBase* TypedArrayBase::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "must be a typed array");
        return static_cast<TypedArrayBase*>(aValue);
    }

    BOOL TypedArrayBase::IsDetachedTypedArray(Var aValue)
    {TRACE_IT(63907);
        return Is(aValue) && FromVar(aValue)->IsDetachedBuffer();
    }

    void TypedArrayBase::Set(TypedArrayBase* source, uint32 offset)
    {TRACE_IT(63908);
        uint32 sourceLength = source->GetLength();
        uint32 totalLength;

        if (UInt32Math::Add(offset, sourceLength, &totalLength) ||
            (totalLength > GetLength()))
        {TRACE_IT(63909);
            JavascriptError::ThrowRangeError(
                GetScriptContext(), JSERR_InvalidTypedArrayLength);
        }

        if (source->IsDetachedBuffer())
        {TRACE_IT(63910);
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_DetachedTypedArray);
        }

        // memmove buffer if views have same bit representation.
        // types of the same size are compatible, with the following exceptions:
        // - we cannot memmove between float and int arrays, due to different bit pattern
        // - we cannot memmove to a uint8 clamped array from an int8 array, due to negatives rounding to 0
        if (GetTypeId() == source->GetTypeId()
            || (GetBytesPerElement() == source->GetBytesPerElement()
            && !(Uint8ClampedArray::Is(this) && Int8Array::Is(source))
            && !Float32Array::Is(this) && !Float32Array::Is(source)
            && !Float64Array::Is(this) && !Float64Array::Is(source)))
        {TRACE_IT(63911);
            const size_t offsetInBytes = offset * BYTES_PER_ELEMENT;
            memmove_s(buffer + offsetInBytes,
                      GetByteLength() - offsetInBytes,
                      source->GetByteBuffer(),
                      source->GetByteLength());
        }
        else
        {TRACE_IT(63912);
            if (source->GetArrayBuffer() != GetArrayBuffer())
            {TRACE_IT(63913);
                for (uint32 i = 0; i < sourceLength; i++)
                {TRACE_IT(63914);
                    DirectSetItemNoDetachCheck(offset + i, source->DirectGetItemNoDetachCheck(i));
                }
            }
            else
            {TRACE_IT(63915);
                // We can have the source and destination coming from the same buffer. element size, start offset, and
                // length for source and dest typed array can be different. Use a separate tmp buffer to copy the elements.
                Js::JavascriptArray* tmpArray = GetScriptContext()->GetLibrary()->CreateArray(sourceLength);
                for (uint32 i = 0; i < sourceLength; i++)
                {TRACE_IT(63916);
                    tmpArray->SetItem(i, source->DirectGetItem(i), PropertyOperation_None);
                }
                for (uint32 i = 0; i < sourceLength; i++)
                {TRACE_IT(63917);
                    DirectSetItem(offset + i, tmpArray->DirectGetItem(i));
                }
            }
            if (source->IsDetachedBuffer() || this->IsDetachedBuffer())
            {TRACE_IT(63918);
                Throw::FatalInternalError();
            }
        }
    }

    uint32 TypedArrayBase::GetSourceLength(RecyclableObject* arraySource, uint32 targetLength, uint32 offset)
    {TRACE_IT(63919);
        ScriptContext* scriptContext = GetScriptContext();
        uint32 sourceLength = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetProperty(arraySource, PropertyIds::length, scriptContext), scriptContext);
        uint32 totalLength;
        if (UInt32Math::Add(offset, sourceLength, &totalLength) ||
            (totalLength > targetLength))
        {TRACE_IT(63920);
            JavascriptError::ThrowRangeError(
                scriptContext, JSERR_InvalidTypedArrayLength);
        }
        return sourceLength;
    }

    void TypedArrayBase::SetObjectNoDetachCheck(RecyclableObject* source, uint32 targetLength, uint32 offset)
    {TRACE_IT(63921);
        ScriptContext* scriptContext = GetScriptContext();
        uint32 sourceLength = GetSourceLength(source, targetLength, offset);
        Assert(!this->IsDetachedBuffer());

        Var itemValue;
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        for (uint32 i = 0; i < sourceLength; i++)
        {TRACE_IT(63922);
            if (!source->GetItem(source, i, &itemValue, scriptContext))
            {TRACE_IT(63923);
                itemValue = undefinedValue;
            }
            DirectSetItemNoDetachCheck(offset + i, itemValue);
        }

        if (this->IsDetachedBuffer())
        {TRACE_IT(63924);
            // We cannot be detached when we are creating the typed array itself. Terminate if that happens.
            Throw::FatalInternalError();
        }
    }

    // Getting length from the source object can detach the typedarray, and thus set it's length as 0,
    // this is observable because RangeError will be thrown instead of a TypeError further down the line
    void TypedArrayBase::SetObject(RecyclableObject* source, uint32 targetLength, uint32 offset)
    {TRACE_IT(63925);
        ScriptContext* scriptContext = GetScriptContext();
        uint32 sourceLength = GetSourceLength(source, targetLength, offset);

        Var itemValue;
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        for (uint32 i = 0; i < sourceLength; i ++)
        {TRACE_IT(63926);
            if (!source->GetItem(source, i, &itemValue, scriptContext))
            {TRACE_IT(63927);
                itemValue = undefinedValue;
            }
            DirectSetItem(offset + i, itemValue);
        }
    }

    HRESULT TypedArrayBase::GetBuffer(Var instance, ArrayBuffer** outBuffer, uint32* outOffset, uint32* outLength)
    {TRACE_IT(63928);
        HRESULT hr = NOERROR;
        if (Js::TypedArrayBase::Is(instance))
        {TRACE_IT(63929);
            Js::TypedArrayBase* typedArrayBase = Js::TypedArrayBase::FromVar(instance);
            *outBuffer = typedArrayBase->GetArrayBuffer()->GetAsArrayBuffer();
            *outOffset = typedArrayBase->GetByteOffset();
            *outLength = typedArrayBase->GetByteLength();
        }
        else if (Js::ArrayBuffer::Is(instance))
        {TRACE_IT(63930);
            Js::ArrayBuffer* buffer = Js::ArrayBuffer::FromVar(instance);
            *outBuffer = buffer;
            *outOffset = 0;
            *outLength = buffer->GetByteLength();
        }
        else if (Js::DataView::Is(instance))
        {TRACE_IT(63931);
            Js::DataView* dView = Js::DataView::FromVar(instance);
            *outBuffer = dView->GetArrayBuffer()->GetAsArrayBuffer();
            *outOffset = dView->GetByteOffset();
            *outLength = dView->GetLength();
        }
        else
        {TRACE_IT(63932);
            hr = E_INVALIDARG;
        }
        return hr;

    }

    Var TypedArrayBase::EntryGetterBuffer(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {TRACE_IT(63933);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        TypedArrayBase* typedArray = TypedArrayBase::FromVar(args[0]);
        ArrayBufferBase* arrayBuffer = typedArray->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {TRACE_IT(63934);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        return arrayBuffer;
    }

    Var TypedArrayBase::EntryGetterByteLength(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {TRACE_IT(63935);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        TypedArrayBase* typedArray = TypedArrayBase::FromVar(args[0]);
        ArrayBufferBase* arrayBuffer = typedArray->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {TRACE_IT(63936);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {TRACE_IT(63937);
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(typedArray->GetByteLength(), scriptContext);
    }

    Var TypedArrayBase::EntryGetterByteOffset(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {TRACE_IT(63938);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        TypedArrayBase* typedArray = TypedArrayBase::FromVar(args[0]);
        ArrayBufferBase* arrayBuffer = typedArray->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {TRACE_IT(63939);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {TRACE_IT(63940);
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(typedArray->GetByteOffset(), scriptContext);
    }

    Var TypedArrayBase::EntryGetterLength(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {TRACE_IT(63941);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        TypedArrayBase* typedArray = TypedArrayBase::FromVar(args[0]);
        ArrayBufferBase* arrayBuffer = typedArray->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {TRACE_IT(63942);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {TRACE_IT(63943);
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(typedArray->GetLength(), scriptContext);
    }

    Var TypedArrayBase::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

    // ES2017 22.2.3.32 get %TypedArray%.prototype[@@toStringTag]
    Var TypedArrayBase::EntryGetterSymbolToStringTag(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        Assert(!(callInfo.Flags & CallFlags_New));

        // 1. Let O be the this value.
        // 2. If Type(O) is not Object, return undefined.
        // 3. If O does not have a[[TypedArrayName]] internal slot, return undefined.
        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {TRACE_IT(63944);
            if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
            {TRACE_IT(63945);
                return library->GetUndefined();
            }
            else
            {TRACE_IT(63946);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
            }
        }

        // 4. Let name be O.[[TypedArrayName]].
        // 5. Assert: name is a String value.
        // 6. Return name.
        Var name;
        switch (JavascriptOperators::GetTypeId(args[0]))
        {
        case TypeIds_Int8Array:
            name = library->CreateStringFromCppLiteral(_u("Int8Array"));
            break;

        case TypeIds_Uint8Array:
            name = library->CreateStringFromCppLiteral(_u("Uint8Array"));
            break;

        case TypeIds_Uint8ClampedArray:
            name = library->CreateStringFromCppLiteral(_u("Uint8ClampedArray"));
            break;

        case TypeIds_Int16Array:
            name = library->CreateStringFromCppLiteral(_u("Int16Array"));
            break;

        case TypeIds_Uint16Array:
            name = library->CreateStringFromCppLiteral(_u("Uint16Array"));
            break;

        case TypeIds_Int32Array:
            name = library->CreateStringFromCppLiteral(_u("Int32Array"));
            break;

        case TypeIds_Uint32Array:
            name = library->CreateStringFromCppLiteral(_u("Uint32Array"));
            break;

        case TypeIds_Float32Array:
            name = library->CreateStringFromCppLiteral(_u("Float32Array"));
            break;

        case TypeIds_Float64Array:
            name = library->CreateStringFromCppLiteral(_u("Float64Array"));
            break;

        default:
            name = library->GetUndefinedDisplayString();
            break;
        }

        return name;
    }

    Var TypedArrayBase::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {TRACE_IT(63947);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        return CommonSet(args);
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    Var TypedArray<TypeName, clamped, virtualAllocated>::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // This method is only called in pre-ES6 compat modes. In those modes, we need to throw an error
        // if the this argument is not the same type as our TypedArray template instance.
        if (args.Info.Count == 0 || !TypedArray::Is(args[0]))
        {TRACE_IT(63948);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        return CommonSet(args);
    }

    Var TypedArrayBase::CommonSet(Arguments& args)
    {TRACE_IT(63949);
        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        ScriptContext* scriptContext = typedArrayBase->GetScriptContext();
        uint32 offset = 0;
        if (args.Info.Count < 2)
        {TRACE_IT(63950);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_TypedArray_NeedSource);
        }

        if (args.Info.Count > 2)
        {TRACE_IT(63951);
            offset = ArrayBuffer::ToIndex(args[2], JSERR_InvalidTypedArrayLength, scriptContext, ArrayBuffer::MaxArrayBufferLength, false);
        }

        if (TypedArrayBase::Is(args[1]))
        {TRACE_IT(63952);
            TypedArrayBase* typedArraySource = TypedArrayBase::FromVar(args[1]);
            if (typedArraySource->IsDetachedBuffer() || typedArrayBase->IsDetachedBuffer()) // If IsDetachedBuffer(targetBuffer) is true, then throw a TypeError exception.
            {TRACE_IT(63953);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("[TypedArray].prototype.set"));
            }
            typedArrayBase->Set(typedArraySource, (uint32)offset);
        }
        else
        {TRACE_IT(63954);
            RecyclableObject* sourceArray;
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[1]);
#endif
            if (!JavascriptConversion::ToObject(args[1], scriptContext, &sourceArray))
            {TRACE_IT(63955);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_TypedArray_NeedSource);
            }
            else if (typedArrayBase->IsDetachedBuffer())
            {TRACE_IT(63956);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("[TypedArray].prototype.set"));
            }
            typedArrayBase->SetObject(sourceArray, typedArrayBase->GetLength(), (uint32)offset);
        }
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var TypedArrayBase::EntrySubarray(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_subarray);

        if (args.Info.Count == 0 || !TypedArrayBase::Is(args[0]))
        {TRACE_IT(63957);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        return CommonSubarray(args);
    }

    uint32 TypedArrayBase::GetFromIndex(Var arg, uint32 length, ScriptContext *scriptContext)
    {TRACE_IT(63958);
        uint32 index = JavascriptArray::GetFromIndex(arg, length, scriptContext);
        return min(index, length);
    }

    Var TypedArrayBase::CommonSubarray(Arguments& args)
    {TRACE_IT(63959);
        TypedArrayBase* typedArrayBase = TypedArrayBase::FromVar(args[0]);
        uint32 length = typedArrayBase->GetLength();
        ScriptContext* scriptContext = typedArrayBase->GetScriptContext();
        int32 begin = 0;
        int end = length;
        if (args.Info.Count > 1)
        {TRACE_IT(63960);
            begin = TypedArrayBase::GetFromIndex(args[1], length, scriptContext);

            if (args.Info.Count > 2 && !JavascriptOperators::IsUndefined(args[2]))
            {TRACE_IT(63961);
                end = TypedArrayBase::GetFromIndex(args[2], length, scriptContext);
            }
        }

        if (end < begin)
        {TRACE_IT(63962);
            end = begin;
        }

        return typedArrayBase->Subarray(begin, end);
    }

    template <typename TypeName, bool clamped, bool virtualAllocated>
    Var TypedArray<TypeName, clamped, virtualAllocated>::Subarray(uint32 begin, uint32 end)
    {TRACE_IT(63963);
        Assert(end >= begin);

        Var newTypedArray;
        ScriptContext* scriptContext = this->GetScriptContext();
        ArrayBufferBase* buffer = this->GetArrayBuffer();
        uint32 srcByteOffset = this->GetByteOffset();
        uint32 beginByteOffset = srcByteOffset + begin * BYTES_PER_ELEMENT;
        uint32 newLength = end - begin;

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {TRACE_IT(63964);
            JavascriptFunction* constructor =
                JavascriptFunction::FromVar(JavascriptOperators::SpeciesConstructor(this, TypedArrayBase::GetDefaultConstructor(this, scriptContext), scriptContext));

            Js::Var constructorArgs[] = { constructor, buffer, JavascriptNumber::ToVar(beginByteOffset, scriptContext), JavascriptNumber::ToVar(newLength, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            newTypedArray = RecyclableObject::FromVar(TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), newLength, scriptContext));
        }
        else
        {TRACE_IT(63965);
            newTypedArray = TypedArray<TypeName, clamped, virtualAllocated>::Create(buffer, beginByteOffset, newLength, scriptContext->GetLibrary());
        }

        return newTypedArray;
    }

    // %TypedArray%.from as described in ES6.0 (draft 22) Section 22.2.2.1
    Var TypedArrayBase::EntryFrom(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].from"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_from);

        if (args.Info.Count < 1 || !JavascriptOperators::IsConstructor(args[0]))
        {TRACE_IT(63966);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, _u("[TypedArray].from"));
        }

        RecyclableObject* constructor = RecyclableObject::FromVar(args[0]);
        RecyclableObject* items = nullptr;

        if (args.Info.Count < 2 || !JavascriptConversion::ToObject(args[1], scriptContext, &items))
        {TRACE_IT(63967);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, _u("[TypedArray].from"));
        }

        bool mapping = false;
        JavascriptFunction* mapFn = nullptr;
        Var mapFnThisArg = nullptr;

        if (args.Info.Count >= 3)
        {TRACE_IT(63968);
            if (!JavascriptFunction::Is(args[2]))
            {TRACE_IT(63969);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].from"));
            }

            mapFn = JavascriptFunction::FromVar(args[2]);

            if (args.Info.Count >= 4)
            {TRACE_IT(63970);
                mapFnThisArg = args[3];
            }
            else
            {TRACE_IT(63971);
                mapFnThisArg = library->GetUndefined();
            }

            mapping = true;
        }

        Var newObj;
        RecyclableObject* iterator = JavascriptOperators::GetIterator(items, scriptContext, true /* optional */);

        if (iterator != nullptr)
        {TRACE_IT(63972);
            DECLARE_TEMP_GUEST_ALLOCATOR(tempAlloc);

            ACQUIRE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext, _u("Runtime"));
            {TRACE_IT(63973);
                // Create a temporary list to hold the items returned from the iterator.
                // We will then iterate over this list and append those items into the object we will return.
                // We have to collect the items into this temporary list because we need to call the
                // new object constructor with a length of items and we don't know what length will be
                // until we iterate across all the items.
                // Consider: Could be possible to fast-path this in order to avoid creating the temporary list
                //       for types we know such as TypedArray. We know the length of a TypedArray but we still
                //       have to be careful in case there is a proxy which could return anything from [[Get]]
                //       or the built-in @@iterator has been replaced.
                JsUtil::List<Var, ArenaAllocator>* tempList = IteratorToList(iterator, scriptContext, tempAlloc);

                uint32 len = tempList->Count();

                Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(len, scriptContext) };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), len, scriptContext);

                TypedArrayBase* newTypedArrayBase = nullptr;
                JavascriptArray* newArr = nullptr;

                if (TypedArrayBase::Is(newObj))
                {TRACE_IT(63974);
                    newTypedArrayBase = TypedArrayBase::FromVar(newObj);
                }
                else if (JavascriptArray::Is(newObj))
                {TRACE_IT(63975);
                    newArr = JavascriptArray::FromVar(newObj);
                }

                for (uint32 k = 0; k < len; k++)
                {TRACE_IT(63976);
                    Var kValue = tempList->Item(k);

                    if (mapping)
                    {TRACE_IT(63977);
                        Assert(mapFn != nullptr);
                        Assert(mapFnThisArg != nullptr);

                        Js::Var mapFnArgs[] = { mapFnThisArg, kValue, JavascriptNumber::ToVar(k, scriptContext) };
                        Js::CallInfo mapFnCallInfo(Js::CallFlags_Value, _countof(mapFnArgs));
                        kValue = mapFn->CallFunction(Js::Arguments(mapFnCallInfo, mapFnArgs));
                    }

                    // We're likely to have constructed a new TypedArray, but the constructor could return any object
                    if (newTypedArrayBase)
                    {TRACE_IT(63978);
                        newTypedArrayBase->DirectSetItem(k, kValue);
                    }
                    else if (newArr)
                    {TRACE_IT(63979);
                        newArr->SetItem(k, kValue, Js::PropertyOperation_ThrowIfNotExtensible);
                    }
                    else
                    {TRACE_IT(63980);
                        JavascriptOperators::OP_SetElementI_UInt32(newObj, k, kValue, scriptContext, Js::PropertyOperation_ThrowIfNotExtensible);
                    }
                }
            }
            RELEASE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext);
        }
        else
        {TRACE_IT(63981);
            Var lenValue = JavascriptOperators::OP_GetLength(items, scriptContext);
            uint32 len = JavascriptConversion::ToUInt32(lenValue, scriptContext);

            TypedArrayBase* itemsTypedArrayBase = nullptr;
            JavascriptArray* itemsArray = nullptr;

            if (TypedArrayBase::Is(items))
            {TRACE_IT(63982);
                itemsTypedArrayBase = TypedArrayBase::FromVar(items);
            }
            else if (JavascriptArray::Is(items))
            {TRACE_IT(63983);
                itemsArray = JavascriptArray::FromVar(items);
            }

            Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(len, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            newObj = TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), len, scriptContext);

            TypedArrayBase* newTypedArrayBase = nullptr;
            JavascriptArray* newArr = nullptr;

            if (TypedArrayBase::Is(newObj))
            {TRACE_IT(63984);
                newTypedArrayBase = TypedArrayBase::FromVar(newObj);
            }
            else if (JavascriptArray::Is(newObj))
            {TRACE_IT(63985);
                newArr = JavascriptArray::FromVar(newObj);
            }

            for (uint32 k = 0; k < len; k++)
            {TRACE_IT(63986);
                Var kValue;

                // The items source could be anything, but we already know if it's a TypedArray or Array
                if (itemsTypedArrayBase)
                {TRACE_IT(63987);
                    kValue = itemsTypedArrayBase->DirectGetItem(k);
                }
                else if (itemsArray)
                {TRACE_IT(63988);
                    kValue = itemsArray->DirectGetItem(k);
                }
                else
                {TRACE_IT(63989);
                    kValue = JavascriptOperators::OP_GetElementI_UInt32(items, k, scriptContext);
                }

                if (mapping)
                {TRACE_IT(63990);
                    Assert(mapFn != nullptr);
                    Assert(mapFnThisArg != nullptr);

                    Js::Var mapFnArgs[] = { mapFnThisArg, kValue, JavascriptNumber::ToVar(k, scriptContext) };
                    Js::CallInfo mapFnCallInfo(Js::CallFlags_Value, _countof(mapFnArgs));
                    kValue = mapFn->CallFunction(Js::Arguments(mapFnCallInfo, mapFnArgs));
                }

                // If constructor built a TypedArray (likely) or Array (maybe likely) we can do a more direct set operation
                if (newTypedArrayBase)
                {TRACE_IT(63991);
                    newTypedArrayBase->DirectSetItem(k, kValue);
                }
                else if (newArr)
                {TRACE_IT(63992);
                    newArr->SetItem(k, kValue, Js::PropertyOperation_ThrowIfNotExtensible);
                }
                else
                {TRACE_IT(63993);
                    JavascriptOperators::OP_SetElementI_UInt32(newObj, k, kValue, scriptContext, Js::PropertyOperation_ThrowIfNotExtensible);
                }
            }
        }

        return newObj;
    }

    Var TypedArrayBase::EntryOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_of);

        if (args.Info.Count < 1)
        {TRACE_IT(63994);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, _u("[TypedArray].of"));
        }

        return JavascriptArray::OfHelper(true, args, scriptContext);
    }

    Var TypedArrayBase::EntryCopyWithin(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_copyWithin);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.copyWithin"));
        uint32 length = typedArrayBase->GetLength();

        return JavascriptArray::CopyWithinHelper(nullptr, typedArrayBase, typedArrayBase, length, args, scriptContext);
    }

    Var TypedArrayBase::GetKeysEntriesValuesHelper(Arguments& args, ScriptContext *scriptContext, LPCWSTR apiName, JavascriptArrayIteratorKind kind)
    {TRACE_IT(63995);
        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, apiName);
        return scriptContext->GetLibrary()->CreateArrayIterator(typedArrayBase, kind);
    }

    Var TypedArrayBase::EntryEntries(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_entries);

        return GetKeysEntriesValuesHelper(args, scriptContext, _u("[TypedArray].prototype.entries"), JavascriptArrayIteratorKind::KeyAndValue);
    }

    Var TypedArrayBase::EntryEvery(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.every"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_every);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.every"));
        return JavascriptArray::EveryHelper(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), args, scriptContext);
    }

    Var TypedArrayBase::EntryFill(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_fill);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.fill"));
        return JavascriptArray::FillHelper(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), args, scriptContext);
    }

    // %TypedArray%.prototype.filter as described in ES6.0 (draft 22) Section 22.2.3.9
    Var TypedArrayBase::EntryFilter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.filter"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_filter);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.filter"));
        uint32 length = typedArrayBase->GetLength();

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(63996);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.filter"));
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg = nullptr;

        if (args.Info.Count > 2)
        {TRACE_IT(63997);
            thisArg = args[2];
        }
        else
        {TRACE_IT(63998);
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var selected = nullptr;
        RecyclableObject* newObj = nullptr;

        DECLARE_TEMP_GUEST_ALLOCATOR(tempAlloc);

        ACQUIRE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext, _u("Runtime"));
        {TRACE_IT(63999);
            // Create a temporary list to hold the items selected by the callback function.
            // We will then iterate over this list and append those items into the object we will return.
            // We have to collect the items into this temporary list because we need to call the
            // new object constructor with a length of items and we don't know what length will be
            // until we know how many items we will collect.
            JsUtil::List<Var, ArenaAllocator>* tempList = JsUtil::List<Var, ArenaAllocator>::New(tempAlloc);

            for (uint32 k = 0; k < length; k++)
            {TRACE_IT(64000);
                // We know that the TypedArray.HasItem will be true because k < length and we can skip the check in the TypedArray version of filter.
                element = typedArrayBase->DirectGetItem(k);

                selected = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    typedArrayBase);

                if (JavascriptConversion::ToBoolean(selected, scriptContext))
                {TRACE_IT(64001);
                    tempList->Add(element);
                }
            }

            uint32 captured = tempList->Count();

            Var constructor = JavascriptOperators::SpeciesConstructor(
                typedArrayBase, TypedArrayBase::GetDefaultConstructor(args[0], scriptContext), scriptContext);

            Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(captured, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            newObj = RecyclableObject::FromVar(TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), captured, scriptContext));

            if (TypedArrayBase::Is(newObj))
            {TRACE_IT(64002);
                // We are much more likely to have a TypedArray here than anything else
                TypedArrayBase* newArr = TypedArrayBase::FromVar(newObj);

                for (uint32 i = 0; i < captured; i++)
                {TRACE_IT(64003);
                    newArr->DirectSetItem(i, tempList->Item(i));
                }
            }
            else
            {TRACE_IT(64004);
                for (uint32 i = 0; i < captured; i++)
                {TRACE_IT(64005);
                    JavascriptOperators::OP_SetElementI_UInt32(newObj, i, tempList->Item(i), scriptContext, PropertyOperation_ThrowIfNotExtensible);
                }
            }
        }
        RELEASE_TEMP_GUEST_ALLOCATOR(tempAlloc, scriptContext);

        return newObj;
    }

    Var TypedArrayBase::EntryFind(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.find"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_find);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.find"));

        return JavascriptArray::FindHelper<false>(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), args, scriptContext);
    }

    Var TypedArrayBase::EntryFindIndex(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.findIndex"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_findIndex);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.findIndex"));

        return JavascriptArray::FindHelper<true>(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), args, scriptContext);
    }

    // %TypedArray%.prototype.forEach as described in ES6.0 (draft 22) Section 22.2.3.12
    Var TypedArrayBase::EntryForEach(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.forEach"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_forEach);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.forEach"));
        uint32 length = typedArrayBase->GetLength();

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(64006);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.forEach"));
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg;

        if (args.Info.Count > 2)
        {TRACE_IT(64007);
            thisArg = args[2];
        }
        else
        {TRACE_IT(64008);
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        for (uint32 k = 0; k < length; k++)
        {TRACE_IT(64009);
            // No need for HasItem, as we have already established that this API can be called only on the TypedArray object. So Proxy scenario cannot happen.

            Var element = typedArrayBase->DirectGetItem(k);

            CALL_FUNCTION(callBackFn, CallInfo(CallFlags_Value, 4),
                thisArg,
                element,
                JavascriptNumber::ToVar(k, scriptContext),
                typedArrayBase);
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var TypedArrayBase::EntryIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_indexOf);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.indexOf"));
        uint32 length = typedArrayBase->GetLength();

        Var search = nullptr;
        uint32 fromIndex;
        if (!JavascriptArray::GetParamForIndexOf(length, args, search, fromIndex, scriptContext))
        {TRACE_IT(64010);
            return TaggedInt::ToVarUnchecked(-1);
        }

        return JavascriptArray::TemplatedIndexOfHelper<false>(typedArrayBase, search, fromIndex, length, scriptContext);
    }

    Var TypedArrayBase::EntryIncludes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_includes);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.includes"));
        uint32 length = typedArrayBase->GetLength();

        Var search = nullptr;
        uint32 fromIndex;
        if (!JavascriptArray::GetParamForIndexOf(length, args, search, fromIndex, scriptContext))
        {TRACE_IT(64011);
            return scriptContext->GetLibrary()->GetFalse();
        }

        return JavascriptArray::TemplatedIndexOfHelper<true>(typedArrayBase, search, fromIndex, length, scriptContext);
    }


    Var TypedArrayBase::EntryJoin(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_join);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.join"));
        uint32 length = typedArrayBase->GetLength();

        JavascriptLibrary* library = scriptContext->GetLibrary();
        JavascriptString* separator = nullptr;

        if (args.Info.Count > 1 && !JavascriptOperators::IsUndefined(args[1]))
        {TRACE_IT(64012);
            separator = JavascriptConversion::ToString(args[1], scriptContext);
        }
        else
        {TRACE_IT(64013);
            separator = library->GetCommaDisplayString();
        }

        if (length == 0)
        {TRACE_IT(64014);
            return library->GetEmptyString();
        }
        else if (length == 1)
        {TRACE_IT(64015);
            return JavascriptConversion::ToString(typedArrayBase->DirectGetItem(0), scriptContext);
        }

        bool hasSeparator = (separator->GetLength() != 0);

        charcount_t estimatedAppendSize = min(
            static_cast<charcount_t>((64 << 20) / sizeof(void *)), // 64 MB worth of pointers
            static_cast<charcount_t>(length + (hasSeparator ? length - 1 : 0)));

        CompoundString* const cs = CompoundString::NewWithPointerCapacity(estimatedAppendSize, library);

        Assert(length >= 2);

        JavascriptString* element = JavascriptConversion::ToString(typedArrayBase->DirectGetItem(0), scriptContext);

        cs->Append(element);

        for (uint32 i = 1; i < length; i++)
        {TRACE_IT(64016);
            if (hasSeparator)
            {TRACE_IT(64017);
                cs->Append(separator);
            }

            // Since i < length, we can be certain that the TypedArray contains an item at index i and we don't have to check for undefined
            element = JavascriptConversion::ToString(typedArrayBase->DirectGetItem(i), scriptContext);

            cs->Append(element);
        }

        return cs;
    }

    Var TypedArrayBase::EntryKeys(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_keys);

        return GetKeysEntriesValuesHelper(args, scriptContext, _u("[TypedArray].prototype.keys"), JavascriptArrayIteratorKind::Key);
    }

    Var TypedArrayBase::EntryLastIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_lastIndexOf);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.lastIndexOf"));
        uint32 length = typedArrayBase->GetLength();

        Var search = nullptr;
        int64 fromIndex;
        if (!JavascriptArray::GetParamForLastIndexOf(length, args, search, fromIndex, scriptContext))
        {TRACE_IT(64018);
            return TaggedInt::ToVarUnchecked(-1);
        }

        return JavascriptArray::LastIndexOfHelper(typedArrayBase, search, fromIndex, scriptContext);
    }

    Var TypedArrayBase::EntryMap(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.map"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_map);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.map"));

        return JavascriptArray::MapHelper(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), args, scriptContext);
    }

    Var TypedArrayBase::EntryReduce(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.reduce"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_reduce);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.reduce"));
        return JavascriptArray::ReduceHelper(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), args, scriptContext);
    }

    Var TypedArrayBase::EntryReduceRight(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.reduceRight"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_reduceRight);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.reduceRight"));
        return JavascriptArray::ReduceRightHelper(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), args, scriptContext);
    }

    Var TypedArrayBase::EntryReverse(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_reverse);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.reverse"));
        return JavascriptArray::ReverseHelper(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), scriptContext);
    }

    Var TypedArrayBase::EntrySlice(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.slice"));
        return JavascriptArray::SliceHelper(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), args, scriptContext);
    }

    Var TypedArrayBase::EntrySome(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.some"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_some);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.some"));

        return JavascriptArray::SomeHelper(nullptr, typedArrayBase, typedArrayBase, typedArrayBase->GetLength(), args, scriptContext);
    }

    template<typename T> int __cdecl TypedArrayCompareElementsHelper(void* context, const void* elem1, const void* elem2)
    {TRACE_IT(64019);
        const T* element1 = static_cast<const T*>(elem1);
        const T* element2 = static_cast<const T*>(elem2);

        Assert(element1 != nullptr);
        Assert(element2 != nullptr);
        Assert(context != nullptr);

        const T x = *element1;
        const T y = *element2;

        if (NumberUtilities::IsNan((double)x))
        {TRACE_IT(64020);
            if (NumberUtilities::IsNan((double)y))
            {TRACE_IT(64021);
                return 0;
            }

            return 1;
        }
        else
        {TRACE_IT(64022);
            if (NumberUtilities::IsNan((double)y))
            {TRACE_IT(64023);
                return -1;
            }
        }

        void **contextArray = (void **)context;
        if (contextArray[1] != nullptr)
        {TRACE_IT(64024);
            RecyclableObject* compFn = RecyclableObject::FromVar(contextArray[1]);
            ScriptContext* scriptContext = compFn->GetScriptContext();
            Var undefined = scriptContext->GetLibrary()->GetUndefined();
            double dblResult;
            Var retVal = CALL_FUNCTION(compFn, CallInfo(CallFlags_Value, 3),
                undefined,
                JavascriptNumber::ToVarWithCheck((double)x, scriptContext),
                JavascriptNumber::ToVarWithCheck((double)y, scriptContext));

            Assert(TypedArrayBase::Is(contextArray[0]));
            if (TypedArrayBase::IsDetachedTypedArray(contextArray[0]))
            {TRACE_IT(64025);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("[TypedArray].prototype.sort"));
            }

            if (TaggedInt::Is(retVal))
            {TRACE_IT(64026);
                return TaggedInt::ToInt32(retVal);
            }

            if (JavascriptNumber::Is_NoTaggedIntCheck(retVal))
            {TRACE_IT(64027);
                dblResult = JavascriptNumber::GetValue(retVal);
            }
            else
            {TRACE_IT(64028);
                dblResult = JavascriptConversion::ToNumber_Full(retVal, scriptContext);
            }

            // ToNumber may execute user-code which can cause the array to become detached
            if (TypedArrayBase::IsDetachedTypedArray(contextArray[0]))
            {TRACE_IT(64029);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("[TypedArray].prototype.sort"));
            }

            if (dblResult < 0)
            {TRACE_IT(64030);
                return -1;
            }
            else if (dblResult > 0)
            {TRACE_IT(64031);
                return 1;
            }

            return 0;
        }
        else
        {TRACE_IT(64032);
            if (x < y)
            {TRACE_IT(64033);
                return -1;
            }
            else if (x > y)
            {TRACE_IT(64034);
                return 1;
            }

            return 0;
        }
    }

    Var TypedArrayBase::EntrySort(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("[TypedArray].prototype.sort"));

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_sort);

        TypedArrayBase* typedArrayBase = ValidateTypedArray(args, scriptContext, _u("[TypedArray].prototype.sort"));
        uint32 length = typedArrayBase->GetLength();

        // If TypedArray has no length, we don't have any work to do.
        if (length == 0)
        {TRACE_IT(64035);
            return typedArrayBase;
        }

        RecyclableObject* compareFn = nullptr;

        if (args.Info.Count > 1)
        {TRACE_IT(64036);
            if (!JavascriptConversion::IsCallable(args[1]))
            {TRACE_IT(64037);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.sort"));
            }

            compareFn = RecyclableObject::FromVar(args[1]);
        }

        // Get the elements comparison function for the type of this TypedArray
        void* elementCompare = reinterpret_cast<void*>(typedArrayBase->GetCompareElementsFunction());

        Assert(elementCompare);

        // Cast compare to the correct function type
        int(__cdecl*elementCompareFunc)(void*, const void*, const void*) = (int(__cdecl*)(void*, const void*, const void*))elementCompare;

        void * contextToPass[] = { typedArrayBase, compareFn };

        // We can always call qsort_s with the same arguments. If user compareFn is non-null, the callback will use it to do the comparison.
        qsort_s(typedArrayBase->GetByteBuffer(), length, typedArrayBase->GetBytesPerElement(), elementCompareFunc, contextToPass);


        return typedArrayBase;
    }

    Var TypedArrayBase::EntryValues(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TypedArray_Prototype_values);

        return GetKeysEntriesValuesHelper(args, scriptContext, _u("[TypedArray].prototype.values"), JavascriptArrayIteratorKind::Value);
    }

    BOOL TypedArrayBase::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        ENTER_PINNED_SCOPE(JavascriptString, toStringResult);
        toStringResult = JavascriptObject::ToStringInternal(this, requestContext);
        stringBuilder->Append(toStringResult->GetString(), toStringResult->GetLength());
        LEAVE_PINNED_SCOPE();
        return TRUE;
    }

    BOOL TypedArrayBase::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(64038);
        switch(GetTypeId())
        {
        case TypeIds_Int8Array:
            stringBuilder->AppendCppLiteral(_u("Object, (Int8Array)"));
            break;

        case TypeIds_Uint8Array:
            stringBuilder->AppendCppLiteral(_u("Object, (Uint8Array)"));
            break;

        case TypeIds_Uint8ClampedArray:
            stringBuilder->AppendCppLiteral(_u("Object, (Uint8ClampedArray)"));
            break;

        case TypeIds_Int16Array:
            stringBuilder->AppendCppLiteral(_u("Object, (Int16Array)"));
            break;

        case TypeIds_Uint16Array:
            stringBuilder->AppendCppLiteral(_u("Object, (Uint16Array)"));
            break;

        case TypeIds_Int32Array:
            stringBuilder->AppendCppLiteral(_u("Object, (Int32Array)"));
            break;

        case TypeIds_Uint32Array:
            stringBuilder->AppendCppLiteral(_u("Object, (Uint32Array)"));
            break;

        case TypeIds_Float32Array:
            stringBuilder->AppendCppLiteral(_u("Object, (Float32Array)"));
            break;

        case TypeIds_Float64Array:
            stringBuilder->AppendCppLiteral(_u("Object, (Float64Array)"));
            break;

        default:
            Assert(false);
            stringBuilder->AppendCppLiteral(_u("Object"));
            break;
        }

        return TRUE;
    }

    bool TypedArrayBase::TryGetLengthForOptimizedTypedArray(const Var var, uint32 *const lengthRef, TypeId *const typeIdRef)
    {TRACE_IT(64039);
        Assert(var);
        Assert(lengthRef);
        Assert(typeIdRef);

        if(!RecyclableObject::Is(var))
        {TRACE_IT(64040);
            return false;
        }

        const TypeId typeId = RecyclableObject::FromVar(var)->GetTypeId();
        switch(typeId)
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
                Assert(ValueType::FromTypeId(typeId,false).IsOptimizedTypedArray());
                *lengthRef = FromVar(var)->GetLength();
                *typeIdRef = typeId;
                return true;
        }

        Assert(!ValueType::FromTypeId(typeId,false).IsOptimizedTypedArray());
        return false;
    }

    _Use_decl_annotations_ BOOL TypedArrayBase::ValidateIndexAndDirectSetItem(Js::Var index, Js::Var value, bool * isNumericIndex)
    {TRACE_IT(64041);
        bool skipSetItem = false;
        uint32 indexToSet = ValidateAndReturnIndex(index, &skipSetItem, isNumericIndex);

        // If index is not numeric, goto [[Set]] property path
        if (*isNumericIndex)
        {TRACE_IT(64042);
            return skipSetItem ?
                DirectSetItemNoSet(indexToSet, value) :
                DirectSetItem(indexToSet, value);
        }
        else
        {TRACE_IT(64043);
            return TRUE;
        }
    }

    // Validate the index used for typed arrays with below rules:
    // 1. if numeric string, let sIndex = ToNumber(index) :
    //    a. if sIndex is not integer, skip set operation
    //    b. if sIndex == -0, skip set operation
    //    c. if sIndex < 0 or sIndex >= length, skip set operation
    //    d. else return sIndex and perform set operation
    // 2. if tagged int, let nIndex = untag(index) :
    //    a. if nIndex < 0 or nIndex >= length, skip set operation
    // 3. else:
    //    a. if index is not integer, skip set operation
    //    b. if index < 0 or index >= length, skip set operation
    //    NOTE: if index == -0, it is treated as 0 and perform set operation
    //          as per 7.1.12.1 of ES6 spec 7.1.12.1 ToString Applied to the Number Type
    _Use_decl_annotations_ uint32 TypedArrayBase::ValidateAndReturnIndex(Js::Var index, bool * skipOperation, bool * isNumericIndex)
    {TRACE_IT(64044);
        *skipOperation = false;
        *isNumericIndex = true;
        uint32 length = GetLength();

        if (TaggedInt::Is(index))
        {TRACE_IT(64045);
            int32 indexInt = TaggedInt::ToInt32(index);
            *skipOperation = (indexInt < 0 || (uint32)indexInt >= length);
            return (uint32)indexInt;
        }
        else
        {TRACE_IT(64046);
            double dIndexValue = 0;
            if (JavascriptString::Is(index))
            {TRACE_IT(64047);
                if (JavascriptConversion::CanonicalNumericIndexString(index, &dIndexValue, GetScriptContext()))
                {TRACE_IT(64048);
                    if (JavascriptNumber::IsNegZero(dIndexValue))
                    {TRACE_IT(64049);
                        *skipOperation = true;
                        return 0;
                    }
                    // If this is numeric index embedded in string, perform regular numeric index checks below
                }
                else
                {TRACE_IT(64050);
                    // not numeric index, go the [[Set]] path to add as string property
                    *isNumericIndex = false;
                    return 0;
                }
            }
            else
            {TRACE_IT(64051);
                // JavascriptNumber::Is_NoTaggedIntCheck(index)
                dIndexValue = JavascriptNumber::GetValue(index);
            }

            // OK to lose data because we want to verify ToInteger()
            uint32 uint32Index = (uint32)dIndexValue;

            // IsInteger()
            if ((double)uint32Index != dIndexValue)
            {TRACE_IT(64052);
                *skipOperation = true;
            }
            // index >= length
            else if (uint32Index >= GetLength())
            {TRACE_IT(64053);
                *skipOperation = true;
            }
            return uint32Index;
        }
    }

    // static
    Var TypedArrayBase::GetDefaultConstructor(Var object, ScriptContext* scriptContext)
    {TRACE_IT(64054);
        TypeId typeId = JavascriptOperators::GetTypeId(object);
        Var defaultConstructor = nullptr;
        switch (typeId)
        {
        case TypeId::TypeIds_Int8Array:
            defaultConstructor = scriptContext->GetLibrary()->GetInt8ArrayConstructor();
            break;
        case TypeId::TypeIds_Uint8Array:
            defaultConstructor = scriptContext->GetLibrary()->GetUint8ArrayConstructor();
            break;
        case TypeId::TypeIds_Uint8ClampedArray:
            defaultConstructor = scriptContext->GetLibrary()->GetUint8ClampedArrayConstructor();
            break;
        case TypeId::TypeIds_Int16Array:
            defaultConstructor = scriptContext->GetLibrary()->GetInt16ArrayConstructor();
            break;
        case TypeId::TypeIds_Uint16Array:
            defaultConstructor = scriptContext->GetLibrary()->GetUint16ArrayConstructor();
            break;
        case TypeId::TypeIds_Int32Array:
            defaultConstructor = scriptContext->GetLibrary()->GetInt32ArrayConstructor();
            break;
        case TypeId::TypeIds_Uint32Array:
            defaultConstructor = scriptContext->GetLibrary()->GetUint32ArrayConstructor();
            break;
        case TypeId::TypeIds_Float32Array:
            defaultConstructor = scriptContext->GetLibrary()->GetFloat32ArrayConstructor();
            break;
        case TypeId::TypeIds_Float64Array:
            defaultConstructor = scriptContext->GetLibrary()->GetFloat64ArrayConstructor();
            break;
        default:
            Assert(false);
        }
        return defaultConstructor;
    }

    Var TypedArrayBase::FindMinOrMax(Js::ScriptContext * scriptContext, TypeId typeId, bool findMax)
    {TRACE_IT(64055);
        if (this->IsDetachedBuffer()) // 9.4.5.8 IntegerIndexedElementGet
        {TRACE_IT(64056);
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_DetachedTypedArray);
        }

        switch (typeId)
        {
        case TypeIds_Int8Array:
            return this->FindMinOrMax<int8, false>(scriptContext, findMax);

        case TypeIds_Uint8Array:
        case TypeIds_Uint8ClampedArray:
            return this->FindMinOrMax<uint8, false>(scriptContext, findMax);

        case TypeIds_Int16Array:
            return this->FindMinOrMax<int16, false>(scriptContext, findMax);

        case TypeIds_Uint16Array:
            return this->FindMinOrMax<uint16, false>(scriptContext, findMax);

        case TypeIds_Int32Array:
            return this->FindMinOrMax<int32, false>(scriptContext, findMax);

        case TypeIds_Uint32Array:
            return this->FindMinOrMax<uint32, false>(scriptContext, findMax);

        case TypeIds_Float32Array:
            return this->FindMinOrMax<float, true>(scriptContext, findMax);

        case TypeIds_Float64Array:
            return this->FindMinOrMax<double, true>(scriptContext, findMax);

        default:
            AssertMsg(false, "Unsupported array for fast path");
            return nullptr;
        }
    }

    template<typename T, bool checkNaNAndNegZero>
    Var TypedArrayBase::FindMinOrMax(Js::ScriptContext * scriptContext, bool findMax)
    {TRACE_IT(64057);
        T* typedBuffer = (T*)this->buffer;
        uint len = this->GetLength();

        Assert(sizeof(T)+GetByteOffset() <= GetArrayBuffer()->GetByteLength());
        T currentRes = typedBuffer[0];
        for (uint i = 0; i < len; i++)
        {TRACE_IT(64058);
            Assert((i + 1) * sizeof(T)+GetByteOffset() <= GetArrayBuffer()->GetByteLength());
            T compare = typedBuffer[i];
            if (checkNaNAndNegZero && JavascriptNumber::IsNan(double(compare)))
            {TRACE_IT(64059);
                return scriptContext->GetLibrary()->GetNaN();
            }
            if (findMax ? currentRes < compare : currentRes > compare ||
                (checkNaNAndNegZero && compare == 0 && Js::JavascriptNumber::IsNegZero(double(currentRes))))
            {TRACE_IT(64060);
                currentRes = compare;
            }
        }
        return Js::JavascriptNumber::ToVarNoCheck(currentRes, scriptContext);
    }

    // static
    TypedArrayBase * TypedArrayBase::ValidateTypedArray(Arguments &args, ScriptContext *scriptContext, LPCWSTR apiName)
    {TRACE_IT(64061);
        if (args.Info.Count == 0)
        {TRACE_IT(64062);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        return ValidateTypedArray(args[0], scriptContext, apiName);
    }

    // static
    TypedArrayBase* TypedArrayBase::ValidateTypedArray(Var aValue, ScriptContext *scriptContext, LPCWSTR apiName)
    {TRACE_IT(64063);
        if (!TypedArrayBase::Is(aValue))
        {TRACE_IT(64064);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedTypedArray);
        }

        TypedArrayBase *typedArrayBase = TypedArrayBase::FromVar(aValue);

        if (typedArrayBase->IsDetachedBuffer())
        {TRACE_IT(64065);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, apiName);
        }

        return typedArrayBase;
    }

    // static
    Var TypedArrayBase::TypedArrayCreate(Var constructor, Arguments *args, uint32 length, ScriptContext *scriptContext)
    {TRACE_IT(64066);
        Var newObj = JavascriptOperators::NewScObject(constructor, *args, scriptContext);

        TypedArrayBase::ValidateTypedArray(newObj, scriptContext, nullptr);

        // ECMA262 22.2.4.6 TypedArrayCreate line 3. "If argumentList is a List of a single Number" (args[0] == constructor)
        if (args->Info.Count == 2 && TypedArrayBase::FromVar(newObj)->GetLength() < length)
        {TRACE_IT(64067);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidTypedArrayLength);
        }

        return newObj;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType TypedArrayBase::GetSnapTag_TTD() const
    {TRACE_IT(64068);
        return TTD::NSSnapObjects::SnapObjectType::SnapTypedArrayObject;
    }

    void TypedArrayBase::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(64069);
        TTD::NSSnapObjects::SnapTypedArrayInfo* stai = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapTypedArrayInfo>();
        stai->ArrayBufferAddr = TTD_CONVERT_VAR_TO_PTR_ID(this->GetArrayBuffer());
        stai->ByteOffset = this->GetByteOffset();
        stai->Length = this->GetLength();

        TTD_PTR_ID* depArray = alloc.SlabAllocateArray<TTD_PTR_ID>(1);
        depArray[0] = TTD_CONVERT_VAR_TO_PTR_ID(this->GetArrayBuffer());

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapTypedArrayInfo*, TTD::NSSnapObjects::SnapObjectType::SnapTypedArrayObject>(objData, stai, alloc, 1, depArray);
    }
#endif

    template<>
    inline BOOL Int8Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64070);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToInt8);
    }

    template<>
    inline BOOL Int8VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64071);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToInt8);
    }

    template<>
    inline BOOL Int8Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64072);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToInt8);
    }

    template<>
    inline BOOL Int8VirtualArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64073);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToInt8);
    }

    template<>
    inline Var Int8Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64074);
        return BaseTypedDirectGetItem(index);
    }

#define DIRECT_SET_NO_DETACH_CHECK(TypedArrayName, convertFn) \
    template<> \
    inline BOOL TypedArrayName##::DirectSetItemNoDetachCheck(__in uint32 index, __in Var value) \
    {TRACE_IT(64075); \
        return BaseTypedDirectSetItemNoDetachCheck(index, value, convertFn); \
    }

    DIRECT_SET_NO_DETACH_CHECK(Int8Array, JavascriptConversion::ToInt8);
    DIRECT_SET_NO_DETACH_CHECK(Int8VirtualArray, JavascriptConversion::ToInt8);
    DIRECT_SET_NO_DETACH_CHECK(Uint8Array, JavascriptConversion::ToUInt8);
    DIRECT_SET_NO_DETACH_CHECK(Uint8VirtualArray, JavascriptConversion::ToUInt8);
    DIRECT_SET_NO_DETACH_CHECK(Int16Array, JavascriptConversion::ToInt16);
    DIRECT_SET_NO_DETACH_CHECK(Int16VirtualArray, JavascriptConversion::ToInt16);
    DIRECT_SET_NO_DETACH_CHECK(Uint16Array, JavascriptConversion::ToUInt16);
    DIRECT_SET_NO_DETACH_CHECK(Uint16VirtualArray, JavascriptConversion::ToUInt16);
    DIRECT_SET_NO_DETACH_CHECK(Int32Array, JavascriptConversion::ToInt32);
    DIRECT_SET_NO_DETACH_CHECK(Int32VirtualArray, JavascriptConversion::ToInt32);
    DIRECT_SET_NO_DETACH_CHECK(Uint32Array, JavascriptConversion::ToUInt32);
    DIRECT_SET_NO_DETACH_CHECK(Uint32VirtualArray, JavascriptConversion::ToUInt32);
    DIRECT_SET_NO_DETACH_CHECK(Float32Array, JavascriptConversion::ToFloat);
    DIRECT_SET_NO_DETACH_CHECK(Float32VirtualArray, JavascriptConversion::ToFloat);
    DIRECT_SET_NO_DETACH_CHECK(Float64Array, JavascriptConversion::ToNumber);
    DIRECT_SET_NO_DETACH_CHECK(Float64VirtualArray, JavascriptConversion::ToNumber);
    DIRECT_SET_NO_DETACH_CHECK(Int64Array, JavascriptConversion::ToInt64);
    DIRECT_SET_NO_DETACH_CHECK(Uint64Array, JavascriptConversion::ToUInt64);
    DIRECT_SET_NO_DETACH_CHECK(Uint8ClampedArray, JavascriptConversion::ToUInt8Clamped);
    DIRECT_SET_NO_DETACH_CHECK(Uint8ClampedVirtualArray, JavascriptConversion::ToUInt8Clamped);
    DIRECT_SET_NO_DETACH_CHECK(BoolArray, JavascriptConversion::ToBool);

#define DIRECT_GET_NO_DETACH_CHECK(TypedArrayName) \
    template<> \
    inline Var TypedArrayName##::DirectGetItemNoDetachCheck(__in uint32 index) \
    {TRACE_IT(64076); \
        return BaseTypedDirectGetItemNoDetachCheck(index); \
    }

#define DIRECT_GET_VAR_CHECK_NO_DETACH_CHECK(TypedArrayName) \
    template<> \
    inline Var TypedArrayName##::DirectGetItemNoDetachCheck(__in uint32 index) \
    {TRACE_IT(64077); \
        return DirectGetItemVarCheckNoDetachCheck(index); \
    }

    DIRECT_GET_NO_DETACH_CHECK(Int8Array);
    DIRECT_GET_NO_DETACH_CHECK(Int8VirtualArray);
    DIRECT_GET_NO_DETACH_CHECK(Uint8Array);
    DIRECT_GET_NO_DETACH_CHECK(Uint8VirtualArray);
    DIRECT_GET_NO_DETACH_CHECK(Int16Array);
    DIRECT_GET_NO_DETACH_CHECK(Int16VirtualArray);
    DIRECT_GET_NO_DETACH_CHECK(Uint16Array);
    DIRECT_GET_NO_DETACH_CHECK(Uint16VirtualArray);
    DIRECT_GET_NO_DETACH_CHECK(Int32Array);
    DIRECT_GET_NO_DETACH_CHECK(Int32VirtualArray);
    DIRECT_GET_NO_DETACH_CHECK(Uint32Array);
    DIRECT_GET_NO_DETACH_CHECK(Uint32VirtualArray);
    DIRECT_GET_NO_DETACH_CHECK(Int64Array);
    DIRECT_GET_NO_DETACH_CHECK(Uint64Array);
    DIRECT_GET_NO_DETACH_CHECK(Uint8ClampedArray);
    DIRECT_GET_NO_DETACH_CHECK(Uint8ClampedVirtualArray);
    DIRECT_GET_VAR_CHECK_NO_DETACH_CHECK(Float32Array);
    DIRECT_GET_VAR_CHECK_NO_DETACH_CHECK(Float32VirtualArray);
    DIRECT_GET_VAR_CHECK_NO_DETACH_CHECK(Float64Array);
    DIRECT_GET_VAR_CHECK_NO_DETACH_CHECK(Float64VirtualArray);

#define TypedArrayBeginStub(type) \
        Assert(GetArrayBuffer() || GetArrayBuffer()->GetBuffer()); \
        Assert(index < GetLength()); \
        ScriptContext *scriptContext = GetScriptContext(); \
        type *buffer = (type*)this->buffer + index;

#ifdef _WIN32
#define InterlockedExchangeAdd8 _InterlockedExchangeAdd8
#define InterlockedExchangeAdd16 _InterlockedExchangeAdd16

#define InterlockedAnd8 _InterlockedAnd8
#define InterlockedAnd16 _InterlockedAnd16

#define InterlockedOr8 _InterlockedOr8
#define InterlockedOr16 _InterlockedOr16

#define InterlockedXor8 _InterlockedXor8
#define InterlockedXor16 _InterlockedXor16

#define InterlockedCompareExchange8 _InterlockedCompareExchange8
#define InterlockedCompareExchange16 _InterlockedCompareExchange16

#define InterlockedExchange8 _InterlockedExchange8
#define InterlockedExchange16 _InterlockedExchange16
#endif

#define InterlockedExchangeAdd32 InterlockedExchangeAdd
#define InterlockedAnd32 InterlockedAnd
#define InterlockedOr32 InterlockedOr
#define InterlockedXor32 InterlockedXor
#define InterlockedCompareExchange32 InterlockedCompareExchange
#define InterlockedExchange32 InterlockedExchange

#define TypedArrayAddOp(TypedArrayName, bit, type, convertType, convertFn) \
    template<> \
    inline Var TypedArrayName##::TypedAdd(__in uint32 index, __in Var second) \
    {TRACE_IT(64078); \
        TypedArrayBeginStub(type); \
        type result = (type)InterlockedExchangeAdd##bit((convertType*)buffer, (convertType)convertFn(second, scriptContext)); \
        return JavascriptNumber::ToVar(result, scriptContext); \
    }

#define TypedArrayAndOp(TypedArrayName, bit, type, convertType, convertFn) \
    template<> \
    inline Var TypedArrayName##::TypedAnd(__in uint32 index, __in Var second) \
    {TRACE_IT(64079); \
        TypedArrayBeginStub(type); \
        type result = (type)InterlockedAnd##bit((convertType*)buffer, (convertType)convertFn(second, scriptContext)); \
        return JavascriptNumber::ToVar(result, scriptContext); \
    }

#define TypedArrayCompareExchangeOp(TypedArrayName, bit, type, convertType, convertFn) \
    template<> \
    inline Var TypedArrayName##::TypedCompareExchange(__in uint32 index, __in Var comparand, __in Var replacementValue) \
    {TRACE_IT(64080); \
        TypedArrayBeginStub(type); \
        type result = (type)InterlockedCompareExchange##bit((convertType*)buffer, (convertType)convertFn(replacementValue, scriptContext), (convertType)convertFn(comparand, scriptContext)); \
        return JavascriptNumber::ToVar(result, scriptContext); \
    }

#define TypedArrayExchangeOp(TypedArrayName, bit, type, convertType, convertFn) \
    template<> \
    inline Var TypedArrayName##::TypedExchange(__in uint32 index, __in Var second) \
    {TRACE_IT(64081); \
        TypedArrayBeginStub(type); \
        type result = (type)InterlockedExchange##bit((convertType*)buffer, (convertType)convertFn(second, scriptContext)); \
        return JavascriptNumber::ToVar(result, scriptContext); \
    }

#define TypedArrayLoadOp(TypedArrayName, bit, type, convertType, convertFn) \
    template<> \
    inline Var TypedArrayName##::TypedLoad(__in uint32 index) \
    {TRACE_IT(64082); \
        TypedArrayBeginStub(type); \
        MemoryBarrier(); \
        type result = (type)*buffer; \
        return JavascriptNumber::ToVar(result, scriptContext); \
    }

#define TypedArrayOrOp(TypedArrayName, bit, type, convertType, convertFn) \
    template<> \
    inline Var TypedArrayName##::TypedOr(__in uint32 index, __in Var second) \
    {TRACE_IT(64083); \
        TypedArrayBeginStub(type); \
        type result = (type)InterlockedOr##bit((convertType*)buffer, (convertType)convertFn(second, scriptContext)); \
        return JavascriptNumber::ToVar(result, scriptContext); \
    }

    // Currently the TypedStore is just using the InterlockedExchange to store the value in the buffer.
    // TODO The InterlockedExchange will have the sequential consistency any way, not sure why do we need the Memory barrier or std::atomic::store to perform this.

#define TypedArrayStoreOp(TypedArrayName, bit, type, convertType, convertFn) \
    template<> \
    inline Var TypedArrayName##::TypedStore(__in uint32 index, __in Var second) \
    {TRACE_IT(64084); \
        TypedArrayBeginStub(type); \
        double d = JavascriptConversion::ToInteger(second, scriptContext); \
        convertType s = (convertType)JavascriptConversion::ToUInt32(d); \
        InterlockedExchange##bit((convertType*)buffer, s); \
        return JavascriptNumber::ToVarWithCheck(d, scriptContext); \
    }

#define TypedArraySubOp(TypedArrayName, bit, type, convertType, convertFn) \
    template<> \
    inline Var TypedArrayName##::TypedSub(__in uint32 index, __in Var second) \
    {TRACE_IT(64085); \
        TypedArrayBeginStub(type); \
        type result = (type)InterlockedExchangeAdd##bit((convertType*)buffer, - (convertType)convertFn(second, scriptContext)); \
        return JavascriptNumber::ToVar(result, scriptContext); \
    }

#define TypedArrayXorOp(TypedArrayName, bit, type, convertType, convertFn) \
    template<> \
    inline Var TypedArrayName##::TypedXor(__in uint32 index, __in Var second) \
    {TRACE_IT(64086); \
        TypedArrayBeginStub(type); \
        type result = (type)InterlockedXor##bit((convertType*)buffer, (convertType)convertFn(second, scriptContext)); \
        return JavascriptNumber::ToVar(result, scriptContext); \
    }

#define GenerateNotSupportedStub1(TypedArrayName, fnName) \
    template<> \
    inline Var TypedArrayName##::Typed##fnName(__in uint32 accessIndex) \
    {TRACE_IT(64087); \
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray); \
    }

#define GenerateNotSupportedStub2(TypedArrayName, fnName) \
    template<> \
    inline Var TypedArrayName##::Typed##fnName(__in uint32 accessIndex, __in Var value) \
    {TRACE_IT(64088); \
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray); \
    }

#define GenerateNotSupportedStub3(TypedArrayName, fnName) \
    template<> \
    inline Var TypedArrayName##::Typed##fnName(__in uint32 accessIndex, __in Var first, __in Var value) \
    {TRACE_IT(64089); \
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray); \
    }

#define GENERATE_FOREACH_TYPEDARRAY(TYPEDARRAY_DEF, NOTSUPPORTEDSTUB, OP) \
        TYPEDARRAY_DEF(Int8Array, 8, int8, char, JavascriptConversion::ToInt8); \
        TYPEDARRAY_DEF(Int8VirtualArray, 8, int8, char, JavascriptConversion::ToInt8); \
        TYPEDARRAY_DEF(Uint8Array, 8, uint8, char, JavascriptConversion::ToUInt8); \
        TYPEDARRAY_DEF(Uint8VirtualArray, 8, uint8, char, JavascriptConversion::ToUInt8); \
        TYPEDARRAY_DEF(Int16Array, 16, int16, short, JavascriptConversion::ToInt16); \
        TYPEDARRAY_DEF(Int16VirtualArray, 16, int16, short, JavascriptConversion::ToInt16); \
        TYPEDARRAY_DEF(Uint16Array, 16, uint16, short, JavascriptConversion::ToUInt16); \
        TYPEDARRAY_DEF(Uint16VirtualArray, 16, uint16, short, JavascriptConversion::ToUInt16); \
        TYPEDARRAY_DEF(Int32Array, 32, int32, LONG, JavascriptConversion::ToInt32); \
        TYPEDARRAY_DEF(Int32VirtualArray, 32, int32, LONG, JavascriptConversion::ToInt32); \
        TYPEDARRAY_DEF(Uint32Array, 32, uint32, LONG, JavascriptConversion::ToUInt32); \
        TYPEDARRAY_DEF(Uint32VirtualArray, 32, uint32, LONG, JavascriptConversion::ToUInt32); \
        NOTSUPPORTEDSTUB(Float32Array, OP); \
        NOTSUPPORTEDSTUB(Float32VirtualArray, OP); \
        NOTSUPPORTEDSTUB(Float64Array, OP); \
        NOTSUPPORTEDSTUB(Float64VirtualArray, OP); \
        NOTSUPPORTEDSTUB(Int64Array, OP); \
        NOTSUPPORTEDSTUB(Uint64Array, OP); \
        NOTSUPPORTEDSTUB(Uint8ClampedArray, OP); \
        NOTSUPPORTEDSTUB(Uint8ClampedVirtualArray, OP); \
        NOTSUPPORTEDSTUB(BoolArray, OP);

    GENERATE_FOREACH_TYPEDARRAY(TypedArrayAddOp, GenerateNotSupportedStub2, Add)
    GENERATE_FOREACH_TYPEDARRAY(TypedArrayAndOp, GenerateNotSupportedStub2, And)
    GENERATE_FOREACH_TYPEDARRAY(TypedArrayCompareExchangeOp, GenerateNotSupportedStub3, CompareExchange)
    GENERATE_FOREACH_TYPEDARRAY(TypedArrayExchangeOp, GenerateNotSupportedStub2, Exchange)
    GENERATE_FOREACH_TYPEDARRAY(TypedArrayLoadOp, GenerateNotSupportedStub1, Load)
    GENERATE_FOREACH_TYPEDARRAY(TypedArrayOrOp, GenerateNotSupportedStub2, Or)
    GENERATE_FOREACH_TYPEDARRAY(TypedArrayStoreOp, GenerateNotSupportedStub2, Store)
    GENERATE_FOREACH_TYPEDARRAY(TypedArraySubOp, GenerateNotSupportedStub2, Sub)
    GENERATE_FOREACH_TYPEDARRAY(TypedArrayXorOp, GenerateNotSupportedStub2, Xor)

    template<>
    inline Var Int8VirtualArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64090);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Uint8Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64091);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToUInt8);
    }

    template<>
    inline BOOL Uint8VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64092);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToUInt8);
    }

    template<>
    inline BOOL Uint8Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64093);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToUInt8);
    }

    template<>
    inline BOOL Uint8VirtualArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64094);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToUInt8);
    }

    template<>
    inline Var Uint8Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64095);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline Var Uint8VirtualArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64096);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Uint8ClampedArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64097);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToUInt8Clamped);
    }

    template<>
    inline BOOL Uint8ClampedArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64098);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToUInt8Clamped);
    }

    template<>
    inline Var Uint8ClampedArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64099);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Uint8ClampedVirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64100);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToUInt8Clamped);
    }

    template<>
    inline BOOL Uint8ClampedVirtualArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64101);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToUInt8Clamped);
    }

    template<>
    inline Var Uint8ClampedVirtualArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64102);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Int16Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64103);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToInt16);
    }

    template<>
    inline BOOL Int16Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64104);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToInt16);
    }

    template<>
    inline Var Int16Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64105);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Int16VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64106);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToInt16);
    }

    template<>
    inline BOOL Int16VirtualArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64107);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToInt16);
    }

    template<>
    inline Var Int16VirtualArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64108);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Uint16Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64109);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToUInt16);
    }

    template<>
    inline BOOL Uint16Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64110);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToUInt16);
    }

    template<>
    inline Var Uint16Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64111);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Uint16VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64112);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToUInt16);
    }

    template<>
    inline BOOL Uint16VirtualArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64113);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToUInt16);
    }

    template<>
    inline Var Uint16VirtualArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64114);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Int32Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64115);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToInt32);
    }

    template<>
    inline BOOL Int32Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64116);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToInt32);
    }

    template<>
    inline Var Int32Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64117);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Int32VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64118);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToInt32);
    }

    template<>
    inline BOOL Int32VirtualArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64119);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToInt32);
    }

    template<>
    inline Var Int32VirtualArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64120);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Uint32Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64121);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToUInt32);
    }

    template<>
    inline BOOL Uint32Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64122);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToUInt32);
    }

    template<>
    inline Var Uint32Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64123);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Uint32VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64124);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToUInt32);
    }

    template<>
    inline BOOL Uint32VirtualArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64125);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToUInt32);
    }

    template<>
    inline Var Uint32VirtualArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64126);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Float32Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64127);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToFloat);
    }

    template<>
    inline BOOL Float32Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64128);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToFloat);
    }

    template<>
    Var Float32Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64129);
        return TypedDirectGetItemWithCheck(index);
    }

    template<>
    inline BOOL Float32VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64130);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToFloat);
    }

    template<>
    inline BOOL Float32VirtualArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64131);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToFloat);
    }

    template<>
    Var Float32VirtualArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64132);
        return TypedDirectGetItemWithCheck(index);
    }

    template<>
    inline BOOL Float64Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64133);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToNumber);
    }

    template<>
    inline BOOL Float64Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64134);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToNumber);
    }

    template<>
    inline Var Float64Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64135);
        return TypedDirectGetItemWithCheck(index);
    }

    template<>
    inline BOOL Float64VirtualArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64136);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToNumber);
    }

    template<>
    inline BOOL Float64VirtualArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64137);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToNumber);
    }

    template<>
    inline Var Float64VirtualArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64138);
        return TypedDirectGetItemWithCheck(index);
    }

    template<>
    inline BOOL Int64Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64139);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToInt64);
    }

    template<>
    inline BOOL Int64Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64140);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToInt64);
    }

    template<>
    inline Var Int64Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64141);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL Uint64Array::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64142);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToUInt64);
    }

    template<>
    inline BOOL Uint64Array::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64143);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToUInt64);
    }

    template<>
    inline Var Uint64Array::DirectGetItem(__in uint32 index)
    {TRACE_IT(64144);
        return BaseTypedDirectGetItem(index);
    }

    template<>
    inline BOOL BoolArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64145);
        return BaseTypedDirectSetItem(index, value, JavascriptConversion::ToBool);
    }

    template<>
    inline BOOL BoolArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64146);
        return BaseTypedDirectSetItemNoSet(index, value, JavascriptConversion::ToBool);
    }

    template<>
    inline Var BoolArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64147);
        if (index < GetLength())
        {TRACE_IT(64148);
            Assert((index+1)* sizeof(bool) +GetByteOffset() <= GetArrayBuffer()->GetByteLength());
            bool* typedBuffer = (bool*)buffer;
            return typedBuffer[index] ? GetLibrary()->GetTrue() : GetLibrary()->GetFalse();
        }
        return GetLibrary()->GetUndefined();
    }

    template<>
    inline Var BoolArray::DirectGetItemNoDetachCheck(__in uint32 index)
    {TRACE_IT(64149);
        Assert((index + 1)* sizeof(bool) + GetByteOffset() <= GetArrayBuffer()->GetByteLength());
        bool* typedBuffer = (bool*)buffer;
        return typedBuffer[index] ? GetLibrary()->GetTrue() : GetLibrary()->GetFalse();
    }

    Var CharArray::Create(ArrayBufferBase* arrayBuffer, uint32 byteOffSet, uint32 mappedLength, JavascriptLibrary* javascriptLibrary)
    {TRACE_IT(64150);
        CharArray* arr;
        uint32 totalLength, mappedByteLength;
        if (UInt32Math::Mul(mappedLength, sizeof(char16), &mappedByteLength) ||
            UInt32Math::Add(byteOffSet, mappedByteLength, &totalLength) ||
            (totalLength > arrayBuffer->GetByteLength()))
        {TRACE_IT(64151);
            JavascriptError::ThrowRangeError(arrayBuffer->GetScriptContext(), JSERR_InvalidTypedArrayLength);
        }
        arr = RecyclerNew(javascriptLibrary->GetRecycler(), CharArray, arrayBuffer, byteOffSet, mappedLength, javascriptLibrary->GetCharArrayType());
        return arr;
    }

    BOOL CharArray::Is(Var value)
    {TRACE_IT(64152);
        return JavascriptOperators::GetTypeId(value) == Js::TypeIds_CharArray;
    }

    Var CharArray::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        AssertMsg(FALSE, "not supported in char array");
        JavascriptError::ThrowTypeError(function->GetScriptContext(), JSERR_This_NeedTypedArray);
    }

    Var CharArray::EntrySubarray(RecyclableObject* function, CallInfo callInfo, ...)
    {
        AssertMsg(FALSE, "not supported in char array");
        JavascriptError::ThrowTypeError(function->GetScriptContext(), JSERR_This_NeedTypedArray);
    }

    inline Var CharArray::Subarray(uint32 begin, uint32 end)
    {
        AssertMsg(FALSE, "not supported in char array");
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_This_NeedTypedArray);
    }

    inline CharArray* CharArray::FromVar(Var aValue)
    {TRACE_IT(64153);
        AssertMsg(CharArray::Is(aValue), "invalid CharArray");
        return static_cast<CharArray*>(RecyclableObject::FromVar(aValue));
    }

    inline BOOL CharArray::DirectSetItem(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64154);
        ScriptContext* scriptContext = GetScriptContext();
        // A typed array is Integer Indexed Exotic object, so doing a get translates to 9.4.5.9 IntegerIndexedElementSet
        LPCWSTR asString = (Js::JavascriptConversion::ToString(value, scriptContext))->GetSz();

        if (this->IsDetachedBuffer())
        {TRACE_IT(64155);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray);
        }

        if (index >= GetLength())
        {TRACE_IT(64156);
            return FALSE;
        }

        AssertMsg(index < GetLength(), "Trying to set out of bound index for typed array.");
        Assert((index + 1)* sizeof(char16) + GetByteOffset() <= GetArrayBuffer()->GetByteLength());
        char16* typedBuffer = (char16*)buffer;

        if (asString != NULL && ::wcslen(asString) == 1)
        {TRACE_IT(64157);
            typedBuffer[index] = asString[0];
        }
        else
        {TRACE_IT(64158);
            Js::JavascriptError::MapAndThrowError(scriptContext, E_INVALIDARG);
        }

        return TRUE;
    }

    inline BOOL CharArray::DirectSetItemNoSet(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64159);
        ScriptContext* scriptContext = GetScriptContext();
        // A typed array is Integer Indexed Exotic object, so doing a get translates to 9.4.5.9 IntegerIndexedElementSet
        Js::JavascriptConversion::ToString(value, scriptContext);

        if (this->IsDetachedBuffer())
        {TRACE_IT(64160);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray);
        }

        return FALSE;
    }

    inline BOOL CharArray::DirectSetItemNoDetachCheck(__in uint32 index, __in Js::Var value)
    {TRACE_IT(64161);
        return DirectSetItem(index, value);
    }

    inline Var CharArray::DirectGetItemNoDetachCheck(__in uint32 index)
    {TRACE_IT(64162);
        return DirectGetItem(index);
    }

    Var CharArray::TypedAdd(__in uint32 index, Var second)
    {TRACE_IT(64163);
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray);
    }

    Var CharArray::TypedAnd(__in uint32 index, Var second)
    {TRACE_IT(64164);
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray);
    }

    Var CharArray::TypedCompareExchange(__in uint32 index, Var comparand, Var replacementValue)
    {TRACE_IT(64165);
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray);
    }

    Var CharArray::TypedExchange(__in uint32 index, Var second)
    {TRACE_IT(64166);
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray);
    }

    Var CharArray::TypedLoad(__in uint32 index)
    {TRACE_IT(64167);
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray);
    }

    Var CharArray::TypedOr(__in uint32 index, Var second)
    {TRACE_IT(64168);
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray);
    }

    Var CharArray::TypedStore(__in uint32 index, Var second)
    {TRACE_IT(64169);
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray);
    }

    Var CharArray::TypedSub(__in uint32 index, Var second)
    {TRACE_IT(64170);
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray);
    }

    Var CharArray::TypedXor(__in uint32 index, Var second)
    {TRACE_IT(64171);
        JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_InvalidOperationOnTypedArray);
    }

    inline Var CharArray::DirectGetItem(__in uint32 index)
    {TRACE_IT(64172);
        // A typed array is Integer Indexed Exotic object, so doing a get translates to 9.4.5.8 IntegerIndexedElementGet
        if (this->IsDetachedBuffer())
        {TRACE_IT(64173);
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_DetachedTypedArray);
        }
        if (index < GetLength())
        {TRACE_IT(64174);
            Assert((index + 1)* sizeof(char16)+GetByteOffset() <= GetArrayBuffer()->GetByteLength());
            char16* typedBuffer = (char16*)buffer;
            return GetLibrary()->GetCharStringCache().GetStringForChar(typedBuffer[index]);
        }
        return GetLibrary()->GetUndefined();
    }

    Var CharArray::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        function->GetScriptContext()->GetThreadContext()->ProbeStack(Js::Constants::MinStackDefault, function->GetScriptContext());

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Assert(!(callInfo.Flags & CallFlags_New) || args[0] == nullptr);
        Var object = TypedArrayBase::CreateNewInstance(args, scriptContext, sizeof(char16), CharArray::Create);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {TRACE_IT(64175);
            object = Js::JavascriptProxy::AutoProxyWrapper(object);
        }
#endif
        return object;
    }
}
