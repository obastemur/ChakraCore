//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeByteCodePch.h"

namespace Js
{
    void ByteCodeReader::Create(FunctionBody * functionRead, uint startOffset /* = 0 */)
    {LOGMEIN("ByteCodeReader.cpp] 10\n");
        Assert(functionRead);
        ByteCodeReader::Create(functionRead, startOffset, /* useOriginalByteCode = */ false);
    }

#if DBG
    void ByteCodeReader::Create(const byte * byteCodeStart, uint startOffset, uint byteCodeLength)
#else
    void ByteCodeReader::Create(const byte * byteCodeStart, uint startOffset)
#endif
    {
        AssertMsg(byteCodeStart != nullptr, "Must have valid byte-code to read");

        m_startLocation = byteCodeStart;
        m_currentLocation = m_startLocation + startOffset;

#if DBG
        m_endLocation = m_startLocation + byteCodeLength;
        Assert(m_currentLocation <= m_endLocation);
#endif
    }

    void ByteCodeReader::Create(FunctionBody* functionRead, uint startOffset, bool useOriginalByteCode)
    {LOGMEIN("ByteCodeReader.cpp] 33\n");
        AssertMsg(functionRead != nullptr, "Must provide valid function to execute");

        ByteBlock * byteCodeBlock = useOriginalByteCode ?
            functionRead->GetOriginalByteCode() :
            functionRead->GetByteCode();

        AssertMsg(byteCodeBlock != nullptr, "Must have valid byte-code to read");

        m_startLocation = byteCodeBlock->GetBuffer();
        m_currentLocation = m_startLocation + startOffset;

#if DBG
        m_endLocation = m_startLocation + byteCodeBlock->GetLength();
        Assert(m_currentLocation <= m_endLocation);
#endif
    }

    template<typename LayoutType>
    const unaligned LayoutType * ByteCodeReader::GetLayout(const byte*& ip)
    {LOGMEIN("ByteCodeReader.cpp] 53\n");
        size_t layoutSize = sizeof(LayoutType);

        AssertMsg((layoutSize > 0) && (layoutSize < 100), "Ensure valid layout size");

        const byte * layoutData = ip;
        ip += layoutSize;
        m_currentLocation = ip;

        Assert(m_currentLocation <= m_endLocation);

        return reinterpret_cast<const unaligned LayoutType *>(layoutData);
    }

    template<>
    const unaligned OpLayoutEmpty * ByteCodeReader::GetLayout<OpLayoutEmpty>(const byte*& ip)
    {LOGMEIN("ByteCodeReader.cpp] 69\n");
        m_currentLocation = ip;
        return nullptr;
    }

    OpCode ByteCodeReader::ReadOp(const byte *&ip, LayoutSize& layoutSize) const
    {LOGMEIN("ByteCodeReader.cpp] 75\n");
        // Return current location and advance past data.

        Assert(ip < m_endLocation);
        OpCode op = (OpCode)*ip++;

        if (!OpCodeUtil::IsPrefixOpcode(op))
        {LOGMEIN("ByteCodeReader.cpp] 82\n");
            layoutSize = SmallLayout;
            return op;
        }

        return ReadPrefixedOp(ip, layoutSize, op);
    }

    OpCode ByteCodeReader::ReadPrefixedOp(const byte *&ip, LayoutSize& layoutSize, OpCode prefix) const
    {LOGMEIN("ByteCodeReader.cpp] 91\n");
        Assert(ip < m_endLocation);
        const uint16 nPrefixes = (uint16)Js::OpCode::Nop / LayoutCount;

        // Make sure the assumption made for the order of the prefix are right
        CompileAssert((uint16)Js::OpCode::ExtendedOpcodePrefix / nPrefixes == SmallLayout);
        CompileAssert((uint16)Js::OpCode::MediumLayoutPrefix / nPrefixes == MediumLayout);
        CompileAssert((uint16)Js::OpCode::ExtendedMediumLayoutPrefix / nPrefixes == MediumLayout);
        CompileAssert((uint16)Js::OpCode::LargeLayoutPrefix / nPrefixes == LargeLayout);
        CompileAssert((uint16)Js::OpCode::ExtendedLargeLayoutPrefix / nPrefixes == LargeLayout);

        CompileAssert((uint16)Js::OpCode::MediumLayoutPrefix % nPrefixes == 0);
        CompileAssert((uint16)Js::OpCode::LargeLayoutPrefix % nPrefixes == 0);
        CompileAssert((uint16)Js::OpCode::ExtendedOpcodePrefix % nPrefixes == 1);
        CompileAssert((uint16)Js::OpCode::ExtendedMediumLayoutPrefix % nPrefixes == 1);
        CompileAssert((uint16)Js::OpCode::ExtendedLargeLayoutPrefix % nPrefixes == 1);

        uint16 shortPrefix = (uint16)prefix;
        layoutSize = (LayoutSize)(shortPrefix / nPrefixes);
        return shortPrefix & 1 ? ReadExtOp(ip) : ReadByteOp(ip);
    }

    OpCode ByteCodeReader::ReadOp(LayoutSize& layoutSize)
    {LOGMEIN("ByteCodeReader.cpp] 114\n");
        OpCode op = ReadOp(m_currentLocation, layoutSize);
#if ENABLE_NATIVE_CODEGEN
        Assert(!OpCodeAttr::BackEndOnly(op));
#endif
        return op;
    }

    OpCodeAsmJs ByteCodeReader::ReadAsmJsOp(LayoutSize& layoutSize)
    {LOGMEIN("ByteCodeReader.cpp] 123\n");
        OpCode op = ReadOp(m_currentLocation, layoutSize);

        return (OpCodeAsmJs)op;
    }

    OpCode ByteCodeReader::ReadPrefixedOp(LayoutSize& layoutSize, OpCode prefix)
    {LOGMEIN("ByteCodeReader.cpp] 130\n");
        Assert(OpCodeUtil::IsPrefixOpcode(prefix));
        return ReadPrefixedOp(m_currentLocation, layoutSize, prefix);
    }
    OpCode ByteCodeReader::PeekOp(LayoutSize& layoutSize) const
    {LOGMEIN("ByteCodeReader.cpp] 135\n");
        const byte * ip = m_currentLocation;
        return ReadOp(ip, layoutSize);
    }

    OpCode ByteCodeReader::PeekOp(const byte * ip, LayoutSize& layoutSize)
    {LOGMEIN("ByteCodeReader.cpp] 141\n");
        return ReadOp(ip, layoutSize);
    }

    OpCode ByteCodeReader::ReadByteOp(const byte*& ip)
    {LOGMEIN("ByteCodeReader.cpp] 146\n");
        return (OpCode)*ip++;
    }

    OpCode ByteCodeReader::PeekByteOp(const byte * ip)
    {LOGMEIN("ByteCodeReader.cpp] 151\n");
        return ReadByteOp(ip);
    }

    OpCode ByteCodeReader::ReadExtOp(const byte*& ip)
    {LOGMEIN("ByteCodeReader.cpp] 156\n");
        uint16*& extIp = (uint16*&)ip;
        return (OpCode)*extIp++;
    }

    OpCode ByteCodeReader::PeekExtOp(const byte * ip)
    {LOGMEIN("ByteCodeReader.cpp] 162\n");
        return ReadExtOp(ip);
    }

    const byte* ByteCodeReader::GetIP()
    {LOGMEIN("ByteCodeReader.cpp] 167\n");
        return m_currentLocation;
    }

    void ByteCodeReader::SetIP(const byte *const ip)
    {LOGMEIN("ByteCodeReader.cpp] 172\n");
        Assert(ip >= m_startLocation);
        Assert(ip < m_endLocation);

        m_currentLocation = ip;
    }

    // Define reading functions
#define LAYOUT_TYPE(layout) \
    const unaligned OpLayout##layout * ByteCodeReader::layout() \
    {LOGMEIN("ByteCodeReader.cpp] 182\n"); \
        return GetLayout<OpLayout##layout>(); \
    } \
    const unaligned OpLayout##layout * ByteCodeReader::layout(const byte*& ip) \
    {LOGMEIN("ByteCodeReader.cpp] 186\n"); \
        return GetLayout<OpLayout##layout>(ip); \
    }
#include "LayoutTypes.h"
    // Define reading functions
#define LAYOUT_TYPE(layout) \
    const unaligned OpLayout##layout * ByteCodeReader::layout() \
    {LOGMEIN("ByteCodeReader.cpp] 193\n"); \
        return GetLayout<OpLayout##layout>(); \
    } \
    const unaligned OpLayout##layout * ByteCodeReader::layout(const byte*& ip) \
    {LOGMEIN("ByteCodeReader.cpp] 197\n"); \
        return GetLayout<OpLayout##layout>(ip); \
    }
#define EXCLUDE_DUP_LAYOUT
#include "LayoutTypesAsmJs.h"

    uint ByteCodeReader::GetCurrentOffset() const
    {LOGMEIN("ByteCodeReader.cpp] 204\n");
        Assert(m_currentLocation >= m_startLocation);
        Assert(m_currentLocation - m_startLocation <= UINT_MAX);
        return (uint)(m_currentLocation - m_startLocation);
    }

    const byte * ByteCodeReader::SetCurrentOffset(int byteOffset)
    {LOGMEIN("ByteCodeReader.cpp] 211\n");
        const byte * ip = m_startLocation + byteOffset;
        Assert(ip < m_endLocation);
        m_currentLocation = ip;
        return ip;
    }

    const byte * ByteCodeReader::SetCurrentRelativeOffset(const byte * ip, int byteOffset)
    {LOGMEIN("ByteCodeReader.cpp] 219\n");
        Assert(ip < m_endLocation);
        const byte * targetip = ip + byteOffset;
        Assert(targetip < m_endLocation);
        m_currentLocation = targetip;
        return targetip;
    }

    template <typename T>
    AuxArray<T> const * ByteCodeReader::ReadAuxArray(uint offset, FunctionBody * functionBody)
    {LOGMEIN("ByteCodeReader.cpp] 229\n");
        Js::AuxArray<T> const * auxArray = (Js::AuxArray<T> const *)(functionBody->GetAuxiliaryData()->GetBuffer() + offset);
        Assert(offset + auxArray->GetDataSize() <= functionBody->GetAuxiliaryData()->GetLength());
        return auxArray;
    }

    template <typename T>
    AuxArray<T> const * ByteCodeReader::ReadAuxArrayWithLock(uint offset, FunctionBody * functionBody)
    {LOGMEIN("ByteCodeReader.cpp] 237\n");
        Js::AuxArray<T> const * auxArray = (Js::AuxArray<T> const *)(functionBody->GetAuxiliaryDataWithLock()->GetBuffer() + offset);
        Assert(offset + auxArray->GetDataSize() <= functionBody->GetAuxiliaryDataWithLock()->GetLength());
        return auxArray;
    }

    // explicit instantiations
    template AuxArray<Var> const * ByteCodeReader::ReadAuxArray<Var>(uint offset, FunctionBody * functionBody);
    template AuxArray<int32> const * ByteCodeReader::ReadAuxArray<int32>(uint offset, FunctionBody * functionBody);
    template AuxArray<uint32> const * ByteCodeReader::ReadAuxArray<uint32>(uint offset, FunctionBody * functionBody);
    template AuxArray<double> const * ByteCodeReader::ReadAuxArray<double>(uint offset, FunctionBody * functionBody);
    template AuxArray<FuncInfoEntry> const * ByteCodeReader::ReadAuxArray<FuncInfoEntry>(uint offset, FunctionBody * functionBody);
    template AuxArray<Var> const * ByteCodeReader::ReadAuxArrayWithLock<Var>(uint offset, FunctionBody * functionBody);
    template AuxArray<int32> const * ByteCodeReader::ReadAuxArrayWithLock<int32>(uint offset, FunctionBody * functionBody);
    template AuxArray<uint32> const * ByteCodeReader::ReadAuxArrayWithLock<uint32>(uint offset, FunctionBody * functionBody);
    template AuxArray<double> const * ByteCodeReader::ReadAuxArrayWithLock<double>(uint offset, FunctionBody * functionBody);
    template AuxArray<FuncInfoEntry> const * ByteCodeReader::ReadAuxArrayWithLock<FuncInfoEntry>(uint offset, FunctionBody * functionBody);
    template const unaligned Js::OpLayoutT_Unsigned1<Js::LayoutSizePolicy<(Js::LayoutSize)0> >* Js::ByteCodeReader::GetLayout<Js::OpLayoutT_Unsigned1<Js::LayoutSizePolicy<(Js::LayoutSize)0> > >(const byte*&);
    template const unaligned Js::OpLayoutT_Unsigned1<Js::LayoutSizePolicy<(Js::LayoutSize)1> >* Js::ByteCodeReader::GetLayout<Js::OpLayoutT_Unsigned1<Js::LayoutSizePolicy<(Js::LayoutSize)1> > >(const byte*&);
    template const unaligned Js::OpLayoutT_Unsigned1<Js::LayoutSizePolicy<(Js::LayoutSize)2> >* Js::ByteCodeReader::GetLayout<Js::OpLayoutT_Unsigned1<Js::LayoutSizePolicy<(Js::LayoutSize)2> > >(const byte*&);

    const Js::PropertyIdArray * ByteCodeReader::ReadPropertyIdArray(uint offset, FunctionBody * functionBody)
    {LOGMEIN("ByteCodeReader.cpp] 259\n");
        Js::PropertyIdArray const * propIds = (Js::PropertyIdArray const *)(functionBody->GetAuxiliaryData()->GetBuffer() + offset);
        Assert(offset + propIds->GetDataSize() <= functionBody->GetAuxiliaryData()->GetLength());
        return propIds;
    }

    const Js::PropertyIdArray * ByteCodeReader::ReadPropertyIdArrayWithLock(uint offset, FunctionBody * functionBody)
    {LOGMEIN("ByteCodeReader.cpp] 266\n");
        Js::PropertyIdArray const * propIds = (Js::PropertyIdArray const *)(functionBody->GetAuxiliaryDataWithLock()->GetBuffer() + offset);
        Assert(offset + propIds->GetDataSize() <= functionBody->GetAuxiliaryDataWithLock()->GetLength());
        return propIds;
    }

    size_t VarArrayVarCount::GetDataSize() const
    {LOGMEIN("ByteCodeReader.cpp] 273\n");
        return sizeof(VarArrayVarCount) + sizeof(Var) * TaggedInt::ToInt32(count);
    }

    void VarArrayVarCount::SetCount(uint count)
    {LOGMEIN("ByteCodeReader.cpp] 278\n");
        this->count = Js::TaggedInt::ToVarUnchecked(count);
    }

    const Js::VarArrayVarCount * ByteCodeReader::ReadVarArrayVarCount(uint offset, FunctionBody * functionBody)
    {LOGMEIN("ByteCodeReader.cpp] 283\n");
        Js::ByteBlock* auxiliaryContextData = functionBody->GetAuxiliaryContextData();
        Js::VarArrayVarCount const * varArray = (Js::VarArrayVarCount const *)(auxiliaryContextData->GetBuffer() + offset);
        Assert(offset + varArray->GetDataSize() <= auxiliaryContextData->GetLength());
        return varArray;
    }

    const Js::VarArrayVarCount * ByteCodeReader::ReadVarArrayVarCountWithLock(uint offset, FunctionBody * functionBody)
    {LOGMEIN("ByteCodeReader.cpp] 291\n");
        Js::ByteBlock* auxiliaryContextData = functionBody->GetAuxiliaryContextDataWithLock();
        Js::VarArrayVarCount const * varArray = (Js::VarArrayVarCount const *)(auxiliaryContextData->GetBuffer() + offset);
        Assert(offset + varArray->GetDataSize() <= auxiliaryContextData->GetLength());
        return varArray;
    }

#if DBG_DUMP
    byte ByteCodeReader::GetRawByte(int i)
    {LOGMEIN("ByteCodeReader.cpp] 300\n");
        return m_startLocation[i];
    }
#endif
} // namespace Js
