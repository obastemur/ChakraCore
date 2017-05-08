//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"
#include "Core/ICustomConfigFlags.h"
#include "Core/CmdParser.h"

using namespace Js;


///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseString
///
///     Parses a string token. There are 2 ways to specify it.
///         1. Quoted   - " " Any character within quotes is parsed as string.
///                       if the quotes are not closed, its an error.
///         2. UnQuoted - End of string is indicated by a space/end of stream.
///                       If fTreatColonAsSeparator is mentioned, then we break
///                       at colon also.
///
///
///     Empty string "" is treated as Exception()
///
///----------------------------------------------------------------------------

LPWSTR
CmdLineArgsParser::ParseString(__inout_ecount(ceBuffer) LPWSTR buffer, size_t ceBuffer, bool fTreatColonAsSeparator)
{TRACE_IT(19630);

    char16 *out = buffer;
    size_t len = 0;

    if('"' == CurChar())
    {TRACE_IT(19631);
        NextChar();

        while('"' != CurChar())
        {TRACE_IT(19632);
            if(0 == CurChar())
            {TRACE_IT(19633);
                throw Exception(_u("Unmatched quote"));
            }

            //
            // MaxTokenSize - 1 because we need 1 extra position for null termination
            //
            if (len >= ceBuffer - 1)
            {TRACE_IT(19634);
                throw Exception(_u("String token too large to parse"));
            }

            out[len++] = CurChar();

            NextChar();
        }
        NextChar();
    }
    else
    {TRACE_IT(19635);
        bool fDone = false;

        while(!fDone)
        {TRACE_IT(19636);
            switch(CurChar())
            {
            case ' ':
            case ',':
            case 0:
                fDone = 1;
                break;
            case '-':
            case ':':
                if(fTreatColonAsSeparator)
                {TRACE_IT(19637);
                    fDone = true;
                    break;
                }
                else
                {TRACE_IT(19638);
                    // Fallthrough
                }
            default:
                if(len >= MaxTokenSize -1)
                {TRACE_IT(19639);
                    throw Exception(_u("String token too large to parse"));
                }
                out[len++] = CurChar();
                NextChar();
            }
        }
    }

    if(0 == len)
    {TRACE_IT(19640);
        throw Exception(_u("String Token Expected"));
    }

    out[len] = '\0';

    return buffer;
}

///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseSourceFunctionIds
///
/// Parses for sourceContextId and FunctionId pairs
///----------------------------------------------------------------------------

Js::SourceFunctionNode
CmdLineArgsParser::ParseSourceFunctionIds()
{TRACE_IT(19641);
    uint functionId, sourceId;

    if ('*' == CurChar())
    {TRACE_IT(19642);
        sourceId = 1;
        functionId = (uint)-2;
        NextChar();
    }
    else if ('+' == CurChar())
    {TRACE_IT(19643);
        sourceId = 1;
        functionId = (uint)-1;
        NextChar();
    }
    else
    {TRACE_IT(19644);
        functionId = sourceId = ParseInteger();

        if ('.' == CurChar())
        {TRACE_IT(19645);
            NextChar();
            if ('*' == CurChar())
            {TRACE_IT(19646);
                functionId = (uint)-2;
                NextChar();
            }
            else if ('+' == CurChar())
            {TRACE_IT(19647);
                functionId = (uint)-1;
                NextChar();
            }
            else
            {TRACE_IT(19648);
                functionId = ParseInteger();
            }
        }
        else
        {TRACE_IT(19649);
            sourceId = 1;
        }
    }
    return SourceFunctionNode(sourceId, functionId);
}

///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseInteger
///
/// Parses signed integer. Checks for overflow and underflows.
///----------------------------------------------------------------------------

int
CmdLineArgsParser::ParseInteger()
{TRACE_IT(19650);
    int result  = 0;
    int sign    = 1;

    if('-' == CurChar())
    {TRACE_IT(19651);
        sign = -1;
        NextChar();
    }
    if(!IsDigit())
    {TRACE_IT(19652);
        throw Exception(_u("Integer Expected"));
    }

    int base = 10;

    if ('0' == CurChar())
    {TRACE_IT(19653);
        NextChar();
        if (CurChar() == 'x')
        {TRACE_IT(19654);
            NextChar();
            base = 16;
        }
        // Should the else case be parse as octal?
    }

    while(IsDigit() || (base == 16 && IsHexDigit()))
    {TRACE_IT(19655);
        int currentDigit = (int)(CurChar() - '0');
        if (currentDigit > 9)
        {TRACE_IT(19656);
            Assert(base == 16);
            if (CurChar() < 'F')
            {TRACE_IT(19657);
                currentDigit = 10 + (int)(CurChar() - 'A');
            }
            else
            {TRACE_IT(19658);
                currentDigit = 10 + (int)(CurChar() - 'a');
            }

            Assert(currentDigit < 16);
        }

        result = result * base + (int)(CurChar() - '0');
        if(result < 0)
        {TRACE_IT(19659);
            // overflow or underflow in case sign = -1
            throw Exception(_u("Integer too large to parse"));
        }

        NextChar();
    }

    return result * sign;
}


///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseRange
///
/// Parses :-
/// range = int | int '-' int | range, range
///
///----------------------------------------------------------------------------

void
CmdLineArgsParser::ParseRange(Js::Range *pRange, Js::Range *oppositeRange)
{TRACE_IT(19660);
    SourceFunctionNode r1 = ParseSourceFunctionIds();
    SourceFunctionNode r2;
    switch(CurChar())
    {
    case '-':
        NextChar();
        r2 = ParseSourceFunctionIds();

        if (r1.sourceContextId > r2.sourceContextId)
        {TRACE_IT(19661);
            throw Exception(_u("Left source index must be smaller than the Right source Index"));
        }
        if ((r1.sourceContextId == r2.sourceContextId) &&
            (r1.functionId > r2.functionId))
        {TRACE_IT(19662);
            throw Exception(_u("Left functionId must be smaller than the Right functionId when Source file is the same"));
        }

        pRange->Add(r1, r2, oppositeRange);
        switch(CurChar())
        {
        case ',':
            NextChar();
            ParseRange(pRange, oppositeRange);
            break;

        case ' ':
        case 0:
            break;

        default:
            throw Exception(_u("Unexpected character while parsing Range"));
        }
        break;

    case ',':
        pRange->Add(r1, oppositeRange);
        NextChar();
        ParseRange(pRange, oppositeRange);
        break;

    case ' ':
    case 0:
        pRange->Add(r1, oppositeRange);
        break;

    default:
        throw Exception(_u("Unexpected character while parsing Range"));
    }

}


void
CmdLineArgsParser::ParseNumberRange(Js::NumberRange *pRange)
{TRACE_IT(19663);
    int start = ParseInteger();
    int end;

    switch (CurChar())
    {
    case '-':
        NextChar();
        end = ParseInteger();

        if (start > end)
        {TRACE_IT(19664);
            throw Exception(_u("Range start must be less than range end"));
        }

        pRange->Add(start, end);
        switch (CurChar())
        {
        case ',':
            NextChar();
            ParseNumberRange(pRange);
            break;

        case ' ':
        case 0:
            break;

        default:
            throw Exception(_u("Unexpected character while parsing Range"));
        }
        break;

    case ',':
        pRange->Add(start);
        NextChar();
        ParseNumberRange(pRange);
        break;

    case ' ':
    case 0:
        pRange->Add(start);
        break;

    default:
        throw Exception(_u("Unexpected character while parsing Range"));
    }

}
///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParsePhase
///
/// Parses comma separated list of:
///     phase[:range]
/// phase is a string defined in Js:PhaseNames.
///
///----------------------------------------------------------------------------

void
CmdLineArgsParser::ParsePhase(Js::Phases *pPhaseList, Js::Phases *oppositePhase)
{TRACE_IT(19665);
    char16 buffer[MaxTokenSize];
    ZeroMemory(buffer, sizeof(buffer));

    Phase phase = ConfigFlagsTable::GetPhase(ParseString(buffer));
    if(InvalidPhase == phase)
    {TRACE_IT(19666);
        throw Exception(_u("Invalid phase :"));
    }

    pPhaseList->Enable(phase);
    switch(CurChar())
    {
    case ':':
    {TRACE_IT(19667);
        NextChar();
        Js::Range* oppositeRange = nullptr;
        if (oppositePhase && oppositePhase->IsEnabled(phase))
        {TRACE_IT(19668);
            oppositeRange = oppositePhase->GetRange(phase);
        }
        ParseRange(pPhaseList->GetRange(phase), oppositeRange);
        break;
    }
    case ',':
        NextChar();
        if (oppositePhase)
        {TRACE_IT(19669);
            // The whole phase is turned on/off so disable the opposite
            oppositePhase->Disable(phase);
        }
        ParsePhase(pPhaseList, oppositePhase);
        break;
    default:
        if (oppositePhase)
        {TRACE_IT(19670);
            // The whole phase is turned on/off so disable the opposite
            oppositePhase->Disable(phase);
        }
        pPhaseList->GetRange(phase)->Clear();
        break;
    }
}

void
CmdLineArgsParser::ParseNumberSet(Js::NumberSet * numberPairSet)
{TRACE_IT(19671);
    while (true)
    {TRACE_IT(19672);
        int x = ParseInteger();
        numberPairSet->Add(x);

        if (CurChar() != ';')
        {
            break;
        }
        NextChar();
    }
}

void
CmdLineArgsParser::ParseNumberPairSet(Js::NumberPairSet * numberPairSet)
{TRACE_IT(19673);
    while (true)
    {TRACE_IT(19674);
        int line = ParseInteger();
        int col = -1;
        if (CurChar() == ',')
        {TRACE_IT(19675);
            NextChar();
            col = ParseInteger();
        }

        numberPairSet->Add(line, col);

        if (CurChar() != ';')
        {
            break;
        }
        NextChar();
    }
}

bool
CmdLineArgsParser::ParseBoolean()
{TRACE_IT(19676);
    if (CurChar() == ':')
    {TRACE_IT(19677);
        throw Exception(_u("':' not expected with a boolean flag"));
    }
    else if (CurChar() != '-' && CurChar() != ' ' && CurChar() != 0)
    {TRACE_IT(19678);
        throw Exception(_u("Invalid character after boolean flag"));
    }
    else
    {TRACE_IT(19679);
        return (CurChar() != '-');
    }
}

BSTR
CmdLineArgsParser::GetCurrentString()
{TRACE_IT(19680);
    char16 buffer[MaxTokenSize];
    ZeroMemory(buffer, sizeof(buffer));

    switch (CurChar())
    {
    case ':':
        NextChar();
        return SysAllocString(ParseString(buffer, MaxTokenSize, false));
    case ' ':
    case 0:
        NextChar();
        return nullptr;
    default:
        throw Exception(_u("Expected ':'"));
    }
}

///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseFlag
///
/// Parses:
///      flag[:parameter]
/// Flag is a string defined in Js:FlagNames.
/// The type of expected parameter depends upon the flag. It can be
///     1. String
///     2. Number
///     3. Boolean
///     4. Phase
///
/// In case of boolean the presence no parameter is expected. the value of the
/// boolean flag is set to 'true'
///
///----------------------------------------------------------------------------

void
CmdLineArgsParser::ParseFlag()
{TRACE_IT(19681);
    char16 buffer[MaxTokenSize];
    ZeroMemory(buffer, sizeof(buffer));

    LPWSTR flagString = ParseString(buffer);
    Flag flag = ConfigFlagsTable::GetFlag(flagString);
    if(InvalidFlag == flag)
    {TRACE_IT(19682);
        if (pCustomConfigFlags != nullptr)
        {TRACE_IT(19683);
            if (pCustomConfigFlags->ParseFlag(flagString, this))
            {TRACE_IT(19684);
                return;
            }
        }
        throw Exception(_u("Invalid Flag"));
    }


    FlagTypes flagType = ConfigFlagsTable::GetFlagType(flag);
    AssertMsg(InvalidFlagType != flagType, "Invalid flag type");

    this->flagTable.Enable(flag);

    if(FlagBoolean == flagType)
    {TRACE_IT(19685);
        Boolean boolValue = ParseBoolean();

        this->flagTable.SetAsBoolean(flag, boolValue);
    }
    else
    {TRACE_IT(19686);
        switch(CurChar())
        {
        case ':':
            NextChar();
            switch(flagType)
            {
            case FlagPhases:
            {TRACE_IT(19687);
                Flag oppositeFlag = this->flagTable.GetOppositePhaseFlag(flag);
                Phases* oppositePhase = nullptr;
                if (oppositeFlag != InvalidFlag)
                {TRACE_IT(19688);
                    this->flagTable.Enable(oppositeFlag);
                    oppositePhase = this->flagTable.GetAsPhase(oppositeFlag);
                }
                ParsePhase(this->flagTable.GetAsPhase(flag), oppositePhase);
                break;
            }

            case FlagString:
                *this->flagTable.GetAsString(flag) = ParseString(buffer, MaxTokenSize, false);
                break;

            case FlagNumber:
                *this->flagTable.GetAsNumber(flag) = ParseInteger();
                break;

            case FlagNumberSet:
                ParseNumberSet(this->flagTable.GetAsNumberSet(flag));
                break;

            case FlagNumberPairSet:
                ParseNumberPairSet(this->flagTable.GetAsNumberPairSet(flag));
                break;

            case FlagNumberRange:
                ParseNumberRange(this->flagTable.GetAsNumberRange(flag));
                break;

            default:
                AssertMsg(0, "Flag not Handled");
            }
            break;
        case ' ':
        case 0:
            break;
        default:
            throw Exception(_u("Expected ':'"));
        }
    }
}

///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::Parse
///
/// The main loop which parses 1 flag at a time
///
///----------------------------------------------------------------------------

int
CmdLineArgsParser::Parse(int argc, __in_ecount(argc) LPWSTR argv[])
{TRACE_IT(19689);
    int err = 0;

    for(int i = 1; i < argc; i++)
    {TRACE_IT(19690);
        if ((err = Parse(argv[i])) != 0)
        {TRACE_IT(19691);
            break;
        }
    }

    if(this->flagTable.Filename == nullptr)
    {TRACE_IT(19692);
        this->flagTable.Filename = _u("ttdSentinal.js");
    }

    return err;
}

int CmdLineArgsParser::Parse(__in LPWSTR oneArg) throw()
{TRACE_IT(19693);
    int err = 0;
    char16 buffer[MaxTokenSize];
    ZeroMemory(buffer, sizeof(buffer));

    this->pszCurrentArg = oneArg;
    AssertMsg(NULL != this->pszCurrentArg, "How can command line give NULL argv's");
    try
    {TRACE_IT(19694);
        switch(CurChar())
        {
        case '-' :
            if ('-' == PeekChar())
            {TRACE_IT(19695);
                //support --
                NextChar();
            }
            //fallthrough
#ifdef _WIN32
        // Only support '/' as a command line switch start char on Windows
        // for legacy reason. Deprecate on xplat, as it starts a path on Unix.
        case '/':
#endif
            NextChar();
            if('?' == CurChar())
            {TRACE_IT(19696);
                PrintUsage();
                return -1;
            }
            ParseFlag();
            break;
        default:
            if(NULL != this->flagTable.Filename)
            {TRACE_IT(19697);
                throw Exception(_u("Duplicate filename entry"));
            }

            this->flagTable.Filename = ParseString(buffer, MaxTokenSize, false);
            break;
        }
    }
    catch(Exception &exp)
    {TRACE_IT(19698);
        wprintf(_u("%s : %s\n"), (LPCWSTR)exp, oneArg);
        err = -1;
    }
    return err;
}


///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::CmdLineArgsParser
///
/// Constructor
///
///----------------------------------------------------------------------------

CmdLineArgsParser::CmdLineArgsParser(ICustomConfigFlags * pCustomConfigFlags, Js::ConfigFlagsTable& flagTable) :
    flagTable(flagTable), pCustomConfigFlags(pCustomConfigFlags)
{TRACE_IT(19699);
    this->pszCurrentArg = NULL;
}

CmdLineArgsParser::~CmdLineArgsParser()
{TRACE_IT(19700);
    flagTable.FinalizeConfiguration();
}

void CmdLineArgsParser::PrintUsage()
{TRACE_IT(19701);
    if (pCustomConfigFlags)
    {TRACE_IT(19702);
        pCustomConfigFlags->PrintUsage();
        return;
    }
    Js::ConfigFlagsTable::PrintUsageString();
}
