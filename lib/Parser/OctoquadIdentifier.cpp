//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{
    // ----------------------------------------------------------------------
    // Trigrams
    // ----------------------------------------------------------------------

    TrigramInfo::TrigramInfo(__in_ecount(PatternLength) char* pat1,__in_ecount(PatternLength) char* pat2, Recycler* recycler)
    {TRACE_IT(29279);
        isTrigramPattern=true;
        hasCachedResultString = false;

        int k;
        triPat1=0;
        triPat2=0;
        resultCount=0;
        for (k=3;k<PatternLength;k++) {TRACE_IT(29280);
            triPat1=(triPat1<<4)+pat1[k];
            triPat2=(triPat2<<4)+pat2[k];
        }
    }

    void TrigramAlphabet::InitTrigramMap() {TRACE_IT(29281);
        input=NULL;
        // set up mapping from 9 bits to trigram
        for (int i=0;i<TrigramMapSize;i++) {TRACE_IT(29282);
            int t1=i>>6;
            int t2=(i>>3)&0x7;
            int t3=i&0x7;
            if ((t1>=AlphaCount)||(t2>=AlphaCount)||(t3>=AlphaCount)) {TRACE_IT(29283);
                trigramMap[i]=TrigramNotInPattern;
            }
            else {TRACE_IT(29284);
                // number of trigram
                trigramMap[i]=(char)((t1<<4)+(t2<<2)+t3);
            }
        }

        for (int j=0;j<TrigramCount;j++) {TRACE_IT(29285);
            trigramStarts[j].count=0;
        }
    }

    bool TrigramAlphabet::AddStarts(__in_xcount(TrigramInfo::PatternLength) char* pat1,__in_xcount(TrigramInfo::PatternLength) char* pat2, RegexPattern* pattern)
    {TRACE_IT(29286);
        for (int k=0;k<TrigramCount;k++) {TRACE_IT(29287);
            char t1=1<<(k>>4);
            char t2=1<<((k>>2)&0x3);
            char t3=1<<(k&0x3);
            if ((t1&pat1[0])&&(t2&pat1[1])&&(t3&pat1[2])) {TRACE_IT(29288);
                if ((t1&pat2[0])&&(t2&pat2[1])&&(t3&pat2[2])) {TRACE_IT(29289);
                    return false;
                }
                else {TRACE_IT(29290);
                    TrigramStart* trigramStart=(&trigramStarts[k]);
                    if (trigramStart->count>=TrigramStart::MaxPatPerStart) {TRACE_IT(29291);
                        return false;
                    }
                    else {TRACE_IT(29292);
                        PatternTri* tri= &(trigramStart->patterns[trigramStart->count++]);
                        tri->pattern=pattern;
                        tri->encodedPattern=pattern->rep.unified.trigramInfo->triPat1;
                    }
                }
            }
            else if ((t1&pat2[0])&&(t2&pat2[1])&&(t3&pat2[2])) {TRACE_IT(29293);
                TrigramStart* trigramStart=(&trigramStarts[k]);
                if (trigramStart->count>=TrigramStart::MaxPatPerStart) {TRACE_IT(29294);
                    return false;
                }
                else {TRACE_IT(29295);
                    PatternTri* tri= &(trigramStart->patterns[trigramStart->count++]);
                    tri->pattern=pattern;
                    tri->encodedPattern=pattern->rep.unified.trigramInfo->triPat2;
                }
            }
        }
        return true;
    }

    void TrigramAlphabet::MegaMatch(__in_ecount(inputLen) const char16* input,int inputLen) {TRACE_IT(29296);
        this->input=input;
        this->inputLen=inputLen;
        if (inputLen<TrigramInfo::PatternLength) {TRACE_IT(29297);
            return;
        }
        // prime the pump
        unsigned char c1=alphaBits[input[0]&UpperCaseMask];
        unsigned char c2=alphaBits[input[1]&UpperCaseMask];
        unsigned char c3=alphaBits[input[2]&UpperCaseMask];
        // pump
        for (int k=3;k<inputLen-5;k++) {TRACE_IT(29298);
            int index=(c1<<6)+(c2<<3)+c3;
            if (index<TrigramMapSize) {TRACE_IT(29299);
                int t=trigramMap[index];
                if (t!=TrigramNotInPattern) {TRACE_IT(29300);
                    int count=trigramStarts[t].count;
                    if (count>0) {TRACE_IT(29301);
                        int inputMask=0;
                        bool validInput=true;
                        for (int j=0;j<5;j++) {TRACE_IT(29302);
                            // ascii check
                            if (input[k+j]<128) {TRACE_IT(29303);
                                int bits=alphaBits[input[k+j]&UpperCaseMask];
                                if (bits==BitsNotInAlpha) {TRACE_IT(29304);
                                    validInput=false;
                                    break;
                                }
                                inputMask=(inputMask<<AlphaCount)+(1<<bits);
                            }
                            else {TRACE_IT(29305);
                                validInput=false;
                                break;
                            }
                        }
                        if (validInput) {TRACE_IT(29306);
                            for (int j=0;j<count;j++) {TRACE_IT(29307);
                                PatternTri* tri= &(trigramStarts[t].patterns[j]);
                                if ((inputMask&(tri->encodedPattern))==inputMask) {TRACE_IT(29308);
                                    if (tri->pattern->rep.unified.trigramInfo->resultCount<TrigramInfo::MaxResults) {TRACE_IT(29309);
                                        tri->pattern->rep.unified.trigramInfo->offsets[tri->pattern->rep.unified.trigramInfo->resultCount++]=k-3;
                                    }
                                    else {TRACE_IT(29310);
                                        tri->pattern->rep.unified.trigramInfo->isTrigramPattern=false;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            c1=c2;
            c2=c3;
            c3=alphaBits[input[k]&UpperCaseMask];
        }
    }

    // ----------------------------------------------------------------------
    // OctoquadIdentifier
    // ----------------------------------------------------------------------

    bool OctoquadIdentifier::Qualifies(const Program *const program)
    {TRACE_IT(29311);
        return (program->flags & (GlobalRegexFlag | IgnoreCaseRegexFlag)) == (GlobalRegexFlag | IgnoreCaseRegexFlag);
    }

    OctoquadIdentifier::OctoquadIdentifier(
        const int numCodes,
        char (&codeToChar)[TrigramAlphabet::AlphaCount],
        char (&charToCode)[TrigramAlphabet::AsciiTableSize])
        : numCodes(numCodes),
        codeToChar(codeToChar),
        charToCode(charToCode),
        currPatternLength(0),
        currPatternNum(-1)
    {TRACE_IT(29312);
        // 'patternBits' will be initialized as necessary
    }

    int OctoquadIdentifier::GetOrAddCharCode(const Char c)
    {TRACE_IT(29313);
        if (c >= static_cast<Char>('A') && c <= static_cast<Char>('Z'))
        {TRACE_IT(29314);
            for (int i = 0; i < numCodes; i++)
            {TRACE_IT(29315);
                if (codeToChar[i] == static_cast<char>(c))
                    return i;
            }
            if (numCodes == TrigramAlphabet::AlphaCount)
                return -1;
            codeToChar[numCodes] = static_cast<char>(c);
            charToCode[c] = static_cast<char>(numCodes);
            return numCodes++;
        }
        else
            return -1;
    }

    bool OctoquadIdentifier::BeginConcat()
    {TRACE_IT(29316);
        if (currPatternNum >= 0 && currPatternLength != TrigramInfo::PatternLength)
            return false;
        if (currPatternNum >= NumPatterns)
            return false;
        currPatternNum++;
        currPatternLength = 0;
        return true;
    }

    bool OctoquadIdentifier::CouldAppend(const CharCount n) const
    {TRACE_IT(29317);
        return n <= static_cast<CharCount>(TrigramInfo::PatternLength - currPatternLength);
    }

    bool OctoquadIdentifier::AppendChar(Char c)
    {TRACE_IT(29318);
        if (currPatternLength >= TrigramInfo::PatternLength || currPatternNum < 0 || currPatternNum >= NumPatterns)
            return false;
        int code = GetOrAddCharCode(c);
        if (code < 0)
            return false;
        patternBits[currPatternNum][currPatternLength++] = 1 << code;
        return true;
    }

    bool OctoquadIdentifier::BeginUnions()
    {TRACE_IT(29319);
        if(currPatternLength >= TrigramInfo::PatternLength || currPatternNum < 0 || currPatternNum >= NumPatterns)
            return false;
        patternBits[currPatternNum][currPatternLength] = 0;
        return true;
    }

    bool OctoquadIdentifier::UnionChar(Char c)
    {TRACE_IT(29320);
        if (currPatternLength >= TrigramInfo::PatternLength || currPatternNum < 0 || currPatternNum >= NumPatterns)
            return false;
        int code = GetOrAddCharCode(c);
        if (code < 0)
            return false;
        patternBits[currPatternNum][currPatternLength] |= 1 << code;
        return true;
    }

    void OctoquadIdentifier::EndUnions()
    {TRACE_IT(29321);
        Assert(currPatternLength < TrigramInfo::PatternLength);
        ++currPatternLength;
    }

    bool OctoquadIdentifier::IsOctoquad()
    {TRACE_IT(29322);
        return
            numCodes == TrigramAlphabet::AlphaCount &&
            currPatternLength == TrigramInfo::PatternLength &&
            currPatternNum == NumPatterns - 1;
    }

    void OctoquadIdentifier::SetTrigramAlphabet(Js::ScriptContext * scriptContext,
        __in_xcount(regex::TrigramAlphabet::AlphaCount) char* alpha
        , __in_xcount(regex::TrigramAlphabet::AsciiTableSize) char* alphaBits)
    {TRACE_IT(29323);
        ArenaAllocator* alloc = scriptContext->RegexAllocator();
        TrigramAlphabet * trigramAlphabet = AnewStruct(alloc, UnifiedRegex::TrigramAlphabet);
        for (uint i = 0; i < UnifiedRegex::TrigramAlphabet::AsciiTableSize; i++) {TRACE_IT(29324);
            trigramAlphabet->alphaBits[i] = UnifiedRegex::TrigramAlphabet::BitsNotInAlpha;
        }
        for (uint i = 0; i < UnifiedRegex::TrigramAlphabet::AlphaCount; i++) {TRACE_IT(29325);
            trigramAlphabet->alpha[i] = alpha[i];
            trigramAlphabet->alphaBits[alpha[i]] = alphaBits[alpha[i]];
        }
        trigramAlphabet->InitTrigramMap();
        scriptContext->SetTrigramAlphabet(trigramAlphabet);
    }

    void OctoquadIdentifier::InitializeTrigramInfo(Js::ScriptContext* scriptContext, RegexPattern* const pattern)
    {TRACE_IT(29326);
        if(!scriptContext->GetTrigramAlphabet())
        {TRACE_IT(29327);
            this->SetTrigramAlphabet(scriptContext, codeToChar, charToCode);
        }
        const auto recycler = scriptContext->GetRecycler();
        pattern->rep.unified.trigramInfo = RecyclerNew(recycler, TrigramInfo, patternBits[0], patternBits[1], recycler);
        pattern->rep.unified.trigramInfo->isTrigramPattern =
            scriptContext->GetTrigramAlphabet()->AddStarts(patternBits[0], patternBits[1], pattern);
    }

    // ----------------------------------------------------------------------
    // OctoquadMatcher
    // ----------------------------------------------------------------------

    OctoquadMatcher::OctoquadMatcher(const StandardChars<Char>* standardChars, CaseInsensitive::MappingSource mappingSource, OctoquadIdentifier* identifier)
    {TRACE_IT(29328);
        for (int i = 0; i < TrigramAlphabet::AlphaCount; i++)
            codeToChar[i] = (Char)identifier->codeToChar[i];

        for (int i = 0; i < TrigramAlphabet::AsciiTableSize; i++)
            charToBits[i] = 0;

        for (int i = 0; i < TrigramAlphabet::AlphaCount; i++)
        {TRACE_IT(29329);
            Char equivs[CaseInsensitive::EquivClassSize];
            standardChars->ToEquivs(mappingSource, codeToChar[i], equivs);
            for (int j = 0; j < CaseInsensitive::EquivClassSize; j++)
            {TRACE_IT(29330);
                if (CTU(equivs[j]) < TrigramAlphabet::AsciiTableSize)
                    charToBits[CTU(equivs[j])] = 1 << i;
            }
        }

        for (int i = 0; i < OctoquadIdentifier::NumPatterns; i++)
        {TRACE_IT(29331);
            patterns[i] = 0;
            for (int j = 0; j < TrigramInfo::PatternLength; j++)
            {TRACE_IT(29332);
                patterns[i] <<= 4;
                patterns[i] |= (uint32)identifier->patternBits[i][j];
            }
        }
    }

    OctoquadMatcher *OctoquadMatcher::New(
        Recycler* recycler,
        const StandardChars<Char>* standardChars,
        CaseInsensitive::MappingSource mappingSource,
        OctoquadIdentifier* identifier)
    {TRACE_IT(29333);
        return RecyclerNewLeaf(recycler, OctoquadMatcher, standardChars, mappingSource, identifier);
    }

    // It exploits the fact that each quad of bits has at most only one bit set.
    inline bool oneBitSetInEveryQuad(uint32 x)
    {TRACE_IT(29334);
        x -= 0x11111111;
        return (x & 0x88888888u) == 0;
    }

    bool OctoquadMatcher::Match
        ( const Char* const input
        , const CharCount inputLength
        , CharCount& offset
#if ENABLE_REGEX_CONFIG_OPTIONS
        , RegexStats* stats
#endif
        )
    {TRACE_IT(29335);
        Assert(TrigramInfo::PatternLength == 8);
        Assert(OctoquadIdentifier::NumPatterns == 2);

        if (inputLength < TrigramInfo::PatternLength)
            return false;
        if (offset > inputLength - TrigramInfo::PatternLength)
            return false;

        uint32 v = 0;
        for (int i = 0; i < TrigramInfo::PatternLength; i++)
        {TRACE_IT(29336);
#if ENABLE_REGEX_CONFIG_OPTIONS
            if (stats != 0)
                stats->numCompares++;
#endif
            v <<= 4;
            if (CTU(input[offset + i]) < TrigramAlphabet::AsciiTableSize)
                v |= charToBits[CTU(input[offset + i])];
        }

        const uint32 lp = patterns[0];
        const uint32 rp = patterns[1];
        CharCount next = offset + TrigramInfo::PatternLength;

        while (true)
        {TRACE_IT(29337);
            if (oneBitSetInEveryQuad(v & lp) || oneBitSetInEveryQuad(v & rp))
            {TRACE_IT(29338);
                offset = next - TrigramInfo::PatternLength;
                return true;
            }
            if (next >= inputLength)
                return false;
#if ENABLE_REGEX_CONFIG_OPTIONS
            if (stats != 0)
                stats->numCompares++;
#endif
            v <<= 4;
            if (CTU(input[next]) < TrigramAlphabet::AsciiTableSize)
                v |= charToBits[CTU(input[next])];
            next++;
        }
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void OctoquadMatcher::Print(DebugWriter* w) const
    {TRACE_IT(29339);
        for (int i = 0; i < OctoquadIdentifier::NumPatterns; i++)
        {TRACE_IT(29340);
            if (i > 0)
                w->Print(_u("|"));
            for (int j = 0; j < TrigramInfo::PatternLength; j++)
            {TRACE_IT(29341);
                uint8 v = (patterns[i] >> ((TrigramInfo::PatternLength - j - 1) * TrigramAlphabet::AlphaCount)) & 0xf;
                int n = 0;
                uint8 x = v;
                while (x > 0)
                {TRACE_IT(29342);
                    x &= x-1;
                    n++;
                }
                if (n != 1)
                    w->Print(_u("["));
                for (int k = 0; k < TrigramAlphabet::AlphaCount; k++)
                {TRACE_IT(29343);
                    if ((v & 1) == 1)
                        w->PrintEscapedChar(codeToChar[k]);
                    v >>= 1;
                }
                if (n != 1)
                    w->Print(_u("]"));
            }
        }
    }
#endif
}




