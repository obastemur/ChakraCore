//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//  Implements typed array.
//----------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{

    Var DataView::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);
        uint32 byteLength = 0;
        uint32 mappedLength;
        int32 offset = 0;
        ArrayBufferBase* arrayBuffer = nullptr;
        DataView* dataView;

        //1.    If NewTarget is undefined, throw a TypeError exception.
        if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
        {LOGMEIN("DataView.cpp] 31\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("DataView"));
        }

        if (args.Info.Count < 2)
        {LOGMEIN("DataView.cpp] 36\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, _u("buffer"));
        }

        // Currently the only reason we check for an external object is projection related, so it remains under conditional compilation.
        RecyclableObject* jsArraySource = NULL;
        if (JavascriptOperators::IsObject(args[1]) && JavascriptConversion::ToObject(args[1], scriptContext, &jsArraySource))
        {LOGMEIN("DataView.cpp] 43\n");
            ArrayBuffer *ab = nullptr;
            HRESULT hr = scriptContext->GetHostScriptContext()->ArrayBufferFromExternalObject(jsArraySource, &ab);
            switch (hr)
            {LOGMEIN("DataView.cpp] 47\n");
            case S_OK:
            case S_FALSE:
                arrayBuffer = ab;
                // Both of these cases will be handled by the arrayBuffer null check.
                break;

            default:
                // Any FAILURE HRESULT or unexpected HRESULT.
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_InvalidArgument, _u("buffer"));
                break;
            }
        }

        //2.    If Type(buffer) is not Object, throw a TypeError exception.
        //3.    If buffer does not have an [[ArrayBufferData]] internal slot, throw a TypeError exception.
        if (arrayBuffer == nullptr)
        {LOGMEIN("DataView.cpp] 64\n");
            if (ArrayBufferBase::Is(args[1]))
            {LOGMEIN("DataView.cpp] 66\n");
                arrayBuffer = ArrayBufferBase::FromVar(args[1]);
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, _u("buffer"));
            }
        }

        //4.    Let offset be ToIndex(byteOffset).
        if (args.Info.Count > 2)
        {LOGMEIN("DataView.cpp] 77\n");
            Var secondArgument = args[2];
            offset = ArrayBuffer::ToIndex(secondArgument, JSERR_ArrayLengthConstructIncorrect, scriptContext, ArrayBuffer::MaxArrayBufferLength, false);
        }

        //5.    If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
        if (arrayBuffer->IsDetached())
        {LOGMEIN("DataView.cpp] 84\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }

        //6.    Let bufferByteLength be the value of buffer's[[ArrayBufferByteLength]] internal slot.
        //7.   If offset > bufferByteLength, throw a RangeError exception.
        byteLength = arrayBuffer->GetByteLength();
        if ((uint32)offset > byteLength)
        {LOGMEIN("DataView.cpp] 92\n");
            JavascriptError::ThrowRangeError(
                scriptContext, JSERR_DataView_InvalidArgument, _u("byteOffset"));
        }

        //8.   If byteLength is either not present or is undefined, then
        //      a.  Let viewByteLength be bufferByteLength - offset.
        //9.   Else,
        //      a.  Let viewByteLength be ToIndex(byteLength).
        //      b.  If offset + viewByteLength > bufferByteLength, throw a RangeError exception.
        if (args.Info.Count > 3 && !JavascriptOperators::IsUndefinedObject(args[3]))
            {LOGMEIN("DataView.cpp] 103\n");
                Var thirdArgument = args[3];
                mappedLength = ArrayBuffer::ToIndex(thirdArgument, JSERR_ArrayLengthConstructIncorrect, scriptContext, ArrayBuffer::MaxArrayBufferLength, false);
                uint32 viewRange = mappedLength + offset;

                if (viewRange > byteLength || viewRange < mappedLength) // overflow indicates out-of-range
                {LOGMEIN("DataView.cpp] 109\n");
                    JavascriptError::ThrowRangeError(
                        scriptContext, JSERR_DataView_InvalidArgument, _u("byteLength"));
                }
            }
        else
        {
            mappedLength = byteLength - offset;
        }

        //10.   Let O be OrdinaryCreateFromConstructor(NewTarget, "%DataViewPrototype%", [[DataView]], [[ViewedArrayBuffer]], [[ByteLength]], [[ByteOffset]]).
        //11.   Set O's[[DataView]] internal slot to true.
        //12.   Set O's[[ViewedArrayBuffer]] internal slot to buffer.
        //13.   Set O's[[ByteLength]] internal slot to viewByteLength.
        //14.   Set O's[[ByteOffset]] internal slot to offset.
        //15.   Return O.
        dataView = scriptContext->GetLibrary()->CreateDataView(arrayBuffer, offset, mappedLength);
        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), dataView, nullptr, scriptContext) :
            dataView;
    }

    DataView::DataView(ArrayBufferBase* arrayBuffer, uint32 byteoffset, uint32 mappedLength, DynamicType* type)
        : ArrayBufferParent(type, mappedLength, arrayBuffer),
          byteOffset(byteoffset)
    {LOGMEIN("DataView.cpp] 134\n");
        buffer = arrayBuffer->GetBuffer() + byteoffset;
    }

    BOOL DataView::Is(Var aValue)
    {LOGMEIN("DataView.cpp] 139\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_DataView;
    }

    Var DataView::EntryGetInt8(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 153\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {LOGMEIN("DataView.cpp] 157\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, _u("offset"));
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->template GetValue<int8>(offset, _u("DataView.prototype.GetInt8"), FALSE);
    }

    Var DataView::EntryGetUint8(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 176\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {LOGMEIN("DataView.cpp] 180\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset"));
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<uint8>(offset, _u("DataView.prototype.GetUint8"), FALSE);
    }

    Var DataView::EntryGetInt16(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 200\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {LOGMEIN("DataView.cpp] 204\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, _u("offset"));
        }
        if (args.Info.Count > 2)
        {LOGMEIN("DataView.cpp] 208\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<int16>(offset, _u("DataView.prototype.GetInt16"), isLittleEndian);
    }

    Var DataView::EntryGetUint16(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 228\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {LOGMEIN("DataView.cpp] 232\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset"));
        }
        if (args.Info.Count > 2)
        {LOGMEIN("DataView.cpp] 236\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->template GetValue<uint16>(offset, _u("DataView.prototype.GetUint16"), isLittleEndian);
    }

    Var DataView::EntryGetUint32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 256\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {LOGMEIN("DataView.cpp] 260\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset"));
        }
        if (args.Info.Count > 2)
        {LOGMEIN("DataView.cpp] 264\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<uint32>(offset, _u("DataView.prototype.GetUint32"), isLittleEndian);
    }

    Var DataView::EntryGetInt32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 284\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {LOGMEIN("DataView.cpp] 288\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, _u("offset"));
        }
        if (args.Info.Count > 2)
        {LOGMEIN("DataView.cpp] 292\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValue<int32>(offset, _u("DataView.prototype.GetInt32"), isLittleEndian);
    }

    Var DataView::EntryGetFloat32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 312\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {LOGMEIN("DataView.cpp] 316\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset"));
        }
        if (args.Info.Count > 2)
        {LOGMEIN("DataView.cpp] 320\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValueWithCheck<float>(offset, _u("DataView.prototype.GetFloat32"), isLittleEndian);
    }

    Var DataView::EntryGetFloat64(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 340\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 2)
        {LOGMEIN("DataView.cpp] 344\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset"));
        }
        if (args.Info.Count > 2)
        {LOGMEIN("DataView.cpp] 348\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[2], scriptContext);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        return dataView->GetValueWithCheck<double>(offset, _u("DataView.prototype.GetFloat64"), isLittleEndian);
    }

    Var DataView::EntrySetInt8(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 367\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {LOGMEIN("DataView.cpp] 371\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset or value"));
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        int8 value = JavascriptConversion::ToInt8(args[2], scriptContext);
        dataView->SetValue<int8>(offset, value, _u("DataView.prototype.SetInt8"));
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetUint8(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 391\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {LOGMEIN("DataView.cpp] 395\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset or value"));
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        uint8 value = JavascriptConversion::ToUInt8(args[2], scriptContext);
        dataView->SetValue<uint8>(offset, value, _u("DataView.prototype.SetUint8"));
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetInt16(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 416\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {LOGMEIN("DataView.cpp] 420\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset or value"));
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        int16 value = JavascriptConversion::ToInt16(args[2], scriptContext);
        if (args.Info.Count > 3)
        {LOGMEIN("DataView.cpp] 427\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<int16>(offset, value, _u("DataView.prototype.SetInt16"), isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetUint16(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 445\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {LOGMEIN("DataView.cpp] 449\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument, _u("offset or value"));
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        uint16 value = JavascriptConversion::ToUInt16(args[2], scriptContext);
        if (args.Info.Count > 3)
        {LOGMEIN("DataView.cpp] 456\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<uint16>(offset, value, _u("DataView.prototype.SetUint16"), isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetInt32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 474\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {LOGMEIN("DataView.cpp] 478\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset or value"));
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        int32 value = JavascriptConversion::ToInt32(args[2], scriptContext);
        if (args.Info.Count > 3)
        {LOGMEIN("DataView.cpp] 485\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<int32>(offset, value, _u("DataView.prototype.SetInt32"), isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetUint32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 503\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {LOGMEIN("DataView.cpp] 507\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset or value"));
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        uint32 value = JavascriptConversion::ToUInt32(args[2], scriptContext);
        if (args.Info.Count > 3)
        {LOGMEIN("DataView.cpp] 514\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<uint32>(offset, value, _u("DataView.prototype.SetUint32"), isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetFloat32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 532\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView,  _u("offset or value"));
        }
        if (args.Info.Count < 3)
        {LOGMEIN("DataView.cpp] 536\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument);
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        float value = JavascriptConversion::ToFloat(args[2], scriptContext);
        if (args.Info.Count > 3)
        {LOGMEIN("DataView.cpp] 543\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<float>(offset, value, _u("DataView.prototype.SetFloat32"), isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntrySetFloat64(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        BOOL isLittleEndian = FALSE;

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 561\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }
        if (args.Info.Count < 3)
        {LOGMEIN("DataView.cpp] 565\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DataView_NeedArgument,  _u("offset or value"));
        }
        DataView* dataView = DataView::FromVar(args[0]);
        uint32 offset = JavascriptConversion::ToUInt32(args[1], scriptContext);
        double value = JavascriptConversion::ToNumber(args[2], scriptContext);
        if (args.Info.Count > 3)
        {LOGMEIN("DataView.cpp] 572\n");
            isLittleEndian = JavascriptConversion::ToBoolean(args[3], scriptContext);
        }
        dataView->SetValue<double>(offset, value, _u("DataView.prototype.SetFloat64"), isLittleEndian);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var DataView::EntryGetterBuffer(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 589\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        ArrayBufferBase* arrayBuffer = dataView->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {LOGMEIN("DataView.cpp] 597\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }

        return arrayBuffer;
    }

    Var DataView::EntryGetterByteLength(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 614\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        ArrayBufferBase* arrayBuffer = dataView->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {LOGMEIN("DataView.cpp] 622\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {LOGMEIN("DataView.cpp] 626\n");
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(dataView->GetLength(), scriptContext);
    }

    Var DataView::EntryGetterByteOffset(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !DataView::Is(args[0]))
        {LOGMEIN("DataView.cpp] 643\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDataView);
        }

        DataView* dataView = DataView::FromVar(args[0]);
        ArrayBufferBase* arrayBuffer = dataView->GetArrayBuffer();

        if (arrayBuffer == nullptr)
        {LOGMEIN("DataView.cpp] 651\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }
        else if (arrayBuffer->IsDetached())
        {LOGMEIN("DataView.cpp] 655\n");
            return TaggedInt::ToVarUnchecked(0);
        }

        return JavascriptNumber::ToVar(dataView->GetByteOffset(), scriptContext);
    }

    BOOL DataView::SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {
        AssertMsg(false, "We don't need a DataView to serve as object's internal array");
        return FALSE;
    }

#ifdef _M_ARM
    // Provide template specialization (only) for memory access at unaligned float/double address which causes data alignment exception otherwise.
    template<>
    Var DataView::GetValueWithCheck<float>(uint32 byteOffset, const char16 *funcName, BOOL isLittleEndian)
    {LOGMEIN("DataView.cpp] 672\n");
        return this->GetValueWithCheck<float, float UNALIGNED*>(byteOffset, isLittleEndian, funcName);
    }

    template<>
    Var DataView::GetValueWithCheck<double>(uint32 byteOffset, const char16 *funcName, BOOL isLittleEndian)
    {LOGMEIN("DataView.cpp] 678\n");
        return this->GetValueWithCheck<double, double UNALIGNED*>(byteOffset, isLittleEndian, funcName);
    }

    template<>
    void DataView::SetValue<float>(uint32 byteOffset, float value, const char16 *funcName, BOOL isLittleEndian)
    {LOGMEIN("DataView.cpp] 684\n");
        this->SetValue<float, float UNALIGNED*>(byteOffset, value, isLittleEndian, funcName);
    }

    template<>
    void DataView::SetValue<double>(uint32 byteOffset, double value, const char16 *funcName, BOOL isLittleEndian)
    {LOGMEIN("DataView.cpp] 690\n");
        this->SetValue<double, double UNALIGNED*>(byteOffset, value, isLittleEndian, funcName);
    }
#endif

}
