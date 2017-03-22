//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"


void
SymTable::Init(Func* func)
{LOGMEIN("SymTable.cpp] 9\n");
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
{LOGMEIN("SymTable.cpp] 25\n");
    int hash;

    newSym->m_id += this->m_IDAdjustment;

    hash = this->Hash(newSym->m_id);

    AssertMsg(newSym->m_next == NULL, "Error inserting a symbol in the SymTable with a non-NULL next ptr.");

    newSym->m_next = m_table[hash];
    m_table[hash] = newSym;

    if (newSym->IsPropertySym())
    {LOGMEIN("SymTable.cpp] 38\n");
        PropertySym * propertySym = newSym->AsPropertySym();
        if (propertySym->m_fieldKind != PropertyKindWriteGuard)
        {LOGMEIN("SymTable.cpp] 41\n");
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
        {LOGMEIN("SymTable.cpp] 53\n");
            BVSparse<JitArenaAllocator> *bvEquivSet;

            if (!this->m_propertyEquivBvMap->TryGetValue(propertySym->m_propertyId, &bvEquivSet))
            {LOGMEIN("SymTable.cpp] 57\n");
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
{LOGMEIN("SymTable.cpp] 80\n");
    int hash;

    id += this->m_IDAdjustment;

    hash = this->Hash(id);

    for (Sym *sym = m_table[hash]; sym != NULL; sym = sym->m_next)
    {LOGMEIN("SymTable.cpp] 88\n");
        if (sym->m_id == id)
        {LOGMEIN("SymTable.cpp] 90\n");
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
{LOGMEIN("SymTable.cpp] 110\n");
    Sym *   sym;

    sym = this->Find(id);

    if (sym)
    {LOGMEIN("SymTable.cpp] 116\n");
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
{LOGMEIN("SymTable.cpp] 136\n");
    Sym *   sym;

    sym = this->Find(id);

    if (sym)
    {LOGMEIN("SymTable.cpp] 142\n");
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
{LOGMEIN("SymTable.cpp] 162\n");
    PropertySym *  propertySym;

    stackSymID += this->m_IDAdjustment;

    SymIdPropIdPair pair(stackSymID, propertyId);
    if (this->m_propertyMap->TryGetValue(pair, &propertySym))
    {LOGMEIN("SymTable.cpp] 169\n");
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
{LOGMEIN("SymTable.cpp] 186\n");
    return (id % k_symTableSize);
}

///----------------------------------------------------------------------------
///
/// SymTable::SetStartingID
///
///----------------------------------------------------------------------------

void
SymTable::SetStartingID(SymID startingID)
{LOGMEIN("SymTable.cpp] 198\n");
    AssertMsg(m_currentID == 0, "SymTable::SetStarting() can only be called before any symbols are allocated");

    m_currentID = startingID;
}

void
SymTable::IncreaseStartingID(SymID IDIncrease)
{LOGMEIN("SymTable.cpp] 206\n");
    m_currentID += IDIncrease;
}

///----------------------------------------------------------------------------
///
/// SymTable::NewID
///
///----------------------------------------------------------------------------

SymID
SymTable::NewID()
{LOGMEIN("SymTable.cpp] 218\n");
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
{LOGMEIN("SymTable.cpp] 236\n");
    StackSym * argSym;

    argSym = StackSym::NewArgSlotSym(argSlotNum, m_func);
    argSym->m_offset = (argSlotNum - 1) * MachPtr;
    argSym->m_allocated = true;
    return argSym;
}

StackSym *          
SymTable::GetImplicitParam(Js::ArgSlot paramSlotNum)
{LOGMEIN("SymTable.cpp] 247\n");
    Assert(paramSlotNum - 1 < this->k_maxImplicitParamSlot);

    if (this->m_implicitParams[paramSlotNum - 1])
    {LOGMEIN("SymTable.cpp] 251\n");
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
{LOGMEIN("SymTable.cpp] 272\n");
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
    {LOGMEIN("SymTable.cpp] 286\n");
        if (sym->IsStackSym())
        {LOGMEIN("SymTable.cpp] 288\n");
            memset(&(sym->AsStackSym()->scratch), 0, sizeof(sym->AsStackSym()->scratch));
        }
    } NEXT_SYM_IN_TABLE;
}
