//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimeWorkItem::JITTimeWorkItem(CodeGenWorkItemIDL * workItemData) :
    m_workItemData(workItemData),
    m_jitBody(workItemData->jitData->bodyData),
    m_fullStatementList(nullptr)
{TRACE_IT(10039);
}

CodeGenWorkItemType
JITTimeWorkItem::Type() const
{TRACE_IT(10040);
    return static_cast<CodeGenWorkItemType>(m_workItemData->type);
}

ExecutionMode
JITTimeWorkItem::GetJitMode() const
{TRACE_IT(10041);
    return static_cast<ExecutionMode>(m_workItemData->jitMode);
}

// loop number if IsLoopBody, otherwise Js::LoopHeader::NoLoop
uint
JITTimeWorkItem::GetLoopNumber() const
{TRACE_IT(10042);
    Assert(IsLoopBody() || m_workItemData->loopNumber == Js::LoopHeader::NoLoop);
    return m_workItemData->loopNumber;
}

bool
JITTimeWorkItem::IsLoopBody() const
{TRACE_IT(10043);
    return Type() == JsLoopBodyWorkItemType;
}

bool
JITTimeWorkItem::IsJitInDebugMode() const
{TRACE_IT(10044);
    // TODO (michhol): flags?
    return Js::Configuration::Global.EnableJitInDebugMode()
        && m_workItemData->isJitInDebugMode;
}

intptr_t
JITTimeWorkItem::GetCallsCountAddress() const
{TRACE_IT(10045);
    Assert(Type() == JsFunctionType);

    return m_workItemData->jitData->callsCountAddress;
}

intptr_t
JITTimeWorkItem::GetJittedLoopIterationsSinceLastBailoutAddr() const
{TRACE_IT(10046);
    Assert(IsLoopBody());
    Assert(m_workItemData->jittedLoopIterationsSinceLastBailoutAddr != 0);

    return m_workItemData->jittedLoopIterationsSinceLastBailoutAddr;
}

const JITLoopHeaderIDL *
JITTimeWorkItem::GetLoopHeader() const
{TRACE_IT(10047);
    return m_jitBody.GetLoopHeaderData(GetLoopNumber());
}

intptr_t
JITTimeWorkItem::GetLoopHeaderAddr() const
{TRACE_IT(10048);
    return m_jitBody.GetLoopHeaderAddr(GetLoopNumber());
}

void
JITTimeWorkItem::InitializeReader(
    Js::ByteCodeReader * reader,
    Js::StatementReader<Js::FunctionBody::ArenaStatementMapList> * statementReader, ArenaAllocator* alloc)
{TRACE_IT(10049);
    uint startOffset = IsLoopBody() ? GetLoopHeader()->startOffset : 0;

    if (IsJitInDebugMode())
    {TRACE_IT(10050);
        // TODO: OOP JIT, directly use the array rather than making a list
        m_fullStatementList = Js::FunctionBody::ArenaStatementMapList::New(alloc);
        CompileAssert(sizeof(StatementMapIDL) == sizeof(Js::FunctionBody::StatementMap));

        StatementMapIDL * fullArr = m_jitBody.GetFullStatementMap();
        for (uint i = 0; i < m_jitBody.GetFullStatementMapCount(); ++i)
        {TRACE_IT(10051);
            m_fullStatementList->Add((Js::FunctionBody::StatementMap*)&fullArr[i]);
        }
    }
#if DBG
    reader->Create(m_jitBody.GetByteCodeBuffer(), startOffset, m_jitBody.GetByteCodeLength());
    if (!JITManager::GetJITManager()->IsOOPJITEnabled())
    {TRACE_IT(10052);
        Js::FunctionBody::StatementMapList * runtimeMap = ((Js::FunctionBody*)m_jitBody.GetAddr())->GetStatementMaps();
        Assert(!m_fullStatementList || ((int)m_jitBody.GetFullStatementMapCount() == runtimeMap->Count() && runtimeMap->Count() >= 0));
        for (uint i = 0; i < m_jitBody.GetFullStatementMapCount(); ++i)
        {TRACE_IT(10053);
            Assert(runtimeMap->Item(i)->byteCodeSpan.begin == m_fullStatementList->Item(i)->byteCodeSpan.begin);
            Assert(runtimeMap->Item(i)->byteCodeSpan.end == m_fullStatementList->Item(i)->byteCodeSpan.end);
            Assert(runtimeMap->Item(i)->sourceSpan.begin == m_fullStatementList->Item(i)->sourceSpan.begin);
            Assert(runtimeMap->Item(i)->sourceSpan.end == m_fullStatementList->Item(i)->sourceSpan.end);
            Assert(runtimeMap->Item(i)->isSubexpression == m_fullStatementList->Item(i)->isSubexpression);
        }
    }
#else
    reader->Create(m_jitBody.GetByteCodeBuffer(), startOffset);
#endif
    bool hasSpanSequenceMap = m_jitBody.InitializeStatementMap(&m_statementMap, alloc);
    Js::SmallSpanSequence * spanSeq = hasSpanSequenceMap ? &m_statementMap : nullptr;
    statementReader->Create(m_jitBody.GetByteCodeBuffer(), startOffset, spanSeq, m_fullStatementList);
}

JITTimeFunctionBody *
JITTimeWorkItem::GetJITFunctionBody()
{TRACE_IT(10054);
    return &m_jitBody;
}

uint16
JITTimeWorkItem::GetProfiledIterations() const
{TRACE_IT(10055);
    return m_workItemData->profiledIterations;
}

CodeGenWorkItemIDL *
JITTimeWorkItem::GetWorkItemData()
{TRACE_IT(10056);
    return m_workItemData;
}

JITTimePolymorphicInlineCacheInfo *
JITTimeWorkItem::GetPolymorphicInlineCacheInfo()
{TRACE_IT(10057);
    return (JITTimePolymorphicInlineCacheInfo *)m_workItemData->selfInfo;
}

JITTimePolymorphicInlineCacheInfo *
JITTimeWorkItem::GetInlineePolymorphicInlineCacheInfo(intptr_t funcBodyAddr)
{TRACE_IT(10058);
    for (uint i = 0; i < m_workItemData->inlineeInfoCount; ++i)
    {TRACE_IT(10059);
        if (m_workItemData->inlineeInfo[i].functionBodyAddr == (void*)funcBodyAddr)
        {TRACE_IT(10060);
            return (JITTimePolymorphicInlineCacheInfo *)&m_workItemData->inlineeInfo[i];
        }
    }
    return nullptr;
}

void
JITTimeWorkItem::SetJITTimeData(FunctionJITTimeDataIDL * jitData)
{TRACE_IT(10061);
    m_workItemData->jitData = jitData;
}

FunctionJITTimeInfo *
JITTimeWorkItem::GetJITTimeInfo() const
{TRACE_IT(10062);
    return reinterpret_cast<FunctionJITTimeInfo *>(m_workItemData->jitData);
}

bool
JITTimeWorkItem::HasSymIdToValueTypeMap() const
{TRACE_IT(10063);
    return m_workItemData->symIdToValueTypeMap != nullptr;
}

bool
JITTimeWorkItem::TryGetValueType(uint symId, ValueType * valueType) const
{TRACE_IT(10064);
    Assert(IsLoopBody());
    uint index = symId - m_jitBody.GetConstCount();
    if (symId >= m_jitBody.GetConstCount() && index < m_workItemData->symIdToValueTypeMapCount)
    {TRACE_IT(10065);
        ValueType type = ((ValueType*)m_workItemData->symIdToValueTypeMap)[index];
        if (type.GetRawData() != 0)
        {TRACE_IT(10066);
            *valueType = type;
            return true;
        }
    }
    return false;
}
