//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeByteCodePch.h"

#ifdef ASMJS_PLAT

namespace Js
{
    template <LayoutSize layoutSize>
    inline uint ByteCodeWriter::Data::EncodeT(OpCodeAsmJs op, ByteCodeWriter* writer, bool isPatching)
    {TRACE_IT(38244);
        Assert(OpCodeUtilAsmJs::IsValidByteCodeOpcode(op));

        Assert(layoutSize == SmallLayout || OpCodeAttrAsmJs::HasMultiSizeLayout(op));
        // Capture offset before encoding the opcode
        uint offset = GetCurrentOffset();
        EncodeOpCode<layoutSize>((ushort)op, writer);

        if (!isPatching)
        {TRACE_IT(38245);
            writer->IncreaseByteCodeCount();
        }
        return offset;
    }

    template <LayoutSize layoutSize>
    inline uint ByteCodeWriter::Data::EncodeT(OpCodeAsmJs op, const void* rawData, int byteSize, ByteCodeWriter* writer, bool isPatching)
    {TRACE_IT(38246);
        AssertMsg((rawData != nullptr) && (byteSize < 100), "Ensure valid data for opcode");

        uint offset = EncodeT<layoutSize>(op, writer, isPatching);
        Write(rawData, byteSize);
        return offset;
    }

    void AsmJsByteCodeWriter::InitData(ArenaAllocator* alloc, int32 initCodeBufferSize)
    {TRACE_IT(38247);
        ByteCodeWriter::InitData(alloc, initCodeBufferSize);
#ifdef BYTECODE_BRANCH_ISLAND
        useBranchIsland = false;
#endif
    }

#define MULTISIZE_LAYOUT_WRITE(layout, ...) \
    if (!TryWrite##layout<SmallLayoutSizePolicy>(__VA_ARGS__) && !TryWrite##layout<MediumLayoutSizePolicy>(__VA_ARGS__)) \
    {TRACE_IT(38248); \
        bool success = TryWrite##layout<LargeLayoutSizePolicy>(__VA_ARGS__); \
        Assert(success); \
    }

    //////////////////////////////////////////////////////////////////////////
    /// Asm.js Specific functions

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmJsUnsigned1(OpCodeAsmJs op, uint C1)
    {TRACE_IT(38249);
        OpLayoutT_AsmUnsigned1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.C1, C1))
        {TRACE_IT(38250);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg1(OpCodeAsmJs op, RegSlot R0)
    {TRACE_IT(38251);
        OpLayoutT_AsmReg1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0))
        {TRACE_IT(38252);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg2(OpCodeAsmJs op, RegSlot R0, RegSlot R1)
    {TRACE_IT(38253);
        OpLayoutT_AsmReg2<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1))
        {TRACE_IT(38254);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg3(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2)
    {TRACE_IT(38255);
        OpLayoutT_AsmReg3<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2))
        {TRACE_IT(38256);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg4(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3)
    {TRACE_IT(38257);
        OpLayoutT_AsmReg4<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2) && SizePolicy::Assign(layout.R3, R3))
        {TRACE_IT(38258);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg5(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4)
    {TRACE_IT(38259);
        OpLayoutT_AsmReg5<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2) && SizePolicy::Assign(layout.R3, R3) && SizePolicy::Assign(layout.R4, R4))
        {TRACE_IT(38260);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg6(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5)
    {TRACE_IT(38261);
        OpLayoutT_AsmReg6<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2) && SizePolicy::Assign(layout.R3, R3) && SizePolicy::Assign(layout.R4, R4) && SizePolicy::Assign(layout.R5, R5))
        {TRACE_IT(38262);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg7(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6)
    {TRACE_IT(38263);
        OpLayoutT_AsmReg7<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2) && SizePolicy::Assign(layout.R3, R3) && SizePolicy::Assign(layout.R4, R4) && SizePolicy::Assign(layout.R5, R5) && SizePolicy::Assign(layout.R6, R6))
        {TRACE_IT(38264);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg9(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8)
    {TRACE_IT(38265);
        OpLayoutT_AsmReg9<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2) && SizePolicy::Assign(layout.R3, R3) && SizePolicy::Assign(layout.R4, R4) &&
            SizePolicy::Assign(layout.R5, R5) && SizePolicy::Assign(layout.R6, R6) && SizePolicy::Assign(layout.R7, R7) && SizePolicy::Assign(layout.R8, R8))
        {TRACE_IT(38266);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg10(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8, RegSlot R9)
    {TRACE_IT(38267);
        OpLayoutT_AsmReg10<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2) && SizePolicy::Assign(layout.R3, R3) && SizePolicy::Assign(layout.R4, R4) &&
            SizePolicy::Assign(layout.R5, R5) && SizePolicy::Assign(layout.R6, R6) && SizePolicy::Assign(layout.R7, R7) && SizePolicy::Assign(layout.R8, R8) && SizePolicy::Assign(layout.R9, R9))
        {TRACE_IT(38268);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg11(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8, RegSlot R9, RegSlot R10)
    {TRACE_IT(38269);
        OpLayoutT_AsmReg11<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2) && SizePolicy::Assign(layout.R3, R3) && SizePolicy::Assign(layout.R4, R4) &&
            SizePolicy::Assign(layout.R5, R5) && SizePolicy::Assign(layout.R6, R6) && SizePolicy::Assign(layout.R7, R7) && SizePolicy::Assign(layout.R8, R8) && SizePolicy::Assign(layout.R9, R9) &&
            SizePolicy::Assign(layout.R10, R10))
        {TRACE_IT(38270);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg17(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8,
                                                               RegSlot R9, RegSlot R10, RegSlot R11, RegSlot R12, RegSlot R13, RegSlot R14, RegSlot R15, RegSlot R16)
    {TRACE_IT(38271);
        OpLayoutT_AsmReg17<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0)   && SizePolicy::Assign(layout.R1, R1)   && SizePolicy::Assign(layout.R2, R2)   && SizePolicy::Assign(layout.R3, R3)   && SizePolicy::Assign(layout.R4, R4)   &&
            SizePolicy::Assign(layout.R5, R5)   && SizePolicy::Assign(layout.R6, R6)   && SizePolicy::Assign(layout.R7, R7)   && SizePolicy::Assign(layout.R8, R8)   && SizePolicy::Assign(layout.R9, R9)   &&
            SizePolicy::Assign(layout.R10, R10) && SizePolicy::Assign(layout.R11, R11) && SizePolicy::Assign(layout.R12, R12) && SizePolicy::Assign(layout.R13, R13) && SizePolicy::Assign(layout.R14, R14) &&
            SizePolicy::Assign(layout.R15, R15) && SizePolicy::Assign(layout.R16, R16))
        {TRACE_IT(38272);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg18(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8,
        RegSlot R9, RegSlot R10, RegSlot R11, RegSlot R12, RegSlot R13, RegSlot R14, RegSlot R15, RegSlot R16, RegSlot R17)
    {TRACE_IT(38273);
        OpLayoutT_AsmReg18<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2) && SizePolicy::Assign(layout.R3, R3) && SizePolicy::Assign(layout.R4, R4) &&
            SizePolicy::Assign(layout.R5, R5) && SizePolicy::Assign(layout.R6, R6) && SizePolicy::Assign(layout.R7, R7) && SizePolicy::Assign(layout.R8, R8) && SizePolicy::Assign(layout.R9, R9) &&
            SizePolicy::Assign(layout.R10, R10) && SizePolicy::Assign(layout.R11, R11) && SizePolicy::Assign(layout.R12, R12) && SizePolicy::Assign(layout.R13, R13) && SizePolicy::Assign(layout.R14, R14) &&
            SizePolicy::Assign(layout.R15, R15) && SizePolicy::Assign(layout.R16, R16) && SizePolicy::Assign(layout.R17, R17))
        {TRACE_IT(38274);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }
    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmReg19(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8,
        RegSlot R9, RegSlot R10, RegSlot R11, RegSlot R12, RegSlot R13, RegSlot R14, RegSlot R15, RegSlot R16, RegSlot R17, RegSlot R18)
    {TRACE_IT(38275);
        OpLayoutT_AsmReg19<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2) && SizePolicy::Assign(layout.R3, R3) && SizePolicy::Assign(layout.R4, R4) &&
            SizePolicy::Assign(layout.R5, R5) && SizePolicy::Assign(layout.R6, R6) && SizePolicy::Assign(layout.R7, R7) && SizePolicy::Assign(layout.R8, R8) && SizePolicy::Assign(layout.R9, R9) &&
            SizePolicy::Assign(layout.R10, R10) && SizePolicy::Assign(layout.R11, R11) && SizePolicy::Assign(layout.R12, R12) && SizePolicy::Assign(layout.R13, R13) && SizePolicy::Assign(layout.R14, R14) &&
            SizePolicy::Assign(layout.R15, R15) && SizePolicy::Assign(layout.R16, R16) && SizePolicy::Assign(layout.R17, R17) && SizePolicy::Assign(layout.R18, R18))
        {TRACE_IT(38276);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteInt1Const1(OpCodeAsmJs op, RegSlot R0, int C1)
    {TRACE_IT(38277);
        OpLayoutT_Int1Const1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.I0, R0) && SizePolicy::Assign(layout.C1, C1))
        {TRACE_IT(38278);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteReg1IntConst1(OpCodeAsmJs op, RegSlot R0, int C1)
    {TRACE_IT(38279);
        OpLayoutT_Reg1IntConst1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.C1, C1))
        {TRACE_IT(38280);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteLong1Const1(OpCodeAsmJs op, RegSlot R0, int64 C1)
    {TRACE_IT(38281);
        OpLayoutT_Long1Const1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.L0, R0) && SizePolicy::Assign(layout.C1, C1))
        {TRACE_IT(38282);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteFloat1Const1(OpCodeAsmJs op, RegSlot R0, float C1)
    {TRACE_IT(38283);
        OpLayoutT_Float1Const1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.F0, R0) && SizePolicy::Assign(layout.C1, C1))
        {TRACE_IT(38284);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteDouble1Const1(OpCodeAsmJs op, RegSlot R0, double C1)
    {TRACE_IT(38285);
        OpLayoutT_Double1Const1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.D0, R0) && SizePolicy::Assign(layout.C1, C1))
        {TRACE_IT(38286);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmBrReg1(OpCodeAsmJs op, ByteCodeLabel labelID, RegSlot R1)
    {TRACE_IT(38287);
        OpLayoutT_BrInt1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.I1, R1))
        {TRACE_IT(38288);
            size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutT_BrInt1<SizePolicy>) - offsetof(OpLayoutT_BrInt1<SizePolicy>, RelativeJumpOffset);
            layout.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmBrReg2(OpCodeAsmJs op, ByteCodeLabel labelID, RegSlot R1, RegSlot R2)
    {TRACE_IT(38289);
        OpLayoutT_BrInt2<SizePolicy> layout;
        if (SizePolicy::Assign(layout.I1, R1) && SizePolicy::Assign(layout.I2, R2))
        {TRACE_IT(38290);
            size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutT_BrInt2<SizePolicy>) - offsetof(OpLayoutT_BrInt2<SizePolicy>, RelativeJumpOffset);
            layout.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmBrReg1Const1(OpCodeAsmJs op, ByteCodeLabel labelID, RegSlot R1, int C1)
    {TRACE_IT(38291);
        OpLayoutT_BrInt1Const1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.I1, R1) && SizePolicy::Assign(layout.C1, C1))
        {TRACE_IT(38292);
            size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutT_BrInt1Const1<SizePolicy>) - offsetof(OpLayoutT_BrInt1Const1<SizePolicy>, RelativeJumpOffset);
            layout.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmCall(OpCodeAsmJs op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, AsmJsRetType retType)
    {TRACE_IT(38293);
        OpLayoutT_AsmCall<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Return, returnValueRegister) && SizePolicy::Assign(layout.Function, functionRegister)
            && SizePolicy::Assign(layout.ArgCount, givenArgCount) && SizePolicy::template Assign<int8>(layout.ReturnType, (int8)retType.which()))
        {TRACE_IT(38294);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmSlot(OpCodeAsmJs op, RegSlot value, RegSlot instance, int32 slotId)
    {TRACE_IT(38295);
        OpLayoutT_ElementSlot<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value) && SizePolicy::Assign(layout.Instance, instance)
            && SizePolicy::Assign(layout.SlotIndex, slotId))
        {TRACE_IT(38296);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmTypedArr(OpCodeAsmJs op, RegSlot value, uint32 slotIndex, ArrayBufferView::ViewType viewType)
    {TRACE_IT(38297);
        OpLayoutT_AsmTypedArr<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value) && SizePolicy::template Assign<int8>(layout.ViewType, (int8)viewType)
            && SizePolicy::Assign(layout.SlotIndex, slotIndex))
        {TRACE_IT(38298);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool AsmJsByteCodeWriter::TryWriteAsmSimdTypedArr(OpCodeAsmJs op, RegSlot value, uint32 slotIndex, uint8 dataWidth, ArrayBufferView::ViewType viewType)
    {TRACE_IT(38299);
        OpLayoutT_AsmSimdTypedArr<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value) && SizePolicy::template Assign<int8>(layout.ViewType, (int8)viewType)
            && SizePolicy::Assign(layout.SlotIndex, slotIndex) && SizePolicy::template Assign<int8>(layout.DataWidth, dataWidth))
        {TRACE_IT(38300);
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void AsmJsByteCodeWriter::EmptyAsm(OpCodeAsmJs op)
    {TRACE_IT(38301);
        m_byteCodeData.Encode(op, this);
    }

    void AsmJsByteCodeWriter::Conv(OpCodeAsmJs op, RegSlot R0, RegSlot R1)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg2, op, R0, R1);
    }

    void AsmJsByteCodeWriter::AsmInt1Const1(OpCodeAsmJs op, RegSlot R0, int C1)
    {
        MULTISIZE_LAYOUT_WRITE(Int1Const1, op, R0, C1);
    }

    void AsmJsByteCodeWriter::AsmReg1IntConst1(OpCodeAsmJs op, RegSlot R0, int C1)
    {
        MULTISIZE_LAYOUT_WRITE(Reg1IntConst1, op, R0, C1);
    }

    void AsmJsByteCodeWriter::AsmLong1Const1(OpCodeAsmJs op, RegSlot R0, int64 C1)
    {
        MULTISIZE_LAYOUT_WRITE(Long1Const1, op, R0, C1);
    }

    void AsmJsByteCodeWriter::AsmFloat1Const1(OpCodeAsmJs op, RegSlot R0, float C1)
    {
        MULTISIZE_LAYOUT_WRITE(Float1Const1, op, R0, C1);
    }

    void AsmJsByteCodeWriter::AsmDouble1Const1(OpCodeAsmJs op, RegSlot R0, double C1)
    {
        MULTISIZE_LAYOUT_WRITE(Double1Const1, op, R0, C1);
    }

    void AsmJsByteCodeWriter::AsmReg1(OpCodeAsmJs op, RegSlot R0)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg1, op, R0);
    }

    void AsmJsByteCodeWriter::AsmReg2(OpCodeAsmJs op, RegSlot R0, RegSlot R1)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg2, op, R0, R1);
    }

    void AsmJsByteCodeWriter::AsmReg3(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg3, op, R0, R1, R2);
    }

    void AsmJsByteCodeWriter::AsmReg4(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg4, op, R0, R1, R2, R3);
    }

    void AsmJsByteCodeWriter::AsmReg5(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg5, op, R0, R1, R2, R3, R4);
    }

    void AsmJsByteCodeWriter::AsmReg6(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg6, op, R0, R1, R2, R3, R4, R5);
    }

    void AsmJsByteCodeWriter::AsmReg7(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg7, op, R0, R1, R2, R3, R4, R5, R6);
    }

    void AsmJsByteCodeWriter::AsmReg9(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg9, op, R0, R1, R2, R3, R4, R5, R6, R7, R8);
    }
    void AsmJsByteCodeWriter::AsmReg10(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8, RegSlot R9)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg10, op, R0, R1, R2, R3, R4, R5, R6, R7, R8, R9);
    }

    void AsmJsByteCodeWriter::AsmReg11(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8, RegSlot R9, RegSlot R10)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg11, op, R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10);
    }

    void AsmJsByteCodeWriter::AsmReg17(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8,
                                                       RegSlot R9, RegSlot R10, RegSlot R11, RegSlot R12, RegSlot R13, RegSlot R14, RegSlot R15, RegSlot R16)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg17, op, R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, R16);
    }

    void AsmJsByteCodeWriter::AsmReg18(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8,
        RegSlot R9, RegSlot R10, RegSlot R11, RegSlot R12, RegSlot R13, RegSlot R14, RegSlot R15, RegSlot R16, RegSlot R17)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg18, op, R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, R16, R17);
    }

    void AsmJsByteCodeWriter::AsmReg19(OpCodeAsmJs op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4, RegSlot R5, RegSlot R6, RegSlot R7, RegSlot R8,
        RegSlot R9, RegSlot R10, RegSlot R11, RegSlot R12, RegSlot R13, RegSlot R14, RegSlot R15, RegSlot R16, RegSlot R17, RegSlot R18)
    {
        MULTISIZE_LAYOUT_WRITE(AsmReg19, op, R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, R16, R17, R18);
    }

    void AsmJsByteCodeWriter::AsmBr(ByteCodeLabel labelID, OpCodeAsmJs op)
    {TRACE_IT(38302);
        CheckOpen();
        CheckLabel(labelID);

        size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutAsmBr) - offsetof(OpLayoutAsmBr, RelativeJumpOffset);
        OpLayoutAsmBr data;
        data.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
        AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
    }

    void AsmJsByteCodeWriter::AsmBrReg1(OpCodeAsmJs op, ByteCodeLabel labelID, RegSlot R1)
    {TRACE_IT(38303);
        CheckOpen();
        CheckLabel(labelID);

        MULTISIZE_LAYOUT_WRITE(AsmBrReg1, op, labelID, R1);
    }

    void AsmJsByteCodeWriter::AsmBrReg2(OpCodeAsmJs op, ByteCodeLabel labelID, RegSlot R1, RegSlot R2)
    {TRACE_IT(38304);
        CheckOpen();
        CheckLabel(labelID);

        MULTISIZE_LAYOUT_WRITE(AsmBrReg2, op, labelID, R1, R2);
    }

    void AsmJsByteCodeWriter::AsmBrReg1Const1(OpCodeAsmJs op, ByteCodeLabel labelID, RegSlot R1, int C1)
    {TRACE_IT(38305);
        CheckOpen();
        CheckLabel(labelID);

        MULTISIZE_LAYOUT_WRITE(AsmBrReg1Const1, op, labelID, R1, C1);
    }

    void AsmJsByteCodeWriter::AsmStartCall(OpCodeAsmJs op, ArgSlot ArgCount, bool isPatching)
    {TRACE_IT(38306);
        CheckOpen();

        OpLayoutStartCall data;
        data.ArgCount = ArgCount;
        m_byteCodeData.Encode(op, &data, sizeof(data), this, isPatching);
    }

    void AsmJsByteCodeWriter::AsmCall(OpCodeAsmJs op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, AsmJsRetType retType)
    {
        MULTISIZE_LAYOUT_WRITE(AsmCall, op, returnValueRegister, functionRegister, givenArgCount, retType);
    }

    void AsmJsByteCodeWriter::AsmTypedArr(OpCodeAsmJs op, RegSlot value, uint32 slotIndex, ArrayBufferView::ViewType viewType)
    {
        MULTISIZE_LAYOUT_WRITE(AsmTypedArr, op, value, slotIndex, viewType);
    }

    void AsmJsByteCodeWriter::AsmSimdTypedArr(OpCodeAsmJs op, RegSlot value, uint32 slotIndex, uint8 dataWidth, ArrayBufferView::ViewType viewType)
    {TRACE_IT(38307);
        Assert(dataWidth >= 4 && dataWidth <= 16);
        MULTISIZE_LAYOUT_WRITE(AsmSimdTypedArr, op, value, slotIndex, dataWidth, viewType);
    }

    void AsmJsByteCodeWriter::AsmSlot(OpCodeAsmJs op, RegSlot value, RegSlot instance, int32 slotId)
    {
        MULTISIZE_LAYOUT_WRITE(AsmSlot, op, value, instance, slotId);
    }

    uint AsmJsByteCodeWriter::EnterLoop(Js::ByteCodeLabel loopEntrance)
    {TRACE_IT(38308);
        uint loopId = m_functionWrite->IncrLoopCount();
        Assert((uint)m_loopHeaders->Count() == loopId);

        m_loopHeaders->Add(LoopHeaderData(m_byteCodeData.GetCurrentOffset(), 0, m_loopNest > 0));
        m_loopNest++;
        Js::OpCodeAsmJs loopBodyOpcode = Js::OpCodeAsmJs::AsmJsLoopBodyStart;
        this->MarkAsmJsLabel(loopEntrance);
        this->AsmJsUnsigned1(loopBodyOpcode, loopId);

        return loopId;
    }

    void AsmJsByteCodeWriter::AsmJsUnsigned1(OpCodeAsmJs op, uint c1)
    {
        MULTISIZE_LAYOUT_WRITE(AsmJsUnsigned1, op, c1);
    }
    void AsmJsByteCodeWriter::ExitLoop(uint loopId)
    {TRACE_IT(38309);
        Assert(m_loopNest > 0);
        m_loopNest--;
        m_loopHeaders->Item(loopId).endOffset = m_byteCodeData.GetCurrentOffset();
    }

    void AsmJsByteCodeWriter::AddJumpOffset(Js::OpCodeAsmJs op, ByteCodeLabel labelId, uint fieldByteOffset)
    {TRACE_IT(38310);
        AssertMsg(fieldByteOffset < 100, "Ensure valid field offset");
        CheckOpen();
        CheckLabel(labelId);

        uint jumpByteOffset = m_byteCodeData.GetCurrentOffset() - fieldByteOffset;

        //
        // Branch targets are created in two passes:
        // - In the instruction stream, write "labelID" into "OpLayoutBrC.Offset". Record this
        //   location in "m_jumpOffsets" to be patched later.
        // - When the bytecode is closed, update all "OpLayoutBrC.Offset"'s with their actual
        //   destinations.
        //

        JumpInfo jumpInfo = { labelId, jumpByteOffset };
        m_jumpOffsets->Add(jumpInfo);
    }

    void AsmJsByteCodeWriter::MarkAsmJsLabel(ByteCodeLabel labelID)
    {TRACE_IT(38311);
        MarkLabel(labelID);
        EmptyAsm(OpCodeAsmJs::Label);
    }
} // namespace Js
#endif
