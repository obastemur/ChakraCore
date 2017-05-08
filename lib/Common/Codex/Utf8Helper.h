//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "Utf8Codex.h"

namespace utf8
{
    ///
    /// Use the codex library to encode a UTF16 string to UTF8.
    /// The caller is responsible for freeing the memory, which is allocated
    /// using Allocator.
    /// The returned string is null terminated.
    ///
    template <class Allocator>
    HRESULT WideStringToNarrow(_In_ LPCWSTR sourceString, size_t sourceCount, _Out_ LPSTR* destStringPtr, _Out_ size_t* destCount, size_t* allocateCount = nullptr)
    {TRACE_IT(18757);
        size_t cchSourceString = sourceCount;

        if (cchSourceString >= MAXUINT32)
        {TRACE_IT(18758);
            return E_OUTOFMEMORY;
        }

        size_t cbDestString = (cchSourceString + 1) * 3;

        // Check for overflow- cbDestString should be >= cchSourceString
        if (cbDestString < cchSourceString)
        {TRACE_IT(18759);
            return E_OUTOFMEMORY;
        }

        utf8char_t* destString = (utf8char_t*)Allocator::allocate(cbDestString);
        if (destString == nullptr)
        {TRACE_IT(18760);
            return E_OUTOFMEMORY;
        }

        size_t cbEncoded = utf8::EncodeTrueUtf8IntoAndNullTerminate(destString, sourceString, (charcount_t) cchSourceString);
        Assert(cbEncoded <= cbDestString);
        static_assert(sizeof(utf8char_t) == sizeof(char), "Needs to be valid for cast");
        *destStringPtr = (char*)destString;
        *destCount = cbEncoded;
        if (allocateCount != nullptr) *allocateCount = cbEncoded;
        return S_OK;
    }

    ///
    /// Use the codex library to encode a UTF8 string to UTF16.
    /// The caller is responsible for freeing the memory, which is allocated
    /// using Allocator.
    /// The returned string is null terminated.
    ///
    template <class Allocator>
    HRESULT NarrowStringToWide(_In_ LPCSTR sourceString, size_t sourceCount, _Out_ LPWSTR* destStringPtr, _Out_ size_t* destCount, size_t* allocateCount = nullptr)
    {TRACE_IT(18761);
        size_t cbSourceString = sourceCount;
        size_t sourceStart = 0;
        size_t cbDestString = (sourceCount + 1) * sizeof(WCHAR);
        if (cbDestString < sourceCount) // overflow ?
        {TRACE_IT(18762);
            return E_OUTOFMEMORY;
        }

        WCHAR* destString = (WCHAR*)Allocator::allocate(cbDestString);
        if (destString == nullptr)
        {TRACE_IT(18763);
            return E_OUTOFMEMORY;
        }

        if (allocateCount != nullptr) *allocateCount = cbDestString;

        for (; sourceStart < sourceCount; sourceStart++)
        {TRACE_IT(18764);
            const char ch = sourceString[sourceStart];
            if ( ! (ch > 0 && ch < 0x0080) )
            {TRACE_IT(18765);
                size_t fallback = sourceStart > 3 ? 3 : sourceStart; // 3 + 1 -> fallback at least 1 unicode char
                sourceStart -= fallback;
                break;
            }
            destString[sourceStart] = (WCHAR) ch;
        }

        if (sourceStart == sourceCount)
        {TRACE_IT(18766);
            *destCount = sourceCount;
            destString[sourceCount] = WCHAR(0);
            *destStringPtr = destString;
        }
        else
        {TRACE_IT(18767);
            LPCUTF8 remSourceString = (LPCUTF8)sourceString + sourceStart;
            WCHAR *remDestString = destString + sourceStart;

            charcount_t cchDestString = utf8::ByteIndexIntoCharacterIndex(remSourceString, cbSourceString - sourceStart);
            cchDestString += (charcount_t)sourceStart;
            Assert (cchDestString <= sourceCount);

            // Some node tests depend on the utf8 decoder not swallowing invalid unicode characters
            // instead of replacing them with the "replacement" chracter. Pass a flag to our
            // decoder to require such behavior
            utf8::DecodeUnitsIntoAndNullTerminateNoAdvance(remDestString, remSourceString, (LPCUTF8) sourceString + cbSourceString, DecodeOptions::doAllowInvalidWCHARs);
            Assert(destString[cchDestString] == 0);
            static_assert(sizeof(utf8char_t) == sizeof(char), "Needs to be valid for cast");
            *destStringPtr = destString;
            *destCount = cchDestString;
        }
        return S_OK;
    }

    class malloc_allocator
    {
    public:
        static void* allocate(size_t size) {TRACE_IT(18768); return ::malloc(size); }
        static void free(void* ptr, size_t count) {TRACE_IT(18769); ::free(ptr); }
    };

    inline HRESULT WideStringToNarrowDynamic(_In_ LPCWSTR sourceString, _Out_ LPSTR* destStringPtr)
    {TRACE_IT(18770);
        size_t unused;
        return WideStringToNarrow<malloc_allocator>(
            sourceString, wcslen(sourceString), destStringPtr, &unused);
    }

    inline HRESULT NarrowStringToWideDynamic(_In_ LPCSTR sourceString, _Out_ LPWSTR* destStringPtr)
    {TRACE_IT(18771);
        size_t unused;
        return NarrowStringToWide<malloc_allocator>(
            sourceString, strlen(sourceString), destStringPtr, &unused);
    }

    inline HRESULT NarrowStringToWideDynamicGetLength(_In_ LPCSTR sourceString, _Out_ LPWSTR* destStringPtr, _Out_ size_t* destLength)
    {TRACE_IT(18772);
        return NarrowStringToWide<malloc_allocator>(
            sourceString, strlen(sourceString), destStringPtr, destLength);
    }

    template <class Allocator, class SrcType, class DstType>
    class NarrowWideStringConverter
    {
    public:
        static size_t Length(const SrcType& src);
        static HRESULT Convert(
            SrcType src, size_t srcCount, DstType* dst, size_t* dstCount, size_t* allocateCount = nullptr);
    };

    template <class Allocator>
    class NarrowWideStringConverter<Allocator, LPCSTR, LPWSTR>
    {
    public:
        // Note: Typically caller should pass in Utf8 string length. Following
        // is used as fallback.
        static size_t Length(LPCSTR src)
        {TRACE_IT(18773);
            return strnlen(src, INT_MAX);
        }

        static HRESULT Convert(
            LPCSTR sourceString, size_t sourceCount,
            LPWSTR* destStringPtr, size_t* destCount, size_t* allocateCount = nullptr)
        {TRACE_IT(18774);
            return NarrowStringToWide<Allocator>(
                sourceString, sourceCount, destStringPtr, destCount, allocateCount);
        }
    };

    template <class Allocator>
    class NarrowWideStringConverter<Allocator, LPCWSTR, LPSTR>
    {
    public:
        // Note: Typically caller should pass in WCHAR string length. Following
        // is used as fallback.
        static size_t Length(LPCWSTR src)
        {TRACE_IT(18775);
            return wcslen(src);
        }

        static HRESULT Convert(
            LPCWSTR sourceString, size_t sourceCount,
            LPSTR* destStringPtr, size_t* destCount, size_t* allocateCount = nullptr)
        {TRACE_IT(18776);
            return WideStringToNarrow<Allocator>(
                sourceString, sourceCount, destStringPtr, destCount, allocateCount);
        }
    };

    template <class Allocator, class SrcType, class DstType>
    class NarrowWideConverter
    {
        typedef NarrowWideStringConverter<Allocator, SrcType, DstType>
            StringConverter;
    private:
        DstType dst;
        size_t dstCount;
        size_t allocateCount;

    public:
        NarrowWideConverter() : dst()
        {TRACE_IT(18777);
            // do nothing
        }

        NarrowWideConverter(const SrcType& src, size_t srcCount = -1): dst()
        {
            Initialize(src, srcCount);
        }

        void Initialize(const SrcType& src, size_t srcCount = -1)
        {TRACE_IT(18778);
            if (srcCount == -1)
            {TRACE_IT(18779);
                srcCount = StringConverter::Length(src);
            }

            StringConverter::Convert(src, srcCount, &dst, &dstCount, &allocateCount);
        }

        ~NarrowWideConverter()
        {TRACE_IT(18780);
            if (dst)
            {TRACE_IT(18781);
                Allocator::free(dst, allocateCount);
            }
        }

        DstType Detach()
        {TRACE_IT(18782);
            DstType result = dst;
            dst = DstType();
            return result;
        }

        operator DstType()
        {TRACE_IT(18783);
            return dst;
        }

        size_t Length() const
        {TRACE_IT(18784);
            return dstCount;
        }
    };

    typedef NarrowWideConverter<malloc_allocator, LPCSTR, LPWSTR> NarrowToWide;
    typedef NarrowWideConverter<malloc_allocator, LPCWSTR, LPSTR> WideToNarrow;
}
