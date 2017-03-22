//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "JSONScanner.h"

using namespace Js;

namespace JSON
{
    // -------- Scanner implementation ------------//
    JSONScanner::JSONScanner()
        : inputText(0), inputLen(0), pToken(0), stringBuffer(0), allocator(0), allocatorObject(0),
        currentRangeCharacterPairList(0), stringBufferLength(0), currentIndex(0)
    {LOGMEIN("JSONScanner.cpp] 15\n");
    }

    void JSONScanner::Finalizer()
    {LOGMEIN("JSONScanner.cpp] 19\n");
        // All dynamic memory allocated by this object is on the arena - either the one this object owns or by the
        // one shared with JSON parser - here we will deallocate ours. The others will be deallocated when JSONParser
        // goes away which should happen right after this.
        if (this->allocatorObject != nullptr)
        {LOGMEIN("JSONScanner.cpp] 24\n");
            // We created our own allocator, so we have to free it
            this->scriptContext->ReleaseTemporaryGuestAllocator(allocatorObject);
        }
    }

    void JSONScanner::Init(const char16* input, uint len, Token* pOutToken, Js::ScriptContext* sc, const char16* current, ArenaAllocator* allocator)
    {LOGMEIN("JSONScanner.cpp] 31\n");
        // Note that allocator could be nullptr from JSONParser, if we could not reuse an allocator, keep our own
        inputText = input;
        currentChar = current;
        inputLen = len;
        pToken = pOutToken;
        scriptContext = sc;
        this->allocator = allocator;
    }

    tokens JSONScanner::Scan()
    {LOGMEIN("JSONScanner.cpp] 42\n");
        pTokenString = currentChar;

        while (currentChar < inputText + inputLen)
        {LOGMEIN("JSONScanner.cpp] 46\n");
            switch(ReadNextChar())
            {LOGMEIN("JSONScanner.cpp] 48\n");
            case 0:
                //EOF
                currentChar--;
                return (pToken->tk = tkEOF);

            case '\t':
            case '\r':
            case '\n':
            case ' ':
                //WS - keep looping
                break;

            case '"':
                //check for string
                return ScanString();

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                //decimal digit starts a number
                {LOGMEIN("JSONScanner.cpp] 76\n");
                    currentChar--;

                    // we use StrToDbl() here for compat with the rest of the engine. StrToDbl() accept a larger syntax.
                    // Verify first the JSON grammar.
                    const char16* saveCurrentChar = currentChar;
                    if(!IsJSONNumber())
                    {LOGMEIN("JSONScanner.cpp] 83\n");
                       ThrowSyntaxError(JSERR_JsonBadNumber);
                    }
                    currentChar = saveCurrentChar;
                    double val;
                    const char16* end;
                    val = Js::NumberUtilities::StrToDbl(currentChar, &end, scriptContext);
                    if(currentChar == end)
                    {LOGMEIN("JSONScanner.cpp] 91\n");
                       ThrowSyntaxError(JSERR_JsonBadNumber);
                    }
                    AssertMsg(!Js::JavascriptNumber::IsNan(val), "Bad result from string to double conversion");
                    pToken->tk = tkFltCon;
                    pToken->SetDouble(val, false);
                    currentChar = end;
                    return tkFltCon;
                }

            case ',':
                return (pToken->tk = tkComma);

            case ':':
                return (pToken->tk = tkColon);

            case '[':
                return (pToken->tk = tkLBrack);

            case ']':
                return (pToken->tk = tkRBrack);

            case '-':
                return (pToken->tk = tkSub);

            case 'n':
                //check for 'null'
                if (currentChar + 2 < inputText + inputLen  && currentChar[0] == 'u' && currentChar[1] == 'l' && currentChar[2] == 'l')
                {LOGMEIN("JSONScanner.cpp] 119\n");
                    currentChar += 3;
                    return (pToken->tk = tkNULL);
                }
               ThrowSyntaxError(JSERR_JsonIllegalChar);

            case 't':
                //check for 'true'
                if (currentChar + 2 < inputText + inputLen  && currentChar[0] == 'r' && currentChar[1] == 'u' && currentChar[2] == 'e')
                {LOGMEIN("JSONScanner.cpp] 128\n");
                    currentChar += 3;
                    return (pToken->tk = tkTRUE);
                }
               ThrowSyntaxError(JSERR_JsonIllegalChar);

            case 'f':
                //check for 'false'
                if (currentChar + 3 < inputText + inputLen  && currentChar[0] == 'a' && currentChar[1] == 'l' && currentChar[2] == 's' && currentChar[3] == 'e')
                {LOGMEIN("JSONScanner.cpp] 137\n");
                    currentChar += 4;
                    return (pToken->tk = tkFALSE);
                }
               ThrowSyntaxError(JSERR_JsonIllegalChar);

            case '{':
                return (pToken->tk = tkLCurly);

            case '}':
                return (pToken->tk = tkRCurly);

            default:
               ThrowSyntaxError(JSERR_JsonIllegalChar);
            }

        }

        return (pToken->tk = tkEOF);
    }

    bool JSONScanner::IsJSONNumber()
    {LOGMEIN("JSONScanner.cpp] 159\n");
        bool firstDigitIsAZero = false;
        if (PeekNextChar() == '0')
        {LOGMEIN("JSONScanner.cpp] 162\n");
            firstDigitIsAZero = true;
            currentChar++;
        }

        //partial verification of number JSON grammar.
        while (currentChar < inputText + inputLen)
        {LOGMEIN("JSONScanner.cpp] 169\n");
            switch(ReadNextChar())
            {LOGMEIN("JSONScanner.cpp] 171\n");
            case 0:
                return false;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (firstDigitIsAZero)
                {LOGMEIN("JSONScanner.cpp] 185\n");
                    return false;
                }
                break;

            case '.':
                {LOGMEIN("JSONScanner.cpp] 191\n");
                    // at least one digit after '.'
                    if(currentChar < inputText + inputLen)
                    {LOGMEIN("JSONScanner.cpp] 194\n");
                        char16 nch = ReadNextChar();
                        if('0' <= nch && nch <= '9')
                        {LOGMEIN("JSONScanner.cpp] 197\n");
                            return true;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        return false;
                    }
                }
                //case 'E':
                //case 'e':
                //    return true;
            default:
                return true;
            }

            firstDigitIsAZero = false;
        }
        return true;
    }

    tokens JSONScanner::ScanString()
    {LOGMEIN("JSONScanner.cpp] 223\n");
        char16 ch;

        this->currentIndex = 0;
        this->currentString = const_cast<char16*>(currentChar);
        bool endFound = false;
        bool isStringDirectInputTextMapped = true;
        LPCWSTR bulkStart = currentChar;
        uint bulkLength = 0;

        while (currentChar < inputText + inputLen)
        {LOGMEIN("JSONScanner.cpp] 234\n");
            ch = ReadNextChar();
            int tempHex;

            if (ch == '"')
            {LOGMEIN("JSONScanner.cpp] 239\n");
                //end of the string
                endFound = true;
                break;
            }
            else if (ch <= 0x1F)
            {LOGMEIN("JSONScanner.cpp] 245\n");
                //JSON doesn't accept \u0000 - \u001f range, LS(\u2028) and PS(\u2029) are ok
               ThrowSyntaxError(JSERR_JsonIllegalChar);
            }
            else if ( 0 == ch )
            {LOGMEIN("JSONScanner.cpp] 250\n");
                currentChar--;
               ThrowSyntaxError(JSERR_JsonNoStrEnd);
            }
            else if ('\\' == ch)
            {LOGMEIN("JSONScanner.cpp] 255\n");
                //JSON escape sequence in a string \", \/, \\, \b, \f, \n, \r, \t, unicode seq
                // unlikely V5.8 regular chars are not escaped, i.e '\g'' in a string is illegal not 'g'
                if (currentChar >= inputText + inputLen )
                {LOGMEIN("JSONScanner.cpp] 259\n");
                   ThrowSyntaxError(JSERR_JsonNoStrEnd);
                }

                ch = ReadNextChar();
                switch (ch)
                {LOGMEIN("JSONScanner.cpp] 265\n");
                case 0:
                    currentChar--;
                   ThrowSyntaxError(JSERR_JsonNoStrEnd);

                case '"':
                case '/':
                case '\\':
                    //keep ch
                    break;

                case 'b':
                    ch = 0x08;
                    break;

                case 'f':
                    ch = 0x0C;
                    break;

                case 'n':
                    ch = 0x0A;
                    break;

                case 'r':
                    ch = 0x0D;
                    break;

                case 't':
                    ch = 0x09;
                    break;

                case 'u':
                    {LOGMEIN("JSONScanner.cpp] 297\n");
                        int chcode;
                        // 4 hex digits
                        if (currentChar + 3 >= inputText + inputLen)
                        {LOGMEIN("JSONScanner.cpp] 301\n");
                            //no room left for 4 hex chars
                           ThrowSyntaxError(JSERR_JsonNoStrEnd);

                        }
                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {LOGMEIN("JSONScanner.cpp] 307\n");
                           ThrowSyntaxError(JSERR_JsonBadHexDigit);
                        }
                        chcode = tempHex * 0x1000;

                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {LOGMEIN("JSONScanner.cpp] 313\n");
                           ThrowSyntaxError(JSERR_JsonBadHexDigit);
                        }
                        chcode += tempHex * 0x0100;

                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {LOGMEIN("JSONScanner.cpp] 319\n");
                           ThrowSyntaxError(JSERR_JsonBadHexDigit);
                        }
                        chcode += tempHex * 0x0010;

                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {LOGMEIN("JSONScanner.cpp] 325\n");
                           ThrowSyntaxError(JSERR_JsonBadHexDigit);
                        }
                        chcode += tempHex;
                        AssertMsg(chcode == (chcode & 0xFFFF), "Bad unicode code");
                        ch = (char16)chcode;
                    }
                    break;

                default:
                    // Any other '\o' is an error in JSON
                   ThrowSyntaxError(JSERR_JsonIllegalChar);
                }

                // flush
                this->GetCurrentRangeCharacterPairList()->Add(RangeCharacterPair((uint)(bulkStart - inputText), bulkLength, ch));

                uint oldIndex = currentIndex;
                currentIndex += bulkLength;
                currentIndex++;

                if (currentIndex < oldIndex)
                {LOGMEIN("JSONScanner.cpp] 347\n");
                    // Overflow
                    Js::Throw::OutOfMemory();
                }

                // mark the mode as 'string transformed' (no direct mapping in inputText possible)
                isStringDirectInputTextMapped = false;

                // reset (to next char)
                bulkStart = currentChar;
                bulkLength = 0;
            }
            else
            {
                // continue
                bulkLength++;
            }
        }

        if (!endFound)
        {LOGMEIN("JSONScanner.cpp] 367\n");
            // no ending '"' found
           ThrowSyntaxError(JSERR_JsonNoStrEnd);
        }

        if (isStringDirectInputTextMapped == false)
        {LOGMEIN("JSONScanner.cpp] 373\n");
            // If the last bulk is not ended with an escape character, make sure that is
            // not built into the final unescaped string
            bool shouldSkipLastCharacter = false;
            if (bulkLength > 0)
            {LOGMEIN("JSONScanner.cpp] 378\n");
                shouldSkipLastCharacter = true;
                this->GetCurrentRangeCharacterPairList()->Add(RangeCharacterPair((uint)(bulkStart - inputText), bulkLength, _u('\0')));
                uint oldIndex = currentIndex;
                currentIndex += bulkLength;
                if (currentIndex < oldIndex)
                {LOGMEIN("JSONScanner.cpp] 384\n");
                    // Overflow
                    Js::Throw::OutOfMemory();
                }
            }

            this->BuildUnescapedString(shouldSkipLastCharacter);
            this->GetCurrentRangeCharacterPairList()->Clear();
            this->currentString = this->stringBuffer;
        }
        else
        {
            // make currentIndex the length (w/o the \0)
            currentIndex = bulkLength;

            OUTPUT_TRACE_DEBUGONLY(Js::JSONPhase, _u("ScanString(): direct-mapped string as '%.*s'\n"),
                GetCurrentStringLen(), GetCurrentString());
        }

        return (pToken->tk = tkStrCon);
    }

    void JSONScanner::BuildUnescapedString(bool shouldSkipLastCharacter)
    {LOGMEIN("JSONScanner.cpp] 407\n");
        AssertMsg(this->allocator != nullptr, "We must have built the allocator");
        AssertMsg(this->currentRangeCharacterPairList != nullptr, "We must have built the currentRangeCharacterPairList");
        AssertMsg(this->currentRangeCharacterPairList->Count() > 0, "We need to build the current string only because we have escaped characters");

        // Step 1: Ensure the buffer has sufficient space
        int requiredSize = this->GetCurrentStringLen();
        if (requiredSize > this->stringBufferLength)
        {LOGMEIN("JSONScanner.cpp] 415\n");
            if (this->stringBuffer)
            {LOGMEIN("JSONScanner.cpp] 417\n");
                AdeleteArray(this->allocator, this->stringBufferLength, this->stringBuffer);
                this->stringBuffer = nullptr;
            }

            this->stringBuffer = AnewArray(this->allocator, char16, requiredSize);
            this->stringBufferLength = requiredSize;
        }

        // Step 2: Copy the data to the buffer
        int totalCopied = 0;
        char16* begin_copy = this->stringBuffer;
        int lastCharacterIndex = this->currentRangeCharacterPairList->Count() - 1;
        for (int i = 0; i <= lastCharacterIndex; i++)
        {LOGMEIN("JSONScanner.cpp] 431\n");
            RangeCharacterPair data = this->currentRangeCharacterPairList->Item(i);
            int charactersToCopy = data.m_rangeLength;
            js_wmemcpy_s(begin_copy, charactersToCopy, this->inputText + data.m_rangeStart, charactersToCopy);
            begin_copy += charactersToCopy;
            totalCopied += charactersToCopy;

            if (i == lastCharacterIndex && shouldSkipLastCharacter)
            {LOGMEIN("JSONScanner.cpp] 439\n");
                continue;
            }

            *begin_copy = data.m_char;
            begin_copy++;
            totalCopied++;
        }

        if (totalCopied != requiredSize)
        {LOGMEIN("JSONScanner.cpp] 449\n");
            OUTPUT_TRACE_DEBUGONLY(Js::JSONPhase, _u("BuildUnescapedString(): allocated size = %d != copying size %d\n"), requiredSize, totalCopied);
            AssertMsg(totalCopied == requiredSize, "BuildUnescapedString(): The allocated size and copying size should match.");
        }

        OUTPUT_TRACE_DEBUGONLY(Js::JSONPhase, _u("BuildUnescapedString(): unescaped string as '%.*s'\n"), GetCurrentStringLen(), this->stringBuffer);
    }

    JSONScanner::RangeCharacterPairList* JSONScanner::GetCurrentRangeCharacterPairList(void)
    {LOGMEIN("JSONScanner.cpp] 458\n");
        if (this->currentRangeCharacterPairList == nullptr)
        {LOGMEIN("JSONScanner.cpp] 460\n");
            if (this->allocator == nullptr)
            {LOGMEIN("JSONScanner.cpp] 462\n");
                this->allocatorObject = this->scriptContext->GetTemporaryGuestAllocator(_u("JSONScanner"));
                this->allocator = this->allocatorObject->GetAllocator();
            }

            this->currentRangeCharacterPairList = Anew(this->allocator, RangeCharacterPairList, this->allocator, 4);
        }

        return this->currentRangeCharacterPairList;
    }
} // namespace JSON
