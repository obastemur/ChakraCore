//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_ES6_CHAR_CLASSIFIER)

#include "strsafe.h"

#define __WRL_ASSERT__(cond) Assert(cond)

#include <wrl\implements.h>

#ifdef NTBUILD
using namespace Windows::Globalization;
using namespace Windows::Foundation::Collections;
#else
using namespace ABI::Windows::Globalization;
using namespace ABI::Windows::Foundation::Collections;
#endif

#define IfFailThrowHr(op) \
    if (FAILED(hr=(op))) \
    {TRACE_IT(38010); \
        JavascriptError::MapAndThrowError(scriptContext, hr);\
    } \

#define IfNullReturnError(EXPR, ERROR) do {TRACE_IT(38011); if (!(EXPR)) {TRACE_IT(38012); return (ERROR); } } while(FALSE)
#define IfFailedReturn(EXPR) do {TRACE_IT(38013); hr = (EXPR); if (FAILED(hr)) {TRACE_IT(38014); return hr; }} while(FALSE)
#define IfFailedSetErrorCodeAndReturn(EXPR, hrVariable) do {TRACE_IT(38015); hr = (EXPR); if (FAILED(hr)) {TRACE_IT(38016); hrVariable = hr; return hr; }} while(FALSE)
#define IfFailedGoLabel(expr, label) if (FAILED(expr)) {TRACE_IT(38017); goto label; }
#define IfFailedGo(expr) IfFailedGoLabel(expr, LReturn)

namespace Js
{
#ifdef ENABLE_INTL_OBJECT
    class HSTRINGIterator : public Microsoft::WRL::RuntimeClass<IIterator<HSTRING>>
    {

        HSTRING *items;
        uint32 length;
        boolean hasMore;
        uint32 currentPosition;

    public:
        HRESULT RuntimeClassInitialize(HSTRING *items, uint32 length)
        {TRACE_IT(38018);
            this->items = items;
            this->currentPosition = 0;
            this->length = length;
            this->hasMore = currentPosition < this->length;

            return S_OK;
        }
        ~HSTRINGIterator()
        {TRACE_IT(38019);
        }

        // IIterator
        IFACEMETHODIMP get_Current(_Out_ HSTRING *current)
        {TRACE_IT(38020);
            if (current != nullptr)
            {TRACE_IT(38021);
                if (hasMore)
                {TRACE_IT(38022);
                    return WindowsDuplicateString(items[currentPosition], current);
                }
                else
                {TRACE_IT(38023);
                    *current = nullptr;
                }
            }
            return E_BOUNDS;
        }

        IFACEMETHODIMP get_HasCurrent(_Out_ boolean *hasCurrent)
        {TRACE_IT(38024);
            if (hasCurrent != nullptr)
            {TRACE_IT(38025);
                *hasCurrent = hasMore;
            }
            return S_OK;
        }

        IFACEMETHODIMP MoveNext(_Out_opt_ boolean *hasCurrent) sealed
        {
            this->currentPosition++;

            this->hasMore = this->currentPosition < this->length;
            if (hasCurrent != nullptr)
            {TRACE_IT(38026);
                *hasCurrent = hasMore;
            }
            return S_OK;
        }

        IFACEMETHODIMP GetMany(_In_ unsigned capacity,
                               _Out_writes_to_(capacity,*actual) HSTRING *value,
                               _Out_ unsigned *actual)
        {TRACE_IT(38027);
            uint count = 0;
            while (this->hasMore)
            {TRACE_IT(38028);
                if (count == capacity)
                {TRACE_IT(38029);
                    break;
                }
                if (value != nullptr)
                {TRACE_IT(38030);
                    get_Current(value + count);
                }

                count ++;
                this->MoveNext(nullptr);
            }
            if (actual != nullptr)
            {TRACE_IT(38031);
                *actual = count;
            }

            return S_OK;
        }
        IFACEMETHOD(GetRuntimeClassName)(_Out_ HSTRING* runtimeName) sealed
        {
            *runtimeName = nullptr;
            HRESULT hr = S_OK;
            const char16 *name = _u("Js.HSTRINGIterator");

            hr = WindowsCreateString(name, static_cast<UINT32>(wcslen(name)), runtimeName);
            return hr;
        }
        IFACEMETHOD(GetTrustLevel)(_Out_ TrustLevel* trustLvl)
        {TRACE_IT(38032);
            *trustLvl = BaseTrust;
            return S_OK;
        }
        IFACEMETHOD(GetIids)(_Out_ ULONG *iidCount, _Outptr_result_buffer_(*iidCount) IID **)
        {TRACE_IT(38033);
            iidCount;
            return E_NOTIMPL;
        }
    };

    class HSTRINGIterable : public Microsoft::WRL::RuntimeClass<IIterable<HSTRING>>
    {

        HSTRING *items;
        uint32 length;

    public:
        HRESULT RuntimeClassInitialize(HSTRING *string, uint32 length)
        {TRACE_IT(38034);
            this->items = HeapNewNoThrowArray(HSTRING, length);

            if (this->items == nullptr)
            {TRACE_IT(38035);
                return E_OUTOFMEMORY;
            }

            for(uint32 i = 0; i < length; i++)
            {TRACE_IT(38036);
                this->items[i] = string[i];
            }
            this->length = length;

            return S_OK;
        }

        ~HSTRINGIterable()
        {TRACE_IT(38037);
            if(this->items != nullptr)
            {TRACE_IT(38038);
                HeapDeleteArray(this->length, items);
            }
        }

        IFACEMETHODIMP First(_Outptr_result_maybenull_ IIterator<HSTRING> **first)
        {TRACE_IT(38039);
            return Microsoft::WRL::MakeAndInitialize<HSTRINGIterator>(first, this->items, this->length);
        }

        IFACEMETHOD(GetRuntimeClassName)(_Out_ HSTRING* runtimeName) sealed
        {
            *runtimeName = nullptr;
            HRESULT hr = S_OK;
            // Return type that does not exist in metadata
            const char16 *name = _u("Js.HSTRINGIterable");
            hr = WindowsCreateString(name, static_cast<UINT32>(wcslen(name)), runtimeName);
            return hr;
        }
        IFACEMETHOD(GetTrustLevel)(_Out_ TrustLevel* trustLvl)
        {TRACE_IT(38040);
            *trustLvl = BaseTrust;
            return S_OK;
        }
        IFACEMETHOD(GetIids)(_Out_ ULONG *iidCount, _Outptr_result_buffer_(*iidCount) IID **)
        {TRACE_IT(38041);
            iidCount;
            return E_NOTIMPL;
        }
    };
#endif

    inline DelayLoadWindowsGlobalization* WindowsGlobalizationAdapter::GetWindowsGlobalizationLibrary(_In_ ScriptContext* scriptContext)
    {TRACE_IT(38042);
        return this->GetWindowsGlobalizationLibrary(scriptContext->GetThreadContext());
    }

    inline DelayLoadWindowsGlobalization* WindowsGlobalizationAdapter::GetWindowsGlobalizationLibrary(_In_ ThreadContext* threadContext)
    {TRACE_IT(38043);
        return threadContext->GetWindowsGlobalizationLibrary();
    }

    template<typename T>
    HRESULT WindowsGlobalizationAdapter::GetActivationFactory(DelayLoadWindowsGlobalization *delayLoadLibrary, LPCWSTR factoryName, T** instance)
    {TRACE_IT(38044);
        *instance = nullptr;

        AutoCOMPtr<IActivationFactory> factory;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;
        HRESULT hr;

        // factoryName will never get truncated as the name of interfaces in Windows.globalization are not that long.
        IfFailedReturn(delayLoadLibrary->WindowsCreateStringReference(factoryName, static_cast<UINT32>(wcslen(factoryName)), &hStringHdr, &hString));
        AnalysisAssert(hString);
        IfFailedReturn(delayLoadLibrary->DllGetActivationFactory(hString, &factory));

        return factory->QueryInterface(__uuidof(T), reinterpret_cast<void**>(instance));
    }

#ifdef ENABLE_INTL_OBJECT
    HRESULT WindowsGlobalizationAdapter::EnsureCommonObjectsInitialized(DelayLoadWindowsGlobalization *library)
    {TRACE_IT(38045);
        HRESULT hr = S_OK;

        if (initializedCommonGlobObjects)
        {TRACE_IT(38046);
            AssertMsg(hrForCommonGlobObjectsInit == S_OK, "If IntlGlobObjects are initialized, we should be returning S_OK.");
            return hrForCommonGlobObjectsInit;
        }
        else if (hrForCommonGlobObjectsInit != S_OK)
        {TRACE_IT(38047);
            return hrForCommonGlobObjectsInit;
        }

        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_Language, &languageFactory), hrForCommonGlobObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_Language, &languageStatics), hrForCommonGlobObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_DateTimeFormatting_DateTimeFormatter, &dateTimeFormatterFactory), hrForCommonGlobObjectsInit);

        hrForCommonGlobObjectsInit = S_OK;
        initializedCommonGlobObjects = true;

        return hr;
    }


    HRESULT WindowsGlobalizationAdapter::EnsureDateTimeFormatObjectsInitialized(DelayLoadWindowsGlobalization *library)
    {TRACE_IT(38048);
        HRESULT hr = S_OK;

        if (initializedDateTimeFormatObjects)
        {TRACE_IT(38049);
            AssertMsg(hrForDateTimeFormatObjectsInit == S_OK, "If DateTimeFormatObjects are initialized, we should be returning S_OK.");
            return hrForDateTimeFormatObjectsInit;
        }
        else if (hrForDateTimeFormatObjectsInit != S_OK)
        {TRACE_IT(38050);
            return hrForDateTimeFormatObjectsInit;
        }

        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_Calendar, &calendarFactory), hrForDateTimeFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(this->CreateTimeZoneOnCalendar(library, &defaultTimeZoneCalendar), hrForDateTimeFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(this->CreateTimeZoneOnCalendar(library, &timeZoneCalendar), hrForDateTimeFormatObjectsInit);

        hrForDateTimeFormatObjectsInit = S_OK;
        initializedDateTimeFormatObjects = true;

        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::EnsureNumberFormatObjectsInitialized(DelayLoadWindowsGlobalization *library)
    {TRACE_IT(38051);
        HRESULT hr = S_OK;

        if (initializedNumberFormatObjects)
        {TRACE_IT(38052);
            AssertMsg(hrForNumberFormatObjectsInit == S_OK, "If NumberFormatObjects are initialized, we should be returning S_OK.");
            return hrForNumberFormatObjectsInit;
        }
        else if (hrForNumberFormatObjectsInit != S_OK)
        {TRACE_IT(38053);
            return hrForNumberFormatObjectsInit;
        }

        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_CurrencyFormatter, &currencyFormatterFactory), hrForNumberFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_DecimalFormatter, &decimalFormatterFactory), hrForNumberFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_PercentFormatter, &percentFormatterFactory), hrForNumberFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_SignificantDigitsNumberRounder, &significantDigitsRounderActivationFactory), hrForNumberFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_IncrementNumberRounder, &incrementNumberRounderActivationFactory), hrForNumberFormatObjectsInit);

        hrForNumberFormatObjectsInit = S_OK;
        initializedNumberFormatObjects = true;

        return hr;
    }

#endif

#if ENABLE_UNICODE_API
    HRESULT WindowsGlobalizationAdapter::EnsureDataTextObjectsInitialized(DelayLoadWindowsGlobalization *library)
    {TRACE_IT(38054);
        HRESULT hr = S_OK;

        if (initializedCharClassifierObjects)
        {TRACE_IT(38055);
            AssertMsg(hrForCharClassifierObjectsInit == S_OK, "If DataTextObjects are initialized, we should be returning S_OK.");
            return hrForCharClassifierObjectsInit;
        }
        else if (hrForCharClassifierObjectsInit != S_OK)
        {TRACE_IT(38056);
            return hrForCharClassifierObjectsInit;
        }

        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Data_Text_UnicodeCharacters, &unicodeStatics), hrForCharClassifierObjectsInit);

        hrForCharClassifierObjectsInit = S_OK;
        initializedCharClassifierObjects = true;

        return hr;
    }
#endif

#ifdef ENABLE_INTL_OBJECT
    HRESULT WindowsGlobalizationAdapter::CreateLanguage(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR languageTag, ILanguage** language)
    {TRACE_IT(38057);
        HRESULT hr = S_OK;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;

        // OK for languageTag to get truncated as it would pass incomplete languageTag below which
        // will be rejected by globalization dll
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(languageTag, static_cast<UINT32>(wcslen(languageTag)), &hStringHdr, &hString));
        AnalysisAssert(hString);
        IfFailedReturn(this->languageFactory->CreateLanguage(hString, language));
        return hr;
    }

    boolean WindowsGlobalizationAdapter::IsWellFormedLanguageTag(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR languageTag)
    {TRACE_IT(38058);
        boolean retVal;
        HRESULT hr;
        HSTRING hString = nullptr;
        HSTRING_HEADER hStringHdr;
        // OK for languageTag to get truncated as it would pass incomplete languageTag below which
        // will be rejected by globalization dll
        IfFailThrowHr(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(languageTag, static_cast<UINT32>(wcslen(languageTag)), &hStringHdr, &hString));
        if (hString == nullptr)
        {TRACE_IT(38059);
            return 0;
        }
        IfFailThrowHr(this->languageStatics->IsWellFormed(hString, &retVal));
        return retVal;
    }

        // OK for timeZoneId to get truncated as it would pass incomplete timeZoneId below which
        // will be rejected by globalization dll
    HRESULT WindowsGlobalizationAdapter::NormalizeLanguageTag(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR languageTag, HSTRING *result)
    {TRACE_IT(38060);
        HRESULT hr;

        AutoCOMPtr<ILanguage> language;
        IfFailedReturn(CreateLanguage(scriptContext, languageTag, &language));

        IfFailedReturn(language->get_LanguageTag(result));
        IfNullReturnError(*result, E_FAIL);
        return hr;
    }

    boolean WindowsGlobalizationAdapter::ValidateAndCanonicalizeTimeZone(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR timeZoneId, HSTRING *result)
    {TRACE_IT(38061);
        HRESULT hr = S_OK;
        HSTRING timeZone;
        HSTRING_HEADER timeZoneHeader;

        // Construct HSTRING of timeZoneId passed
        IfFailThrowHr(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(timeZoneId, static_cast<UINT32>(wcslen(timeZoneId)), &timeZoneHeader, &timeZone));

        // The warning is timeZone could be '0'. This is valid scenario and in that case, ChangeTimeZone() would
        // return error HR in which case we will throw.
#pragma warning(suppress:6387)
        // ChangeTimeZone should fail if this is not a valid time zone
        hr = timeZoneCalendar->ChangeTimeZone(timeZone);
        if (hr != S_OK)
        {TRACE_IT(38062);
            return false;
        }
        // Retrieve canonicalize timeZone name
        IfFailThrowHr(timeZoneCalendar->GetTimeZone(result));
        if (*result == nullptr)
        {TRACE_IT(38063);
            return false;
        }
        return true;
    }

    HRESULT WindowsGlobalizationAdapter::GetDefaultTimeZoneId(_In_ ScriptContext* scriptContext, HSTRING *result)
    {TRACE_IT(38064);
        HRESULT hr = S_OK;
        IfFailThrowHr(defaultTimeZoneCalendar->GetTimeZone(result));
        IfNullReturnError(*result, E_FAIL);
        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreateTimeZoneOnCalendar(_In_ DelayLoadWindowsGlobalization *library, __out::ITimeZoneOnCalendar**  result)
    {TRACE_IT(38065);
        AutoCOMPtr<::ICalendar> calendar;

        HRESULT hr = S_OK;

        // initialize hard-coded default languages
        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, 1), 1);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, 1), 1);
        IfFailedReturn(library->WindowsCreateStringReference(_u("en-US"), static_cast<UINT32>(wcslen(_u("en-US"))), (headers), (arr)));
        Microsoft::WRL::ComPtr<IIterable<HSTRING>> defaultLanguages;
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&defaultLanguages, arr, 1));


        // Create calendar object
        IfFailedReturn(this->calendarFactory->CreateCalendarDefaultCalendarAndClock(defaultLanguages.Get(), &calendar));

        // Get ITimeZoneOnCalendar part of calendar object
        IfFailedReturn(calendar->QueryInterface(__uuidof(::ITimeZoneOnCalendar), reinterpret_cast<void**>(result)));

        return hr;
    }

#define DetachAndReleaseFactoryObjects(object) \
if (this->object) \
{TRACE_IT(38066); \
    this->object.Detach()->Release(); \
}

    void WindowsGlobalizationAdapter::ResetCommonFactoryObjects()
    {TRACE_IT(38067);
        // Reset only if its not initialized completely.
        if (!this->initializedCommonGlobObjects)
        {TRACE_IT(38068);
            this->hrForCommonGlobObjectsInit = S_OK;
            DetachAndReleaseFactoryObjects(languageFactory);
            DetachAndReleaseFactoryObjects(languageStatics);
            DetachAndReleaseFactoryObjects(dateTimeFormatterFactory);
        }
    }

    void WindowsGlobalizationAdapter::ResetTimeZoneFactoryObjects()
    {TRACE_IT(38069);
        DetachAndReleaseFactoryObjects(timeZoneCalendar);
        DetachAndReleaseFactoryObjects(defaultTimeZoneCalendar);
    }

    void WindowsGlobalizationAdapter::ResetDateTimeFormatFactoryObjects()
    {TRACE_IT(38070);
        // Reset only if its not initialized completely.
        if (!this->initializedDateTimeFormatObjects)
        {TRACE_IT(38071);
            this->hrForDateTimeFormatObjectsInit = S_OK;
            DetachAndReleaseFactoryObjects(calendarFactory);
            DetachAndReleaseFactoryObjects(timeZoneCalendar);
            DetachAndReleaseFactoryObjects(defaultTimeZoneCalendar);
        }
    }

    void WindowsGlobalizationAdapter::ResetNumberFormatFactoryObjects()
    {TRACE_IT(38072);
        // Reset only if its not initialized completely.
        if (!this->initializedNumberFormatObjects)
        {TRACE_IT(38073);
            this->hrForNumberFormatObjectsInit = S_OK;
            DetachAndReleaseFactoryObjects(currencyFormatterFactory);
            DetachAndReleaseFactoryObjects(decimalFormatterFactory);
            DetachAndReleaseFactoryObjects(percentFormatterFactory);
            DetachAndReleaseFactoryObjects(incrementNumberRounderActivationFactory);
            DetachAndReleaseFactoryObjects(significantDigitsRounderActivationFactory);
        }
    }

#undef DetachAndReleaseFactoryObjects

    HRESULT WindowsGlobalizationAdapter::CreateCurrencyFormatterCode(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR currencyCode, NumberFormatting::ICurrencyFormatter** currencyFormatter)
    {TRACE_IT(38074);
        HRESULT hr;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;

        // OK for currencyCode to get truncated as it would pass incomplete currencyCode below which
        // will be rejected by globalization dll
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(currencyCode, static_cast<UINT32>(wcslen(currencyCode)), &hStringHdr, &hString));
        AnalysisAssert(hString);
        IfFailedReturn(this->currencyFormatterFactory->CreateCurrencyFormatterCode(hString, currencyFormatter));
        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreateCurrencyFormatter(_In_ ScriptContext* scriptContext, PCWSTR* localeStrings, uint32 numLocaleStrings, _In_z_ PCWSTR currencyCode, NumberFormatting::ICurrencyFormatter** currencyFormatter)
    {TRACE_IT(38075);
        HRESULT hr;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;

        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, numLocaleStrings), numLocaleStrings);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, numLocaleStrings), numLocaleStrings);
        for(uint32 i = 0; i< numLocaleStrings; i++)
        {TRACE_IT(38076);
            // OK for localeString to get truncated as it would pass incomplete localeString below which
            // will be rejected by globalization dll.
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(localeStrings[i], static_cast<UINT32>(wcslen(localeStrings[i])), (headers + i), (arr + i)));
        }

        Microsoft::WRL::ComPtr<IIterable<HSTRING>> languages(nullptr);
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&languages, arr, numLocaleStrings));

        HSTRING geoString;
        HSTRING_HEADER geoStringHeader;
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(_u("ZZ"), 2, &geoStringHeader, &geoString));
        AnalysisAssert(geoString);
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(currencyCode, static_cast<UINT32>(wcslen(currencyCode)), &hStringHdr, &hString));
        AnalysisAssert(hString);
        IfFailedReturn(this->currencyFormatterFactory->CreateCurrencyFormatterCodeContext(hString, languages.Get(), geoString, currencyFormatter));
        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreateNumberFormatter(_In_ ScriptContext* scriptContext, PCWSTR* localeStrings, uint32 numLocaleStrings, NumberFormatting::INumberFormatter** numberFormatter)
    {TRACE_IT(38077);
        HRESULT hr = S_OK;

        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, numLocaleStrings), numLocaleStrings);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, numLocaleStrings), numLocaleStrings);
        for(uint32 i = 0; i< numLocaleStrings; i++)
        {TRACE_IT(38078);
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(localeStrings[i], static_cast<UINT32>(wcslen(localeStrings[i])), (headers + i), (arr + i)));
        }

        Microsoft::WRL::ComPtr<IIterable<HSTRING>> languages(nullptr);
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&languages, arr, numLocaleStrings));

        HSTRING geoString;
        HSTRING_HEADER geoStringHeader;
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(_u("ZZ"), 2, &geoStringHeader, &geoString));
        AnalysisAssert(geoString);
        IfFailedReturn(this->decimalFormatterFactory->CreateDecimalFormatter(languages.Get(), geoString, numberFormatter));
        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreatePercentFormatter(_In_ ScriptContext* scriptContext, PCWSTR* localeStrings, uint32 numLocaleStrings, NumberFormatting::INumberFormatter** numberFormatter)
    {TRACE_IT(38079);
        HRESULT hr = S_OK;

        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, numLocaleStrings), numLocaleStrings);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, numLocaleStrings), numLocaleStrings);
        for(uint32 i = 0; i< numLocaleStrings; i++)
        {TRACE_IT(38080);
            // OK for localeString to get truncated as it would pass incomplete localeString below which
            // will be rejected by globalization dll.
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(localeStrings[i], static_cast<UINT32>(wcslen(localeStrings[i])), (headers + i), (arr + i)));
        }

        Microsoft::WRL::ComPtr<IIterable<HSTRING>> languages(nullptr);
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&languages, arr, numLocaleStrings));

        HSTRING geoString;
        HSTRING_HEADER geoStringHeader;
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(_u("ZZ"), 2, &geoStringHeader, &geoString));
        AnalysisAssert(geoString);
        IfFailedReturn(this->percentFormatterFactory->CreatePercentFormatter(languages.Get(), geoString, numberFormatter));

        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreateDateTimeFormatter(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR formatString, _In_z_ PCWSTR* localeStrings,
        uint32 numLocaleStrings, _In_opt_z_ PCWSTR calendar, _In_opt_z_ PCWSTR clock, _Out_ DateTimeFormatting::IDateTimeFormatter** result)
    {TRACE_IT(38081);
        HRESULT hr = S_OK;

        if(numLocaleStrings == 0) return E_INVALIDARG;

        AnalysisAssert((calendar == nullptr && clock == nullptr) || (calendar != nullptr && clock != nullptr));

        HSTRING fsHString;
        HSTRING_HEADER fsHStringHdr;

        // OK for formatString to get truncated as it would pass incomplete formatString below which
        // will be rejected by globalization dll.
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(formatString, static_cast<UINT32>(wcslen(formatString)), &fsHStringHdr, &fsHString));
        AnalysisAssert(fsHString);

        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, numLocaleStrings), numLocaleStrings);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, numLocaleStrings), numLocaleStrings);
        for(uint32 i = 0; i< numLocaleStrings; i++)
        {TRACE_IT(38082);
            // OK for localeString to get truncated as it would pass incomplete localeString below which
            // will be rejected by globalization dll.
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(localeStrings[i], static_cast<UINT32>(wcslen(localeStrings[i])), (headers + i), (arr + i)));
        }

        Microsoft::WRL::ComPtr<IIterable<HSTRING>> languages(nullptr);
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&languages, arr, numLocaleStrings));

        if(clock == nullptr)
        {TRACE_IT(38083);
            IfFailedReturn(this->dateTimeFormatterFactory->CreateDateTimeFormatterLanguages(fsHString, languages.Get(), result));
        }
        else
        {TRACE_IT(38084);
            HSTRING geoString;
            HSTRING_HEADER geoStringHeader;
            HSTRING calString;
            HSTRING_HEADER calStringHeader;
            HSTRING clockString;
            HSTRING_HEADER clockStringHeader;

            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(_u("ZZ"), 2, &geoStringHeader, &geoString));
            AnalysisAssert(geoString);

            // OK for calendar/clock to get truncated as it would pass incomplete text below which
            // will be rejected by globalization dll.
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(calendar, static_cast<UINT32>(wcslen(calendar)), &calStringHeader, &calString));
            AnalysisAssert(calString);
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(clock, static_cast<UINT32>(wcslen(clock)), &clockStringHeader, &clockString));
            AnalysisAssert(clockString);
            IfFailedReturn(this->dateTimeFormatterFactory->CreateDateTimeFormatterContext(fsHString, languages.Get(), geoString, calString, clockString, result));
        }
        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreateIncrementNumberRounder(_In_ ScriptContext* scriptContext, NumberFormatting::INumberRounder** numberRounder)
    {TRACE_IT(38085);
        return incrementNumberRounderActivationFactory->ActivateInstance(reinterpret_cast<IInspectable**>(numberRounder));
    }

    HRESULT WindowsGlobalizationAdapter::CreateSignificantDigitsRounder(_In_ ScriptContext* scriptContext, NumberFormatting::INumberRounder** numberRounder)
    {TRACE_IT(38086);
        return significantDigitsRounderActivationFactory->ActivateInstance(reinterpret_cast<IInspectable**>(numberRounder));
    }

    HRESULT WindowsGlobalizationAdapter::GetResolvedLanguage(_In_ DateTimeFormatting::IDateTimeFormatter* formatter, HSTRING * locale)
    {TRACE_IT(38087);
        HRESULT hr = formatter->get_ResolvedLanguage(locale);
        return VerifyResult(locale, hr);
    }

    HRESULT WindowsGlobalizationAdapter::GetResolvedLanguage(_In_ NumberFormatting::INumberFormatterOptions* formatter, HSTRING * locale)
    {TRACE_IT(38088);
        HRESULT hr = formatter->get_ResolvedLanguage(locale);
        return VerifyResult(locale, hr);
    }

    HRESULT WindowsGlobalizationAdapter::GetNumeralSystem(_In_ NumberFormatting::INumberFormatterOptions* formatter, HSTRING * hNumeralSystem)
    {TRACE_IT(38089);
        HRESULT hr = formatter->get_NumeralSystem(hNumeralSystem);
        return VerifyResult(hNumeralSystem, hr);
    }

    HRESULT WindowsGlobalizationAdapter::GetNumeralSystem(_In_ DateTimeFormatting::IDateTimeFormatter* formatter, HSTRING * hNumeralSystem)
    {TRACE_IT(38090);
        HRESULT hr = formatter->get_NumeralSystem(hNumeralSystem);
        return VerifyResult(hNumeralSystem, hr);
    }

    HRESULT WindowsGlobalizationAdapter::GetCalendar(_In_ DateTimeFormatting::IDateTimeFormatter* formatter, HSTRING * hCalendar)
    {TRACE_IT(38091);
        HRESULT hr = formatter->get_Calendar(hCalendar);
        return VerifyResult(hCalendar, hr);
    }

    HRESULT WindowsGlobalizationAdapter::GetClock(_In_ DateTimeFormatting::IDateTimeFormatter* formatter, HSTRING * hClock)
    {TRACE_IT(38092);
        HRESULT hr = formatter->get_Clock(hClock);
        return VerifyResult(hClock, hr);
    }

    HRESULT WindowsGlobalizationAdapter::GetItemAt(_In_ IVectorView<HSTRING>* vector, _In_ uint32 index, HSTRING * item)
    {TRACE_IT(38093);
        HRESULT hr = vector->GetAt(index, item);
        return VerifyResult(item, hr);
    }

    /* static */
    HRESULT WindowsGlobalizationAdapter::VerifyResult(HSTRING * result, HRESULT errCode)
    {TRACE_IT(38094);
        HRESULT hr = S_OK;
        IfFailedReturn(errCode);
        IfNullReturnError(*result, E_FAIL);
        return hr;
    }
#endif
}


#endif
