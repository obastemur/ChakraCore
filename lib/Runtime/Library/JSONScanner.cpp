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
    {TRACE_IT(55899);
    }

    void JSONScanner::Finalizer()
    {TRACE_IT(55900);
        // All dynamic memory allocated by this object is on the arena - either the one this object owns or by the
        // one shared with JSON parser - here we will deallocate ours. The others will be deallocated when JSONParser
        // goes away which should happen right after this.
        if (this->allocatorObject != nullptr)
        {TRACE_IT(55901);
            // We created our own allocator, so we have to free it
            this->scriptContext->ReleaseTemporaryGuestAllocator(allocatorObject);
        }
    }

    void JSONScanner::Init(const char16* input, uint len, Token* pOutToken, Js::ScriptContext* sc, const char16* current, ArenaAllocator* allocator)
    {TRACE_IT(55902);
        // Note that allocator could be nullptr from JSONParser, if we could not reuse an allocator, keep our own
        inputText = input;
        currentChar = current;
        inputLen = len;
        pToken = pOutToken;
        scriptContext = sc;
        this->allocator = allocator;
    }

    tokens JSONScanner::Scan()
    {TRACE_IT(55903);
        pTokenString = currentChar;

        while (currentChar < inputText + inputLen)
        {TRACE_IT(55904);
            switch(ReadNextChar())
            {
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
                {TRACE_IT(55905);
                    currentChar--;

                    // we use StrToDbl() here for compat with the rest of the engine. StrToDbl() accept a larger syntax.
                    // Verify first the JSON grammar.
                    const char16* saveCurrentChar = currentChar;
                    if(!IsJSONNumber())
                    {TRACE_IT(55906);
                       ThrowSyntaxError(JSERR_JsonBadNumber);
                    }
                    currentChar = saveCurrentChar;
                    double val;
                    const char16* end;
                    val = Js::NumberUtilities::StrToDbl(currentChar, &end, scriptContext);
                    if(currentChar == end)
                    {TRACE_IT(55907);
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
                {TRACE_IT(55908);
                    currentChar += 3;
                    return (pToken->tk = tkNULL);
                }
               ThrowSyntaxError(JSERR_JsonIllegalChar);

            case 't':
                //check for 'true'
                if (currentChar + 2 < inputText + inputLen  && currentChar[0] == 'r' && currentChar[1] == 'u' && currentChar[2] == 'e')
                {TRACE_IT(55909);
                    currentChar += 3;
                    return (pToken->tk = tkTRUE);
                }
               ThrowSyntaxError(JSERR_JsonIllegalChar);

            case 'f':
                //check for 'false'
                if (currentChar + 3 < inputText + inputLen  && currentChar[0] == 'a' && currentChar[1] == 'l' && currentChar[2] == 's' && currentChar[3] == 'e')
                {TRACE_IT(55910);
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
    {TRACE_IT(55911);
        bool firstDigitIsAZero = false;
        if (PeekNextChar() == '0')
        {TRACE_IT(55912);
            firstDigitIsAZero = true;
            currentChar++;
        }

        //partial verification of number JSON grammar.
        while (currentChar < inputText + inputLen)
        {TRACE_IT(55913);
            switch(ReadNextChar())
            {
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
                {TRACE_IT(55914);
                    return false;
                }
                break;

            case '.':
                {TRACE_IT(55915);
                    // at least one digit after '.'
                    if(currentChar < inputText + inputLen)
                    {TRACE_IT(55916);
                        char16 nch = ReadNextChar();
                        if('0' <= nch && nch <= '9')
                        {TRACE_IT(55917);
                            return true;
                        }
                        else
                        {TRACE_IT(55918);
                            return false;
                        }
                    }
                    else
                    {TRACE_IT(55919);
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
    {TRACE_IT(55920);
        char16 ch;

        this->currentIndex = 0;
        this->currentString = const_cast<char16*>(currentChar);
        bool endFound = false;
        bool isStringDirectInputTextMapped = true;
        LPCWSTR bulkStart = currentChar;
        uint bulkLength = 0;

        while (currentChar < inputText + inputLen)
        {TRACE_IT(55921);
            ch = ReadNextChar();
            int tempHex;

            if (ch == '"')
            {TRACE_IT(55922);
                //end of the string
                endFound = true;
                break;
            }
            else if (ch <= 0x1F)
            {TRACE_IT(55923);
                //JSON doesn't accept \u0000 - \u001f range, LS(\u2028) and PS(\u2029) are ok
               ThrowSyntaxError(JSERR_JsonIllegalChar);
            }
            else if ( 0 == ch )
            {TRACE_IT(55924);
                currentChar--;
               ThrowSyntaxError(JSERR_JsonNoStrEnd);
            }
            else if ('\\' == ch)
            {TRACE_IT(55925);
                //JSON escape sequence in a string \", \/, \\, \b, \f, \n, \r, \t, unicode seq
                // unlikely V5.8 regular chars are not escaped, i.e '\g'' in a string is illegal not 'g'
                if (currentChar >= inputText + inputLen )
                {TRACE_IT(55926);
                   ThrowSyntaxError(JSERR_JsonNoStrEnd);
                }

                ch = ReadNextChar();
                switch (ch)
                {
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
                    {TRACE_IT(55927);
                        int chcode;
                        // 4 hex digits
                        if (currentChar + 3 >= inputText + inputLen)
                        {TRACE_IT(55928);
                            //no room left for 4 hex chars
                           ThrowSyntaxError(JSERR_JsonNoStrEnd);

                        }
                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {TRACE_IT(55929);
                           ThrowSyntaxError(JSERR_JsonBadHexDigit);
                        }
                        chcode = tempHex * 0x1000;

                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {TRACE_IT(55930);
                           ThrowSyntaxError(JSERR_JsonBadHexDigit);
                        }
                        chcode += tempHex * 0x0100;

                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {TRACE_IT(55931);
                           ThrowSyntaxError(JSERR_JsonBadHexDigit);
                        }
                        chcode += tempHex * 0x0010;

                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {TRACE_IT(55932);
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
                {TRACE_IT(55933);
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
            {TRACE_IT(55934);
                // continue
                bulkLength++;
            }
        }

        if (!endFound)
        {TRACE_IT(55935);
            // no ending '"' found
           ThrowSyntaxError(JSERR_JsonNoStrEnd);
        }

        if (isStringDirectInputTextMapped == false)
        {TRACE_IT(55936);
            // If the last bulk is not ended with an escape character, make sure that is
            // not built into the final unescaped string
            bool shouldSkipLastCharacter = false;
            if (bulkLength > 0)
            {TRACE_IT(55937);
                shouldSkipLastCharacter = true;
                this->GetCurrentRangeCharacterPairList()->Add(RangeCharacterPair((uint)(bulkStart - inputText), bulkLength, _u('\0')));
                uint oldIndex = currentIndex;
                currentIndex += bulkLength;
                if (currentIndex < oldIndex)
                {TRACE_IT(55938);
                    // Overflow
                    Js::Throw::OutOfMemory();
                }
            }

            this->BuildUnescapedString(shouldSkipLastCharacter);
            this->GetCurrentRangeCharacterPairList()->Clear();
            this->currentString = this->stringBuffer;
        }
        else
        {TRACE_IT(55939);
            // make currentIndex the length (w/o the \0)
            currentIndex = bulkLength;

            OUTPUT_TRACE_DEBUGONLY(Js::JSONPhase, _u("ScanString(): direct-mapped string as '%.*s'\n"),
                GetCurrentStringLen(), GetCurrentString());
        }

        return (pToken->tk = tkStrCon);
    }

    void JSONScanner::BuildUnescapedString(bool shouldSkipLastCharacter)
    {TRACE_IT(55940);
        AssertMsg(this->allocator != nullptr, "We must have built the allocator");
        AssertMsg(this->currentRangeCharacterPairList != nullptr, "We must have built the currentRangeCharacterPairList");
        AssertMsg(this->currentRangeCharacterPairList->Count() > 0, "We need to build the current string only because we have escaped characters");

        // Step 1: Ensure the buffer has sufficient space
        int requiredSize = this->GetCurrentStringLen();
        if (requiredSize > this->stringBufferLength)
        {TRACE_IT(55941);
            if (this->stringBuffer)
            {TRACE_IT(55942);
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
        {TRACE_IT(55943);
            RangeCharacterPair data = this->currentRangeCharacterPairList->Item(i);
            int charactersToCopy = data.m_rangeLength;
            js_wmemcpy_s(begin_copy, charactersToCopy, this->inputText + data.m_rangeStart, charactersToCopy);
            begin_copy += charactersToCopy;
            totalCopied += charactersToCopy;

            if (i == lastCharacterIndex && shouldSkipLastCharacter)
            {TRACE_IT(55944);
                continue;
            }

            *begin_copy = data.m_char;
            begin_copy++;
            totalCopied++;
        }

        if (totalCopied != requiredSize)
        {TRACE_IT(55945);
            OUTPUT_TRACE_DEBUGONLY(Js::JSONPhase, _u("BuildUnescapedString(): allocated size = %d != copying size %d\n"), requiredSize, totalCopied);
            AssertMsg(totalCopied == requiredSize, "BuildUnescapedString(): The allocated size and copying size should match.");
        }

        OUTPUT_TRACE_DEBUGONLY(Js::JSONPhase, _u("BuildUnescapedString(): unescaped string as '%.*s'\n"), GetCurrentStringLen(), this->stringBuffer);
    }

    JSONScanner::RangeCharacterPairList* JSONScanner::GetCurrentRangeCharacterPairList(void)
    {TRACE_IT(55946);
        if (this->currentRangeCharacterPairList == nullptr)
        {TRACE_IT(55947);
            if (this->allocator == nullptr)
            {TRACE_IT(55948);
                this->allocatorObject = this->scriptContext->GetTemporaryGuestAllocator(_u("JSONScanner"));
                this->allocator = this->allocatorObject->GetAllocator();
            }

            this->currentRangeCharacterPairList = Anew(this->allocator, RangeCharacterPairList, this->allocator, 4);
        }

        return this->currentRangeCharacterPairList;
    }
} // namespace JSON
