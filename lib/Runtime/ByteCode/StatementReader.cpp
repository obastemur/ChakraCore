//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    template <typename TStatementMapList>
    void StatementReader<TStatementMapList>::Create(FunctionBody * functionRead, uint startOffset /* = 0 */)
    {TRACE_IT(41959);
        Assert(functionRead);
        StatementReader::Create(functionRead, startOffset, false);
    }

    template <typename TStatementMapList>
    void StatementReader<TStatementMapList>::Create(
        _In_ const byte * byteCodeStart,
        uint startOffset,
        Js::SmallSpanSequence * statementMap,
        TStatementMapList* fullstatementMap)
    {TRACE_IT(41960);
        m_startLocation = byteCodeStart;

        const byte * currentLocation = m_startLocation + startOffset;

        m_statementMap = statementMap;
        m_fullstatementMap = fullstatementMap;

        if (m_statementMap && m_statementMap->Count())
        {TRACE_IT(41961);
            m_statementMap->Reset(m_statementMapIter);

            m_statementIndex = 0;
            m_startOfStatement = true;

            StatementData data;
            if (!m_statementMap->Seek(m_statementIndex, data))
            {TRACE_IT(41962);
                Assert(FALSE);
            }

            m_nextStatementBoundary = m_startLocation + data.bytecodeBegin;

            // If we starting in the middle of the function (e.g., loop body), find out where the next statement is.
            while (m_nextStatementBoundary < currentLocation)
            {TRACE_IT(41963);
                this->MoveNextStatementBoundary();
            }
        }
        else if (m_fullstatementMap && m_fullstatementMap->Count())
        {TRACE_IT(41964);
            m_statementIndex = 0;
            m_startOfStatement = true;
            FunctionBody::StatementMap *nextMap = Js::FunctionBody::GetNextNonSubexpressionStatementMap(m_fullstatementMap, m_statementIndex);
            if (!nextMap)
            {TRACE_IT(41965);
                // set to a location that will never match
                m_nextStatementBoundary = currentLocation - 1;
            }
            else
            {TRACE_IT(41966);
                m_nextStatementBoundary = m_startLocation + m_fullstatementMap->Item(m_statementIndex)->byteCodeSpan.begin;

                // If we starting in the middle of the function (e.g., loop body), find out where the next statement is.
                while (m_nextStatementBoundary < currentLocation)
                {TRACE_IT(41967);
                    this->MoveNextStatementBoundary();
                }
            }
        }
        else
        {TRACE_IT(41968);
            // set to a location that will never match
            m_nextStatementBoundary = currentLocation - 1;
        }
    }
    template <>
    void StatementReader<FunctionBody::ArenaStatementMapList>::Create(FunctionBody* functionRead, uint startOffset, bool useOriginalByteCode)
    {TRACE_IT(41969);
        Assert(UNREACHED);
    }

    template <>
    void StatementReader<FunctionBody::StatementMapList>::Create(FunctionBody* functionRead, uint startOffset, bool useOriginalByteCode)
    {TRACE_IT(41970);
        AssertMsg(functionRead != nullptr, "Must provide valid function to execute");

        ByteBlock * pblkByteCode = useOriginalByteCode ?
            functionRead->GetOriginalByteCode() :
            functionRead->GetByteCode();

        AssertMsg(pblkByteCode != nullptr, "Must have valid byte-code to read");

        SmallSpanSequence* statementMap = functionRead->GetStatementMapSpanSequence();
        FunctionBody::StatementMapList* fullMap = nullptr;
        if (statementMap == nullptr && functionRead->IsInDebugMode())
        {TRACE_IT(41971);
            fullMap = functionRead->GetStatementMaps();
        }
        Create(pblkByteCode->GetBuffer(), startOffset, statementMap, fullMap);
    }

    template <typename TStatementMapList>
    uint32 StatementReader<TStatementMapList>::MoveNextStatementBoundary()
    {TRACE_IT(41972);
        StatementData data;
        uint32 retStatement = Js::Constants::NoStatementIndex;

        if (m_startOfStatement)
        {TRACE_IT(41973);
            m_statementIndex++;
            if (m_statementMap && (uint32)m_statementIndex < m_statementMap->Count() && m_statementMap->Item(m_statementIndex, m_statementMapIter, data))
            {TRACE_IT(41974);
                // The end boundary is the last byte of the last instruction in the previous range.
                // We want to track the beginning of the next instruction for AtStatementBoundary.
                m_nextStatementBoundary = m_startLocation + data.bytecodeBegin;

                // The next user statement is adjacent in the bytecode
                retStatement = m_statementIndex;
            }
            else if (m_fullstatementMap && m_statementIndex < m_fullstatementMap->Count())
            {TRACE_IT(41975);
                int nextInstrStart = m_fullstatementMap->Item(m_statementIndex - 1)->byteCodeSpan.end + 1;
                m_nextStatementBoundary = m_startLocation + nextInstrStart;
                Js::FunctionBody::GetNextNonSubexpressionStatementMap(m_fullstatementMap, m_statementIndex);

                if (nextInstrStart == m_fullstatementMap->Item(m_statementIndex)->byteCodeSpan.begin)
                {TRACE_IT(41976);
                    retStatement = m_statementIndex;
                }
                else
                {TRACE_IT(41977);
                    m_startOfStatement = false;
                }
            }
            else
            {TRACE_IT(41978);
                m_startOfStatement = false;
            }
        }
        else
        {TRACE_IT(41979);
            m_startOfStatement = true;
            if (m_statementMap && (uint32)m_statementIndex < m_statementMap->Count() && m_statementMap->Item(m_statementIndex, m_statementMapIter, data))
            {TRACE_IT(41980);
                // Start a range of bytecode that maps to a user statement
                m_nextStatementBoundary = m_startLocation + data.bytecodeBegin;
                retStatement = m_statementIndex;
            }
            else if (m_fullstatementMap && m_statementIndex < m_fullstatementMap->Count())
            {TRACE_IT(41981);
                FunctionBody::StatementMap *nextMap = Js::FunctionBody::GetNextNonSubexpressionStatementMap(m_fullstatementMap, m_statementIndex);
                if (!nextMap)
                {TRACE_IT(41982);
                    // set to a location that will never match
                    m_nextStatementBoundary = m_startLocation - 1;
                }
                else
                {TRACE_IT(41983);
                    // Start a range of bytecode that maps to a user statement
                    m_nextStatementBoundary = m_startLocation + m_fullstatementMap->Item(m_statementIndex)->byteCodeSpan.begin;
                    retStatement = m_statementIndex;
                }
            }
            else
            {TRACE_IT(41984);
                // The remaining bytecode instructions do not map to a user statement, set a statementBoundary that cannot match
                m_nextStatementBoundary = m_startLocation - 1;
            }
        }

        return retStatement;
    }

    // explicit instantiations
    template class StatementReader<FunctionBody::ArenaStatementMapList>;
    template class StatementReader<FunctionBody::StatementMapList>;
} // namespace Js

