//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "JsrtPch.h"
#include "JsrtInternal.h"
#include "jsrtHelper.h"
#include "JsrtContextCore.h"
#include "ChakraCore.h"

CHAKRA_API
JsInitializeModuleRecord(
    _In_opt_ JsModuleRecord referencingModule,
    _In_ JsValueRef normalizedSpecifier,
    _Outptr_result_maybenull_ JsModuleRecord* moduleRecord)
{TRACE_IT(27515);
    PARAM_NOT_NULL(moduleRecord);

    Js::SourceTextModuleRecord* childModuleRecord = nullptr;

    JsErrorCode errorCode = ContextAPIWrapper_NoRecord<true>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {
        childModuleRecord = Js::SourceTextModuleRecord::Create(scriptContext);
        if (referencingModule == nullptr)
        {TRACE_IT(27516);
            childModuleRecord->SetIsRootModule();
        }
        if (normalizedSpecifier != JS_INVALID_REFERENCE)
        {TRACE_IT(27517);
            childModuleRecord->SetSpecifier(normalizedSpecifier);
            if (Js::SourceTextModuleRecord::Is(referencingModule) && Js::JavascriptString::Is(normalizedSpecifier))
            {TRACE_IT(27518);
                childModuleRecord->SetParent(Js::SourceTextModuleRecord::FromHost(referencingModule), Js::JavascriptString::FromVar(normalizedSpecifier)->GetSz());
            }
        }
        return JsNoError;
    });
    if (errorCode == JsNoError)
    {TRACE_IT(27519);
        *moduleRecord = childModuleRecord;
    }
    else
    {TRACE_IT(27520);
        *moduleRecord = JS_INVALID_REFERENCE;
    }
    return errorCode;
}

CHAKRA_API
JsParseModuleSource(
    _In_ JsModuleRecord requestModule,
    _In_ JsSourceContext sourceContext,
    _In_ byte* sourceText,
    _In_ unsigned int sourceLength,
    _In_ JsParseModuleSourceFlags sourceFlag,
    _Outptr_result_maybenull_ JsValueRef* exceptionValueRef)
{TRACE_IT(27521);
    PARAM_NOT_NULL(requestModule);
    PARAM_NOT_NULL(exceptionValueRef);
    if (sourceFlag > JsParseModuleSourceFlags_DataIsUTF8)
    {TRACE_IT(27522);
        return JsErrorInvalidArgument;
    }

    *exceptionValueRef = JS_INVALID_REFERENCE;
    HRESULT hr;
    if (!Js::SourceTextModuleRecord::Is(requestModule))
    {TRACE_IT(27523);
        return JsErrorInvalidArgument;
    }
    Js::SourceTextModuleRecord* moduleRecord = Js::SourceTextModuleRecord::FromHost(requestModule);
    if (moduleRecord->WasParsed())
    {TRACE_IT(27524);
        return JsErrorModuleParsed;
    }
    Js::ScriptContext* scriptContext = moduleRecord->GetScriptContext();
    JsErrorCode errorCode = GlobalAPIWrapper_NoRecord([&]() -> JsErrorCode {
        SourceContextInfo* sourceContextInfo = scriptContext->GetSourceContextInfo(sourceContext, nullptr);
        if (sourceContextInfo == nullptr)
        {TRACE_IT(27525);
            sourceContextInfo = scriptContext->CreateSourceContextInfo(sourceContext, nullptr, 0, nullptr, nullptr, 0);
        }
        SRCINFO si = {
            /* sourceContextInfo   */ sourceContextInfo,
            /* dlnHost             */ 0,
            /* ulColumnHost        */ 0,
            /* lnMinHost           */ 0,
            /* ichMinHost          */ 0,
            /* ichLimHost          */ static_cast<ULONG>(sourceLength),
            /* ulCharOffset        */ 0,
            /* mod                 */ 0,
            /* grfsi               */ 0
        };
        hr = moduleRecord->ParseSource(sourceText, sourceLength, &si, exceptionValueRef, sourceFlag == JsParseModuleSourceFlags_DataIsUTF8 ? true : false);
        if (FAILED(hr))
        {TRACE_IT(27526);
            return JsErrorScriptCompile;
        }
        return JsNoError;
    });
    return errorCode;
}

CHAKRA_API
JsModuleEvaluation(
    _In_ JsModuleRecord requestModule,
    _Outptr_result_maybenull_ JsValueRef* result)
{TRACE_IT(27527);
    if (!Js::SourceTextModuleRecord::Is(requestModule))
    {TRACE_IT(27528);
        return JsErrorInvalidArgument;
    }
    Js::SourceTextModuleRecord* moduleRecord = Js::SourceTextModuleRecord::FromHost(requestModule);
    if (moduleRecord->WasEvaluated())
    {TRACE_IT(27529);
        return JsErrorModuleEvaluated;
    }
    if (result != nullptr)
    {TRACE_IT(27530);
        *result = JS_INVALID_REFERENCE;
    }
    Js::ScriptContext* scriptContext = moduleRecord->GetScriptContext();
    JsrtContext* jsrtContext = (JsrtContext*)scriptContext->GetLibrary()->GetJsrtContext();
    JsErrorCode errorCode = SetContextAPIWrapper(jsrtContext, [&](Js::ScriptContext *scriptContext) -> JsErrorCode {
        SmartFPUControl smartFpuControl;
        if (smartFpuControl.HasErr())
        {TRACE_IT(27531);
            return JsErrorBadFPUState;
        }
        JsValueRef returnRef = moduleRecord->ModuleEvaluation();
        if (result != nullptr)
        {TRACE_IT(27532);
            *result = returnRef;
        }
        return JsNoError;
    });
    return errorCode;
}

CHAKRA_API
JsSetModuleHostInfo(
    _In_ JsModuleRecord requestModule,
    _In_ JsModuleHostInfoKind moduleHostInfo,
    _In_ void* hostInfo)
{TRACE_IT(27533);
    if (!Js::SourceTextModuleRecord::Is(requestModule))
    {TRACE_IT(27534);
        return JsErrorInvalidArgument;
    }
    Js::SourceTextModuleRecord* moduleRecord = Js::SourceTextModuleRecord::FromHost(requestModule);
    Js::ScriptContext* scriptContext = moduleRecord->GetScriptContext();
    JsrtContext* jsrtContext = (JsrtContext*)scriptContext->GetLibrary()->GetJsrtContext();
    JsErrorCode errorCode = SetContextAPIWrapper(jsrtContext, [&](Js::ScriptContext *scriptContext) -> JsErrorCode {
        JsrtContextCore* currentContext = static_cast<JsrtContextCore*>(JsrtContextCore::GetCurrent());
        switch (moduleHostInfo)
        {
        case JsModuleHostInfo_Exception:
            moduleRecord->OnHostException(hostInfo);
            break;
        case JsModuleHostInfo_HostDefined:
            moduleRecord->SetHostDefined(hostInfo);
            break;
        case JsModuleHostInfo_FetchImportedModuleCallback:
            currentContext->GetHostScriptContext()->SetFetchImportedModuleCallback(reinterpret_cast<FetchImportedModuleCallBack>(hostInfo));
            break;
        case JsModuleHostInfo_NotifyModuleReadyCallback:
            currentContext->GetHostScriptContext()->SetNotifyModuleReadyCallback(reinterpret_cast<NotifyModuleReadyCallback>(hostInfo));
            break;
        default:
            return JsInvalidModuleHostInfoKind;
        };
        return JsNoError;
    });
    return errorCode;
}

CHAKRA_API
JsGetModuleHostInfo(
    _In_  JsModuleRecord requestModule,
    _In_ JsModuleHostInfoKind moduleHostInfo,
    _Outptr_result_maybenull_ void** hostInfo)
{TRACE_IT(27535);
    if (!Js::SourceTextModuleRecord::Is(requestModule) || (hostInfo == nullptr))
    {TRACE_IT(27536);
        return JsErrorInvalidArgument;
    }
    *hostInfo = nullptr;
    Js::SourceTextModuleRecord* moduleRecord = Js::SourceTextModuleRecord::FromHost(requestModule);
    Js::ScriptContext* scriptContext = moduleRecord->GetScriptContext();
    JsrtContext* jsrtContext = (JsrtContext*)scriptContext->GetLibrary()->GetJsrtContext();
    JsErrorCode errorCode = SetContextAPIWrapper(jsrtContext, [&](Js::ScriptContext *scriptContext) -> JsErrorCode {
        JsrtContextCore* currentContext = static_cast<JsrtContextCore*>(JsrtContextCore::GetCurrent());
        switch (moduleHostInfo)
        {
        case JsModuleHostInfo_Exception:
            if (moduleRecord->GetErrorObject() != nullptr)
            {TRACE_IT(27537);
                *hostInfo = moduleRecord->GetErrorObject();
            }
            break;
        case JsModuleHostInfo_HostDefined:
            *hostInfo = moduleRecord->GetHostDefined();
            break;
        case JsModuleHostInfo_FetchImportedModuleCallback:
            *hostInfo = reinterpret_cast<void*>(currentContext->GetHostScriptContext()->GetFetchImportedModuleCallback());
            break;
        case JsModuleHostInfo_NotifyModuleReadyCallback:
            *hostInfo = reinterpret_cast<void*>(currentContext->GetHostScriptContext()->GetNotifyModuleReadyCallback());
            break;
        default:
            return JsInvalidModuleHostInfoKind;
        };
        return JsNoError;
    });
    return errorCode;
}
