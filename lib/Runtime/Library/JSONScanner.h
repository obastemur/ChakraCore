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
        void Init(const CHAR_T* input, uint len, Token* pOutToken,
            ::Js::ScriptContext* sc, const CHAR_T* current, ArenaAllocator* allocator);

        void Finalizer();
        CHAR_T* GetCurrentString() { return currentString; }
        uint GetCurrentStringLen() { return currentIndex; }
        uint GetScanPosition() { return uint(currentChar - inputText); }

        void __declspec(noreturn) ThrowSyntaxError(int wErr)
        {
            CHAR_T scanPos[16];
            ::_itow_s(GetScanPosition(), scanPos, _countof(scanPos) / sizeof(CHAR_T), 10);
            Js::JavascriptError::ThrowSyntaxError(scriptContext, wErr, scanPos);
        }

    private:

        // Data structure for unescaping strings
        struct RangeCharacterPair {
        public:
            uint m_rangeStart;
            uint m_rangeLength;
            CHAR_T m_char;
            RangeCharacterPair() {}
            RangeCharacterPair(uint rangeStart, uint rangeLength, CHAR_T ch) : m_rangeStart(rangeStart), m_rangeLength(rangeLength), m_char(ch) {}
        };

        typedef JsUtil::List<RangeCharacterPair, ArenaAllocator> RangeCharacterPairList;

        RangeCharacterPairList* currentRangeCharacterPairList;

        Js::TempGuestArenaAllocatorObject* allocatorObject;
        ArenaAllocator* allocator;
        void BuildUnescapedString(bool shouldSkipLastCharacter);

        RangeCharacterPairList* GetCurrentRangeCharacterPairList(void);

        inline CHAR_T ReadNextChar(void)
        {
            return *currentChar++;
        }

        inline CHAR_T PeekNextChar(void)
        {
            return *currentChar;
        }

        tokens ScanString();
        bool IsJSONNumber();

        const CHAR_T* inputText;
        uint    inputLen;
        const CHAR_T* currentChar;
        const CHAR_T* pTokenString;

        Token*   pToken;
        ::Js::ScriptContext* scriptContext;

        uint     currentIndex;
        CHAR_T* currentString;
        __field_ecount(stringBufferLength) CHAR_T* stringBuffer;
        int      stringBufferLength;

        friend class JSONParser;
    };
} // namespace JSON
