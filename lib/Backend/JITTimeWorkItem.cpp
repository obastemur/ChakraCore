//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimeWorkItem::JITTimeWorkItem(CodeGenWorkItemIDL * workItemData) :
    m_workItemData(workItemData),
    m_jitBody(workItemData->jitData->bodyData),
    m_fullStatementList(nullptr)
{LOGMEIN("JITTimeWorkItem.cpp] 11\n");
}

CodeGenWorkItemType
JITTimeWorkItem::Type() const
{LOGMEIN("JITTimeWorkItem.cpp] 16\n");
    return static_cast<CodeGenWorkItemType>(m_workItemData->type);
}

ExecutionMode
JITTimeWorkItem::GetJitMode() const
{LOGMEIN("JITTimeWorkItem.cpp] 22\n");
    return static_cast<ExecutionMode>(m_workItemData->jitMode);
}

// loop number if IsLoopBody, otherwise Js::LoopHeader::NoLoop
uint
JITTimeWorkItem::GetLoopNumber() const
{LOGMEIN("JITTimeWorkItem.cpp] 29\n");
    Assert(IsLoopBody() || m_workItemData->loopNumber == Js::LoopHeader::NoLoop);
    return m_workItemData->loopNumber;
}

bool
JITTimeWorkItem::IsLoopBody() const
{LOGMEIN("JITTimeWorkItem.cpp] 36\n");
    return Type() == JsLoopBodyWorkItemType;
}

bool
JITTimeWorkItem::IsJitInDebugMode() const
{LOGMEIN("JITTimeWorkItem.cpp] 42\n");
    // TODO (michhol): flags?
    return Js::Configuration::Global.EnableJitInDebugMode()
        && m_workItemData->isJitInDebugMode;
}

intptr_t
JITTimeWorkItem::GetCallsCountAddress() const
{LOGMEIN("JITTimeWorkItem.cpp] 50\n");
    Assert(Type() == JsFunctionType);

    return m_workItemData->jitData->callsCountAddress;
}

intptr_t
JITTimeWorkItem::GetJittedLoopIterationsSinceLastBailoutAddr() const
{LOGMEIN("JITTimeWorkItem.cpp] 58\n");
    Assert(IsLoopBody());
    Assert(m_workItemData->jittedLoopIterationsSinceLastBailoutAddr != 0);

    return m_workItemData->jittedLoopIterationsSinceLastBailoutAddr;
}

const JITLoopHeaderIDL *
JITTimeWorkItem::GetLoopHeader() const
{LOGMEIN("JITTimeWorkItem.cpp] 67\n");
    return m_jitBody.GetLoopHeaderData(GetLoopNumber());
}

intptr_t
JITTimeWorkItem::GetLoopHeaderAddr() const
{LOGMEIN("JITTimeWorkItem.cpp] 73\n");
    return m_jitBody.GetLoopHeaderAddr(GetLoopNumber());
}

void
JITTimeWorkItem::InitializeReader(
    Js::ByteCodeReader * reader,
    Js::StatementReader<Js::FunctionBody::ArenaStatementMapList> * statementReader, ArenaAllocator* alloc)
{LOGMEIN("JITTimeWorkItem.cpp] 81\n");
    uint startOffset = IsLoopBody() ? GetLoopHeader()->startOffset : 0;

    if (IsJitInDebugMode())
    {LOGMEIN("JITTimeWorkItem.cpp] 85\n");
        // TODO: OOP JIT, directly use the array rather than making a list
        m_fullStatementList = Js::FunctionBody::ArenaStatementMapList::New(alloc);
        CompileAssert(sizeof(StatementMapIDL) == sizeof(Js::FunctionBody::StatementMap));

        StatementMapIDL * fullArr = m_jitBody.GetFullStatementMap();
        for (uint i = 0; i < m_jitBody.GetFullStatementMapCount(); ++i)
        {LOGMEIN("JITTimeWorkItem.cpp] 92\n");
            m_fullStatementList->Add((Js::FunctionBody::StatementMap*)&fullArr[i]);
        }
    }
#if DBG
    reader->Create(m_jitBody.GetByteCodeBuffer(), startOffset, m_jitBody.GetByteCodeLength());
    if (!JITManager::GetJITManager()->IsOOPJITEnabled())
    {LOGMEIN("JITTimeWorkItem.cpp] 99\n");
        Js::FunctionBody::StatementMapList * runtimeMap = ((Js::FunctionBody*)m_jitBody.GetAddr())->GetStatementMaps();
        Assert(!m_fullStatementList || ((int)m_jitBody.GetFullStatementMapCount() == runtimeMap->Count() && runtimeMap->Count() >= 0));
        for (uint i = 0; i < m_jitBody.GetFullStatementMapCount(); ++i)
        {LOGMEIN("JITTimeWorkItem.cpp] 103\n");
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
{LOGMEIN("JITTimeWorkItem.cpp] 121\n");
    return &m_jitBody;
}

uint16
JITTimeWorkItem::GetProfiledIterations() const
{LOGMEIN("JITTimeWorkItem.cpp] 127\n");
    return m_workItemData->profiledIterations;
}

CodeGenWorkItemIDL *
JITTimeWorkItem::GetWorkItemData()
{LOGMEIN("JITTimeWorkItem.cpp] 133\n");
    return m_workItemData;
}

JITTimePolymorphicInlineCacheInfo *
JITTimeWorkItem::GetPolymorphicInlineCacheInfo()
{LOGMEIN("JITTimeWorkItem.cpp] 139\n");
    return (JITTimePolymorphicInlineCacheInfo *)m_workItemData->selfInfo;
}

JITTimePolymorphicInlineCacheInfo *
JITTimeWorkItem::GetInlineePolymorphicInlineCacheInfo(intptr_t funcBodyAddr)
{LOGMEIN("JITTimeWorkItem.cpp] 145\n");
    for (uint i = 0; i < m_workItemData->inlineeInfoCount; ++i)
    {LOGMEIN("JITTimeWorkItem.cpp] 147\n");
        if (m_workItemData->inlineeInfo[i].functionBodyAddr == (void*)funcBodyAddr)
        {LOGMEIN("JITTimeWorkItem.cpp] 149\n");
            return (JITTimePolymorphicInlineCacheInfo *)&m_workItemData->inlineeInfo[i];
        }
    }
    return nullptr;
}

void
JITTimeWorkItem::SetJITTimeData(FunctionJITTimeDataIDL * jitData)
{LOGMEIN("JITTimeWorkItem.cpp] 158\n");
    m_workItemData->jitData = jitData;
}

FunctionJITTimeInfo *
JITTimeWorkItem::GetJITTimeInfo() const
{LOGMEIN("JITTimeWorkItem.cpp] 164\n");
    return reinterpret_cast<FunctionJITTimeInfo *>(m_workItemData->jitData);
}

bool
JITTimeWorkItem::HasSymIdToValueTypeMap() const
{LOGMEIN("JITTimeWorkItem.cpp] 170\n");
    return m_workItemData->symIdToValueTypeMap != nullptr;
}

bool
JITTimeWorkItem::TryGetValueType(uint symId, ValueType * valueType) const
{LOGMEIN("JITTimeWorkItem.cpp] 176\n");
    Assert(IsLoopBody());
    uint index = symId - m_jitBody.GetConstCount();
    if (symId >= m_jitBody.GetConstCount() && index < m_workItemData->symIdToValueTypeMapCount)
    {LOGMEIN("JITTimeWorkItem.cpp] 180\n");
        ValueType type = ((ValueType*)m_workItemData->symIdToValueTypeMap)[index];
        if (type.GetRawData() != 0)
        {LOGMEIN("JITTimeWorkItem.cpp] 183\n");
            *valueType = type;
            return true;
        }
    }
    return false;
}
