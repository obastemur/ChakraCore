//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


///----------------------------------------------------------------------------
///
/// Sym::IsStackSym
///
///----------------------------------------------------------------------------

inline bool
Sym::IsStackSym() const
{TRACE_IT(15737);
    return m_kind == SymKindStack;
}

///----------------------------------------------------------------------------
///
/// Sym::AsStackSym
///
///     Use this symbol as a StackSym.
///
///----------------------------------------------------------------------------

inline StackSym *
Sym::AsStackSym()
{TRACE_IT(15738);
    AssertMsg(this->IsStackSym(), "Bad call to AsStackSym()");

    return reinterpret_cast<StackSym *>(this);
}

///----------------------------------------------------------------------------
///
/// Sym::IsStackSym
///
///----------------------------------------------------------------------------

inline bool
Sym::IsPropertySym() const
{TRACE_IT(15739);
    return m_kind == SymKindProperty;
}

///----------------------------------------------------------------------------
///
/// Sym::AsPropertySym
///
///     Use this symbol as a PropertySym.
///
///----------------------------------------------------------------------------

inline PropertySym *
Sym::AsPropertySym()
{TRACE_IT(15740);
    AssertMsg(this->IsPropertySym(), "Bad call to AsPropertySym()");

    return reinterpret_cast<PropertySym *>(this);
}

///----------------------------------------------------------------------------
///
/// Sym::IsArgSlotSym
///
///----------------------------------------------------------------------------

inline bool
StackSym::IsArgSlotSym() const
{TRACE_IT(15741);
    if(m_isArgSlotSym)
    {TRACE_IT(15742);
        Assert(this->m_slotNum != 0);
    }
    return m_isArgSlotSym;
}

///----------------------------------------------------------------------------
///
/// Sym::IsParamSlotSym
///
///----------------------------------------------------------------------------

inline bool
StackSym::IsParamSlotSym() const
{TRACE_IT(15743);
    return m_isParamSym;
}

///----------------------------------------------------------------------------
///
/// Sym::IsAllocated
///
///----------------------------------------------------------------------------

inline bool
StackSym::IsAllocated() const
{TRACE_IT(15744);
    return m_allocated;
}
