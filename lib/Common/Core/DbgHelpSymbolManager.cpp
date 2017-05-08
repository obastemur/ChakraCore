//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"

#ifdef DBGHELP_SYMBOL_MANAGER
#include "Core/DbgHelpSymbolManager.h"

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
#pragma init_seg(".CRT$XCAP")

DbgHelpSymbolManager DbgHelpSymbolManager::Instance;

void
DbgHelpSymbolManager::Initialize()
{TRACE_IT(19960);
    char16 *wszSearchPath = nullptr;
    char16 *wszModuleDrive = nullptr;
    char16 *wszModuleDir = nullptr;
    char16 *wszOldSearchPath = nullptr;
    char16 *wszNewSearchPath = nullptr;
    char16 *wszModuleName = nullptr;
    char16 const *wszModule = nullptr;

    const size_t ceModuleName = _MAX_PATH;
    const size_t ceOldSearchPath = 32767;
    const size_t ceNewSearchPath = ceOldSearchPath + _MAX_PATH + 1;

    if (isInitialized)
    {TRACE_IT(19961);
        return;
    }

    AutoCriticalSection autocs(&cs);
    if (isInitialized)
    {TRACE_IT(19962);
        goto end;
    }
    isInitialized = true;
    hProcess = GetCurrentProcess();

    // Let's make sure the directory where chakra.dll is, is on the symbol path.

    wszModule = AutoSystemInfo::GetJscriptDllFileName();
    wszModuleName = NoCheckHeapNewArray(char16, ceModuleName);
    if (wszModuleName == nullptr)
    {TRACE_IT(19963);
        goto end;
    }

    if (wcscmp(wszModule, _u("")) == 0)
    {
        if (GetModuleFileName(NULL, wszModuleName, static_cast<DWORD>(ceModuleName)))
        {TRACE_IT(19964);
            wszModule = wszModuleName;
        }
        else
        {TRACE_IT(19965);
            wszModule = nullptr;
        }
    }

    if (wszModule != nullptr)
    {TRACE_IT(19966);
        wszModuleDrive = NoCheckHeapNewArray(char16, _MAX_DRIVE);
        if (wszModuleDrive == nullptr)
        {TRACE_IT(19967);
            goto end;
        }

        wszModuleDir = NoCheckHeapNewArray(char16, _MAX_DIR);
        if (wszModuleDir == nullptr)
        {TRACE_IT(19968);
            goto end;
        }

        _wsplitpath_s(wszModule, wszModuleDrive, _MAX_DRIVE, wszModuleDir, _MAX_DIR, NULL, 0, NULL, 0);
        _wmakepath_s(wszModuleName, ceModuleName, wszModuleDrive, wszModuleDir, NULL, NULL);

        wszOldSearchPath = NoCheckHeapNewArray(char16, ceOldSearchPath);
        if (wszOldSearchPath == nullptr)
        {TRACE_IT(19969);
            goto end;
        }

        wszNewSearchPath = NoCheckHeapNewArray(char16, ceNewSearchPath);
        if (wszNewSearchPath == nullptr)
        {TRACE_IT(19970);
            goto end;
        }

        if (GetEnvironmentVariable(_u("_NT_SYMBOL_PATH"), wszOldSearchPath, static_cast<DWORD>(ceOldSearchPath)) != 0)
        {
            swprintf_s(wszNewSearchPath, ceNewSearchPath, _u("%s;%s"), wszOldSearchPath, wszModuleName);
            wszSearchPath = wszNewSearchPath;
        }
        else
        {TRACE_IT(19971);
            wszSearchPath = wszModuleName;
        }
    }

    hDbgHelpModule = LoadLibraryEx(_u("dbghelp.dll"), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hDbgHelpModule == nullptr)
    {TRACE_IT(19972);
        goto end;
    }

    {
        typedef BOOL(__stdcall *PfnSymInitialize)(HANDLE, PCWSTR, BOOL);
        PfnSymInitialize pfnSymInitialize = (PfnSymInitialize)GetProcAddress(hDbgHelpModule, "SymInitializeW");
        if (pfnSymInitialize)
        {
            pfnSymInitialize(hProcess, wszSearchPath, TRUE);
            pfnSymFromAddrW = (PfnSymFromAddrW)GetProcAddress(hDbgHelpModule, "SymFromAddrW");
            pfnSymGetLineFromAddr64W = (PfnSymGetLineFromAddr64W)GetProcAddress(hDbgHelpModule, "SymGetLineFromAddrW64");

            // load line information
            typedef DWORD(__stdcall *PfnSymGetOptions)();
            typedef VOID(__stdcall *PfnSymSetOptions)(DWORD);
            PfnSymGetOptions pfnSymGetOptions = (PfnSymGetOptions)GetProcAddress(hDbgHelpModule, "SymGetOptions");
            PfnSymSetOptions pfnSymSetOptions = (PfnSymSetOptions)GetProcAddress(hDbgHelpModule, "SymSetOptions");

            DWORD options = pfnSymGetOptions();
            options |= SYMOPT_LOAD_LINES;
            pfnSymSetOptions(options);
        }
    }

end:
    if (wszModuleName != nullptr)
    {
        NoCheckHeapDeleteArray(ceModuleName, wszModuleName);
        wszModuleName = nullptr;
    }

    if (wszModuleDrive != nullptr)
    {
        NoCheckHeapDeleteArray(_MAX_DRIVE, wszModuleDrive);
        wszModuleDrive = nullptr;
    }

    if (wszModuleDir != nullptr)
    {
        NoCheckHeapDeleteArray(_MAX_DIR, wszModuleDir);
        wszModuleDir = nullptr;
    }

    if (wszOldSearchPath != nullptr)
    {
        NoCheckHeapDeleteArray(ceOldSearchPath, wszOldSearchPath);
        wszOldSearchPath = nullptr;
    }

    if (wszNewSearchPath != nullptr)
    {
        NoCheckHeapDeleteArray(ceNewSearchPath, wszNewSearchPath);
        wszNewSearchPath = nullptr;
    }
}

DbgHelpSymbolManager::~DbgHelpSymbolManager()
{TRACE_IT(19973);
    if (hDbgHelpModule)
    {TRACE_IT(19974);
        typedef BOOL(__stdcall *PfnSymCleanup)(HANDLE);
        PfnSymCleanup pfnSymCleanup = (PfnSymCleanup)GetProcAddress(hDbgHelpModule, "SymCleanup");
        if (pfnSymCleanup)
        {TRACE_IT(19975);
            pfnSymCleanup(hProcess);
        }

        FreeLibrary(hDbgHelpModule);
    }
}

BOOL
DbgHelpSymbolManager::SymFromAddr(PVOID address, DWORD64 * dwDisplacement, PSYMBOL_INFO pSymbol)
{TRACE_IT(19976);
    if (Instance.pfnSymFromAddrW)
    {TRACE_IT(19977);
        return Instance.pfnSymFromAddrW(Instance.hProcess, (DWORD64)address, dwDisplacement, pSymbol);
    }
    return FALSE;
}

BOOL
DbgHelpSymbolManager::SymGetLineFromAddr64(_In_ PVOID address, _Out_ PDWORD pdwDisplacement, _Out_ PIMAGEHLP_LINEW64 pLine)
{TRACE_IT(19978);
    if (pdwDisplacement != nullptr)
    {TRACE_IT(19979);
        *pdwDisplacement = 0;
    }

    if (pLine != nullptr)
    {
        ZeroMemory(pLine, sizeof(IMAGEHLP_LINEW64));
        pLine->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    }

    if (Instance.pfnSymGetLineFromAddr64W)
    {TRACE_IT(19980);
        return Instance.pfnSymGetLineFromAddr64W(Instance.hProcess, (DWORD64)address, pdwDisplacement, pLine);
    }
    return FALSE;
}

size_t DbgHelpSymbolManager::PrintSymbol(PVOID address)
{TRACE_IT(19981);
    size_t retValue = 0;
    DWORD64  dwDisplacement = 0;
    char buffer[sizeof(SYMBOL_INFO)+MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    IMAGEHLP_LINE64 lineInfo;
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    if (DbgHelpSymbolManager::SymFromAddr(address, &dwDisplacement, pSymbol))
    {TRACE_IT(19982);
        DWORD dwDisplacementDWord = static_cast<DWORD>(dwDisplacement);
        if (DbgHelpSymbolManager::SymGetLineFromAddr64(address, &dwDisplacementDWord, &lineInfo))
        {TRACE_IT(19983);
            retValue += Output::Print(_u("0x%p %s+0x%llx (%s:%d)"), address, pSymbol->Name, dwDisplacement, lineInfo.FileName, lineInfo.LineNumber);
        }
        else
        {TRACE_IT(19984);
            // SymGetLineFromAddr64 failed
            retValue += Output::Print(_u("0x%p %s+0x%llx"), address, pSymbol->Name, dwDisplacement);
        }
    }
    else
    {TRACE_IT(19985);
        // SymFromAddr failed
        retValue += Output::Print(_u("0x%p"), address);
    }
    return retValue;
}
#endif
