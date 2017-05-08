//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"
#include "Core/DelayLoadLibrary.h"

DelayLoadLibrary::DelayLoadLibrary()
{TRACE_IT(19988);
    m_hModule = nullptr;
    m_isInit = false;
}

DelayLoadLibrary::~DelayLoadLibrary()
{TRACE_IT(19989);
    if (m_hModule)
    {TRACE_IT(19990);
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
    }
}

void DelayLoadLibrary::Ensure(DWORD dwFlags)
{TRACE_IT(19991);
    if (!m_isInit)
    {TRACE_IT(19992);
        m_hModule = LoadLibraryEx(GetLibraryName(), nullptr, dwFlags);
        m_isInit = true;
    }
}

void DelayLoadLibrary::EnsureFromSystemDirOnly()
{TRACE_IT(19993);
    Ensure(LOAD_LIBRARY_SEARCH_SYSTEM32);
}


FARPROC DelayLoadLibrary::GetFunction(__in LPCSTR lpFunctionName)
{TRACE_IT(19994);
    if (m_hModule)
    {TRACE_IT(19995);
        return GetProcAddress(m_hModule, lpFunctionName);
    }

    return nullptr;
}

bool DelayLoadLibrary::IsAvailable()
{TRACE_IT(19996);
    return m_hModule != nullptr;
}

#if _WIN32

static NtdllLibrary NtdllLibraryObject;
NtdllLibrary* NtdllLibrary::Instance = &NtdllLibraryObject;

LPCTSTR NtdllLibrary::GetLibraryName() const
{TRACE_IT(19997);
    return _u("ntdll.dll");
}

#if PDATA_ENABLED

_Success_(return == 0)
DWORD NtdllLibrary::AddGrowableFunctionTable( _Out_ PVOID * DynamicTable,
    _In_reads_(MaximumEntryCount) PRUNTIME_FUNCTION FunctionTable,
    _In_ DWORD EntryCount,
    _In_ DWORD MaximumEntryCount,
    _In_ ULONG_PTR RangeBase,
    _In_ ULONG_PTR RangeEnd )
{TRACE_IT(19998);
    if(m_hModule)
    {TRACE_IT(19999);
        if(addGrowableFunctionTable == NULL)
        {TRACE_IT(20000);
            addGrowableFunctionTable = (PFnRtlAddGrowableFunctionTable)GetFunction("RtlAddGrowableFunctionTable");
            if(addGrowableFunctionTable == NULL)
            {TRACE_IT(20001);
                Assert(false);
                return 1;
            }
        }
        return addGrowableFunctionTable(DynamicTable,
            FunctionTable,
            EntryCount,
            MaximumEntryCount,
            RangeBase,
            RangeEnd);
    }
    return 1;
}

VOID NtdllLibrary::DeleteGrowableFunctionTable( _In_ PVOID DynamicTable )
{TRACE_IT(20002);
    if(m_hModule)
    {TRACE_IT(20003);
        if(deleteGrowableFunctionTable == NULL)
        {TRACE_IT(20004);
            deleteGrowableFunctionTable = (PFnRtlDeleteGrowableFunctionTable)GetFunction("RtlDeleteGrowableFunctionTable");
            if(deleteGrowableFunctionTable == NULL)
            {TRACE_IT(20005);
                Assert(false);
                return;
            }
        }
        deleteGrowableFunctionTable(DynamicTable);
    }
}

VOID NtdllLibrary::GrowFunctionTable(_Inout_ PVOID DynamicTable, _In_ ULONG NewEntryCount)
{TRACE_IT(20006);
    if (m_hModule)
    {TRACE_IT(20007);
        if (growFunctionTable == nullptr)
        {TRACE_IT(20008);
            growFunctionTable = (PFnRtlGrowFunctionTable)GetFunction("RtlGrowFunctionTable");
            if (growFunctionTable == nullptr)
            {TRACE_IT(20009);
                Assert(false);
                return;
            }
        }

        growFunctionTable(DynamicTable, NewEntryCount);
    }
}
#endif // PDATA_ENABLED

VOID NtdllLibrary::InitializeObjectAttributes(
    POBJECT_ATTRIBUTES   InitializedAttributes,
    PUNICODE_STRING      ObjectName,
    ULONG                Attributes,
    HANDLE               RootDirectory,
    PSECURITY_DESCRIPTOR SecurityDescriptor)
{TRACE_IT(20010);
    InitializedAttributes->Length = sizeof(OBJECT_ATTRIBUTES);
    InitializedAttributes->RootDirectory = RootDirectory;
    InitializedAttributes->Attributes = Attributes;
    InitializedAttributes->ObjectName = ObjectName;
    InitializedAttributes->SecurityDescriptor = SecurityDescriptor;
    InitializedAttributes->SecurityQualityOfService = NULL;
}

#ifndef DELAYLOAD_SECTIONAPI
extern "C"
WINBASEAPI
NtdllLibrary::NTSTATUS
WINAPI
NtCreateSection(
    _Out_    PHANDLE            SectionHandle,
    _In_     ACCESS_MASK        DesiredAccess,
    _In_opt_ NtdllLibrary::POBJECT_ATTRIBUTES ObjectAttributes,
    _In_opt_ PLARGE_INTEGER     MaximumSize,
    _In_     ULONG              SectionPageProtection,
    _In_     ULONG              AllocationAttributes,
    _In_opt_ HANDLE             FileHandle
);
#endif

NtdllLibrary::NTSTATUS NtdllLibrary::CreateSection(
    _Out_    PHANDLE            SectionHandle,
    _In_     ACCESS_MASK        DesiredAccess,
    _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_opt_ PLARGE_INTEGER     MaximumSize,
    _In_     ULONG              SectionPageProtection,
    _In_     ULONG              AllocationAttributes,
    _In_opt_ HANDLE             FileHandle)
{TRACE_IT(20011);
#ifdef DELAYLOAD_SECTIONAPI
    if (m_hModule)
    {TRACE_IT(20012);
        if (createSection == nullptr)
        {TRACE_IT(20013);
            createSection = (PFnNtCreateSection)GetFunction("NtCreateSection");
            if (createSection == nullptr)
            {TRACE_IT(20014);
                Assert(false);
                SectionHandle = nullptr;
                return -1;
            }
        }
        return createSection(SectionHandle, DesiredAccess, ObjectAttributes, MaximumSize, SectionPageProtection, AllocationAttributes, FileHandle);
    }
    SectionHandle = nullptr;
    return -1;
#else
    return NtCreateSection(SectionHandle, DesiredAccess, ObjectAttributes, MaximumSize, SectionPageProtection, AllocationAttributes, FileHandle);
#endif
}

#ifndef DELAYLOAD_SECTIONAPI
extern "C"
WINBASEAPI
NtdllLibrary::NTSTATUS
WINAPI
NtMapViewOfSection(
    _In_        HANDLE          SectionHandle,
    _In_        HANDLE          ProcessHandle,
    _Inout_     PVOID           *BaseAddress,
    _In_        ULONG_PTR       ZeroBits,
    _In_        SIZE_T          CommitSize,
    _Inout_opt_ PLARGE_INTEGER  SectionOffset,
    _Inout_     PSIZE_T         ViewSize,
    _In_        NtdllLibrary::SECTION_INHERIT InheritDisposition,
    _In_        ULONG           AllocationType,
    _In_        ULONG           Win32Protect
);
#endif

NtdllLibrary::NTSTATUS NtdllLibrary::MapViewOfSection(
    _In_        HANDLE          SectionHandle,
    _In_        HANDLE          ProcessHandle,
    _Inout_     PVOID           *BaseAddress,
    _In_        ULONG_PTR       ZeroBits,
    _In_        SIZE_T          CommitSize,
    _Inout_opt_ PLARGE_INTEGER  SectionOffset,
    _Inout_     PSIZE_T         ViewSize,
    _In_        SECTION_INHERIT InheritDisposition,
    _In_        ULONG           AllocationType,
    _In_        ULONG           Win32Protect)
{TRACE_IT(20015);
#ifdef DELAYLOAD_SECTIONAPI
    if (m_hModule)
    {TRACE_IT(20016);
        if (mapViewOfSection == nullptr)
        {TRACE_IT(20017);
            mapViewOfSection = (PFnNtMapViewOfSection)GetFunction("NtMapViewOfSection");
            if (mapViewOfSection == nullptr)
            {TRACE_IT(20018);
                Assert(false);
                return -1;
            }
        }
        return mapViewOfSection(SectionHandle, ProcessHandle, BaseAddress, ZeroBits, CommitSize, SectionOffset, ViewSize, InheritDisposition, AllocationType, Win32Protect);
    }
    return -1;
#else
    return NtMapViewOfSection(SectionHandle, ProcessHandle, BaseAddress, ZeroBits, CommitSize, SectionOffset, ViewSize, InheritDisposition, AllocationType, Win32Protect);
#endif
}

#ifndef DELAYLOAD_SECTIONAPI
extern "C"
WINBASEAPI
NtdllLibrary::NTSTATUS
WINAPI
NtUnmapViewOfSection(
    _In_     HANDLE ProcessHandle,
    _In_opt_ PVOID  BaseAddress
);
#endif

NtdllLibrary::NTSTATUS NtdllLibrary::UnmapViewOfSection(
    _In_     HANDLE ProcessHandle,
    _In_opt_ PVOID  BaseAddress)
{TRACE_IT(20019);
#ifdef DELAYLOAD_SECTIONAPI
    if (m_hModule)
    {TRACE_IT(20020);
        if (unmapViewOfSection == nullptr)
        {TRACE_IT(20021);
            unmapViewOfSection = (PFnNtUnmapViewOfSection)GetFunction("NtUnmapViewOfSection");
            if (unmapViewOfSection == nullptr)
            {TRACE_IT(20022);
                Assert(false);
                return -1;
            }
        }
        return unmapViewOfSection(ProcessHandle, BaseAddress);
    }
    return -1;
#else
    return NtUnmapViewOfSection(ProcessHandle, BaseAddress);
#endif
}

#ifndef DELAYLOAD_SECTIONAPI
extern "C"
WINBASEAPI
NtdllLibrary::NTSTATUS
WINAPI
NtClose(_In_ HANDLE Handle);
#endif

NtdllLibrary::NTSTATUS NtdllLibrary::Close(_In_ HANDLE Handle)
{TRACE_IT(20023);
#ifdef DELAYLOAD_SECTIONAPI
    if (m_hModule)
    {TRACE_IT(20024);
        if (close == nullptr)
        {TRACE_IT(20025);
            close = (PFnNtClose)GetFunction("NtClose");
            if (close == nullptr)
            {TRACE_IT(20026);
                Assert(false);
                return -1;
            }
        }
        return close(Handle);
    }
    return -1;
#else
    return NtClose(Handle);
#endif
}

#endif // _WIN32
