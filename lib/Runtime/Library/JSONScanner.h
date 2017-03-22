//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JSON
{
    class JSONParser;

    // Small scanner for exclusive JSON purpose. The general
    // JScript scanner is not appropriate here because of the JSON restricted lexical grammar
    // token enums and structures are shared although the token semantics is slightly different.
    class JSONScanner
    {
    public:
        JSONScanner();
        tokens Scan();
        void Init(const char16* input, uint len, Token* pOutToken,
            ::Js::ScriptContext* sc, const char16* current, ArenaAllocator* allocator);

        void Finalizer();
        char16* GetCurrentString() {LOGMEIN("JSONScanner.h] 22\n"); return currentString; } 
        uint GetCurrentStringLen() {LOGMEIN("JSONScanner.h] 23\n"); return currentIndex; }
        uint GetScanPosition() {LOGMEIN("JSONScanner.h] 24\n"); return uint(currentChar - inputText); }

        void __declspec(noreturn) ThrowSyntaxError(int wErr)
        {LOGMEIN("JSONScanner.h] 27\n");
            char16 scanPos[16];
            ::_itow_s(GetScanPosition(), scanPos, _countof(scanPos) / sizeof(char16), 10);
            Js::JavascriptError::ThrowSyntaxError(scriptContext, wErr, scanPos);
        }

    private:

        // Data structure for unescaping strings
        struct RangeCharacterPair {
        public:
            uint m_rangeStart;
            uint m_rangeLength;
            char16 m_char;
            RangeCharacterPair() {LOGMEIN("JSONScanner.h] 41\n");}
            RangeCharacterPair(uint rangeStart, uint rangeLength, char16 ch) : m_rangeStart(rangeStart), m_rangeLength(rangeLength), m_char(ch) {LOGMEIN("JSONScanner.h] 42\n");}
        };

        typedef JsUtil::List<RangeCharacterPair, ArenaAllocator> RangeCharacterPairList;

        RangeCharacterPairList* currentRangeCharacterPairList;

        Js::TempGuestArenaAllocatorObject* allocatorObject;
        ArenaAllocator* allocator;
        void BuildUnescapedString(bool shouldSkipLastCharacter);

        RangeCharacterPairList* GetCurrentRangeCharacterPairList(void);

        inline char16 ReadNextChar(void)
        {LOGMEIN("JSONScanner.h] 56\n");
            return *currentChar++;
        }

        inline char16 PeekNextChar(void)
        {LOGMEIN("JSONScanner.h] 61\n");
            return *currentChar;
        }

        tokens ScanString();
        bool IsJSONNumber();

        const char16* inputText;
        uint    inputLen;
        const char16* currentChar;
        const char16* pTokenString;

        Token*   pToken;
        ::Js::ScriptContext* scriptContext;

        uint     currentIndex;
        char16* currentString;
        __field_ecount(stringBufferLength) char16* stringBuffer;
        int      stringBufferLength;

        friend class JSONParser;
    };
} // namespace JSON
