//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

#if ENABLE_REGEX_CONFIG_OPTIONS

namespace UnifiedRegex
{
    const char16* RegexStats::PhaseNames[RegexStats::NumPhases] = { _u("parse"), _u("compile"), _u("execute") };
    const char16* RegexStats::UseNames[RegexStats::NumUses] = { _u("match"), _u("exec"), _u("test"), _u("replace"), _u("split"), _u("search") };

    RegexStats::RegexStats(RegexPattern* pattern)
        : pattern(pattern)
        , inputLength(0)
        , numCompares(0)
        , numPushes(0)
        , numPops(0)
        , stackHWM(0)
        , numInsts(0)
    {TRACE_IT(32545);
        for (int i = 0; i < NumPhases; i++)
            phaseTicks[i] = 0;
        for (int i = 0; i < NumUses; i++)
            useCounts[i] = 0;
    }

    void RegexStats::Print(DebugWriter* w, RegexStats* totals, Ticks ticksPerMillisecond)
    {TRACE_IT(32546);
        if (pattern == 0)
            w->PrintEOL(_u("TOTAL"));
        else
            pattern->Print(w);
        w->EOL();
        w->Indent();

        for (int i = 0; i < NumPhases; i++)
        {TRACE_IT(32547);
            double ms = (double)phaseTicks[i] / (double)ticksPerMillisecond;
            if (totals == 0 || totals->phaseTicks[i] == 0)
                w->PrintEOL(_u("%-12s: %10.4fms"), PhaseNames[i], ms);
            else
            {TRACE_IT(32548);
                double pc = (double)phaseTicks[i] * 100.0  / (double)totals->phaseTicks[i];
                w->PrintEOL(_u("%-12s: %10.4fms (%10.4f%%)"), PhaseNames[i], ms, pc);
            }
        }

        for (int i = 0; i < NumUses; i++)
        {TRACE_IT(32549);
            if (useCounts[i] > 0)
            {TRACE_IT(32550);
                if (totals == 0 || totals->useCounts[i] == 0)
                    w->PrintEOL(_u("#%-11s: %10I64u"), UseNames[i], useCounts[i]);
                else
                {TRACE_IT(32551);
                    double pc = (double)useCounts[i] * 100.0 / (double)totals->useCounts[i];
                    w->PrintEOL(_u("#%-11s: %10I64u   (%10.4f%%)"), UseNames[i], useCounts[i], pc);
                }
            }
        }

        if (inputLength > 0)
        {TRACE_IT(32552);
            double r = (double)numCompares * 100.0 / (double)inputLength;
            if (totals == 0 || totals->numCompares == 0)
                w->PrintEOL(_u("numCompares : %10.4f%%"), r);
            else
            {TRACE_IT(32553);
                double pc = (double)numCompares * 100.0 / (double)totals->numCompares;
                w->PrintEOL(_u("numCompares : %10.4f%%  (%10.4f%%)"), r, pc);
            }
        }

        if (totals == 0 || totals->inputLength == 0)
            w->PrintEOL(_u("inputLength : %10I64u"), inputLength);
        else
        {TRACE_IT(32554);
            double pc = (double)inputLength * 100.0 / (double)totals->inputLength;
            w->PrintEOL(_u("inputLength : %10I64u   (%10.4f%%)"), inputLength, pc);
        }

        if (totals == 0 || totals->numPushes == 0)
            w->PrintEOL(_u("numPushes   : %10I64u"), numPushes);
        else
        {TRACE_IT(32555);
            double pc = (double)numPushes * 100.0 / (double)totals->numPushes;
            w->PrintEOL(_u("numPushes   : %10I64u   (%10.4f%%)"), numPushes, pc);
        }

        if (totals == 0 || totals->numPops == 0)
            w->PrintEOL(_u("numPops     : %10I64u"), numPops);
        else
        {TRACE_IT(32556);
            double pc = (double)numPops * 100.0 / (double)totals->numPops;
            w->PrintEOL(_u("numPops     : %10I64u   (%10.4f%%)"), numPops, pc);
        }

        if (totals == 0 || totals->stackHWM == 0)
            w->PrintEOL(_u("stackHWM    : %10I64u"), stackHWM);
        else
        {TRACE_IT(32557);
            double pc = (double)stackHWM * 100.0 / (double)totals->stackHWM;
            w->PrintEOL(_u("stackHWM    : %10I64u   (%10.4f%%)"), stackHWM, pc);
        }

        if (totals == 0 || totals->numInsts == 0)
            w->PrintEOL(_u("numInsts    : %10I64u"), numInsts);
        else
        {TRACE_IT(32558);
            double pc = (double)numInsts * 100.0 / (double)totals->numInsts;
            w->PrintEOL(_u("numInsts    : %10I64u   (%10.4f%%)"), numInsts, pc);
        }

        w->Unindent();
    }

    void RegexStats::Add(RegexStats* other)
    {TRACE_IT(32559);
        for (int i = 0; i < NumPhases; i++)
            phaseTicks[i] += other->phaseTicks[i];
        for (int i = 0; i < NumUses; i++)
            useCounts[i] += other->useCounts[i];
        inputLength += other->inputLength;
        numCompares += other->numCompares;
        numPushes += other->numPushes;
        numPops += other->numPops;
        if (other->stackHWM > stackHWM)
            stackHWM = other->stackHWM;
        numInsts += other->numInsts;
    }

    RegexStats::Ticks RegexStatsDatabase::Now()
    {TRACE_IT(32560);
        LARGE_INTEGER tmp;
        if (QueryPerformanceCounter(&tmp))
            return tmp.QuadPart;
        else
        {TRACE_IT(32561);
            Assert(false);
            return 0;
        }
    }

    RegexStats::Ticks RegexStatsDatabase::Freq()
    {TRACE_IT(32562);
        LARGE_INTEGER tmp;
        if (QueryPerformanceFrequency(&tmp))
        {TRACE_IT(32563);
            return tmp.QuadPart / 1000;
        }
        else
        {TRACE_IT(32564);
            Assert(false);
            return 1;
        }
    }

    RegexStatsDatabase::RegexStatsDatabase(ArenaAllocator* allocator)
        : start(0), allocator(allocator)
    {TRACE_IT(32565);
        ticksPerMillisecond = Freq();
        map = Anew(allocator, RegexStatsMap, allocator, 17);
    }

    RegexStats* RegexStatsDatabase::GetRegexStats(RegexPattern* pattern)
    {TRACE_IT(32566);
        Js::InternalString str = pattern->GetSource();
        RegexStats *res;
        if (!map->TryGetValue(str, &res))
        {TRACE_IT(32567);
            res = Anew(allocator, RegexStats, pattern);
            map->Add(str, res);
        }
        return res;
    }

    void RegexStatsDatabase::BeginProfile()
    {TRACE_IT(32568);
        start = Now();
    }

    void RegexStatsDatabase::EndProfile(RegexStats* stats, RegexStats::Phase phase)
    {TRACE_IT(32569);
        stats->phaseTicks[phase] += Now() - start;
    }

    void RegexStatsDatabase::Print(DebugWriter* w)
    {TRACE_IT(32570);
        RegexStats totals(0);

        Output::Print(_u("Regular Expression Statistics\n"));
        Output::Print(_u("=============================\n"));

        for (int i = 0; i < map->Count(); i++)
            totals.Add(map->GetValueAt(i));

        for (int i = 0; i < map->Count(); i++)
            map->GetValueAt(i)->Print(w, &totals, ticksPerMillisecond);

        totals.Print(w, 0, ticksPerMillisecond);

        allocator->Free(w, sizeof(DebugWriter));
    }
}

#endif
