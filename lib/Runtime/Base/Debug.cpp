//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#if DBG_DUMP
#ifndef USING_PAL_STDLIB
#include <strsafe.h>
#endif
#include "Language/JavascriptStackWalker.h"

WCHAR* DumpCallStack(uint frameCount) {TRACE_IT(33642); return DumpCallStackFull(frameCount, /*print*/ true); }

WCHAR* DumpCallStackFull(uint frameCount, bool print)
{TRACE_IT(33643);
    Js::ScriptContext* scriptContext = ThreadContext::GetContextForCurrentThread()->GetScriptContextList();
    Js::JavascriptStackWalker walker(scriptContext);

    WCHAR buffer[512];
    Js::StringBuilder<ArenaAllocator> sb(scriptContext->GeneralAllocator());
    uint fc = 0;
    while (walker.Walk())
    {TRACE_IT(33644);
        void * codeAddr = walker.GetCurrentCodeAddr();
        if (walker.IsJavascriptFrame())
        {TRACE_IT(33645);
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            StringCchPrintf(buffer, _countof(buffer), _u("0x%p  "), codeAddr);
            sb.AppendSz(buffer);
            // Found a JavascriptFunction.  Dump its name and parameters.
            Js::JavascriptFunction *jsFunc = walker.GetCurrentFunction();

            Js::FunctionBody * jsBody = jsFunc->GetFunctionBody();
            const Js::CallInfo callInfo = walker.GetCallInfo();
            const WCHAR* sourceFileName = _u("NULL");
            ULONG line = 0; LONG column = 0;
            walker.GetSourcePosition(&sourceFileName, &line, &column);

            StringCchPrintf(buffer, _countof(buffer), _u("%s [%s] (0x%p, Args=%u"), jsBody->GetDisplayName(), jsBody->GetDebugNumberSet(debugStringBuffer), jsFunc,
                callInfo.Count);
            sb.AppendSz(buffer);

            for (uint i = 0; i < callInfo.Count; i++)
            {
                StringCchPrintf(buffer, _countof(buffer), _u(", 0x%p"), walker.GetJavascriptArgs()[i]);
                sb.AppendSz(buffer);
            }
            StringCchPrintf(buffer, _countof(buffer), _u(")[%s (%u, %d)]\n"), sourceFileName, line + 1, column + 1);
            sb.AppendSz(buffer);
            fc++;
            if(fc >= frameCount)
            {TRACE_IT(33646);
                break;
            }
       }
    }
    sb.AppendCppLiteral(_u("----------------------------------------------------------------------\n"));
    WCHAR* stack = sb.Detach();
    if(print)
    {TRACE_IT(33647);
        Output::Print(stack);
    }
    return stack;
}
#endif
