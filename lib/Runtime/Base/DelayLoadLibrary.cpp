//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#ifdef _CONTROL_FLOW_GUARD
#if !defined(DELAYLOAD_SET_CFG_TARGET)
extern "C"
WINBASEAPI
BOOL
WINAPI
SetProcessValidCallTargets(
    _In_ HANDLE hProcess,
    _In_ PVOID VirtualAddress,
    _In_ SIZE_T RegionSize,
    _In_ ULONG NumberOfOffsets,
    _In_reads_(NumberOfOffsets) PCFG_CALL_TARGET_INFO OffsetInformation
    );
#endif
#endif

namespace Js
{
    HRESULT DelayLoadWinRtString::WindowsCreateString(_In_reads_opt_(length) const WCHAR * sourceString, UINT32 length, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string)
    {TRACE_IT(33651);
        if (m_hModule)
        {TRACE_IT(33652);
            if (m_pfnWindowsCreateString == nullptr)
            {TRACE_IT(33653);
                m_pfnWindowsCreateString = (PFNCWindowsCreateString)GetFunction("WindowsCreateString");
                if (m_pfnWindowsCreateString == nullptr)
                {TRACE_IT(33654);
                    *string = nullptr;
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsCreateString != nullptr);
            return m_pfnWindowsCreateString(sourceString, length, string);
        }

        *string = nullptr;
        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtString::WindowsCreateStringReference(_In_reads_opt_(length + 1) const WCHAR *sourceString, UINT32 length, _Out_ HSTRING_HEADER *hstringHeader, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string)
    {TRACE_IT(33655);
        if (m_hModule)
        {TRACE_IT(33656);
            if (m_pfnWindowsCreateStringReference == nullptr)
            {TRACE_IT(33657);
                m_pfnWindowsCreateStringReference = (PFNCWindowsCreateStringReference)GetFunction("WindowsCreateStringReference");
                if (m_pfnWindowsCreateStringReference == nullptr)
                {TRACE_IT(33658);
                    *string = nullptr;
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsCreateStringReference != nullptr);
            return m_pfnWindowsCreateStringReference(sourceString, length, hstringHeader, string);
        }

        *string = nullptr;
        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtString::WindowsDeleteString(_In_opt_ HSTRING string)
    {TRACE_IT(33659);
        if (m_hModule)
        {TRACE_IT(33660);
            if (m_pfnWindowsDeleteString == nullptr)
            {TRACE_IT(33661);
                m_pfnWindowsDeleteString = (PFNCWindowsDeleteString)GetFunction("WindowsDeleteString");
                if (m_pfnWindowsDeleteString == nullptr)
                {TRACE_IT(33662);
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsDeleteString != nullptr);
            HRESULT hr = m_pfnWindowsDeleteString(string);
            Assert(SUCCEEDED(hr));
            return hr;
        }

        return E_NOTIMPL;
    }

    PCWSTR DelayLoadWinRtString::WindowsGetStringRawBuffer(_In_opt_ HSTRING string, _Out_opt_ UINT32 * length)
    {TRACE_IT(33663);
        if (m_hModule)
        {TRACE_IT(33664);
            if (m_pfWindowsGetStringRawBuffer == nullptr)
            {TRACE_IT(33665);
                m_pfWindowsGetStringRawBuffer = (PFNCWindowsGetStringRawBuffer)GetFunction("WindowsGetStringRawBuffer");
                if (m_pfWindowsGetStringRawBuffer == nullptr)
                {TRACE_IT(33666);
                    if (length)
                    {TRACE_IT(33667);
                        *length = 0;
                    }
                    return _u("\0");
                }
            }

            Assert(m_pfWindowsGetStringRawBuffer != nullptr);
            return m_pfWindowsGetStringRawBuffer(string, length);
        }

        if (length)
        {TRACE_IT(33668);
            *length = 0;
        }
        return _u("\0");
    }

    HRESULT DelayLoadWinRtString::WindowsCompareStringOrdinal(_In_opt_ HSTRING string1, _In_opt_ HSTRING string2, _Out_ INT32 * result)
    {TRACE_IT(33669);
        if (m_hModule)
        {TRACE_IT(33670);
            if (m_pfnWindowsCompareStringOrdinal == nullptr)
            {TRACE_IT(33671);
                m_pfnWindowsCompareStringOrdinal = (PFNCWindowsCompareStringOrdinal)GetFunction("WindowsCompareStringOrdinal");
                if (m_pfnWindowsCompareStringOrdinal == nullptr)
                {TRACE_IT(33672);
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsCompareStringOrdinal != nullptr);
            return m_pfnWindowsCompareStringOrdinal(string1,string2,result);
        }

        return E_NOTIMPL;
    }
    HRESULT DelayLoadWinRtString::WindowsDuplicateString(_In_opt_ HSTRING original, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING *newString)
    {TRACE_IT(33673);
        if(m_hModule)
        {TRACE_IT(33674);
            if(m_pfnWindowsDuplicateString == nullptr)
            {TRACE_IT(33675);
                m_pfnWindowsDuplicateString = (PFNCWindowsDuplicateString)GetFunction("WindowsDuplicateString");
                if(m_pfnWindowsDuplicateString == nullptr)
                {TRACE_IT(33676);
                    *newString = nullptr;
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnWindowsDuplicateString != nullptr);
            return m_pfnWindowsDuplicateString(original, newString);
        }
        *newString = nullptr;
        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtTypeResolution::RoParseTypeName(__in HSTRING typeName, __out DWORD *partsCount, __RPC__deref_out_ecount_full_opt(*partsCount) HSTRING **typeNameParts)
    {TRACE_IT(33677);
        if (m_hModule)
        {TRACE_IT(33678);
            if (m_pfnRoParseTypeName == nullptr)
            {TRACE_IT(33679);
                m_pfnRoParseTypeName = (PFNCWRoParseTypeName)GetFunction("RoParseTypeName");
                if (m_pfnRoParseTypeName == nullptr)
                {TRACE_IT(33680);
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnRoParseTypeName != nullptr);
            return m_pfnRoParseTypeName(typeName, partsCount, typeNameParts);
        }

        return E_NOTIMPL;
    }

    bool DelayLoadWindowsGlobalization::HasGlobalizationDllLoaded()
    {TRACE_IT(33681);
        return this->hasGlobalizationDllLoaded;
    }

    HRESULT DelayLoadWindowsGlobalization::DllGetActivationFactory(
        __in HSTRING activatibleClassId,
        __out IActivationFactory** factory)
    {TRACE_IT(33682);
        if (m_hModule)
        {TRACE_IT(33683);
            if (m_pfnFNCWDllGetActivationFactory == nullptr)
            {TRACE_IT(33684);
                m_pfnFNCWDllGetActivationFactory = (PFNCWDllGetActivationFactory)GetFunction("DllGetActivationFactory");
                if (m_pfnFNCWDllGetActivationFactory == nullptr)
                {TRACE_IT(33685);
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnFNCWDllGetActivationFactory != nullptr);
            return m_pfnFNCWDllGetActivationFactory(activatibleClassId, factory);
        }

        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtFoundation::RoGetActivationFactory(
        __in HSTRING activatibleClassId,
        __in REFIID iid,
        __out IActivationFactory** factory)
    {TRACE_IT(33686);
        if (m_hModule)
        {TRACE_IT(33687);
            if (m_pfnFNCWRoGetActivationFactory == nullptr)
            {TRACE_IT(33688);
                m_pfnFNCWRoGetActivationFactory = (PFNCWRoGetActivationFactory)GetFunction("RoGetActivationFactory");
                if (m_pfnFNCWRoGetActivationFactory == nullptr)
                {TRACE_IT(33689);
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnFNCWRoGetActivationFactory != nullptr);
            return m_pfnFNCWRoGetActivationFactory(activatibleClassId, iid, factory);
        }

        return E_NOTIMPL;
    }

    HRESULT DelayLoadWinRtTypeResolution::RoResolveNamespace(
        __in_opt const HSTRING namespaceName,
        __in_opt const HSTRING windowsMetaDataPath,
        __in const DWORD packageGraphPathsCount,
        __in_opt const HSTRING *packageGraphPaths,
        __out DWORD *metaDataFilePathsCount,
        HSTRING **metaDataFilePaths,
        __out DWORD *subNamespacesCount,
        HSTRING **subNamespaces)
    {TRACE_IT(33690);
        if (m_hModule)
        {TRACE_IT(33691);
            if (m_pfnRoResolveNamespace == nullptr)
            {TRACE_IT(33692);
                m_pfnRoResolveNamespace = (PFNCRoResolveNamespace)GetFunction("RoResolveNamespace");
                if (m_pfnRoResolveNamespace == nullptr)
                {TRACE_IT(33693);
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnRoResolveNamespace != nullptr);
            return m_pfnRoResolveNamespace(namespaceName, windowsMetaDataPath, packageGraphPathsCount, packageGraphPaths,
                metaDataFilePathsCount, metaDataFilePaths, subNamespacesCount, subNamespaces);
        }

        return E_NOTIMPL;
    }

    void DelayLoadWindowsGlobalization::Ensure(Js::DelayLoadWinRtString *winRTStringLibrary)
    {TRACE_IT(33694);
        if (!this->m_isInit)
        {TRACE_IT(33695);
            DelayLoadLibrary::EnsureFromSystemDirOnly();

#if DBG
            // This unused variable is to allow one to see the value of lastError in case both LoadLibrary (DelayLoadLibrary::Ensure has one) fail.
            // As the issue might be with the first one, as opposed to the second
            DWORD errorWhenLoadingBluePlus = GetLastError();
            Unused(errorWhenLoadingBluePlus);
#endif
            //Perform a check to see if Windows.Globalization.dll was loaded; if not try loading jsIntl.dll as we are on Win7.
            if (m_hModule == nullptr)
            {TRACE_IT(33696);
                m_hModule = LoadLibraryEx(GetWin7LibraryName(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            }

            // Set the flag depending on Windows.globalization.dll or jsintl.dll was loaded successfully or not
            if (m_hModule != nullptr)
            {TRACE_IT(33697);
                hasGlobalizationDllLoaded = true;
            }
            this->winRTStringLibrary = winRTStringLibrary;
            this->winRTStringsPresent = GetFunction("WindowsDuplicateString") != nullptr;
        }
    }

    HRESULT DelayLoadWindowsGlobalization::WindowsCreateString(_In_reads_opt_(length) const WCHAR * sourceString, UINT32 length, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string)
    {TRACE_IT(33698);
        //If winRtStringLibrary isn't nullptr, that means it is available and we are on Win8+
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {TRACE_IT(33699);
            return winRTStringLibrary->WindowsCreateString(sourceString, length, string);
        }

        return DelayLoadWinRtString::WindowsCreateString(sourceString, length, string);
    }
    HRESULT DelayLoadWindowsGlobalization::WindowsCreateStringReference(_In_reads_opt_(length + 1) const WCHAR * sourceString, UINT32 length, _Out_ HSTRING_HEADER * header, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING * string)
    {TRACE_IT(33700);
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {TRACE_IT(33701);
            return winRTStringLibrary->WindowsCreateStringReference(sourceString, length, header, string);
        }
        return DelayLoadWinRtString::WindowsCreateStringReference(sourceString, length, header, string);
    }
    HRESULT DelayLoadWindowsGlobalization::WindowsDeleteString(_In_opt_ HSTRING string)
    {TRACE_IT(33702);
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {TRACE_IT(33703);
            return winRTStringLibrary->WindowsDeleteString(string);
        }
        return DelayLoadWinRtString::WindowsDeleteString(string);
    }
    PCWSTR DelayLoadWindowsGlobalization::WindowsGetStringRawBuffer(_In_opt_ HSTRING string, _Out_opt_ UINT32 * length)
    {TRACE_IT(33704);
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {TRACE_IT(33705);
            return winRTStringLibrary->WindowsGetStringRawBuffer(string, length);
        }
        return DelayLoadWinRtString::WindowsGetStringRawBuffer(string, length);
    }
    HRESULT DelayLoadWindowsGlobalization::WindowsCompareStringOrdinal(_In_opt_ HSTRING string1, _In_opt_ HSTRING string2, _Out_ INT32 * result)
    {TRACE_IT(33706);
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {TRACE_IT(33707);
            return winRTStringLibrary->WindowsCompareStringOrdinal(string1, string2, result);
        }
        return DelayLoadWinRtString::WindowsCompareStringOrdinal(string1, string2, result);
    }

    HRESULT DelayLoadWindowsGlobalization::WindowsDuplicateString(_In_opt_ HSTRING original, _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING *newString)
    {TRACE_IT(33708);
        //First, we attempt to use the WinStringRT api encapsulated in the globalization dll; if it is available then it is a downlevel dll.
        //Otherwise; we might run into an error where we are using the Win8 (because testing is being done for instance) with the downlevel dll, and that would cause errors.
        if(!winRTStringsPresent && winRTStringLibrary->IsAvailable())
        {TRACE_IT(33709);
            return winRTStringLibrary->WindowsDuplicateString(original, newString);
        }
        return DelayLoadWinRtString::WindowsDuplicateString(original, newString);
    }

#ifdef ENABLE_PROJECTION
    HRESULT DelayLoadWinRtError::RoClearError()
    {TRACE_IT(33710);
        if (m_hModule)
        {TRACE_IT(33711);
            if (m_pfnRoClearError == nullptr)
            {TRACE_IT(33712);
                m_pfnRoClearError = (PFNCRoClearError)GetFunction("RoClearError");
                if (m_pfnRoClearError == nullptr)
                {TRACE_IT(33713);
                    return E_UNEXPECTED;
                }
            }

            Assert(m_pfnRoClearError != nullptr);
            m_pfnRoClearError();

            return S_OK;
        }

        return E_NOTIMPL;
    }

    BOOL DelayLoadWinRtError::RoOriginateLanguageException(__in HRESULT error, __in_opt HSTRING message, __in IUnknown * languageException)
    {TRACE_IT(33714);
        if (m_hModule)
        {TRACE_IT(33715);
            if (m_pfnRoOriginateLanguageException == nullptr)
            {TRACE_IT(33716);
                m_pfnRoOriginateLanguageException = (PFNCRoOriginateLanguageException)GetFunction("RoOriginateLanguageException");
                if (m_pfnRoOriginateLanguageException == nullptr)
                {TRACE_IT(33717);
                    return FALSE;
                }
            }

            Assert(m_pfnRoOriginateLanguageException != nullptr);
            return m_pfnRoOriginateLanguageException(error, message, languageException);
        }

        return FALSE;
    }
#endif

#ifdef _CONTROL_FLOW_GUARD
// Note. __declspec(guard(nocf)) causes the CFG check to be removed
// inside this function, and is needed only for test binaries (chk and FRETEST)
#if defined(DELAYLOAD_SET_CFG_TARGET)
    DECLSPEC_GUARDNOCF
#endif
    BOOL DelayLoadWinCoreMemory::SetProcessCallTargets(_In_ HANDLE hProcess,
        _In_ PVOID VirtualAddress,
        _In_ SIZE_T RegionSize,
        _In_ ULONG NumberOfOffsets,
        _In_reads_(NumberOfOffsets) PCFG_CALL_TARGET_INFO OffsetInformation)
    {TRACE_IT(33718);
#if defined(ENABLE_JIT_CLAMP)
        // Ensure that dynamic code generation is allowed for this thread as
        // this is required for the call to SetProcessValidCallTargets to
        // succeed.
        AutoEnableDynamicCodeGen enableCodeGen;
#endif

#if defined(DELAYLOAD_SET_CFG_TARGET)
        if (m_hModule)
        {TRACE_IT(33719);
            if (m_pfnSetProcessValidCallTargets == nullptr)
            {TRACE_IT(33720);
                m_pfnSetProcessValidCallTargets = (PFNCSetProcessValidCallTargets) GetFunction("SetProcessValidCallTargets");
                if (m_pfnSetProcessValidCallTargets == nullptr)
                {TRACE_IT(33721);
                    return FALSE;
                }
            }

            Assert(m_pfnSetProcessValidCallTargets != nullptr);
            return m_pfnSetProcessValidCallTargets(hProcess, VirtualAddress, RegionSize, NumberOfOffsets, OffsetInformation);
        }

        return FALSE;
#else
        return SetProcessValidCallTargets(hProcess, VirtualAddress, RegionSize, NumberOfOffsets, OffsetInformation);
#endif
    }
#endif

    BOOL DelayLoadWinCoreProcessThreads::GetMitigationPolicyForProcess(
        __in HANDLE hProcess,
        __in PROCESS_MITIGATION_POLICY MitigationPolicy,
        __out_bcount(nLength) PVOID lpBuffer,
        __in SIZE_T nLength
        )
    {TRACE_IT(33722);
#if defined(DELAYLOAD_SET_CFG_TARGET)
        if (m_hModule)
        {TRACE_IT(33723);
            if (m_pfnGetProcessMitigationPolicy == nullptr)
            {TRACE_IT(33724);
                m_pfnGetProcessMitigationPolicy = (PFNCGetMitigationPolicyForProcess) GetFunction("GetProcessMitigationPolicy");
                if (m_pfnGetProcessMitigationPolicy == nullptr)
                {TRACE_IT(33725);
                    return FALSE;
                }
            }

            Assert(m_pfnGetProcessMitigationPolicy != nullptr);
            return m_pfnGetProcessMitigationPolicy(hProcess, MitigationPolicy, lpBuffer, nLength);
        }
        return FALSE;
#else
        return BinaryFeatureControl::GetMitigationPolicyForProcess(hProcess, MitigationPolicy, lpBuffer, nLength);
#endif // ENABLE_DEBUG_CONFIG_OPTIONS
    }

    BOOL DelayLoadWinCoreProcessThreads::GetProcessInformation(
        __in HANDLE hProcess,
        __in PROCESS_INFORMATION_CLASS ProcessInformationClass,
        __out_bcount(nLength) PVOID lpBuffer,
        __in SIZE_T nLength
        )
    {TRACE_IT(33726);
#if defined(DELAYLOAD_SET_CFG_TARGET) || defined(_M_ARM)
        if (m_hModule)
        {TRACE_IT(33727);
            if (m_pfnGetProcessInformation == nullptr)
            {TRACE_IT(33728);
                m_pfnGetProcessInformation = (PFNCGetProcessInformation) GetFunction("GetProcessInformation");
                if (m_pfnGetProcessInformation == nullptr)
                {TRACE_IT(33729);
                    return FALSE;
                }
            }

            Assert(m_pfnGetProcessInformation != nullptr);
            return m_pfnGetProcessInformation(hProcess, ProcessInformationClass, lpBuffer, nLength);
        }
#endif
        return FALSE;
    }
}
