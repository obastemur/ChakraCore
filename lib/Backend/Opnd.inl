//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace IR {

///----------------------------------------------------------------------------
///
/// Opnd::Use
///
///     If this operand is not inUse, use it.  Otherwise, make a copy.
///
///----------------------------------------------------------------------------

inline Opnd *
Opnd::Use(Func *func)
{
    AssertMsg(!isDeleted, "Using deleted operand");
    if (!m_inUse)
    {TRACE_IT(14818);
        m_inUse = true;
        return this;
    }

    Opnd * newOpnd = this->Copy(func);
    newOpnd->m_inUse = true;

    return newOpnd;
}

///----------------------------------------------------------------------------
///
/// Opnd::UnUse
///
///----------------------------------------------------------------------------

inline void
Opnd::UnUse()
{
    AssertMsg(m_inUse, "Expected inUse to be set...");
    m_inUse = false;
}

///----------------------------------------------------------------------------
///
/// Opnd::IsSymOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsSymOpnd() const
{TRACE_IT(14819);
    return GetKind() == OpndKindSym;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsSymOpnd
///
///     Use this opnd as a SymOpnd.
///
///----------------------------------------------------------------------------

inline SymOpnd *
Opnd::AsSymOpnd()
{TRACE_IT(14820);
    AssertMsg(this->IsSymOpnd(), "Bad call to AsSymOpnd()");

    return reinterpret_cast<SymOpnd *>(this);
}

inline PropertySymOpnd *
Opnd::AsPropertySymOpnd()
{TRACE_IT(14821);
    AssertMsg(this->IsSymOpnd() && this->AsSymOpnd()->IsPropertySymOpnd(), "Bad call to AsPropertySymOpnd()");

    return reinterpret_cast<PropertySymOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsRegOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsRegOpnd() const
{TRACE_IT(14822);
    return GetKind() == OpndKindReg;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsRegOpnd
///
///     Use this opnd as a RegOpnd.
///
///----------------------------------------------------------------------------

inline const RegOpnd *
Opnd::AsRegOpnd() const
{TRACE_IT(14823);
    AssertMsg(this->IsRegOpnd(), "Bad call to AsRegOpnd()");

    return reinterpret_cast<const RegOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::AsRegOpnd
///
///     Use this opnd as a RegOpnd.
///
///----------------------------------------------------------------------------

inline RegOpnd *
Opnd::AsRegOpnd()
{TRACE_IT(14824);
    AssertMsg(this->IsRegOpnd(), "Bad call to AsRegOpnd()");

    return reinterpret_cast<RegOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsRegBVOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsRegBVOpnd() const
{TRACE_IT(14825);
    return GetKind() == OpndKindRegBV;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsRegBVOpnd
///
///     Use this opnd as a RegBVOpnd.
///
///----------------------------------------------------------------------------
inline RegBVOpnd *
Opnd::AsRegBVOpnd()
{TRACE_IT(14826);
    AssertMsg(this->IsRegBVOpnd(), "Bad call to AsRegOpnd()");

    return reinterpret_cast<RegBVOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsIntConstOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsIntConstOpnd() const
{TRACE_IT(14827);
    return GetKind() == OpndKindIntConst;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsIntConstOpnd
///
///     Use this opnd as an IntConstOpnd.
///
///----------------------------------------------------------------------------

inline IntConstOpnd *
Opnd::AsIntConstOpnd()
{TRACE_IT(14828);
    AssertMsg(this->IsIntConstOpnd(), "Bad call to AsIntConstOpnd()");

    return reinterpret_cast<IntConstOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsInt64ConstOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsInt64ConstOpnd() const
{TRACE_IT(14829);
    return GetKind() == OpndKindInt64Const;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsInt64ConstOpnd
///
///     Use this opnd as an Int64ConstOpnd.
///
///----------------------------------------------------------------------------

inline Int64ConstOpnd *
Opnd::AsInt64ConstOpnd()
{TRACE_IT(14830);
    AssertMsg(this->IsInt64ConstOpnd(), "Bad call to AsInt64ConstOpnd()");
    return reinterpret_cast<Int64ConstOpnd *>(this);
}


///----------------------------------------------------------------------------
///
/// Opnd::IsFloatConstOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsFloatConstOpnd() const
{TRACE_IT(14831);
    return GetKind() == OpndKindFloatConst;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsFloatConstOpnd
///
///     Use this opnd as a FloatConstOpnd.
///
///----------------------------------------------------------------------------

inline FloatConstOpnd *
Opnd::AsFloatConstOpnd()
{TRACE_IT(14832);
    AssertMsg(this->IsFloatConstOpnd(), "Bad call to AsFloatConstOpnd()");

    return reinterpret_cast<FloatConstOpnd *>(this);
}

inline bool
Opnd::IsSimd128ConstOpnd() const
{TRACE_IT(14833);
    return GetKind() == OpndKindSimd128Const;
}

inline Simd128ConstOpnd *
Opnd::AsSimd128ConstOpnd()
{TRACE_IT(14834);
    AssertMsg(this->IsSimd128ConstOpnd(), "Bad call to AsSimd128ConstOpnd()");

    return reinterpret_cast<Simd128ConstOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsHelperCallOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsHelperCallOpnd() const
{TRACE_IT(14835);
    return GetKind() == OpndKindHelperCall;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsHelperCallOpnd
///
///     Use this opnd as a HelperCallOpnd.
///
///----------------------------------------------------------------------------

inline HelperCallOpnd *
Opnd::AsHelperCallOpnd()
{TRACE_IT(14836);
    AssertMsg(this->IsHelperCallOpnd(), "Bad call to AsHelperCallOpnd()");

    return reinterpret_cast<HelperCallOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsAddrOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsAddrOpnd() const
{TRACE_IT(14837);
    return GetKind() == OpndKindAddr;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsAddrOpnd
///
///     Use this opnd as an AddrOpnd.
///
///----------------------------------------------------------------------------

inline AddrOpnd *
Opnd::AsAddrOpnd()
{TRACE_IT(14838);
    AssertMsg(this->IsAddrOpnd(), "Bad call to AsAddrOpnd()");

    return reinterpret_cast<AddrOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsIndirOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsIndirOpnd() const
{TRACE_IT(14839);
    return GetKind() == OpndKindIndir;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsIndirOpnd
///
///     Use this opnd as an IndirOpnd.
///
///----------------------------------------------------------------------------

inline IndirOpnd *
Opnd::AsIndirOpnd()
{TRACE_IT(14840);
    AssertMsg(this->IsIndirOpnd(), "Bad call to AsIndirOpnd()");

    return reinterpret_cast<IndirOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsMemRefOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsMemRefOpnd() const
{TRACE_IT(14841);
    return GetKind() == OpndKindMemRef;
}

///----------------------------------------------------------------------------
///
/// Opnd::AsMemRefOpnd
///
///     Use this opnd as a MemRefOpnd.
///
///----------------------------------------------------------------------------

inline MemRefOpnd *
Opnd::AsMemRefOpnd()
{TRACE_IT(14842);
    AssertMsg(this->IsMemRefOpnd(), "Bad call to AsMemRefOpnd()");

    return reinterpret_cast<MemRefOpnd *>(this);
}

inline bool
Opnd::IsLabelOpnd() const
{TRACE_IT(14843);
    return GetKind() == OpndKindLabel;
}

inline LabelOpnd *
Opnd::AsLabelOpnd()
{TRACE_IT(14844);
    AssertMsg(this->IsLabelOpnd(), "Bad call to AsLabelOpnd()");

    return reinterpret_cast<LabelOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// Opnd::IsImmediateOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsImmediateOpnd() const
{TRACE_IT(14845);
    return this->IsIntConstOpnd() || this->IsInt64ConstOpnd() || this->IsAddrOpnd() || this->IsHelperCallOpnd();
}


inline bool Opnd::IsMemoryOpnd() const
{TRACE_IT(14846);
    switch(GetKind())
    {
        case OpndKindSym:
        case OpndKindIndir:
        case OpndKindMemRef:
            return true;
    }
    return false;
}

///----------------------------------------------------------------------------
///
/// Opnd::IsConstOpnd
///
///----------------------------------------------------------------------------

inline bool
Opnd::IsConstOpnd() const
{TRACE_IT(14847);
    bool result =  this->IsImmediateOpnd() || this->IsFloatConstOpnd();

    result = result || this->IsSimd128ConstOpnd();
    return result;
}

///----------------------------------------------------------------------------
///
/// RegOpnd::AsArrayRegOpnd
///
///----------------------------------------------------------------------------

inline ArrayRegOpnd *RegOpnd::AsArrayRegOpnd()
{TRACE_IT(14848);
    Assert(IsArrayRegOpnd());
    return static_cast<ArrayRegOpnd *>(this);
}

///----------------------------------------------------------------------------
///
/// RegOpnd::GetReg
///
///----------------------------------------------------------------------------

inline RegNum
RegOpnd::GetReg() const
{TRACE_IT(14849);
    return m_reg;
}

///----------------------------------------------------------------------------
///
/// RegOpnd::SetReg
///
///----------------------------------------------------------------------------

inline void
RegOpnd::SetReg(RegNum reg)
{TRACE_IT(14850);
    m_reg = reg;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::GetBaseOpnd
///
///----------------------------------------------------------------------------

inline RegOpnd *
IndirOpnd::GetBaseOpnd() const
{TRACE_IT(14851);
    return this->m_baseOpnd;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::GetIndexOpnd
///
///----------------------------------------------------------------------------

inline RegOpnd *
IndirOpnd::GetIndexOpnd()
{TRACE_IT(14852);
    return m_indexOpnd;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::GetOffset
///
///----------------------------------------------------------------------------

inline int32
IndirOpnd::GetOffset() const
{TRACE_IT(14853);
    return m_offset;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::SetOffset
///
///----------------------------------------------------------------------------

inline void
IndirOpnd::SetOffset(int32 offset, bool dontEncode /* = false */)
{TRACE_IT(14854);
    m_offset = offset;
    m_dontEncode = dontEncode;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::GetScale
///
///----------------------------------------------------------------------------

inline byte
IndirOpnd::GetScale() const
{TRACE_IT(14855);
    return m_scale;
}

///----------------------------------------------------------------------------
///
/// IndirOpnd::SetScale
///
///----------------------------------------------------------------------------

inline void
IndirOpnd::SetScale(byte scale)
{TRACE_IT(14856);
    m_scale = scale;
}

///----------------------------------------------------------------------------
///
/// MemRefOpnd::GetMemLoc
///
///----------------------------------------------------------------------------

inline intptr_t
MemRefOpnd::GetMemLoc() const
{TRACE_IT(14857);
    return m_memLoc;
}

///----------------------------------------------------------------------------
///
/// MemRefOpnd::SetMemLoc
///
///----------------------------------------------------------------------------

inline void
MemRefOpnd::SetMemLoc(intptr_t pMemLoc)
{TRACE_IT(14858);
    m_memLoc = pMemLoc;
}

inline LabelInstr *
LabelOpnd::GetLabel() const
{TRACE_IT(14859);
    return m_label;
}

inline void
LabelOpnd::SetLabel(LabelInstr * labelInstr)
{TRACE_IT(14860);
    m_label = labelInstr;
}

inline BVUnit32
RegBVOpnd::GetValue() const
{TRACE_IT(14861);
    return m_value;
}

} // namespace IR
