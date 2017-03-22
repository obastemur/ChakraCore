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
        WritableStringBuffer(_In_count_(length) char16* str, _In_ charcount_t length) : m_pszString(str), m_pszCurrentPtr(str), m_length(length) {LOGMEIN("JSONString.h] 18\n");}

        void Append(char16 c);
        void Append(const char16 * str, charcount_t countNeeded);
        void AppendLarge(const char16 * str, charcount_t countNeeded);
    private:
        char16* m_pszString;
        char16* m_pszCurrentPtr;
        charcount_t m_length;
#if DBG
        charcount_t GetCount()
        {LOGMEIN("JSONString.h] 29\n");
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
        virtual const char16* GetSz() override;
    protected:
        DEFINE_VTABLE_CTOR(JSONString, JavascriptString);
        DECLARE_CONCRETE_STRING_CLASS;
    private:
        Field(JavascriptString*) m_originalString;
        Field(charcount_t) m_start; /* start of the escaping operation */

    private:
        JSONString(JavascriptString* originalString, charcount_t start, charcount_t length);
        static const WCHAR escapeMap[128];
        static const BYTE escapeMapCount[128];
    public:
        template <EscapingOperation op>
        static Js::JavascriptString* Escape(Js::JavascriptString* value, uint start = 0, WritableStringBuffer* outputString = nullptr)
        {LOGMEIN("JSONString.h] 56\n");
            uint len = value->GetLength();

            if (0 == len)
            {LOGMEIN("JSONString.h] 60\n");
                Js::ScriptContext* scriptContext = value->GetScriptContext();
                return scriptContext->GetLibrary()->GetQuotesString();
            }
            else
            {
                const char16* szValue = value->GetSz();
                return EscapeNonEmptyString<op, Js::JSONString, Js::ConcatStringWrapping<_u('"'), _u('"')>, Js::JavascriptString*>(value, szValue, start, len, outputString);
            }
        }

        template <EscapingOperation op, class TJSONString, class TConcatStringWrapping, class TJavascriptString>
        static TJavascriptString EscapeNonEmptyString(Js::JavascriptString* value, const char16* szValue, uint start, charcount_t len, WritableStringBuffer* outputString)
        {LOGMEIN("JSONString.h] 73\n");
            charcount_t extra = 0;
            TJavascriptString result;

            // Optimize for the case when we don't need to change anything, just wrap with quotes.
            // If we realize we need to change the inside of the string, start over in "modification needed" mode.
            if (op == EscapingOperation_Escape)
            {LOGMEIN("JSONString.h] 80\n");
                outputString->Append(_u('\"'));
                if (start != 0)
                {LOGMEIN("JSONString.h] 83\n");
                    outputString->AppendLarge(szValue, start);
                }
            }
            const wchar* endSz = szValue + len;
            const wchar* startSz = szValue + start;
            const wchar* lastFlushSz = startSz;
            for (const wchar* current = startSz; current < endSz; current++)
            {LOGMEIN("JSONString.h] 91\n");
                WCHAR wch = *current;

                if (op == EscapingOperation_Count)
                {LOGMEIN("JSONString.h] 95\n");
                    if (wch < _countof(escapeMap))
                    {LOGMEIN("JSONString.h] 97\n");
                        extra = UInt32Math::Add(extra, escapeMapCount[static_cast<int>((char)wch)]);
                    }
                }
                else
                {
                    WCHAR specialChar;
                    if (wch < _countof(escapeMap))
                    {LOGMEIN("JSONString.h] 105\n");
                        specialChar = escapeMap[static_cast<int>((char)wch)];
                    }
                    else
                    {
                        specialChar = '\0';
                    }

                    if (specialChar != '\0')
                    {
                        if (op == EscapingOperation_Escape)
                        {LOGMEIN("JSONString.h] 116\n");
                            outputString->AppendLarge(lastFlushSz, (charcount_t)(current - lastFlushSz));
                            lastFlushSz = current + 1;
                            outputString->Append(_u('\\'));
                            outputString->Append(specialChar);
                            if (specialChar == _u('u'))
                            {LOGMEIN("JSONString.h] 122\n");
                                char16 bf[5];
                                _ltow_s(wch, bf, _countof(bf), 16);
                                size_t count = wcslen(bf);
                                if (count < 4)
                                {LOGMEIN("JSONString.h] 127\n");
                                    if (count == 1)
                                    {LOGMEIN("JSONString.h] 129\n");
                                        outputString->Append(_u("000"), 3);
                                    }
                                    else if (count == 2)
                                    {LOGMEIN("JSONString.h] 133\n");
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
            {LOGMEIN("JSONString.h] 154\n");
                if (lastFlushSz < endSz)
                {LOGMEIN("JSONString.h] 156\n");
                    outputString->AppendLarge(lastFlushSz, (charcount_t)(endSz - lastFlushSz));
                }
                outputString->Append(_u('\"'));
                result = nullptr;
            }
            else if (op == EscapingOperation_Count)
            {LOGMEIN("JSONString.h] 163\n");
                result = TJSONString::New(value, start, extra);
            }
            else
            {
                // If we got here, we don't need to change the inside, just wrap the string with quotes.
                result = TConcatStringWrapping::New(value);
            }

            return result;
        }

        static WCHAR* EscapeNonEmptyString(ArenaAllocator* allocator, const char16* szValue)
        {LOGMEIN("JSONString.h] 176\n");
            WCHAR* result = nullptr;
            StringProxy::allocator = allocator;
            charcount_t len = (charcount_t)wcslen(szValue);
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
            {LOGMEIN("JSONString.h] 194\n");
                this->m_needEscape = false;
            }

            StringProxy(int start, int extra) : m_start(start), m_extra(extra)
            {LOGMEIN("JSONString.h] 199\n");
                this->m_needEscape = true;
            }

            static StringProxy* New(Js::JavascriptString* value)
            {LOGMEIN("JSONString.h] 204\n");
                // Case 1: The string do not need to be escaped at all
                Assert(value == nullptr);
                Assert(allocator != nullptr);
                return Anew(allocator, StringProxy);
            }

            static StringProxy* New(Js::JavascriptString* value, uint start, uint length)
            {LOGMEIN("JSONString.h] 212\n");
                // Case 2: The string requires escaping, and the length is computed
                Assert(value == nullptr);
                Assert(allocator != nullptr);
                return Anew(allocator, StringProxy, start, length);
            }

            WCHAR* GetResult(const WCHAR* originalString, charcount_t originalLength)
            {LOGMEIN("JSONString.h] 220\n");
                if (this->m_needEscape)
                {LOGMEIN("JSONString.h] 222\n");
                    charcount_t unescapedStringLength = originalLength + m_extra + 2 /* for the quotes */;
                    WCHAR* buffer = AnewArray(allocator, WCHAR, unescapedStringLength + 1); /* for terminating null */
                    buffer[unescapedStringLength] = '\0';
                    WritableStringBuffer stringBuffer(buffer, unescapedStringLength);
                    StringProxy* proxy = JSONString::EscapeNonEmptyString<EscapingOperation_Escape, StringProxy, StringProxy, StringProxy*>(nullptr, originalString, m_start, originalLength, &stringBuffer);
                    Assert(proxy == nullptr);
                    Assert(buffer[unescapedStringLength] == '\0');
                    return buffer;
                }
                else
                {
                    WCHAR* buffer = AnewArray(allocator, WCHAR, originalLength + 3); /* quotes and terminating null */
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
