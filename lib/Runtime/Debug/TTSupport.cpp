//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#if ENABLE_TTD

void TTDAbort_fatal_error(const char* msg)
{LOGMEIN("TTSupport.cpp] 9\n");
    printf("TTD assert failed: %s\n", msg);

    int scenario = 101;
    ReportFatalException(NULL, E_UNEXPECTED, Fatal_TTDAbort, scenario);
}

namespace TTD
{
    TTModeStack::TTModeStack()
        : m_stackEntries(nullptr), m_stackTop(0), m_stackMax(16)
    {LOGMEIN("TTSupport.cpp] 20\n");
        this->m_stackEntries = TT_HEAP_ALLOC_ARRAY_ZERO(TTDMode, 16);
    }

    TTModeStack::~TTModeStack()
    {
        TT_HEAP_FREE_ARRAY(TTDMode, this->m_stackEntries, this->m_stackMax);
    }

    uint32 TTModeStack::Count() const
    {LOGMEIN("TTSupport.cpp] 30\n");
        return this->m_stackTop;
    }

    TTDMode TTModeStack::GetAt(uint32 index) const
    {LOGMEIN("TTSupport.cpp] 35\n");
        TTDAssert(index < this->m_stackTop, "index is out of range");

        return this->m_stackEntries[index];
    }

    void TTModeStack::SetAt(uint32 index, TTDMode m)
    {LOGMEIN("TTSupport.cpp] 42\n");
        TTDAssert(index < this->m_stackTop, "index is out of range");

        this->m_stackEntries[index] = m;
    }

    void TTModeStack::Push(TTDMode m)
    {LOGMEIN("TTSupport.cpp] 49\n");
        if(this->m_stackTop == this->m_stackMax)
        {LOGMEIN("TTSupport.cpp] 51\n");
            uint32 newMax = this->m_stackMax + 16;
            TTDMode* newStack = TT_HEAP_ALLOC_ARRAY_ZERO(TTDMode, newMax);
            js_memcpy_s(newStack, newMax * sizeof(TTDMode), this->m_stackEntries, this->m_stackMax * sizeof(TTDMode));

            TT_HEAP_FREE_ARRAY(TTDMode, this->m_stackEntries, this->m_stackMax);

            this->m_stackMax = newMax;
            this->m_stackEntries = newStack;
        }

        this->m_stackEntries[this->m_stackTop] = m;
        this->m_stackTop++;
    }

    TTDMode TTModeStack::Peek() const
    {LOGMEIN("TTSupport.cpp] 67\n");
        TTDAssert(this->m_stackTop > 0, "Undeflow in stack pop.");

        return this->m_stackEntries[this->m_stackTop - 1];
    }

    void TTModeStack::Pop()
    {LOGMEIN("TTSupport.cpp] 74\n");
        TTDAssert(this->m_stackTop > 0, "Undeflow in stack pop.");

        this->m_stackTop--;
    }

    namespace UtilSupport
    {
        TTAutoString::TTAutoString()
            : m_allocSize(-1), m_contents(nullptr), m_optFormatBuff(nullptr)
        {LOGMEIN("TTSupport.cpp] 84\n");
            ;
        }

        TTAutoString::TTAutoString(const char16* str)
            : m_allocSize(-1), m_contents(nullptr), m_optFormatBuff(nullptr)
        {LOGMEIN("TTSupport.cpp] 90\n");
            size_t clen = wcslen(str) + 1;

            this->m_contents = TT_HEAP_ALLOC_ARRAY_ZERO(char16, clen);
            this->m_allocSize = (int32)clen;

            js_memcpy_s(this->m_contents, clen * sizeof(char16), str, clen * sizeof(char16));
        }

        TTAutoString::TTAutoString(const TTAutoString& str)
            : m_allocSize(-1), m_contents(nullptr), m_optFormatBuff(nullptr)
        {LOGMEIN("TTSupport.cpp] 101\n");
            this->Append(str.m_contents);
        }

        TTAutoString& TTAutoString::operator=(const TTAutoString& str)
        {LOGMEIN("TTSupport.cpp] 106\n");
            if(this != &str)
            {LOGMEIN("TTSupport.cpp] 108\n");
                this->Clear();

                this->Append(str.GetStrValue());
            }

            return *this;
        }

        TTAutoString::~TTAutoString()
        {LOGMEIN("TTSupport.cpp] 118\n");
            this->Clear();
        }

        void TTAutoString::Clear()
        {LOGMEIN("TTSupport.cpp] 123\n");
            if(this->m_contents != nullptr)
            {
                TT_HEAP_FREE_ARRAY(char16, this->m_contents, (size_t)this->m_allocSize);
                this->m_allocSize = -1;
                this->m_contents = nullptr;
            }

            if(this->m_optFormatBuff != nullptr)
            {
                TT_HEAP_FREE_ARRAY(char16, this->m_optFormatBuff, 64);
                this->m_optFormatBuff = nullptr;
            }
        }

        bool TTAutoString::IsNullString() const
        {LOGMEIN("TTSupport.cpp] 139\n");
            return this->m_contents == nullptr;
        }

        void TTAutoString::Append(const char16* str, size_t start, size_t end)
        {LOGMEIN("TTSupport.cpp] 144\n");
            Assert(end > start);

            if(this->m_contents == nullptr && str == nullptr)
            {LOGMEIN("TTSupport.cpp] 148\n");
                return;
            }

            size_t origsize = (this->m_contents != nullptr ? wcslen(this->m_contents) : 0);
            size_t strsize = 0;
            if(start == 0 && end == SIZE_T_MAX)
            {LOGMEIN("TTSupport.cpp] 155\n");
                strsize = (str != nullptr ? wcslen(str) : 0);
            }
            else
            {
                strsize = (end - start) + 1;
            }

            size_t nsize = origsize + strsize + 1;
            char16* nbuff = TT_HEAP_ALLOC_ARRAY_ZERO(char16, nsize);

            if(this->m_contents != nullptr)
            {
                js_memcpy_s(nbuff, nsize * sizeof(char16), this->m_contents, origsize * sizeof(char16));

                TT_HEAP_FREE_ARRAY(char16, this->m_contents, origsize + 1);
                this->m_allocSize = -1;
                this->m_contents = nullptr;
            }

            if(str != nullptr)
            {LOGMEIN("TTSupport.cpp] 176\n");
                size_t curr = origsize;
                for(size_t i = start; i <= end && str[i] != '\0'; ++i)
                {LOGMEIN("TTSupport.cpp] 179\n");
                    nbuff[curr] = str[i];
                    curr++;
                }
                nbuff[curr] = _u('\0');
            }

            this->m_contents = nbuff;
            this->m_allocSize = (int64)nsize;
        }

        void TTAutoString::Append(const TTAutoString& str, size_t start, size_t end)
        {LOGMEIN("TTSupport.cpp] 191\n");
            this->Append(str.GetStrValue(), start, end);
        }

        void TTAutoString::Append(uint64 val)
        {LOGMEIN("TTSupport.cpp] 196\n");
            if(this->m_optFormatBuff == nullptr)
            {LOGMEIN("TTSupport.cpp] 198\n");
                this->m_optFormatBuff = TT_HEAP_ALLOC_ARRAY_ZERO(char16, 64);
            }

            swprintf_s(this->m_optFormatBuff, 32, _u("%I64u"), val); //64 char16s is 32 words

            this->Append(this->m_optFormatBuff);
        }

        void TTAutoString::Append(LPCUTF8 strBegin, LPCUTF8 strEnd)
        {LOGMEIN("TTSupport.cpp] 208\n");
            int32 strCount = (int32)((strEnd - strBegin) + 1);
            char16* buff = TT_HEAP_ALLOC_ARRAY_ZERO(char16, (size_t)strCount);

            LPCUTF8 curr = strBegin;
            int32 i = 0;
            while(curr != strEnd)
            {LOGMEIN("TTSupport.cpp] 215\n");
                buff[i] = (char16)*curr;
                i++;
                curr++;
            }
            TTDAssert(i + 1 == strCount, "Our indexing is off.");

            buff[i] = _u('\0');
            this->Append(buff);

            TT_HEAP_FREE_ARRAY(char16, buff, (size_t)strCount);
        }

        int32 TTAutoString::GetLength() const
        {LOGMEIN("TTSupport.cpp] 229\n");
            TTDAssert(!this->IsNullString(), "That doesn't make sense.");

            return (int32)wcslen(this->m_contents);
        }

        char16 TTAutoString::GetCharAt(int32 pos) const
        {LOGMEIN("TTSupport.cpp] 236\n");
            TTDAssert(!this->IsNullString(), "That doesn't make sense.");
            TTDAssert(0 <= pos && pos < this->GetLength(), "Not in valid range.");

            return this->m_contents[pos];
        }

        const char16* TTAutoString::GetStrValue() const
        {LOGMEIN("TTSupport.cpp] 244\n");
            return this->m_contents;
        }
    }

    //////////////////

    void LoadValuesForHashTables(uint32 targetSize, uint32* powerOf2, uint32* closePrime, uint32* midPrime)
    {
        TTD_TABLE_FACTORLOAD_BASE(128, 127, 61)

            TTD_TABLE_FACTORLOAD(256, 251, 127)
            TTD_TABLE_FACTORLOAD(512, 511, 251)
            TTD_TABLE_FACTORLOAD(1024, 1021, 511)
            TTD_TABLE_FACTORLOAD(2048, 2039, 1021)
            TTD_TABLE_FACTORLOAD(4096, 4093, 2039)
            TTD_TABLE_FACTORLOAD(8192, 8191, 4093)
            TTD_TABLE_FACTORLOAD(16384, 16381, 8191)
            TTD_TABLE_FACTORLOAD(32768, 32749, 16381)
            TTD_TABLE_FACTORLOAD(65536, 65521, 32749)
            TTD_TABLE_FACTORLOAD(131072, 131071, 65521)
            TTD_TABLE_FACTORLOAD(262144, 262139, 131071)
            TTD_TABLE_FACTORLOAD(524288, 524287, 262139)
            TTD_TABLE_FACTORLOAD(1048576, 1048573, 524287)
            TTD_TABLE_FACTORLOAD(2097152, 2097143, 1048573)
            TTD_TABLE_FACTORLOAD(4194304, 4194301, 2097143)
            TTD_TABLE_FACTORLOAD(8388608, 8388593, 4194301)

        TTD_TABLE_FACTORLOAD_FINAL(16777216, 16777213, 8388593)
    }

    //////////////////

    void InitializeAsNullPtrTTString(TTString& str)
    {LOGMEIN("TTSupport.cpp] 278\n");
        str.Length = 0;
        str.Contents = nullptr;
    }

    bool IsNullPtrTTString(const TTString& str)
    {LOGMEIN("TTSupport.cpp] 284\n");
        return str.Contents == nullptr;
    }

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
    bool TTStringEQForDiagnostics(const TTString& str1, const TTString& str2)
    {LOGMEIN("TTSupport.cpp] 290\n");
        if(IsNullPtrTTString(str1) || IsNullPtrTTString(str2))
        {LOGMEIN("TTSupport.cpp] 292\n");
            return IsNullPtrTTString(str1) && IsNullPtrTTString(str2);
        }

        if(str1.Length != str2.Length)
        {LOGMEIN("TTSupport.cpp] 297\n");
            return false;
        }

        for(uint32 i = 0; i < str1.Length; ++i)
        {LOGMEIN("TTSupport.cpp] 302\n");
            if(str1.Contents[i] != str2.Contents[i])
            {LOGMEIN("TTSupport.cpp] 304\n");
                return false;
            }
        }

        return true;
    }
#endif

    //////////////////

    MarkTable::MarkTable()
        : m_capcity(TTD_MARK_TABLE_INIT_SIZE), m_h2Prime(TTD_MARK_TABLE_INIT_H2PRIME), m_count(0), m_iterPos(0)
    {LOGMEIN("TTSupport.cpp] 317\n");
        this->m_addrArray = TT_HEAP_ALLOC_ARRAY_ZERO(uint64, this->m_capcity);
        this->m_markArray = TT_HEAP_ALLOC_ARRAY_ZERO(MarkTableTag, this->m_capcity);

        memset(this->m_handlerCounts, 0, ((uint32)MarkTableTag::KindTagCount) * sizeof(uint32));
    }

    MarkTable::~MarkTable()
    {
        TT_HEAP_FREE_ARRAY(uint64, this->m_addrArray, this->m_capcity);
        TT_HEAP_FREE_ARRAY(MarkTableTag, this->m_markArray, this->m_capcity);
    }

    void MarkTable::Clear()
    {LOGMEIN("TTSupport.cpp] 331\n");
        if(this->m_capcity == TTD_MARK_TABLE_INIT_SIZE)
        {LOGMEIN("TTSupport.cpp] 333\n");
            memset(this->m_addrArray, 0, TTD_MARK_TABLE_INIT_SIZE * sizeof(uint64));
            memset(this->m_markArray, 0, TTD_MARK_TABLE_INIT_SIZE * sizeof(MarkTableTag));
        }
        else
        {
            TT_HEAP_FREE_ARRAY(uint64, this->m_addrArray, this->m_capcity);
            TT_HEAP_FREE_ARRAY(MarkTableTag, this->m_markArray, this->m_capcity);

            this->m_capcity = TTD_MARK_TABLE_INIT_SIZE;
            this->m_h2Prime = TTD_MARK_TABLE_INIT_H2PRIME;
            this->m_addrArray = TT_HEAP_ALLOC_ARRAY_ZERO(uint64, this->m_capcity);
            this->m_markArray = TT_HEAP_ALLOC_ARRAY_ZERO(MarkTableTag, this->m_capcity);
        }

        this->m_count = 0;
        this->m_iterPos = 0;

        memset(this->m_handlerCounts, 0, ((uint32)MarkTableTag::KindTagCount) * sizeof(uint32));
    }
}

#endif
