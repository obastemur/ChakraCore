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
{LOGMEIN("DebuggingFlags.cpp] 19\n");
    return this->m_forceInterpreter;
}

void DebuggingFlags::SetForceInterpreter(bool value)
{LOGMEIN("DebuggingFlags.cpp] 24\n");
    this->m_forceInterpreter = value;
}

//static
size_t DebuggingFlags::GetForceInterpreterOffset()
{LOGMEIN("DebuggingFlags.cpp] 30\n");
    return offsetof(DebuggingFlags, m_forceInterpreter);
}

int DebuggingFlags::GetByteCodeOffsetAfterIgnoreException() const
{LOGMEIN("DebuggingFlags.cpp] 35\n");
    return this->m_byteCodeOffsetAfterIgnoreException;
}

uint DebuggingFlags::GetFuncNumberAfterIgnoreException() const
{LOGMEIN("DebuggingFlags.cpp] 40\n");
    return this->m_funcNumberAfterIgnoreException;
}

void DebuggingFlags::SetByteCodeOffsetAfterIgnoreException(int offset)
{LOGMEIN("DebuggingFlags.cpp] 45\n");
    this->m_byteCodeOffsetAfterIgnoreException = offset;
    this->m_isIgnoringException = offset != InvalidByteCodeOffset;
}

void DebuggingFlags::SetByteCodeOffsetAndFuncAfterIgnoreException(int offset, uint functionNumber)
{LOGMEIN("DebuggingFlags.cpp] 51\n");
    this->SetByteCodeOffsetAfterIgnoreException(offset);
    this->m_funcNumberAfterIgnoreException = functionNumber;
}

void DebuggingFlags::ResetByteCodeOffsetAndFuncAfterIgnoreException()
{LOGMEIN("DebuggingFlags.cpp] 57\n");
    this->SetByteCodeOffsetAfterIgnoreException(InvalidByteCodeOffset);
    this->m_funcNumberAfterIgnoreException = InvalidFuncNumber;
}

/* static */
size_t DebuggingFlags::GetByteCodeOffsetAfterIgnoreExceptionOffset()
{LOGMEIN("DebuggingFlags.cpp] 64\n");
    return offsetof(DebuggingFlags, m_byteCodeOffsetAfterIgnoreException);
}

bool DebuggingFlags::IsBuiltInWrapperPresent() const
{LOGMEIN("DebuggingFlags.cpp] 69\n");
    return m_isBuiltInWrapperPresent;
}

void DebuggingFlags::SetIsBuiltInWrapperPresent(bool value /* = true */)
{LOGMEIN("DebuggingFlags.cpp] 74\n");
    m_isBuiltInWrapperPresent = value;
}
