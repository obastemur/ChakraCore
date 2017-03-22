//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITObjTypeSpecFldInfo::JITObjTypeSpecFldInfo(ObjTypeSpecFldIDL * data) :
    m_data(*data)
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 9\n");
    CompileAssert(sizeof(ObjTypeSpecFldIDL) == sizeof(JITObjTypeSpecFldInfo));
}

bool
JITObjTypeSpecFldInfo::UsesAuxSlot() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 15\n");
    return GetFlags().usesAuxSlot;
}

bool
JITObjTypeSpecFldInfo::UsesAccessor() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 21\n");
    return GetFlags().usesAccessor;
}

bool
JITObjTypeSpecFldInfo::IsRootObjectNonConfigurableFieldLoad() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 27\n");
    return GetFlags().isRootObjectNonConfigurableFieldLoad;
}

bool
JITObjTypeSpecFldInfo::HasEquivalentTypeSet() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 33\n");
    return m_data.typeSet != nullptr;
}

bool
JITObjTypeSpecFldInfo::DoesntHaveEquivalence() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 39\n");
    return GetFlags().doesntHaveEquivalence;
}

bool
JITObjTypeSpecFldInfo::IsPoly() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 45\n");
    return GetFlags().isPolymorphic;
}

bool
JITObjTypeSpecFldInfo::IsMono() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 51\n");
    return !IsPoly();
}

bool
JITObjTypeSpecFldInfo::IsBuiltIn() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 57\n");
    return GetFlags().isBuiltIn;
}

bool
JITObjTypeSpecFldInfo::IsLoadedFromProto() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 63\n");
    return GetFlags().isLoadedFromProto;
}

bool
JITObjTypeSpecFldInfo::HasFixedValue() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 69\n");
    return GetFlags().hasFixedValue;
}

bool
JITObjTypeSpecFldInfo::IsBeingStored() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 75\n");
    return GetFlags().isBeingStored;
}

bool
JITObjTypeSpecFldInfo::IsBeingAdded() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 81\n");
    return GetFlags().isBeingAdded;
}

bool
JITObjTypeSpecFldInfo::IsRootObjectNonConfigurableField() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 87\n");
    return GetFlags().isRootObjectNonConfigurableField;
}

bool
JITObjTypeSpecFldInfo::HasInitialType() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 93\n");
    return IsMono() && !IsLoadedFromProto() && m_data.initialType != nullptr;
}

bool
JITObjTypeSpecFldInfo::IsMonoObjTypeSpecCandidate() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 99\n");
    return IsMono();
}

bool
JITObjTypeSpecFldInfo::IsPolyObjTypeSpecCandidate() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 105\n");
    return IsPoly();
}

Js::TypeId
JITObjTypeSpecFldInfo::GetTypeId() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 111\n");
    Assert(m_data.typeId != Js::TypeIds_Limit);
    return (Js::TypeId)m_data.typeId;
}

Js::TypeId
JITObjTypeSpecFldInfo::GetTypeId(uint i) const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 118\n");
    Assert(IsPoly());
    return (Js::TypeId)m_data.fixedFieldInfoArray[i].type->typeId;
}

Js::PropertyId
JITObjTypeSpecFldInfo::GetPropertyId() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 125\n");
    return (Js::PropertyId)m_data.propertyId;
}

uint16
JITObjTypeSpecFldInfo::GetSlotIndex() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 131\n");
    return m_data.slotIndex;
}

uint16
JITObjTypeSpecFldInfo::GetFixedFieldCount() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 137\n");
    return m_data.fixedFieldCount;
}

uint
JITObjTypeSpecFldInfo::GetObjTypeSpecFldId() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 143\n");
    return m_data.id;
}

intptr_t
JITObjTypeSpecFldInfo::GetProtoObject() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 149\n");
    return m_data.protoObjectAddr;
}

intptr_t
JITObjTypeSpecFldInfo::GetFieldValue(uint i) const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 155\n");
    Assert(IsPoly());
    return m_data.fixedFieldInfoArray[i].fieldValue;
}

intptr_t
JITObjTypeSpecFldInfo::GetPropertyGuardValueAddr() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 162\n");
    return m_data.propertyGuardValueAddr;
}

intptr_t
JITObjTypeSpecFldInfo::GetFieldValueAsFixedDataIfAvailable() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 168\n");
    Assert(HasFixedValue() && GetFixedFieldCount() == 1);

    return m_data.fixedFieldInfoArray[0].fieldValue;
}

JITTimeConstructorCache *
JITObjTypeSpecFldInfo::GetCtorCache() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 176\n");
    return (JITTimeConstructorCache*)m_data.ctorCache;
}

Js::EquivalentTypeSet *
JITObjTypeSpecFldInfo::GetEquivalentTypeSet() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 182\n");
    return (Js::EquivalentTypeSet *)m_data.typeSet;
}

JITTypeHolder
JITObjTypeSpecFldInfo::GetType() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 188\n");
    Assert(IsMono());
    return JITTypeHolder((JITType *)m_data.fixedFieldInfoArray[0].type);
}

JITTypeHolder
JITObjTypeSpecFldInfo::GetType(uint i) const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 195\n");
    Assert(IsPoly());
    return JITTypeHolder((JITType *)m_data.fixedFieldInfoArray[i].type);
}

JITTypeHolder
JITObjTypeSpecFldInfo::GetInitialType() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 202\n");
    return JITTypeHolder((JITType *)m_data.initialType);
}

JITTypeHolder
JITObjTypeSpecFldInfo::GetFirstEquivalentType() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 208\n");
    Assert(HasEquivalentTypeSet());
    return JITTypeHolder(GetEquivalentTypeSet()->GetFirstType());
}

void
JITObjTypeSpecFldInfo::SetIsBeingStored(bool value)
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 215\n");
    ((Js::ObjTypeSpecFldInfoFlags*)&m_data.flags)->isBeingStored = value;
}

JITTimeFixedField *
JITObjTypeSpecFldInfo::GetFixedFieldIfAvailableAsFixedFunction()
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 221\n");
    Assert(HasFixedValue());
    if (IsPoly() && DoesntHaveEquivalence())
    {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 224\n");
        return nullptr;
    }

    Assert(m_data.fixedFieldInfoArray);
    if (m_data.fixedFieldInfoArray[0].funcInfoAddr != 0)
    {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 230\n");
        return (JITTimeFixedField *)&m_data.fixedFieldInfoArray[0];
    }
    return nullptr;
}

JITTimeFixedField *
JITObjTypeSpecFldInfo::GetFixedFieldIfAvailableAsFixedFunction(uint i)
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 238\n");
    Assert(HasFixedValue());
    Assert(IsPoly());
    if (m_data.fixedFieldCount > 0 && m_data.fixedFieldInfoArray[i].funcInfoAddr != 0)
    {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 242\n");
        return (JITTimeFixedField *)&m_data.fixedFieldInfoArray[i];
    }
    return nullptr;
}

JITTimeFixedField *
JITObjTypeSpecFldInfo::GetFixedFieldInfoArray()
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 250\n");
    return (JITTimeFixedField*)m_data.fixedFieldInfoArray;
}

/* static */
void
JITObjTypeSpecFldInfo::BuildObjTypeSpecFldInfoArray(
    __in ArenaAllocator * alloc,
    _In_reads_(arrayLength) Field(Js::ObjTypeSpecFldInfo*)* objTypeSpecInfo,
    __in uint arrayLength,
    _Inout_updates_(arrayLength) ObjTypeSpecFldIDL * jitData)
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 261\n");
    for (uint i = 0; i < arrayLength; ++i)
    {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 263\n");
        if (objTypeSpecInfo[i] == nullptr)
        {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 265\n");
            continue;
        }
        jitData[i].inUse = TRUE;
        if (objTypeSpecInfo[i]->IsLoadedFromProto())
        {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 270\n");
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
        {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 282\n");
            jitData[i].initialType = AnewStructZ(alloc, TypeIDL);
            JITType::BuildFromJsType(objTypeSpecInfo[i]->GetInitialType(), (JITType*)jitData[i].initialType);
        }

        if (objTypeSpecInfo[i]->GetCtorCache() != nullptr)
        {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 288\n");
            jitData[i].ctorCache = objTypeSpecInfo[i]->GetCtorCache()->GetData();
        }

        CompileAssert(sizeof(Js::EquivalentTypeSet) == sizeof(EquivalentTypeSetIDL));
        Js::EquivalentTypeSet * equivTypeSet = objTypeSpecInfo[i]->GetEquivalentTypeSet();
        if (equivTypeSet != nullptr)
        {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 295\n");
            jitData[i].typeSet = (EquivalentTypeSetIDL*)equivTypeSet;
        }

        jitData[i].fixedFieldInfoArraySize = jitData[i].fixedFieldCount;
        if (jitData[i].fixedFieldInfoArraySize == 0)
        {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 301\n");
            jitData[i].fixedFieldInfoArraySize = 1;
        }
        jitData[i].fixedFieldInfoArray = AnewArrayZ(alloc, FixedFieldIDL, jitData[i].fixedFieldInfoArraySize);
        Js::FixedFieldInfo * ffInfo = objTypeSpecInfo[i]->GetFixedFieldInfoArray();
        for (uint16 j = 0; j < jitData[i].fixedFieldInfoArraySize; ++j)
        {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 307\n");
            jitData[i].fixedFieldInfoArray[j].fieldValue = (intptr_t)PointerValue(ffInfo[j].fieldValue);
            jitData[i].fixedFieldInfoArray[j].nextHasSameFixedField = ffInfo[j].nextHasSameFixedField;
            if (ffInfo[j].fieldValue != nullptr && Js::JavascriptFunction::Is(ffInfo[j].fieldValue))
            {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 311\n");
                Js::JavascriptFunction * funcObj = Js::JavascriptFunction::FromVar(ffInfo[j].fieldValue);
                jitData[i].fixedFieldInfoArray[j].valueType = ValueType::FromObject(funcObj).GetRawData();
                jitData[i].fixedFieldInfoArray[j].funcInfoAddr = (intptr_t)funcObj->GetFunctionInfo();
                jitData[i].fixedFieldInfoArray[j].isClassCtor = funcObj->GetFunctionInfo()->IsClassConstructor();
                jitData[i].fixedFieldInfoArray[j].localFuncId = (intptr_t)funcObj->GetFunctionInfo()->GetLocalFunctionId();
                if (Js::ScriptFunction::Is(ffInfo[j].fieldValue))
                {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 318\n");
                    jitData[i].fixedFieldInfoArray[j].environmentAddr = (intptr_t)Js::ScriptFunction::FromVar(funcObj)->GetEnvironment();
                }
            }
            if (ffInfo[j].type != nullptr)
            {LOGMEIN("JITObjTypeSpecFldInfo.cpp] 323\n");
                jitData[i].fixedFieldInfoArray[j].type = AnewStructZ(alloc, TypeIDL);
                // TODO: OOP JIT, maybe type should be out of line? might not save anything on x64 though
                JITType::BuildFromJsType(ffInfo[j].type, (JITType*)jitData[i].fixedFieldInfoArray[j].type);
            }
        }
    }
}

Js::ObjTypeSpecFldInfoFlags
JITObjTypeSpecFldInfo::GetFlags() const
{LOGMEIN("JITObjTypeSpecFldInfo.cpp] 334\n");
    return (Js::ObjTypeSpecFldInfoFlags)m_data.flags;
}
