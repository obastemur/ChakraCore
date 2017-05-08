//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"

#ifdef PROFILE_EXEC
#include "Core/ProfileInstrument.h"

#define HIRES_PROFILER
namespace Js
{
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// class Profiler::UnitData
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------


    ///----------------------------------------------------------------------------
    ///
    /// UnitData::UnitData
    ///
    /// Constructor
    ///
    ///----------------------------------------------------------------------------

    UnitData::UnitData()
    {TRACE_IT(20398);
        this->incl  = 0;
        this->excl  = 0;
        this->max   = 0;
        this->count = 0;
    }

    ///----------------------------------------------------------------------------
    ///
    /// UnitData::Add
    ///
    ///----------------------------------------------------------------------------

    void
    UnitData::Add(TimeStamp incl, TimeStamp excl)
    {TRACE_IT(20399);
        this->incl      += incl;
        this->excl      += excl;
        this->count++;
        if (incl > this->max)
        {TRACE_IT(20400);
            this->max = incl;
        }
    }

    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// class Profiler
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Profiler
    ///
    /// Constructor
    ///
    ///----------------------------------------------------------------------------

    Profiler::Profiler(ArenaAllocator * allocator) :
        alloc(allocator),
        rootNode(NULL)
    {TRACE_IT(20401);
        this->curNode = &this->rootNode;

        for(int i = 0; i < PhaseCount; i++)
        {TRACE_IT(20402);
            this->inclSumAtLevel[i] = 0;
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Begin
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::Begin(Phase tag)
    {
        Push(TimeEntry(tag, GetTime()));
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::End
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::End(Phase tag)
    {
        Pop(TimeEntry(tag, GetTime()));
    }

    void
    Profiler::Suspend(Phase tag, SuspendRecord * suspendRecord)
    {TRACE_IT(20403);
        suspendRecord->count = 0;
        Phase topTag;
        do
        {TRACE_IT(20404);
            topTag = timeStack.Peek()->tag;
            Pop(TimeEntry(topTag, GetTime()));
            suspendRecord->phase[suspendRecord->count++] = topTag;
        } while(topTag != tag);
    }

    void
    Profiler::Resume(SuspendRecord * suspendRecord)
    {TRACE_IT(20405);
        while (suspendRecord->count)
        {TRACE_IT(20406);
            suspendRecord->count--;
            Begin(suspendRecord->phase[suspendRecord->count]);
        }
    }
    ///----------------------------------------------------------------------------
    ///
    /// Profiler::EndAllUpTo
    ///
    /// Ends all phases up to the specified phase. Useful for catching exceptions
    /// after a phase was started, and ending all intermediate phases until the
    /// first phase that was started.
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::EndAllUpTo(Phase tag)
    {TRACE_IT(20407);
        Phase topTag;
        do
        {TRACE_IT(20408);
            topTag = timeStack.Peek()->tag;
            Pop(TimeEntry(topTag, GetTime()));
        } while(topTag != tag);
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Push
    ///
    ///     1.  Push entry on stack.
    ///     2.  Update curNode
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::Push(TimeEntry entry)
    {TRACE_IT(20409);
        AssertMsg(NULL != curNode, "Profiler Stack Corruption");

        this->timeStack.Push(entry);
        if(!curNode->ChildExistsAt(entry.tag))
        {TRACE_IT(20410);
            TypeNode * node = AnewNoThrow(this->alloc, TypeNode, curNode);
            // We crash if we run out of memory here and we don't care
            curNode->SetChildAt(entry.tag, node);
        }
        curNode = curNode->GetChildAt(entry.tag);
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Pop
    ///
    /// Core logic for the timer. Calculated the exclusive, inclusive times.
    /// There is a list inclSumAtLevel which stores accumulates the inclusive sum
    /// of all the tags that where 'pushed' after this tag.
    ///
    /// Consider the following calls. fx indicates Push and fx', the corresponding Pop
    ///
    /// f1
    ///     f2
    ///         f3
    ///         f3'
    ///     f2'
    ///     f4
    ///         f5
    ///         f5'
    ///     f4'
    /// f1'
    ///
    /// calculating the inclusive times are trivial. Let us calculate the exclusive
    /// time for f1. That would be
    ///         excl(f1) = incl(f1) - [incl(f2) + incl(f4)]
    ///
    /// Basically if a function is at level 'x' then we need to deduct from its
    /// exclusive times, the inclusive times of all the functions at level 'x + 1'
    /// We don't care about deeper levels. Hence 'inclSumAtLevel' array which accumulates
    /// the sum of variables at different levels.
    ///
    /// Reseting the next level is also required. In the above example, f3 and f5 are
    /// at the same level. if we don't reset level 3 when popping f2, then we will
    /// have wrong sums for f4. So once a tag has been popped, all sums at its higher
    /// levels is set to zero. (Of course we just need to reset the next level and
    /// all above levels will invariably remain zero)
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::Pop(TimeEntry curEntry)
    {TRACE_IT(20411);
        int         curLevel                = this->timeStack.Count();
        TimeEntry   *entry                   = this->timeStack.Pop();

        AssertMsg(entry->tag == curEntry.tag, "Profiler Stack corruption, push pop entries do not correspond to the same tag");

        TimeStamp   inclusive               = curEntry.time - entry->time;
        TimeStamp   exclusive               = inclusive - this->inclSumAtLevel[curLevel +1];

        Assert(inclusive >= 0);
        Assert(exclusive >= 0);

        this->inclSumAtLevel[curLevel + 1]  = 0;
        this->inclSumAtLevel[curLevel]     += inclusive;

        curNode->GetValue()->Add(inclusive, exclusive);
        curNode                             = curNode->GetParent();

        AssertMsg(curNode != NULL, "Profiler stack corruption");
    }

    void
    Profiler::Merge(Profiler * profiler)
    {
        MergeTree(&rootNode, &profiler->rootNode);
        if (profiler->timeStack.Count() > 1)
        {
            FixedStack<TimeEntry, MaxStackDepth> reverseStack;
            do
            {TRACE_IT(20412);
                reverseStack.Push(*profiler->timeStack.Pop());
            }
            while (profiler->timeStack.Count() > 1);

            do
            {TRACE_IT(20413);
                TimeEntry * entry = reverseStack.Pop();
                this->Push(*entry);
                profiler->timeStack.Push(*entry);
            }
            while (reverseStack.Count() != 0);
        }
    }

    void
    Profiler::MergeTree(TypeNode * toNode, TypeNode * fromNode)
    {TRACE_IT(20414);
        UnitData * toData = toNode->GetValue();
        const UnitData * fromData = fromNode->GetValue();

        toData->count += fromData->count;
        toData->incl += fromData->incl;
        toData->excl += fromData->excl;
        if (fromData->max > toData->max)
        {TRACE_IT(20415);
            toData->max = fromData->max;
        }
        for (int i = 0; i < PhaseCount; i++)
        {TRACE_IT(20416);
            if (fromNode->ChildExistsAt(i))
            {TRACE_IT(20417);
                TypeNode * fromChild = fromNode->GetChildAt(i);
                TypeNode * toChild;
                if (!toNode->ChildExistsAt(i))
                {TRACE_IT(20418);
                    toChild = Anew(this->alloc, TypeNode, toNode);
                    toNode->SetChildAt(i, toChild);
                }
                else
                {TRACE_IT(20419);
                    toChild = toNode->GetChildAt(i);
                }
                MergeTree(toChild, fromChild);
            }
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::PrintTree
    ///
    /// Private method that walks the tree and prints it recursively.
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::PrintTree(TypeNode *node, TypeNode *baseNode, int column, TimeStamp freq)
    {TRACE_IT(20420);
        const UnitData *base        = baseNode->GetValue();

        for(int i = 0; i < PhaseCount; i++)
        {TRACE_IT(20421);
            if(node->ChildExistsAt(i))
            {TRACE_IT(20422);
                UnitData *data = node->GetChildAt(i)->GetValue();
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
                if( int(data->incl * 100 / base->incl) >= Configuration::Global.flags.ProfileThreshold) // threshold
#endif
                {TRACE_IT(20423);

                    Output::SkipToColumn(column);

                    Output::Print(_u("%-*s %7.1f %5d %7.1f %5d %7.1f %7.1f %5d\n"),
                            (Profiler::PhaseNameWidth-column), PhaseNames[i],
                            (double)data->incl / freq ,                    // incl
                            int(data->incl * 100 / base->incl ),        // incl %
                            (double)data->excl / freq ,                    // excl
                            int(data->excl * 100 / base->incl ),        // excl %
                            (double)data->max  / freq ,                    // max
                            (double)data->incl / ( freq * data->count ),   // mean
                            int(data->count)                            // count
                           );
                }

                PrintTree(node->GetChildAt(i), baseNode, column + Profiler::TabWidth, freq);
            }
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Print
    ///
    /// Pretty printer
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::Print(Phase baseTag)
    {TRACE_IT(20424);
        if (baseTag == InvalidPhase)
        {TRACE_IT(20425);
            baseTag = AllPhase;     // default to all phase
        }
        const TimeStamp freq = this->GetFrequency();

        bool foundNode = false;
        ForEachNode(baseTag, &rootNode, [&](TypeNode *const baseNode, const Phase parentTag)
        {
            if(!foundNode)
            {TRACE_IT(20426);
                foundNode = true;
                Output::Print(_u("%-*s:%7s %5s %7s %5s %7s %7s %5s\n"),
                                (Profiler::PhaseNameWidth-0),
                                _u("Profiler Report"),
                                _u("Incl"),
                                _u("(%)"),
                                _u("Excl"),
                                _u("(%)"),
                                _u("Max"),
                                _u("Mean"),
                                _u("Count")
                                );
                Output::Print(_u("-------------------------------------------------------------------------------\n"));
            }

            UnitData *data      = baseNode->GetValue();

            if(0 == data->count)
            {TRACE_IT(20427);
                Output::Print(_u("The phase : %s was never started"), PhaseNames[baseTag]);
                return;
            }

            int indent = 0;

            if(parentTag != InvalidPhase)
            {TRACE_IT(20428);
                TypeNode *const parentNode = baseNode->GetParent();
                Assert(parentNode);

                Output::Print(_u("%-*s\n"), (Profiler::PhaseNameWidth-0), PhaseNames[parentTag]);
                indent += Profiler::TabWidth;
            }

            if(indent)
            {TRACE_IT(20429);
                Output::SkipToColumn(indent);
            }
            Output::Print(_u("%-*s %7.1f %5d %7.1f %5d %7.1f %7.1f %5d\n"),
                    (Profiler::PhaseNameWidth-indent),
                    PhaseNames[baseTag],
                    (double)data->incl / freq ,                 // incl
                    int(100),                                   // incl %
                    (double)data->excl / freq ,                 // excl
                    int(data->excl * 100 / data->incl ),        // excl %
                    (double)data->max  / freq ,                 // max
                    (double)data->incl / ( freq * data->count ),// mean
                    int(data->count)                            // count
                    );
            indent += Profiler::TabWidth;

            PrintTree(baseNode, baseNode, indent, freq);
        });

        if(foundNode)
        {TRACE_IT(20430);
            Output::Print(_u("-------------------------------------------------------------------------------\n"));
            Output::Flush();
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::FindNode
    ///
    /// Does a tree traversal(DFS) and finds the first occurrence of the 'tag'
    ///
    ///----------------------------------------------------------------------------

    template<class FVisit>
    void
    Profiler::ForEachNode(Phase tag, TypeNode *node, FVisit visit, Phase parentTag)
    {TRACE_IT(20431);
        AssertMsg(node != NULL, "Invalid usage: node must always be non null");

        for(int i = 0; i < PhaseCount; i++)
        {TRACE_IT(20432);
            if(node->ChildExistsAt(i))
            {TRACE_IT(20433);
                TypeNode * child = node->GetChildAt(i);
                if(i == tag)
                {
                    visit(child, parentTag);
                }
                else
                {
                    ForEachNode(tag, child, visit, static_cast<Phase>(i));
                }
            }
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::GetTime
    ///
    ///----------------------------------------------------------------------------

    TimeStamp
    Profiler::GetTime()
    {TRACE_IT(20434);
#if !defined HIRES_PROFILER && (defined(_M_IX86) || defined(_M_X64))
        return __rdtsc();
#else
        LARGE_INTEGER tmp;
        if(QueryPerformanceCounter(&tmp))
        {TRACE_IT(20435);
            return tmp.QuadPart;
        }
        else
        {
            AssertMsg(0, "Could not get time. Don't know what to do");
            return 0;
        }
#endif
    }


    ///----------------------------------------------------------------------------
    ///
    /// Profiler::GetFrequency
    ///
    ///----------------------------------------------------------------------------

    TimeStamp
    Profiler::GetFrequency()
    {TRACE_IT(20436);
#if !defined HIRES_PROFILER && (defined(_M_IX86) || defined(_M_X64))
        long long start, end;
        int CPUInfo[4];

        // Flush pipeline
        __cpuid(CPUInfo, 0);

        // Measure 1 second / 5
        start = GetTime();
        Sleep(1000/5);
        end = GetTime();

        return  ((end - start) * 5) / FrequencyScale;
#else
        LARGE_INTEGER tmp;
        if(QueryPerformanceFrequency(&tmp))
        {TRACE_IT(20437);
            return tmp.QuadPart / FrequencyScale;
        }
        else
        {
            AssertMsg(0, "Could not get time. Don't know what to do");
            return 0;
        }
#endif
    }


} //namespace Js

#endif
