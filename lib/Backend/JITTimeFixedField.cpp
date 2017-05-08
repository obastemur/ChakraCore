//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

CompileAssert(sizeof(FixedFieldIDL) == sizeof(JITTimeFixedField));

void
JITTimeFixedField::SetNextHasSameFixedField()
{TRACE_IT(9771);
    m_data.nextHasSameFixedField = TRUE;
}

bool
JITTimeFixedField::IsClassCtor() const
{TRACE_IT(9772);
    return m_data.isClassCtor != FALSE;
}

bool
JITTimeFixedField::NextHasSameFixedField() const
{TRACE_IT(9773);
    return m_data.nextHasSameFixedField != FALSE;
}

uint
JITTimeFixedField::GetLocalFuncId() const
{TRACE_IT(9774);
    return m_data.localFuncId;
}

ValueType
JITTimeFixedField::GetValueType() const
{TRACE_IT(9775);
    CompileAssert(sizeof(ValueType) == sizeof(uint16));
    return *(ValueType*)&m_data.valueType;
}

intptr_t
JITTimeFixedField::GetFieldValue() const
{TRACE_IT(9776);
    return m_data.fieldValue;
}

intptr_t
JITTimeFixedField::GetFuncInfoAddr() const
{TRACE_IT(9777);
    return m_data.funcInfoAddr;
}

intptr_t
JITTimeFixedField::GetEnvironmentAddr() const
{TRACE_IT(9778);
    return m_data.environmentAddr;
}

JITType *
JITTimeFixedField::GetType() const
{TRACE_IT(9779);
    return (JITType*)m_data.type;
}
