//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#ifdef LEAK_REPORT
// Initialization order
//  AB AutoSystemInfo
//  AD PerfCounter
//  AE PerfCounterSet
//  AM Output/Configuration
//  AN MemProtectHeap
//  AP DbgHelpSymbolManager
//  AQ CFGLogger
//  AR LeakReport
//  AS JavascriptDispatch/RecyclerObjectDumper
//  AT HeapAllocator/RecyclerHeuristic
//  AU RecyclerWriteBarrierManager
#pragma warning(disable:4075)       // initializers put in unrecognized initialization area on purpose
#pragma init_seg(".CRT$XCAR")

CriticalSection LeakReport::s_cs;
DWORD LeakReport::nestedSectionCount = 0;
DWORD LeakReport::nestedRedirectOutputCount = 0;
AutoFILE LeakReport::file;
FILE * oldFile = nullptr;
bool LeakReport::openReportFileFailed = false;
LeakReport::UrlRecord * LeakReport::urlRecordHead = nullptr;
LeakReport::UrlRecord * LeakReport::urlRecordTail = nullptr;

void
LeakReport::StartRedirectOutput()
{TRACE_IT(24639);
    if (!EnsureLeakReportFile())
    {TRACE_IT(24640);
        return;
    }
    s_cs.Enter();
    if (nestedRedirectOutputCount == 0)
    {TRACE_IT(24641);
        Assert(oldFile == nullptr);
        oldFile = Output::SetFile(file);
    }
    nestedRedirectOutputCount++;
}

void
LeakReport::EndRedirectOutput()
{TRACE_IT(24642);
    if (nestedRedirectOutputCount == 0)
    {TRACE_IT(24643);
        return;
    }
    Assert(file != nullptr);
    nestedRedirectOutputCount--;

    if (nestedRedirectOutputCount == 0)
    {TRACE_IT(24644);
        fflush(file);
        FILE * tmpFile = Output::SetFile(oldFile);
        Assert(tmpFile == file);
        oldFile = nullptr;
    }
    s_cs.Leave();
}

void
LeakReport::StartSection(char16 const * msg, ...)
{
    va_list argptr;
    va_start(argptr, msg);
    StartSection(msg, argptr);
    va_end(argptr);
}

void
LeakReport::StartSection(char16 const * msg, va_list argptr)
{TRACE_IT(24645);
    s_cs.Enter();
    if (!EnsureLeakReportFile())
    {TRACE_IT(24646);
        return;
    }
    nestedSectionCount++;


    Print(_u("--------------------------------------------------------------------------------\n"));
    vfwprintf(file, msg, argptr);
    Print(_u("\n"));
    Print(_u("--------------------------------------------------------------------------------\n"));
}

void
LeakReport::EndSection()
{TRACE_IT(24647);
    s_cs.Leave();
    if (file == nullptr)
    {TRACE_IT(24648);
        return;
    }
    nestedSectionCount--;
}

void
LeakReport::Print(char16 const * msg, ...)
{
    AutoCriticalSection autocs(&s_cs);
    if (!EnsureLeakReportFile())
    {TRACE_IT(24649);
        return;
    }

    va_list argptr;
    va_start(argptr, msg);
    vfwprintf(file, msg, argptr);
    va_end(argptr);
}

bool
LeakReport::EnsureLeakReportFile()
{TRACE_IT(24650);
    AutoCriticalSection autocs(&s_cs);
    if (openReportFileFailed)
    {TRACE_IT(24651);
        return false;
    }
    if (file != nullptr)
    {TRACE_IT(24652);
        return true;
    }

    char16 const * filename = Js::Configuration::Global.flags.LeakReport;
    char16 const * openMode = _u("w+");
    char16 defaultFilename[_MAX_PATH];
    if (filename == nullptr)
    {TRACE_IT(24653);
        // xplat-todo: Implement swprintf_s in the PAL
#ifdef _MSC_VER
        swprintf_s(defaultFilename, _u("jsleakreport-%u.txt"), ::GetCurrentProcessId());
#else
        _snwprintf(defaultFilename, _countof(defaultFilename), _u("jsleakreport-%u.txt"), ::GetCurrentProcessId());
#endif

        filename = defaultFilename;
        openMode = _u("a+");   // append mode
    }
    if (_wfopen_s(&file, filename, openMode) != 0)
    {TRACE_IT(24654);
        openReportFileFailed = true;
        return false;
    }
    Print(_u("================================================================================\n"));
    Print(_u("Chakra Leak Report - PID: %d\n"), ::GetCurrentProcessId());

    return true;
}

LeakReport::UrlRecord *
LeakReport::LogUrl(char16 const * url, void * globalObject)
{TRACE_IT(24655);
    UrlRecord * record = NoCheckHeapNewStruct(UrlRecord);

    size_t length = wcslen(url) + 1; // Add 1 for the NULL.
    char16* urlCopy = NoCheckHeapNewArray(char16, length);
    js_memcpy_s(urlCopy, (length - 1) * sizeof(char16), url, (length - 1) * sizeof(char16));
    urlCopy[length - 1] = _u('\0');

    record->url = urlCopy;
#if _MSC_VER
    record->time = _time64(NULL);
#else
    record->time = time(NULL);
#endif
    record->tid = ::GetCurrentThreadId();
    record->next = nullptr;
    record->scriptEngine = nullptr;
    record->globalObject = globalObject;

    AutoCriticalSection autocs(&s_cs);
    if (LeakReport::urlRecordHead == nullptr)
    {TRACE_IT(24656);
        Assert(LeakReport::urlRecordTail == nullptr);
        LeakReport::urlRecordHead = record;
        LeakReport::urlRecordTail = record;
    }
    else
    {TRACE_IT(24657);
        LeakReport::urlRecordTail->next = record;
        LeakReport::urlRecordTail = record;
    }

    return record;
}

void
LeakReport::DumpUrl(DWORD tid)
{TRACE_IT(24658);
    AutoCriticalSection autocs(&s_cs);
    if (!EnsureLeakReportFile())
    {TRACE_IT(24659);
        return;
    }

    UrlRecord * prev = nullptr;
    UrlRecord ** pprev = &LeakReport::urlRecordHead;
    UrlRecord * curr = *pprev;
    while (curr != nullptr)
    {TRACE_IT(24660);
        if (curr->tid == tid)
        {TRACE_IT(24661);
            char16 timeStr[26] = _u("00:00");

            // xplat-todo: Need to implement _wasctime_s in the PAL
#if _MSC_VER
            struct tm local_time;
            _localtime64_s(&local_time, &curr->time);
            _wasctime_s(timeStr, &local_time);
#endif
            timeStr[wcslen(timeStr) - 1] = 0;
            Print(_u("%s - (%p, %p) %s\n"), timeStr, curr->scriptEngine, curr->globalObject, curr->url);
            *pprev = curr->next;
            NoCheckHeapDeleteArray(wcslen(curr->url) + 1, curr->url);
            NoCheckHeapDelete(curr);
        }
        else
        {TRACE_IT(24662);
            pprev = &curr->next;
            prev = curr;
        }
        curr = *pprev;
    }

    if (prev == nullptr)
    {TRACE_IT(24663);
        LeakReport::urlRecordTail = nullptr;
    }
    else if (prev->next == nullptr)
    {TRACE_IT(24664);
        LeakReport::urlRecordTail = prev;
    }
}

AutoLeakReportSection::AutoLeakReportSection(Js::ConfigFlagsTable& flags, char16 const * msg, ...):
    m_flags(flags)
{TRACE_IT(24665);
    if (flags.IsEnabled(Js::LeakReportFlag))
    {TRACE_IT(24666);
        va_list argptr;
        va_start(argptr, msg);
        LeakReport::StartSection(msg, argptr);
        va_end(argptr);
    }
}

AutoLeakReportSection::~AutoLeakReportSection()
{TRACE_IT(24667);
    if (m_flags.IsEnabled(Js::LeakReportFlag))
    {TRACE_IT(24668);
        LeakReport::EndSection();
    }
}
#endif
