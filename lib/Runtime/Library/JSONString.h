//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    enum EscapingOperation : BYTE
    {
        EscapingOperation_NotEscape,
        EscapingOperation_Escape,
        EscapingOperation_Count
    };

    class WritableStringBuffer
    {
    public:
        WritableStringBuffer(_In_count_(length) CHAR_T* str, _In_ charcount_t length) : m_pszString(str), m_pszCurrentPtr(str), m_length(length) {}

        void Append(CHAR_T c);
        void Append(const CHAR_T * str, charcount_t countNeeded);
        void AppendLarge(const CHAR_T * str, charcount_t countNeeded);
    private:
        CHAR_T* m_pszString;
        CHAR_T* m_pszCurrentPtr;
        charcount_t m_length;
#if DBG
        charcount_t GetCount()
        {
            Assert(m_pszCurrentPtr >= m_pszString);
            Assert(m_pszCurrentPtr - m_pszString <= MaxCharCount);
            return static_cast<charcount_t>(m_pszCurrentPtr - m_pszString);
        }
#endif
    };

    class JSONString : public JavascriptString
    {
    public:
        static JSONString* New(JavascriptString* originalString, charcount_t start, charcount_t extraChars);
        virtual const CHAR_T* GetSz() override;
    protected:
        DEFINE_VTABLE_CTOR(JSONString, JavascriptString);
    private:
        Field(JavascriptString*) m_originalString;
        Field(charcount_t) m_start; /* start of the escaping operation */

    private:
        JSONString(JavascriptString* originalString, charcount_t start, charcount_t length);
        static const CHAR_T escapeMap[128];
        static const BYTE escapeMapCount[128];
    public:
        template <EscapingOperation op>
        static Js::JavascriptString* Escape(Js::JavascriptString* value, uint start = 0, WritableStringBuffer* outputString = nullptr)
        {
            uint len = value->GetLength();

            if (0 == len)
            {
                Js::ScriptContext* scriptContext = value->GetScriptContext();
                return scriptContext->GetLibrary()->GetQuotesString();
            }
            else
            {
                const CHAR_T* szValue = value->GetSz();
                return EscapeNonEmptyString<op, Js::JSONString, Js::ConcatStringWrapping<_u('"'), _u('"')>, Js::JavascriptString*>(value, szValue, start, len, outputString);
            }
        }

        template <EscapingOperation op, class TJSONString, class TConcatStringWrapping, class TJavascriptString>
        static TJavascriptString EscapeNonEmptyString(Js::JavascriptString* value, const CHAR_T* szValue, uint start, charcount_t len, WritableStringBuffer* outputString)
        {
            charcount_t extra = 0;
            TJavascriptString result;

            // Optimize for the case when we don't need to change anything, just wrap with quotes.
            // If we realize we need to change the inside of the string, start over in "modification needed" mode.
            if (op == EscapingOperation_Escape)
            {
                outputString->Append(_u('\"'));
                if (start != 0)
                {
                    outputString->AppendLarge(szValue, start);
                }
            }
            const CHAR_T* endSz = szValue + len;
            const CHAR_T* startSz = szValue + start;
            const CHAR_T* lastFlushSz = startSz;
            for (const CHAR_T* current = startSz; current < endSz; current++)
            {
                CHAR_T wch = *current;

                if (op == EscapingOperation_Count)
                {
                    if (wch < _countof(escapeMap))
                    {
                        extra = UInt32Math::Add(extra, escapeMapCount[static_cast<int>((char)wch)]);
                    }
                }
                else
                {
                    CHAR_T specialChar;
                    if (wch < _countof(escapeMap))
                    {
                        specialChar = escapeMap[static_cast<int>((char)wch)];
                    }
                    else
                    {
                        specialChar = '\0';
                    }

                    if (specialChar != '\0')
                    {
                        if (op == EscapingOperation_Escape)
                        {
                            outputString->AppendLarge(lastFlushSz, (charcount_t)(current - lastFlushSz));
                            lastFlushSz = current + 1;
                            outputString->Append(_u('\\'));
                            outputString->Append(specialChar);
                            if (specialChar == _u('u'))
                            {
                                CHAR_T bf[5];
                                _ltow_s(wch, bf, _countof(bf), 16);
                                size_t count = cstrlen(bf);
                                if (count < 4)
                                {
                                    if (count == 1)
                                    {
                                        outputString->Append(_u("000"), 3);
                                    }
                                    else if (count == 2)
                                    {
                                        outputString->Append(_u("00"), 2);
                                    }
                                    else
                                    {
                                        outputString->Append(_u("0"), 1);
                                    }
                                }
                                outputString->Append(bf, (charcount_t)count);
                            }
                        }
                        else
                        {
                            charcount_t i = (charcount_t)(current - startSz);
                            return EscapeNonEmptyString<EscapingOperation_Count, TJSONString, TConcatStringWrapping, TJavascriptString>(value, szValue, i ? i - 1 : 0, len, outputString);
                        }
                    }
                }
            } // for.

            if (op == EscapingOperation_Escape)
            {
                if (lastFlushSz < endSz)
                {
                    outputString->AppendLarge(lastFlushSz, (charcount_t)(endSz - lastFlushSz));
                }
                outputString->Append(_u('\"'));
                result = nullptr;
            }
            else if (op == EscapingOperation_Count)
            {
                result = TJSONString::New(value, start, extra);
            }
            else
            {
                // If we got here, we don't need to change the inside, just wrap the string with quotes.
                result = TConcatStringWrapping::New(value);
            }

            return result;
        }

        static CHAR_T* EscapeNonEmptyString(ArenaAllocator* allocator, const CHAR_T* szValue)
        {
            CHAR_T* result = nullptr;
            StringProxy::allocator = allocator;
            charcount_t len = (charcount_t)cstrlen(szValue);
            StringProxy* proxy = EscapeNonEmptyString<EscapingOperation_NotEscape, StringProxy, StringProxy, StringProxy*>(nullptr, szValue, 0, len, nullptr);
            result = proxy->GetResult(szValue, len);
            StringProxy::allocator = nullptr;
            return result;
        }

        // This class has the same interface (with respect to the EscapeNonEmptyString method) as JSONString and TConcatStringWrapping
        // It is used in scenario where we want to use the JSON escaping capability without having a script context.
        class StringProxy
        {
        public:
            static ArenaAllocator* allocator;

            StringProxy()
            {
                this->m_needEscape = false;
            }

            StringProxy(int start, int extra) : m_start(start), m_extra(extra)
            {
                this->m_needEscape = true;
            }

            static StringProxy* New(Js::JavascriptString* value)
            {
                // Case 1: The string do not need to be escaped at all
                Assert(value == nullptr);
                Assert(allocator != nullptr);
                return Anew(allocator, StringProxy);
            }

            static StringProxy* New(Js::JavascriptString* value, uint start, uint length)
            {
                // Case 2: The string requires escaping, and the length is computed
                Assert(value == nullptr);
                Assert(allocator != nullptr);
                return Anew(allocator, StringProxy, start, length);
            }

            CHAR_T* GetResult(const CHAR_T* originalString, charcount_t originalLength)
            {
                if (this->m_needEscape)
                {
                    charcount_t unescapedStringLength = originalLength + m_extra + 2 /* for the quotes */;
                    CHAR_T* buffer = AnewArray(allocator, CHAR_T, unescapedStringLength + 1); /* for terminating null */
                    buffer[unescapedStringLength] = '\0';
                    WritableStringBuffer stringBuffer(buffer, unescapedStringLength);
                    StringProxy* proxy = JSONString::EscapeNonEmptyString<EscapingOperation_Escape, StringProxy, StringProxy, StringProxy*>(nullptr, originalString, m_start, originalLength, &stringBuffer);
                    Assert(proxy == nullptr);
                    Assert(buffer[unescapedStringLength] == '\0');
                    return buffer;
                }
                else
                {
                    CHAR_T* buffer = AnewArray(allocator, CHAR_T, originalLength + 3); /* quotes and terminating null */
                    buffer[0] = _u('\"');
                    buffer[originalLength + 1] = _u('\"');
                    buffer[originalLength + 2] = _u('\0');
                    js_wmemcpy_s(buffer + 1, originalLength, originalString, originalLength);
                    return buffer;
                }
            }

        private:
            int m_extra;
            int m_start;
            bool m_needEscape;
        };
    };
}
