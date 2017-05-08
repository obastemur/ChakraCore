//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

//static
DebuggingFlags::DebuggingFlags() :
    m_forceInterpreter(false),
    m_isIgnoringException(false),
    m_byteCodeOffsetAfterIgnoreException(InvalidByteCodeOffset),
    m_funcNumberAfterIgnoreException(InvalidFuncNumber),
    m_isBuiltInWrapperPresent(false)
{
    // In Lowerer::LowerBailForDebugger we rely on the following:
    CompileAssert(offsetof(DebuggingFlags, m_isIgnoringException) == offsetof(DebuggingFlags, m_forceInterpreter) + 1);
}

bool DebuggingFlags::GetForceInterpreter() const
{TRACE_IT(42260);
    return this->m_forceInterpreter;
}

void DebuggingFlags::SetForceInterpreter(bool value)
{TRACE_IT(42261);
    this->m_forceInterpreter = value;
}

//static
size_t DebuggingFlags::GetForceInterpreterOffset()
{TRACE_IT(42262);
    return offsetof(DebuggingFlags, m_forceInterpreter);
}

int DebuggingFlags::GetByteCodeOffsetAfterIgnoreException() const
{TRACE_IT(42263);
    return this->m_byteCodeOffsetAfterIgnoreException;
}

uint DebuggingFlags::GetFuncNumberAfterIgnoreException() const
{TRACE_IT(42264);
    return this->m_funcNumberAfterIgnoreException;
}

void DebuggingFlags::SetByteCodeOffsetAfterIgnoreException(int offset)
{TRACE_IT(42265);
    this->m_byteCodeOffsetAfterIgnoreException = offset;
    this->m_isIgnoringException = offset != InvalidByteCodeOffset;
}

void DebuggingFlags::SetByteCodeOffsetAndFuncAfterIgnoreException(int offset, uint functionNumber)
{TRACE_IT(42266);
    this->SetByteCodeOffsetAfterIgnoreException(offset);
    this->m_funcNumberAfterIgnoreException = functionNumber;
}

void DebuggingFlags::ResetByteCodeOffsetAndFuncAfterIgnoreException()
{TRACE_IT(42267);
    this->SetByteCodeOffsetAfterIgnoreException(InvalidByteCodeOffset);
    this->m_funcNumberAfterIgnoreException = InvalidFuncNumber;
}

/* static */
size_t DebuggingFlags::GetByteCodeOffsetAfterIgnoreExceptionOffset()
{TRACE_IT(42268);
    return offsetof(DebuggingFlags, m_byteCodeOffsetAfterIgnoreException);
}

bool DebuggingFlags::IsBuiltInWrapperPresent() const
{TRACE_IT(42269);
    return m_isBuiltInWrapperPresent;
}

void DebuggingFlags::SetIsBuiltInWrapperPresent(bool value /* = true */)
{TRACE_IT(42270);
    m_isBuiltInWrapperPresent = value;
}
