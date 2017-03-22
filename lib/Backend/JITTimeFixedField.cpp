//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

CompileAssert(sizeof(FixedFieldIDL) == sizeof(JITTimeFixedField));

void
JITTimeFixedField::SetNextHasSameFixedField()
{LOGMEIN("JITTimeFixedField.cpp] 11\n");
    m_data.nextHasSameFixedField = TRUE;
}

bool
JITTimeFixedField::IsClassCtor() const
{LOGMEIN("JITTimeFixedField.cpp] 17\n");
    return m_data.isClassCtor != FALSE;
}

bool
JITTimeFixedField::NextHasSameFixedField() const
{LOGMEIN("JITTimeFixedField.cpp] 23\n");
    return m_data.nextHasSameFixedField != FALSE;
}

uint
JITTimeFixedField::GetLocalFuncId() const
{LOGMEIN("JITTimeFixedField.cpp] 29\n");
    return m_data.localFuncId;
}

ValueType
JITTimeFixedField::GetValueType() const
{LOGMEIN("JITTimeFixedField.cpp] 35\n");
    CompileAssert(sizeof(ValueType) == sizeof(uint16));
    return *(ValueType*)&m_data.valueType;
}

intptr_t
JITTimeFixedField::GetFieldValue() const
{LOGMEIN("JITTimeFixedField.cpp] 42\n");
    return m_data.fieldValue;
}

intptr_t
JITTimeFixedField::GetFuncInfoAddr() const
{LOGMEIN("JITTimeFixedField.cpp] 48\n");
    return m_data.funcInfoAddr;
}

intptr_t
JITTimeFixedField::GetEnvironmentAddr() const
{LOGMEIN("JITTimeFixedField.cpp] 54\n");
    return m_data.environmentAddr;
}

JITType *
JITTimeFixedField::GetType() const
{LOGMEIN("JITTimeFixedField.cpp] 60\n");
    return (JITType*)m_data.type;
}
