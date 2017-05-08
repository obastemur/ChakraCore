//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITObjTypeSpecFldInfo::JITObjTypeSpecFldInfo(ObjTypeSpecFldIDL * data) :
    m_data(*data)
{TRACE_IT(9658);
    CompileAssert(sizeof(ObjTypeSpecFldIDL) == sizeof(JITObjTypeSpecFldInfo));
}

bool
JITObjTypeSpecFldInfo::UsesAuxSlot() const
{TRACE_IT(9659);
    return GetFlags().usesAuxSlot;
}

bool
JITObjTypeSpecFldInfo::UsesAccessor() const
{TRACE_IT(9660);
    return GetFlags().usesAccessor;
}

bool
JITObjTypeSpecFldInfo::IsRootObjectNonConfigurableFieldLoad() const
{TRACE_IT(9661);
    return GetFlags().isRootObjectNonConfigurableFieldLoad;
}

bool
JITObjTypeSpecFldInfo::HasEquivalentTypeSet() const
{TRACE_IT(9662);
    return m_data.typeSet != nullptr;
}

bool
JITObjTypeSpecFldInfo::DoesntHaveEquivalence() const
{TRACE_IT(9663);
    return GetFlags().doesntHaveEquivalence;
}

bool
JITObjTypeSpecFldInfo::IsPoly() const
{TRACE_IT(9664);
    return GetFlags().isPolymorphic;
}

bool
JITObjTypeSpecFldInfo::IsMono() const
{TRACE_IT(9665);
    return !IsPoly();
}

bool
JITObjTypeSpecFldInfo::IsBuiltIn() const
{TRACE_IT(9666);
    return GetFlags().isBuiltIn;
}

bool
JITObjTypeSpecFldInfo::IsLoadedFromProto() const
{TRACE_IT(9667);
    return GetFlags().isLoadedFromProto;
}

bool
JITObjTypeSpecFldInfo::HasFixedValue() const
{TRACE_IT(9668);
    return GetFlags().hasFixedValue;
}

bool
JITObjTypeSpecFldInfo::IsBeingStored() const
{TRACE_IT(9669);
    return GetFlags().isBeingStored;
}

bool
JITObjTypeSpecFldInfo::IsBeingAdded() const
{TRACE_IT(9670);
    return GetFlags().isBeingAdded;
}

bool
JITObjTypeSpecFldInfo::IsRootObjectNonConfigurableField() const
{TRACE_IT(9671);
    return GetFlags().isRootObjectNonConfigurableField;
}

bool
JITObjTypeSpecFldInfo::HasInitialType() const
{TRACE_IT(9672);
    return IsMono() && !IsLoadedFromProto() && m_data.initialType != nullptr;
}

bool
JITObjTypeSpecFldInfo::IsMonoObjTypeSpecCandidate() const
{TRACE_IT(9673);
    return IsMono();
}

bool
JITObjTypeSpecFldInfo::IsPolyObjTypeSpecCandidate() const
{TRACE_IT(9674);
    return IsPoly();
}

Js::TypeId
JITObjTypeSpecFldInfo::GetTypeId() const
{TRACE_IT(9675);
    Assert(m_data.typeId != Js::TypeIds_Limit);
    return (Js::TypeId)m_data.typeId;
}

Js::TypeId
JITObjTypeSpecFldInfo::GetTypeId(uint i) const
{TRACE_IT(9676);
    Assert(IsPoly());
    return (Js::TypeId)m_data.fixedFieldInfoArray[i].type->typeId;
}

Js::PropertyId
JITObjTypeSpecFldInfo::GetPropertyId() const
{TRACE_IT(9677);
    return (Js::PropertyId)m_data.propertyId;
}

uint16
JITObjTypeSpecFldInfo::GetSlotIndex() const
{TRACE_IT(9678);
    return m_data.slotIndex;
}

uint16
JITObjTypeSpecFldInfo::GetFixedFieldCount() const
{TRACE_IT(9679);
    return m_data.fixedFieldCount;
}

uint
JITObjTypeSpecFldInfo::GetObjTypeSpecFldId() const
{TRACE_IT(9680);
    return m_data.id;
}

intptr_t
JITObjTypeSpecFldInfo::GetProtoObject() const
{TRACE_IT(9681);
    return m_data.protoObjectAddr;
}

intptr_t
JITObjTypeSpecFldInfo::GetFieldValue(uint i) const
{TRACE_IT(9682);
    Assert(IsPoly());
    return m_data.fixedFieldInfoArray[i].fieldValue;
}

intptr_t
JITObjTypeSpecFldInfo::GetPropertyGuardValueAddr() const
{TRACE_IT(9683);
    return m_data.propertyGuardValueAddr;
}

intptr_t
JITObjTypeSpecFldInfo::GetFieldValueAsFixedDataIfAvailable() const
{TRACE_IT(9684);
    Assert(HasFixedValue() && GetFixedFieldCount() == 1);

    return m_data.fixedFieldInfoArray[0].fieldValue;
}

JITTimeConstructorCache *
JITObjTypeSpecFldInfo::GetCtorCache() const
{TRACE_IT(9685);
    return (JITTimeConstructorCache*)m_data.ctorCache;
}

Js::EquivalentTypeSet *
JITObjTypeSpecFldInfo::GetEquivalentTypeSet() const
{TRACE_IT(9686);
    return (Js::EquivalentTypeSet *)m_data.typeSet;
}

JITTypeHolder
JITObjTypeSpecFldInfo::GetType() const
{TRACE_IT(9687);
    Assert(IsMono());
    return JITTypeHolder((JITType *)m_data.fixedFieldInfoArray[0].type);
}

JITTypeHolder
JITObjTypeSpecFldInfo::GetType(uint i) const
{TRACE_IT(9688);
    Assert(IsPoly());
    return JITTypeHolder((JITType *)m_data.fixedFieldInfoArray[i].type);
}

JITTypeHolder
JITObjTypeSpecFldInfo::GetInitialType() const
{TRACE_IT(9689);
    return JITTypeHolder((JITType *)m_data.initialType);
}

JITTypeHolder
JITObjTypeSpecFldInfo::GetFirstEquivalentType() const
{TRACE_IT(9690);
    Assert(HasEquivalentTypeSet());
    return JITTypeHolder(GetEquivalentTypeSet()->GetFirstType());
}

void
JITObjTypeSpecFldInfo::SetIsBeingStored(bool value)
{TRACE_IT(9691);
    ((Js::ObjTypeSpecFldInfoFlags*)&m_data.flags)->isBeingStored = value;
}

JITTimeFixedField *
JITObjTypeSpecFldInfo::GetFixedFieldIfAvailableAsFixedFunction()
{TRACE_IT(9692);
    Assert(HasFixedValue());
    if (IsPoly() && DoesntHaveEquivalence())
    {TRACE_IT(9693);
        return nullptr;
    }

    Assert(m_data.fixedFieldInfoArray);
    if (m_data.fixedFieldInfoArray[0].funcInfoAddr != 0)
    {TRACE_IT(9694);
        return (JITTimeFixedField *)&m_data.fixedFieldInfoArray[0];
    }
    return nullptr;
}

JITTimeFixedField *
JITObjTypeSpecFldInfo::GetFixedFieldIfAvailableAsFixedFunction(uint i)
{TRACE_IT(9695);
    Assert(HasFixedValue());
    Assert(IsPoly());
    if (m_data.fixedFieldCount > 0 && m_data.fixedFieldInfoArray[i].funcInfoAddr != 0)
    {TRACE_IT(9696);
        return (JITTimeFixedField *)&m_data.fixedFieldInfoArray[i];
    }
    return nullptr;
}

JITTimeFixedField *
JITObjTypeSpecFldInfo::GetFixedFieldInfoArray()
{TRACE_IT(9697);
    return (JITTimeFixedField*)m_data.fixedFieldInfoArray;
}

/* static */
void
JITObjTypeSpecFldInfo::BuildObjTypeSpecFldInfoArray(
    __in ArenaAllocator * alloc,
    _In_reads_(arrayLength) Field(Js::ObjTypeSpecFldInfo*)* objTypeSpecInfo,
    __in uint arrayLength,
    _Inout_updates_(arrayLength) ObjTypeSpecFldIDL * jitData)
{TRACE_IT(9698);
    for (uint i = 0; i < arrayLength; ++i)
    {TRACE_IT(9699);
        if (objTypeSpecInfo[i] == nullptr)
        {TRACE_IT(9700);
            continue;
        }
        jitData[i].inUse = TRUE;
        if (objTypeSpecInfo[i]->IsLoadedFromProto())
        {TRACE_IT(9701);
            jitData[i].protoObjectAddr = (intptr_t)objTypeSpecInfo[i]->GetProtoObject();
        }
        jitData[i].propertyGuardValueAddr = (intptr_t)objTypeSpecInfo[i]->GetPropertyGuard()->GetAddressOfValue();
        jitData[i].propertyId = objTypeSpecInfo[i]->GetPropertyId();
        jitData[i].typeId = objTypeSpecInfo[i]->GetTypeId();
        jitData[i].id = objTypeSpecInfo[i]->GetObjTypeSpecFldId();
        jitData[i].flags = objTypeSpecInfo[i]->GetFlags();
        jitData[i].slotIndex = objTypeSpecInfo[i]->GetSlotIndex();
        jitData[i].fixedFieldCount = objTypeSpecInfo[i]->GetFixedFieldCount();

        if (objTypeSpecInfo[i]->HasInitialType())
        {TRACE_IT(9702);
            jitData[i].initialType = AnewStructZ(alloc, TypeIDL);
            JITType::BuildFromJsType(objTypeSpecInfo[i]->GetInitialType(), (JITType*)jitData[i].initialType);
        }

        if (objTypeSpecInfo[i]->GetCtorCache() != nullptr)
        {TRACE_IT(9703);
            jitData[i].ctorCache = objTypeSpecInfo[i]->GetCtorCache()->GetData();
        }

        CompileAssert(sizeof(Js::EquivalentTypeSet) == sizeof(EquivalentTypeSetIDL));
        Js::EquivalentTypeSet * equivTypeSet = objTypeSpecInfo[i]->GetEquivalentTypeSet();
        if (equivTypeSet != nullptr)
        {TRACE_IT(9704);
            jitData[i].typeSet = (EquivalentTypeSetIDL*)equivTypeSet;
        }

        jitData[i].fixedFieldInfoArraySize = jitData[i].fixedFieldCount;
        if (jitData[i].fixedFieldInfoArraySize == 0)
        {TRACE_IT(9705);
            jitData[i].fixedFieldInfoArraySize = 1;
        }
        jitData[i].fixedFieldInfoArray = AnewArrayZ(alloc, FixedFieldIDL, jitData[i].fixedFieldInfoArraySize);
        Js::FixedFieldInfo * ffInfo = objTypeSpecInfo[i]->GetFixedFieldInfoArray();
        for (uint16 j = 0; j < jitData[i].fixedFieldInfoArraySize; ++j)
        {TRACE_IT(9706);
            jitData[i].fixedFieldInfoArray[j].fieldValue = (intptr_t)PointerValue(ffInfo[j].fieldValue);
            jitData[i].fixedFieldInfoArray[j].nextHasSameFixedField = ffInfo[j].nextHasSameFixedField;
            if (ffInfo[j].fieldValue != nullptr && Js::JavascriptFunction::Is(ffInfo[j].fieldValue))
            {TRACE_IT(9707);
                Js::JavascriptFunction * funcObj = Js::JavascriptFunction::FromVar(ffInfo[j].fieldValue);
                jitData[i].fixedFieldInfoArray[j].valueType = ValueType::FromObject(funcObj).GetRawData();
                jitData[i].fixedFieldInfoArray[j].funcInfoAddr = (intptr_t)funcObj->GetFunctionInfo();
                jitData[i].fixedFieldInfoArray[j].isClassCtor = funcObj->GetFunctionInfo()->IsClassConstructor();
                jitData[i].fixedFieldInfoArray[j].localFuncId = (intptr_t)funcObj->GetFunctionInfo()->GetLocalFunctionId();
                if (Js::ScriptFunction::Test(funcObj))
                {TRACE_IT(9708);
                    jitData[i].fixedFieldInfoArray[j].environmentAddr = (intptr_t)Js::ScriptFunction::FromVar(funcObj)->GetEnvironment();
                }
            }
            if (ffInfo[j].type != nullptr)
            {TRACE_IT(9709);
                jitData[i].fixedFieldInfoArray[j].type = AnewStructZ(alloc, TypeIDL);
                // TODO: OOP JIT, maybe type should be out of line? might not save anything on x64 though
                JITType::BuildFromJsType(ffInfo[j].type, (JITType*)jitData[i].fixedFieldInfoArray[j].type);
            }
        }
    }
}

Js::ObjTypeSpecFldInfoFlags
JITObjTypeSpecFldInfo::GetFlags() const
{TRACE_IT(9710);
    return (Js::ObjTypeSpecFldInfoFlags)m_data.flags;
}
