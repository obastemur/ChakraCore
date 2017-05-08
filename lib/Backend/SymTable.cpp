//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"


void
SymTable::Init(Func* func)
{TRACE_IT(15745);
    m_func = func;
    m_propertyMap = JitAnew(func->m_alloc, PropertyMap, func->m_alloc);
    m_propertyEquivBvMap = JitAnew(func->m_alloc, PropertyEquivBvMap, func->m_alloc);
}

///----------------------------------------------------------------------------
///
/// SymTable::Add
///
///     Add newSym to this symbol table.
///
///----------------------------------------------------------------------------

void
SymTable::Add(Sym * newSym)
{TRACE_IT(15746);
    int hash;

    newSym->m_id += this->m_IDAdjustment;

    hash = this->Hash(newSym->m_id);

    AssertMsg(newSym->m_next == NULL, "Error inserting a symbol in the SymTable with a non-NULL next ptr.");

    newSym->m_next = m_table[hash];
    m_table[hash] = newSym;

    if (newSym->IsPropertySym())
    {TRACE_IT(15747);
        PropertySym * propertySym = newSym->AsPropertySym();
        if (propertySym->m_fieldKind != PropertyKindWriteGuard)
        {TRACE_IT(15748);
            SymIdPropIdPair pair(propertySym->m_stackSym->m_id, propertySym->m_propertyId);
#if DBG
            PropertySym * foundPropertySym;
            Assert(!this->m_propertyMap->TryGetValue(pair, &foundPropertySym));
#endif
            this->m_propertyMap->Add(pair, propertySym);
        }

        if (propertySym->m_fieldKind == PropertyKindSlots ||
            propertySym->m_fieldKind == PropertyKindData ||
            propertySym->m_fieldKind == PropertyKindWriteGuard)
        {TRACE_IT(15749);
            BVSparse<JitArenaAllocator> *bvEquivSet;

            if (!this->m_propertyEquivBvMap->TryGetValue(propertySym->m_propertyId, &bvEquivSet))
            {TRACE_IT(15750);
                bvEquivSet = JitAnew(this->m_func->m_alloc, BVSparse<JitArenaAllocator>, this->m_func->m_alloc);
                this->m_propertyEquivBvMap->Add(propertySym->m_propertyId, bvEquivSet);
            }
            bvEquivSet->Set(propertySym->m_id);
            propertySym->m_propertyEquivSet = bvEquivSet;
        }
    }

    m_func->OnAddSym(newSym);
}

///----------------------------------------------------------------------------
///
/// SymTable::Find
///
///     Find the symbol with the given SymID in this table.
///     Returns NULL if it can't find one.
///
///----------------------------------------------------------------------------

Sym *
SymTable::Find(SymID id) const
{TRACE_IT(15751);
    int hash;

    id += this->m_IDAdjustment;

    hash = this->Hash(id);

    for (Sym *sym = m_table[hash]; sym != NULL; sym = sym->m_next)
    {TRACE_IT(15752);
        if (sym->m_id == id)
        {TRACE_IT(15753);
            return sym;
        }
    }

    return NULL;
}

///----------------------------------------------------------------------------
///
/// SymTable::FindStackSym
///
///     Find the stack symbol with the given SymID in this table.  If the
///     symbol exists, it needs to be a StackSym.
///     Returns NULL if it can't find one.
///
///----------------------------------------------------------------------------

StackSym *
SymTable::FindStackSym(SymID id) const
{TRACE_IT(15754);
    Sym *   sym;

    sym = this->Find(id);

    if (sym)
    {TRACE_IT(15755);
        AssertMsg(sym->IsStackSym(), "Looking for StackSym, found something else");
        return sym->AsStackSym();
    }

    return NULL;
}

///----------------------------------------------------------------------------
///
/// SymTable::FindPropertySym
///
///     Find the field symbol with the given SymID in this table.  If the
///     symbol exists, it needs to be a PropertySym.
///     Returns NULL if it can't find one.
///
///----------------------------------------------------------------------------

PropertySym *
SymTable::FindPropertySym(SymID id) const
{TRACE_IT(15756);
    Sym *   sym;

    sym = this->Find(id);

    if (sym)
    {TRACE_IT(15757);
        AssertMsg(sym->IsPropertySym(), "Looking for PropertySym, found something else");
        return sym->AsPropertySym();
    }

    return NULL;
}

///----------------------------------------------------------------------------
///
/// SymTable::FindPropertySym
///
///     Find the stack symbol with the corresponding to given StackSym and
///     propertyId.
///     Returns NULL if it can't find one.
///
///----------------------------------------------------------------------------

PropertySym *
SymTable::FindPropertySym(SymID stackSymID, int32 propertyId) const
{TRACE_IT(15758);
    PropertySym *  propertySym;

    stackSymID += this->m_IDAdjustment;

    SymIdPropIdPair pair(stackSymID, propertyId);
    if (this->m_propertyMap->TryGetValue(pair, &propertySym))
    {TRACE_IT(15759);
        Assert(propertySym->m_propertyId == propertyId);
        Assert(propertySym->m_stackSym->m_id == stackSymID);

        return propertySym;
    }
    return NULL;
}

///----------------------------------------------------------------------------
///
/// SymTable::Hash
///
///----------------------------------------------------------------------------

int
SymTable::Hash(SymID id) const
{TRACE_IT(15760);
    return (id % k_symTableSize);
}

///----------------------------------------------------------------------------
///
/// SymTable::SetStartingID
///
///----------------------------------------------------------------------------

void
SymTable::SetStartingID(SymID startingID)
{TRACE_IT(15761);
    AssertMsg(m_currentID == 0, "SymTable::SetStarting() can only be called before any symbols are allocated");

    m_currentID = startingID;
}

void
SymTable::IncreaseStartingID(SymID IDIncrease)
{TRACE_IT(15762);
    m_currentID += IDIncrease;
}

///----------------------------------------------------------------------------
///
/// SymTable::NewID
///
///----------------------------------------------------------------------------

SymID
SymTable::NewID()
{TRACE_IT(15763);
    SymID newId = m_currentID++;

    AssertMsg(m_currentID > newId, "Too many symbols: m_currentID overflow!");

    return newId - m_IDAdjustment;
}

///----------------------------------------------------------------------------
///
/// SymTable::GetArgSlotSym
///
///     Get a StackSym to represent this argument slot.
///
///----------------------------------------------------------------------------

StackSym *
SymTable::GetArgSlotSym(Js::ArgSlot argSlotNum)
{TRACE_IT(15764);
    StackSym * argSym;

    argSym = StackSym::NewArgSlotSym(argSlotNum, m_func);
    argSym->m_offset = (argSlotNum - 1) * MachPtr;
    argSym->m_allocated = true;
    return argSym;
}

StackSym *          
SymTable::GetImplicitParam(Js::ArgSlot paramSlotNum)
{TRACE_IT(15765);
    Assert(paramSlotNum - 1 < this->k_maxImplicitParamSlot);

    if (this->m_implicitParams[paramSlotNum - 1])
    {TRACE_IT(15766);
        return this->m_implicitParams[paramSlotNum - 1];
    }
    StackSym *implicitParamSym = StackSym::NewParamSlotSym(paramSlotNum, this->m_func, TyVar);
    implicitParamSym->m_isImplicitParamSym = true;

    this->m_implicitParams[paramSlotNum - 1] = implicitParamSym;

    return implicitParamSym;
}

///----------------------------------------------------------------------------
///
/// SymTable::GetMaxSymID
///
///     Returns the largest SymID in the table at this point.
///
///----------------------------------------------------------------------------

SymID
SymTable::GetMaxSymID() const
{TRACE_IT(15767);
    return m_currentID - 1;
}

///----------------------------------------------------------------------------
///
/// SymTable::ClearStackSymScratch
///
///----------------------------------------------------------------------------

void
SymTable::ClearStackSymScratch()
{
    FOREACH_SYM_IN_TABLE(sym, this)
    {TRACE_IT(15768);
        if (sym->IsStackSym())
        {TRACE_IT(15769);
            memset(&(sym->AsStackSym()->scratch), 0, sizeof(sym->AsStackSym()->scratch));
        }
    } NEXT_SYM_IN_TABLE;
}
