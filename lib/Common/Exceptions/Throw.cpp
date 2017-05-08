//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "CommonExceptionsPch.h"

#ifndef USING_PAL_STDLIB
// === C Runtime Header Files ===
#pragma warning(push)
#pragma warning(disable: 4995) /* 'function': name was marked as #pragma deprecated */
#include <strsafe.h>
#pragma warning(pop)
#endif

#include "StackOverflowException.h"
#include "AsmJsParseException.h"
#include "OutOfMemoryException.h"
#include "NotImplementedException.h"

// Header files required before including ConfigFlagsTable.h

#include "EnumHelp.h"
#include "Common/MathUtil.h"
#include "Core/AllocSizeMath.h"
#include "Core/FaultInjection.h"

#include "Core/BasePtr.h"
#include "Core/AutoFile.h"
#include "Core/Output.h"

// Memory Management
namespace Memory {
    class ArenaAllocator;
}
using namespace Memory;
#include "Memory/Allocator.h"
#include "Memory/HeapAllocator.h"
#include "Memory/RecyclerPointers.h"

// Data structure
#include "DataStructures/Comparer.h"
#include "DataStructures/SizePolicy.h"
#include "DataStructures/SList.h"
#include "DataStructures/KeyValuePair.h"
#include "DataStructures/DefaultContainerLockPolicy.h"
#include "DataStructures/BaseDictionary.h"
#include "Core/ConfigFlagsTable.h"

#include "Core/StackBackTrace.h"


#ifdef GENERATE_DUMP
// dbghelp.h is not clean with warning 4091
#pragma warning(push)
#pragma warning(disable: 4091) /* warning C4091: 'typedef ': ignored on left of '' when no variable is declared */
#include <dbghelp.h>
#pragma warning(pop)
#endif // GENERATE_DUMP

extern "C"{
    BOOLEAN IsMessageBoxWPresent();
}

namespace Js {
#if defined(GENERATE_DUMP) && defined(STACK_BACK_TRACE)
    THREAD_LOCAL StackBackTrace * Throw::stackBackTrace = nullptr;
#endif
    void Throw::FatalInternalError()
    {TRACE_IT(22553);
        int scenario = 2;
        ReportFatalException(NULL, E_FAIL, Fatal_Internal_Error, scenario);
    }

    void Throw::FatalInternalErrorEx(int scenario)
    {
        ReportFatalException(NULL, E_FAIL, Fatal_Internal_Error, scenario);
    }

    void Throw::FatalProjectionError()
    {TRACE_IT(22554);
        RaiseException((DWORD)DBG_TERMINATE_PROCESS, EXCEPTION_NONCONTINUABLE, 0, NULL);
    }

    void Throw::InternalError()
    {
        AssertOrFailFastMsg(false, "Internal error!!");
    }

    void Throw::OutOfMemory()
    {TRACE_IT(22555);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (CONFIG_FLAG(PrintSystemException))
        {TRACE_IT(22556);
            Output::Print(_u("SystemException: OutOfMemory\n"));
            Output::Flush();
        }
#endif
        if (JsUtil::ExternalApi::RaiseOutOfMemoryIfScriptActive())
        {
            AssertMsg(false, "We shouldn't be here");
        }
        throw OutOfMemoryException();
    }
    void Throw::CheckAndThrowOutOfMemory(BOOLEAN status)
    {TRACE_IT(22557);
        if (!status)
        {TRACE_IT(22558);
            OutOfMemory();
        }
    }
    void Throw::StackOverflow(ScriptContext *scriptContext, PVOID returnAddress)
    {TRACE_IT(22559);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (CONFIG_FLAG(PrintSystemException))
        {TRACE_IT(22560);
            Output::Print(_u("SystemException: StackOverflow\n"));
            Output::Flush();
        }
#endif
        if (JsUtil::ExternalApi::RaiseStackOverflowIfScriptActive(scriptContext, returnAddress))
        {
            AssertMsg(false, "We shouldn't be here");
        }
        throw StackOverflowException();
    }

    void Throw::NotImplemented()
    {
        AssertMsg(false, "This functionality is not yet implemented");

        throw NotImplementedException();
    }

#ifdef GENERATE_DUMP
    CriticalSection Throw::csGenerateDump;
    void Throw::GenerateDump(LPCWSTR filePath, bool terminate, bool needLock)
    {TRACE_IT(22561);
        __try
        {
            if (terminate)
            {TRACE_IT(22562);
                RaiseException((DWORD)DBG_TERMINATE_PROCESS, EXCEPTION_NONCONTINUABLE, 0, NULL);
            }
            else
            {
                RaiseException(0, 0, 0, NULL);
            }
        }
        __except(Throw::GenerateDump(GetExceptionInformation(), filePath,
            terminate? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER), needLock)
        {TRACE_IT(22563);
            // we don't do anything interesting in this handler
        }
    }

    void Throw::GenerateDumpForAssert(LPCWSTR filePath)
    {TRACE_IT(22564);
        __try
        {
            RaiseException(STATUS_ASSERTION_FAILURE, EXCEPTION_NONCONTINUABLE, 0, NULL);
        }
        __except (Throw::GenerateDump(GetExceptionInformation(), filePath, EXCEPTION_CONTINUE_SEARCH), false)
        {TRACE_IT(22565);
            // no-op
        }
    }

    int Throw::GenerateDump(PEXCEPTION_POINTERS exceptInfo, LPCWSTR filePath, int ret, bool needLock)
    {TRACE_IT(22566);
        WCHAR tempFilePath[MAX_PATH];
        WCHAR tempFileName[MAX_PATH];
        HANDLE hTempFile;
        DWORD retVal;

        if (filePath == NULL)
        {TRACE_IT(22567);
            retVal = GetTempPath(MAX_PATH, tempFilePath);

            if (retVal > MAX_PATH || (retVal == 0))
            {TRACE_IT(22568);
                return ret;
            }
            filePath = tempFilePath;
        }

        StringCchPrintf(tempFileName, _countof(tempFileName), _u("%s\\CH_%u_%u.dmp"), filePath, GetCurrentProcessId(), GetCurrentThreadId());
        Output::Print(_u("dump filename %s \n"), tempFileName);
        Output::Flush();

        hTempFile = CreateFile(tempFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (hTempFile == INVALID_HANDLE_VALUE)
        {TRACE_IT(22569);
            return GetLastError();
        }

        MINIDUMP_EXCEPTION_INFORMATION dumpExceptInfo;
        dumpExceptInfo.ThreadId = GetCurrentThreadId();
        dumpExceptInfo.ExceptionPointers = exceptInfo;
        dumpExceptInfo.ClientPointers = FALSE;

        {
            MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithPrivateReadWriteMemory);

            // Generating full dump for the TE process (reason : it contains both managed and native memory)
            if (CONFIG_FLAG(FullMemoryDump))
            {TRACE_IT(22570);
                dumpType = static_cast<MINIDUMP_TYPE>(dumpType | MiniDumpWithFullMemory);
            }

            BOOL dumpGenerated = false;
            if (needLock)
            {TRACE_IT(22571);
                // the critical section might have been destructed at process shutdown time. At that time we don't need
                // to lock.
                AutoCriticalSection autocs(&csGenerateDump);

                dumpGenerated = MiniDumpWriteDump(GetCurrentProcess(),
                    GetCurrentProcessId(),
                    hTempFile,
                    dumpType,
                    &dumpExceptInfo,
                    NULL,
                    NULL);
            }
            else
            {TRACE_IT(22572);
                dumpGenerated = MiniDumpWriteDump(GetCurrentProcess(),
                    GetCurrentProcessId(),
                    hTempFile,
                    dumpType,
                    &dumpExceptInfo,
                    NULL,
                    NULL);
            }
            if (!dumpGenerated)
            {TRACE_IT(22573);
                Output::Print(_u("Unable to write minidump (0x%08X)\n"), GetLastError());
                Output::Flush();
            }
        }
        FlushFileBuffers(hTempFile);
        CloseHandle(hTempFile);
        return ret;
    }
#endif // GENERATE_DUMP

#if DBG
    // After assert the program should terminate. Sometime we saw the program continue somehow
    // log the existence of assert for debugging.
    void Throw::LogAssert()
    {TRACE_IT(22574);
        IsInAssert = true;

#if defined(GENERATE_DUMP) && defined(STACK_BACK_TRACE)
        // This should be the last thing to happen in the process. Therefore, leaks are not an issue.
        stackBackTrace = StackBackTrace::Capture(&NoCheckHeapAllocator::Instance, Throw::StackToSkip, Throw::StackTraceDepth);
#endif
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
#define CHAKRA_ASSERT_CAPTION _u("CHAKRA ASSERT")
#endif

    bool Throw::ReportAssert(__in LPCSTR fileName, uint lineNumber, __in LPCSTR error, __in LPCSTR message)
    {TRACE_IT(22575);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::AssertBreakFlag))
        {TRACE_IT(22576);
            DebugBreak();
            return false;
        }
        if (Js::Configuration::Global.flags.IsEnabled(Js::AssertIgnoreFlag))
        {TRACE_IT(22577);
            return true;
        }
#endif
        if (AssertsToConsole)
        {
            fprintf(stderr, "ASSERTION %u: (%s, line %u) %s\n Failure: %s\n", GetCurrentProcessId(), fileName, lineNumber, message, error);
            fflush(stderr);
#ifdef GENERATE_DUMP
            // force dump if we have assert in jc.exe. check build only.
            if (!Js::Configuration::Global.flags.IsEnabled(Js::DumpOnCrashFlag))
            {TRACE_IT(22578);
                return false;
            }
            Throw::GenerateDumpForAssert(Js::Configuration::Global.flags.DumpOnCrash);
#else
            return false;
#endif
        }

        // The following code is applicable only when we are hosted in an
        // GUI environment
#if defined(ENABLE_DEBUG_CONFIG_OPTIONS) && defined(_WIN32)
        // Then if DumpOncrashFlag is not specified it directly returns,
        // otherwise if will raise a non-continuable exception, generate the dump and terminate the process.
        // the popup message box might be useful when testing in IE
        if (Js::Configuration::Global.flags.AssertPopUp && IsMessageBoxWPresent())
        {TRACE_IT(22579);
            char16 buff[1024];

            swprintf_s(buff, _countof(buff), _u("%S (%u)\n%S\n%S"), fileName, lineNumber, message, error);
            buff[_countof(buff)-1] = 0;

            int ret = MessageBox(nullptr, buff, CHAKRA_ASSERT_CAPTION, MB_ABORTRETRYIGNORE);

            switch (ret)
            {
            case IDIGNORE:
                return true;
            case IDABORT:
                Throw::FatalInternalError();
            default:
                return false;
            }
        }
#endif
        return false;
    }

#endif

} // namespace Js
