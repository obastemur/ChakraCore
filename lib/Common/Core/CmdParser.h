//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once



///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
///
/// class CmdLineArgsParser
///
/// Parses the following grammar
///
///      range      = integer | integer - integer | range,range
///      parameter  = integer | string  | phase[:range]
///      flag       = string
///      phase      = string
///      [-|/]flag[:parameter]
///
///----------------------------------------------------------------------------
///----------------------------------------------------------------------------

class CmdLineArgsParser : private ICmdLineArgsParser
{
// Data
private:
    static const int  MaxTokenSize  = 512;

    Js::ConfigFlagsTable& flagTable;
    LPCHAR_T           pszCurrentArg;
    ICustomConfigFlags * pCustomConfigFlags;

// Methods
public:
    int                Parse(int argc, __in_ecount(argc) LPCHAR_T argv[]);
    int Parse(__in LPCHAR_T token) throw();
    CmdLineArgsParser(ICustomConfigFlags * pCustomConfigFlags = nullptr, Js::ConfigFlagsTable& flagTable = Js::Configuration::Global.flags);
    ~CmdLineArgsParser();

// Helper Classes
private:

    ///----------------------------------------------------------------------------
    ///
    /// class Exception
    ///
    ///----------------------------------------------------------------------------

    class Exception {
        LPCCHAR_T        pszMsg;
    public:
        Exception(LPCCHAR_T message):
            pszMsg(message)
        {}

        operator LPCCHAR_T () const
        {
            return this->pszMsg;
        }
    };

// Implementation
private:
            bool                       ParseBoolean();
            LPCHAR_T                     ParseString(__inout_ecount(ceBuffer) LPCHAR_T buffer, size_t ceBuffer = MaxTokenSize, bool fTreatColonAsSeparator = true);
            int                        ParseInteger();
            Js::SourceFunctionNode     ParseSourceFunctionIds();
            void                       ParsePhase(Js::Phases *pPhase, Js::Phases *oppositePhase);
            void                       ParseRange(Js::Range *range, Js::Range *oppositeRange);
            void                       ParseNumberRange(Js::NumberRange *range);
            void                       ParseFlag();
            void                       ParseNumberSet(Js::NumberSet * numberSet);
            void                       ParseNumberPairSet(Js::NumberPairSet * numberPairSet);
            void                       ParseNumberTrioSet(Js::NumberTrioSet * numberTrioSet);
            void                       PrintUsage();

            CHAR_T CurChar()
            {
                return this->pszCurrentArg[0];
            }

            CHAR_T PeekChar()
            {
                return this->pszCurrentArg[1];
            }

            void NextChar()
            {
                this->pszCurrentArg++;
            }

            bool IsDigit()
            {
                return (CurChar() >='0' && CurChar() <= '9');
            }

            bool IsHexDigit()
            {
                return (CurChar() >= '0' && CurChar() <= '9') ||
                    (CurChar() >= 'A' && CurChar() <= 'F') ||
                    (CurChar() >= 'a' && CurChar() <= 'f');
            }

            // Implements ICmdLineArgsParser
            virtual BSTR GetCurrentString() override;
            virtual bool GetCurrentBoolean() override
            {
                return ParseBoolean();
            }
            virtual int GetCurrentInt() override
            {
                NextChar();
                return  ParseInteger();
            }
};
