//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#ifdef ENABLE_FOUNDATION_OBJECT

using namespace Windows::Foundation::Diagnostics;

#define IfFailReturnNULL(op) \
    if (FAILED(hr=(op))) {TRACE_IT(37991); return NULL; }; \

namespace Js
{
    inline DelayLoadWinRtString* WindowsFoundationAdapter::GetWinRtStringLibrary(_In_ ScriptContext* scriptContext)
    {TRACE_IT(37992);
        return scriptContext->GetThreadContext()->GetWinRTStringLibrary();
    }

    inline DelayLoadWinRtFoundation* WindowsFoundationAdapter::GetWinRtFoundationLibrary(_In_ ScriptContext* scriptContext)
    {TRACE_IT(37993);
        return scriptContext->GetThreadContext()->GetWinRtFoundationLibrary();
    }

    IActivationFactory* WindowsFoundationAdapter::GetAsyncCausalityTracerActivationFactory(_In_ ScriptContext* scriptContext)
    {TRACE_IT(37994);
        if (! asyncCausalityTracerActivationFactory)
        {TRACE_IT(37995);
            HRESULT hr;
            HSTRING hString;
            HSTRING_HEADER hStringHdr;
            LPCWSTR factoryName = _u("Windows.Foundation.Diagnostics.AsyncCausalityTracer");
            UINT32 factoryNameLen = _countof(_u("Windows.Foundation.Diagnostics.AsyncCausalityTracer")) - 1;
            IID factoryIID = __uuidof(IAsyncCausalityTracerStatics);

            IfFailReturnNULL(GetWinRtStringLibrary(scriptContext)->WindowsCreateStringReference(factoryName, factoryNameLen, &hStringHdr, &hString));
            IfFailReturnNULL(GetWinRtFoundationLibrary(scriptContext)->RoGetActivationFactory(hString, factoryIID, &asyncCausalityTracerActivationFactory));

            Assert(asyncCausalityTracerActivationFactory != NULL);
        }

        return asyncCausalityTracerActivationFactory;
    }

    IAsyncCausalityTracerStatics* WindowsFoundationAdapter::GetAsyncCausalityTracerStatics(_In_ ScriptContext* scriptContext)
    {TRACE_IT(37996);
        if (! asyncCausalityTracerStatics)
        {TRACE_IT(37997);
            IActivationFactory* factory = GetAsyncCausalityTracerActivationFactory(scriptContext);
            if (!factory)
            {TRACE_IT(37998);
                return NULL;
            }

            HRESULT hr;
            IfFailReturnNULL(factory->QueryInterface(__uuidof(IAsyncCausalityTracerStatics), reinterpret_cast<void**>(&asyncCausalityTracerStatics)));

            Assert(asyncCausalityTracerStatics != NULL);
        }

        return asyncCausalityTracerStatics;
    }

    HRESULT WindowsFoundationAdapter::TraceOperationCreation(
        _In_ ScriptContext* scriptContext,
        _In_ INT traceLevel,
        _In_ INT source,
        _In_ GUID platformId,
        _In_ UINT64 operationId,
        _In_z_ PCWSTR operationName,
        _In_ UINT64 relatedContext)
    {TRACE_IT(37999);
        HRESULT hr;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;

        Assert(traceLevel <= CausalityTraceLevel::CausalityTraceLevel_Verbose && traceLevel >= CausalityTraceLevel_Required);
        Assert(source <= CausalitySource::CausalitySource_System && source >= CausalitySource_Application);
        size_t operationNameLen = wcslen(operationName);
        if (operationNameLen > UINT_MAX)
        {TRACE_IT(38000);
            return E_OUTOFMEMORY;
        }

        IFFAILRET(GetWinRtStringLibrary(scriptContext)->WindowsCreateStringReference(operationName, static_cast<UINT32>(operationNameLen), &hStringHdr, &hString));

        IAsyncCausalityTracerStatics* tracerStatics = GetAsyncCausalityTracerStatics(scriptContext);

        if (!tracerStatics)
        {TRACE_IT(38001);
            return E_UNEXPECTED;
        }

        return tracerStatics->TraceOperationCreation((CausalityTraceLevel)traceLevel, (CausalitySource)source, platformId, operationId, hString, relatedContext);
    }

    HRESULT WindowsFoundationAdapter::TraceOperationCompletion(
        _In_ ScriptContext* scriptContext,
        _In_ INT traceLevel,
        _In_ INT source,
        _In_ GUID platformId,
        _In_ UINT64 operationId,
        _In_ INT status)
    {TRACE_IT(38002);
        Assert(traceLevel <= CausalityTraceLevel::CausalityTraceLevel_Verbose && traceLevel >= CausalityTraceLevel_Required);
        Assert(source <= CausalitySource::CausalitySource_System && source >= CausalitySource_Application);
        Assert(status <= (INT)AsyncStatus::Error && status >= (INT)AsyncStatus::Started);

        IAsyncCausalityTracerStatics* tracerStatics = GetAsyncCausalityTracerStatics(scriptContext);

        if (!tracerStatics)
        {TRACE_IT(38003);
            return E_UNEXPECTED;
        }

        return tracerStatics->TraceOperationCompletion((CausalityTraceLevel)traceLevel, (CausalitySource)source, platformId, operationId, (AsyncStatus)status);
    }

    HRESULT WindowsFoundationAdapter::TraceOperationRelation(
        _In_ ScriptContext* scriptContext,
        _In_ INT traceLevel,
        _In_ INT source,
        _In_ GUID platformId,
        _In_ UINT64 operationId,
        _In_ INT relation)
    {TRACE_IT(38004);
        Assert(traceLevel <= CausalityTraceLevel::CausalityTraceLevel_Verbose && traceLevel >= CausalityTraceLevel_Required);
        Assert(source <= CausalitySource::CausalitySource_System && source >= CausalitySource_Application);
        Assert(relation <= CausalityRelation::CausalityRelation_Error && relation >= CausalityRelation_AssignDelegate);

        IAsyncCausalityTracerStatics* tracerStatics = GetAsyncCausalityTracerStatics(scriptContext);

        if (!tracerStatics)
        {TRACE_IT(38005);
            return E_UNEXPECTED;
        }

        return tracerStatics->TraceOperationRelation((CausalityTraceLevel)traceLevel, (CausalitySource)source, platformId, operationId, (CausalityRelation)relation);
    }

    HRESULT WindowsFoundationAdapter::TraceSynchronousWorkStart(
        _In_ ScriptContext* scriptContext,
        _In_ INT traceLevel,
        _In_ INT source,
        _In_ GUID platformId,
        _In_ UINT64 operationId,
        _In_ INT work)
    {TRACE_IT(38006);
        Assert(traceLevel <= CausalityTraceLevel::CausalityTraceLevel_Verbose && traceLevel >= CausalityTraceLevel_Required);
        Assert(source <= CausalitySource::CausalitySource_System && source >= CausalitySource_Application);
        Assert(work <= CausalitySynchronousWork::CausalitySynchronousWork_Execution && work >= CausalitySynchronousWork_CompletionNotification);

        IAsyncCausalityTracerStatics* tracerStatics = GetAsyncCausalityTracerStatics(scriptContext);

        if (!tracerStatics)
        {TRACE_IT(38007);
            return E_UNEXPECTED;
        }

        return tracerStatics->TraceSynchronousWorkStart((CausalityTraceLevel)traceLevel, (CausalitySource)source, platformId, operationId, (CausalitySynchronousWork)work);
    }

    HRESULT WindowsFoundationAdapter::TraceSynchronousWorkCompletion(
        _In_ ScriptContext* scriptContext,
        _In_ INT traceLevel,
        _In_ INT source,
        _In_ INT work)
    {TRACE_IT(38008);
        Assert(traceLevel <= CausalityTraceLevel::CausalityTraceLevel_Verbose && traceLevel >= CausalityTraceLevel_Required);
        Assert(source <= CausalitySource::CausalitySource_System && source >= CausalitySource_Application);
        Assert(work <= CausalitySynchronousWork::CausalitySynchronousWork_Execution && work >= CausalitySynchronousWork_CompletionNotification);

        IAsyncCausalityTracerStatics* tracerStatics = GetAsyncCausalityTracerStatics(scriptContext);

        if (!tracerStatics)
        {TRACE_IT(38009);
            return E_UNEXPECTED;
        }

        return tracerStatics->TraceSynchronousWorkCompletion((CausalityTraceLevel)traceLevel, (CausalitySource)source, (CausalitySynchronousWork)work);
    }

}
#endif
