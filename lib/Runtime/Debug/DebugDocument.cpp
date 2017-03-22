//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

namespace Js
{
    DebugDocument::DebugDocument(Utf8SourceInfo* utf8SourceInfo, Js::FunctionBody* functionBody) :
        utf8SourceInfo(utf8SourceInfo),
        m_breakpointList(nullptr)
    {LOGMEIN("DebugDocument.cpp] 11\n");
        Assert(utf8SourceInfo != nullptr);
        if (functionBody != nullptr)
        {LOGMEIN("DebugDocument.cpp] 14\n");
            this->functionBody.Root(functionBody, this->utf8SourceInfo->GetScriptContext()->GetRecycler());
        }
    }

    DebugDocument::~DebugDocument()
    {LOGMEIN("DebugDocument.cpp] 20\n");
        Assert(this->utf8SourceInfo == nullptr);
        Assert(this->m_breakpointList == nullptr);
    }

    void DebugDocument::CloseDocument()
    {LOGMEIN("DebugDocument.cpp] 26\n");
        if (this->m_breakpointList != nullptr)
        {LOGMEIN("DebugDocument.cpp] 28\n");
            this->ClearAllBreakPoints();
        }

        Assert(this->utf8SourceInfo != nullptr);

        if (functionBody)
        {LOGMEIN("DebugDocument.cpp] 35\n");
            functionBody.Unroot(this->utf8SourceInfo->GetScriptContext()->GetRecycler());
        }

        this->utf8SourceInfo = nullptr;
    }

    BreakpointProbeList* DebugDocument::GetBreakpointList()
    {LOGMEIN("DebugDocument.cpp] 43\n");
        if (m_breakpointList != nullptr)
        {LOGMEIN("DebugDocument.cpp] 45\n");
            return m_breakpointList;
        }

        ScriptContext * scriptContext = this->utf8SourceInfo->GetScriptContext();
        if (scriptContext == nullptr || scriptContext->IsClosed())
        {LOGMEIN("DebugDocument.cpp] 51\n");
            return nullptr;
        }

        ArenaAllocator* diagnosticArena = scriptContext->AllocatorForDiagnostics();
        AssertMem(diagnosticArena);

        m_breakpointList = this->NewBreakpointList(diagnosticArena);
        return m_breakpointList;
    }

    BreakpointProbeList* DebugDocument::NewBreakpointList(ArenaAllocator* arena)
    {LOGMEIN("DebugDocument.cpp] 63\n");
        return BreakpointProbeList::New(arena);
    }

    HRESULT DebugDocument::SetBreakPoint(int32 ibos, BREAKPOINT_STATE breakpointState)
    {LOGMEIN("DebugDocument.cpp] 68\n");
        ScriptContext* scriptContext = this->utf8SourceInfo->GetScriptContext();

        if (scriptContext == nullptr || scriptContext->IsClosed())
        {LOGMEIN("DebugDocument.cpp] 72\n");
            return E_UNEXPECTED;
        }

        StatementLocation statement;
        if (!this->GetStatementLocation(ibos, &statement))
        {LOGMEIN("DebugDocument.cpp] 78\n");
            return E_FAIL;
        }

        this->SetBreakPoint(statement, breakpointState);

        return S_OK;
    }

    BreakpointProbe* DebugDocument::SetBreakPoint(StatementLocation statement, BREAKPOINT_STATE bps)
    {LOGMEIN("DebugDocument.cpp] 88\n");
        ScriptContext* scriptContext = this->utf8SourceInfo->GetScriptContext();

        if (scriptContext == nullptr || scriptContext->IsClosed())
        {LOGMEIN("DebugDocument.cpp] 92\n");
            return nullptr;
        }

        switch (bps)
        {LOGMEIN("DebugDocument.cpp] 97\n");
            default:
                AssertMsg(FALSE, "Bad breakpoint state");
                // Fall thru
            case BREAKPOINT_DISABLED:
            case BREAKPOINT_DELETED:
            {LOGMEIN("DebugDocument.cpp] 103\n");
                BreakpointProbeList* pBreakpointList = this->GetBreakpointList();
                if (pBreakpointList)
                {LOGMEIN("DebugDocument.cpp] 106\n");
                    ArenaAllocator arena(_u("TemporaryBreakpointList"), scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticPageAllocator(), Throw::OutOfMemory);
                    BreakpointProbeList* pDeleteList = this->NewBreakpointList(&arena);

                    pBreakpointList->Map([&statement, scriptContext, pDeleteList](int index, BreakpointProbe * breakpointProbe)
                    {
                        if (breakpointProbe->Matches(statement.function, statement.statement.begin))
                        {LOGMEIN("DebugDocument.cpp] 113\n");
                            scriptContext->GetDebugContext()->GetProbeContainer()->RemoveProbe(breakpointProbe);
                            pDeleteList->Add(breakpointProbe);
                        }
                    });

                    pDeleteList->Map([pBreakpointList](int index, BreakpointProbe * breakpointProbe)
                    {
                        pBreakpointList->Remove(breakpointProbe);
                    });
                    pDeleteList->Clear();
                }

                break;
            }
            case BREAKPOINT_ENABLED:
            {LOGMEIN("DebugDocument.cpp] 129\n");
                BreakpointProbe* pProbe = Anew(scriptContext->AllocatorForDiagnostics(), BreakpointProbe, this, statement,
                    scriptContext->GetThreadContext()->GetDebugManager()->GetNextBreakpointId());

                scriptContext->GetDebugContext()->GetProbeContainer()->AddProbe(pProbe);
                BreakpointProbeList* pBreakpointList = this->GetBreakpointList();
                pBreakpointList->Add(pProbe);
                return pProbe;
                break;
            }
        }
        return nullptr;
    }

    void DebugDocument::RemoveBreakpointProbe(BreakpointProbe *probe)
    {LOGMEIN("DebugDocument.cpp] 144\n");
        Assert(probe);
        if (m_breakpointList)
        {LOGMEIN("DebugDocument.cpp] 147\n");
            m_breakpointList->Remove(probe);
        }
    }

    void DebugDocument::ClearAllBreakPoints(void)
    {LOGMEIN("DebugDocument.cpp] 153\n");
        if (m_breakpointList != nullptr)
        {LOGMEIN("DebugDocument.cpp] 155\n");
            m_breakpointList->Clear();
            m_breakpointList = nullptr;
        }
    }

    Js::BreakpointProbe* DebugDocument::FindBreakpoint(StatementLocation statement)
    {LOGMEIN("DebugDocument.cpp] 162\n");
        Js::BreakpointProbe* probe = nullptr;
        if (m_breakpointList != nullptr)
        {LOGMEIN("DebugDocument.cpp] 165\n");
            m_breakpointList->MapUntil([&](int index, BreakpointProbe* bpProbe) -> bool
            {
                if (bpProbe != nullptr && bpProbe->Matches(statement))
                {LOGMEIN("DebugDocument.cpp] 169\n");
                    probe = bpProbe;
                    return true;
                }
                return false;
            });
        }

        return probe;
    }

    bool DebugDocument::FindBPStatementLocation(UINT bpId, StatementLocation * statement)
    {LOGMEIN("DebugDocument.cpp] 181\n");
        bool foundStatement = false;
        if (m_breakpointList != nullptr)
        {LOGMEIN("DebugDocument.cpp] 184\n");
            m_breakpointList->MapUntil([&](int index, BreakpointProbe* bpProbe) -> bool
            {
                if (bpProbe != nullptr && bpProbe->GetId() == bpId)
                {LOGMEIN("DebugDocument.cpp] 188\n");
                    bpProbe->GetStatementLocation(statement);
                    foundStatement = true;
                    return true;
                }
                return false;
            });
        }
        return foundStatement;
    }

    BOOL DebugDocument::GetStatementSpan(int32 ibos, StatementSpan* pStatement)
    {LOGMEIN("DebugDocument.cpp] 200\n");
        StatementLocation statement;
        if (GetStatementLocation(ibos, &statement))
        {LOGMEIN("DebugDocument.cpp] 203\n");
            pStatement->ich = statement.statement.begin;
            pStatement->cch = statement.statement.end - statement.statement.begin;
            return TRUE;
        }
        return FALSE;
    }

    FunctionBody * DebugDocument::GetFunctionBodyAt(int32 ibos)
    {LOGMEIN("DebugDocument.cpp] 212\n");
        StatementLocation location = {};
        if (GetStatementLocation(ibos, &location))
        {LOGMEIN("DebugDocument.cpp] 215\n");
            return location.function;
        }

        return nullptr;
    }

    BOOL DebugDocument::HasLineBreak(int32 _start, int32 _end)
    {LOGMEIN("DebugDocument.cpp] 223\n");
        return this->functionBody->HasLineBreak(_start, _end);
    }

    BOOL DebugDocument::GetStatementLocation(int32 ibos, StatementLocation* plocation)
    {LOGMEIN("DebugDocument.cpp] 228\n");
        if (ibos < 0)
        {LOGMEIN("DebugDocument.cpp] 230\n");
            return FALSE;
        }

        ScriptContext* scriptContext = this->utf8SourceInfo->GetScriptContext();
        if (scriptContext == nullptr || scriptContext->IsClosed())
        {LOGMEIN("DebugDocument.cpp] 236\n");
            return FALSE;
        }

        uint32 ubos = static_cast<uint32>(ibos);

        // Getting the appropriate statement on the asked position works on the heuristic which requires two
        // probable candidates. These candidates will be closest to the ibos where first.range.start < ibos and
        // second.range.start >= ibos. They will be fetched out by going into each FunctionBody.

        StatementLocation candidateMatch1 = {};
        StatementLocation candidateMatch2 = {};

        this->utf8SourceInfo->MapFunction([&](FunctionBody* pFuncBody)
        {
            uint32 functionStart = pFuncBody->StartInDocument();
            uint32 functionEnd = functionStart + pFuncBody->LengthInBytes();

            // For the first candidate, we should allow the current function to participate if its range
            // (instead of just start offset) is closer to the ubos compared to already found candidate1.

            if (candidateMatch1.function == nullptr ||
                ((candidateMatch1.statement.begin <= static_cast<int>(functionStart) ||
                candidateMatch1.statement.end <= static_cast<int>(functionEnd)) &&
                ubos > functionStart) ||
                candidateMatch2.function == nullptr ||
                (candidateMatch2.statement.begin > static_cast<int>(functionStart) &&
                ubos <= functionStart) ||
                (functionStart <= ubos &&
                ubos < functionEnd))
            {LOGMEIN("DebugDocument.cpp] 266\n");
                // We need to find out two possible candidate from the current FunctionBody.
                pFuncBody->FindClosestStatements(ibos, &candidateMatch1, &candidateMatch2);
            }
        });

        if (candidateMatch1.function == nullptr && candidateMatch2.function == nullptr)
        {LOGMEIN("DebugDocument.cpp] 273\n");
            return FALSE; // No Match found
        }

        if (candidateMatch1.function == nullptr || candidateMatch2.function == nullptr)
        {LOGMEIN("DebugDocument.cpp] 278\n");
            *plocation = (candidateMatch1.function == nullptr) ? candidateMatch2 : candidateMatch1;

            return TRUE;
        }

        // If one of the func is inner to another one, and ibos is in the inner one, disregard the outer one/let the inner one win.
        // See WinBlue 575634. Scenario is like this: var foo = function () {this;} -- and BP is set to 'this;' 'function'.
        if (candidateMatch1.function != candidateMatch2.function)
        {LOGMEIN("DebugDocument.cpp] 287\n");
            Assert(candidateMatch1.function && candidateMatch2.function);

            regex::Interval func1Range(candidateMatch1.function->StartInDocument());
            func1Range.End(func1Range.Begin() + candidateMatch1.function->LengthInBytes());
            regex::Interval func2Range(candidateMatch2.function->StartInDocument());
            func2Range.End(func2Range.Begin() + candidateMatch2.function->LengthInBytes());

            // If cursor (ibos) is just after the closing braces of the inner function then we can't
            // directly choose inner function and have to make line break check, so fallback
            // function foo(){function bar(){var y=1;}#var x=1;bar();}foo(); - ibos is #
            if (func1Range.Includes(func2Range) && func2Range.Includes(ibos) && func2Range.End() != ibos)
            {LOGMEIN("DebugDocument.cpp] 299\n");
                *plocation = candidateMatch2;
                return TRUE;
            }
            else if (func2Range.Includes(func1Range) && func1Range.Includes(ibos) && func1Range.End() != ibos)
            {LOGMEIN("DebugDocument.cpp] 304\n");
                *plocation = candidateMatch1;
                return TRUE;
            }
        }

        // At this point we have both candidate to consider.

        Assert(candidateMatch1.statement.begin < candidateMatch2.statement.begin);
        Assert(candidateMatch1.statement.begin < ibos);
        Assert(candidateMatch2.statement.begin >= ibos);

        // Default selection
        *plocation = candidateMatch1;

        // If the second candidate start at ibos or
        // if the first candidate has line break between ibos and the second candidate is on the same line as ibos
        // then consider the second one.

        BOOL fNextHasLineBreak = this->HasLineBreak(ibos, candidateMatch2.statement.begin);

        if ((candidateMatch2.statement.begin == ibos)
            || (this->HasLineBreak(candidateMatch1.statement.begin, ibos) && !fNextHasLineBreak))
        {LOGMEIN("DebugDocument.cpp] 327\n");
            *plocation = candidateMatch2;
        }
        // If ibos is out of the range of first candidate, choose second candidate if  ibos is on the same line as second candidate
        // or ibos is not on the same line of the end of the first candidate.
        else if (candidateMatch1.statement.end < ibos && (!fNextHasLineBreak || this->HasLineBreak(candidateMatch1.statement.end, ibos)))
        {LOGMEIN("DebugDocument.cpp] 333\n");
            *plocation = candidateMatch2;
        }

        return TRUE;
    }
}
