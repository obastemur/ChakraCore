//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#define PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix) \
    CompileAssert(OpCodeInfoAsmJs<OpCodeAsmJs::name>::Layout == OpLayoutTypeAsmJs::layout); \
    const unaligned OpLayout##layout##suffix * playout = mReader.layout##suffix(ip);

#define PROCESS_ENCODE_CUSTOM_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
        func(playout); \
        break; \
    }
#define PROCESS_ENCODE_CUSTOM(name,func,layout) PROCESS_ENCODE_CUSTOM_COMMON(name,func,layout,)

#define PROCESS_ENCODE_INT2_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
        AsmJsJitTemplate::func::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->I0), CalculateOffset<int>(playout->I1) );\
        break; \
    }
#define PROCESS_ENCODE_INT2(name,func,layout) PROCESS_ENCODE_INT2_COMMON(name,func,layout,)

#define PROCESS_ENCODE_INT3_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
        AsmJsJitTemplate::func::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->I0), CalculateOffset<int>(playout->I1), CalculateOffset<int>(playout->I2) );\
        break; \
    }
#define PROCESS_ENCODE_INT3(name,func,layout) PROCESS_ENCODE_INT3_COMMON(name,func,layout,)

#define PROCESS_ENCODE_DOUBLE2_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
        AsmJsJitTemplate::func::ApplyTemplate( this, mPc, CalculateOffset<double>(playout->D0), CalculateOffset<double>(playout->D1) );\
        break; \
    }
#define PROCESS_ENCODE_DOUBLE2(name,func,layout) PROCESS_ENCODE_DOUBLE2_COMMON(name,func,layout,)

#define PROCESS_ENCODE_DOUBLE3_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
        AsmJsJitTemplate::func::ApplyTemplate( this, mPc, CalculateOffset<double>(playout->D0), CalculateOffset<double>(playout->D1), CalculateOffset<double>(playout->D2) );\
        break; \
    }
#define PROCESS_ENCODE_DOUBLE3(name,func,layout) PROCESS_ENCODE_DOUBLE3_COMMON(name,func,layout,)

#define PROCESS_ENCODE_INT1DOUBLE2_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
        AsmJsJitTemplate::func::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->I0), CalculateOffset<double>(playout->D1), CalculateOffset<double>(playout->D2) );\
        break; \
    }
#define PROCESS_ENCODE_INT1DOUBLE2(name,func,layout) PROCESS_ENCODE_INT1DOUBLE2_COMMON(name,func,layout,)

typedef double( *UnaryDoubleFunc )( double );
#define PROCESS_ENCODE_CALLDOUBLE2_COMMON(name, func, addEsp, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, Double2, suffix); \
        int offsets[2] = {CalculateOffset<double>(playout->D0),CalculateOffset<double>(playout->D1)};\
        AsmJsJitTemplate::Call_Db::ApplyTemplate( this, mPc, 2, offsets, ((void*)(UnaryDoubleFunc)(func)),addEsp );\
        break; \
    }
#define PROCESS_ENCODE_CALLDOUBLE2(name,func,layout) PROCESS_ENCODE_CALLDOUBLE2_COMMON(name,func,layout,)

typedef double( *BinaryDoubleFunc )( double, double );
#define PROCESS_ENCODE_CALLDOUBLE3_COMMON(name, func, addEsp, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, Double3, suffix); \
        int offsets[3] = {CalculateOffset<double>(playout->D0),CalculateOffset<double>(playout->D1),CalculateOffset<double>(playout->D2)};\
        AsmJsJitTemplate::Call_Db::ApplyTemplate( this, mPc, 3, offsets, ((void*)(BinaryDoubleFunc)(func)),addEsp );\
        break; \
    }
#define PROCESS_ENCODE_CALLDOUBLE3(name,func,addEsp) PROCESS_ENCODE_CALLDOUBLE3_COMMON(name,func,addEsp,)

//Floats
#define PROCESS_ENCODE_FLOAT2_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
{ \
    PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
    AsmJsJitTemplate::func::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->F0), CalculateOffset<float>(playout->F1)); \
    break; \
}
#define PROCESS_ENCODE_FLOAT2(name,func,layout) PROCESS_ENCODE_FLOAT2_COMMON(name,func,layout,)

#define PROCESS_ENCODE_FLOAT3_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
{ \
    PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
    AsmJsJitTemplate::func::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->F0), CalculateOffset<float>(playout->F1), CalculateOffset<float>(playout->F2)); \
    break; \
}
#define PROCESS_ENCODE_FLOAT3(name,func,layout) PROCESS_ENCODE_FLOAT3_COMMON(name,func,layout,)

#define PROCESS_ENCODE_INT1FLOAT2_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
{ \
    PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
    AsmJsJitTemplate::func::ApplyTemplate(this, mPc, CalculateOffset<int>(playout->I0), CalculateOffset<float>(playout->F1), CalculateOffset<float>(playout->F2)); \
    break; \
}
#define PROCESS_ENCODE_INT1FLOAT2(name,func,layout) PROCESS_ENCODE_INT1FLOAT2_COMMON(name,func,layout,)

typedef float(*UnaryFloatFunc)(float);
#define PROCESS_ENCODE_CALLFLOAT2_COMMON(name, func, addEsp, suffix) \
    case OpCodeAsmJs::name: \
{ \
    PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, Float2, suffix); \
    int offsets[2] = { CalculateOffset<float>(playout->F0), CalculateOffset<float>(playout->F1) }; \
    AsmJsJitTemplate::Call_Flt::ApplyTemplate(this, mPc, 2, offsets, ((void*)(UnaryFloatFunc)(func)), addEsp); \
    break; \
}
#define PROCESS_ENCODE_CALLFLOAT2(name,func,layout) PROCESS_ENCODE_CALLFLOAT2_COMMON(name,func,layout,)



#define PROCESS_ENCODE_ELEMENTSLOT_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, layout, suffix); \
        AsmJsJitTemplate::func::ApplyTemplate( this, mPc, CalculateOffset<Var>(playout->Value), CalculateOffset<Var>(playout->Instance), CalculateOffset<int>(playout->SlotIndex) );\
        break; \
    }
#define PROCESS_ENCODE_ELEMENTSLOT(name,func,layout) PROCESS_ENCODE_ELEMENTSLOT_COMMON(name,func,layout,)

#define PROCESS_ENCODE_TYPED_ARR_COMMON(name, func, viewType, suffix, type ) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_ENCODE_READ_LAYOUT_ASMJS(name, ElementSlot, suffix); \
        AsmJsJitTemplate::func::ApplyTemplate( this, mPc, CalculateOffset<type>(playout->Value), CalculateOffset<int>(playout->SlotIndex), ArrayBufferView::viewType );\
        break; \
    }
#define PROCESS_ENCODE_TYPED_ARR(name,viewType, func,layout) PROCESS_ENCODE_TYPED_ARR_COMMON(name,viewType, func,layout,)

namespace Js
{
    template <class T>
    void AsmJsEncoder::OP_Empty( const unaligned T* playout )
    {TRACE_IT(46232);

    }

    void AsmJsEncoder::OP_Label( const unaligned OpLayoutEmpty* playout )
    {TRACE_IT(46233);
        const int labelOffset = mReader.GetCurrentOffset() - 1;
        AsmJsJitTemplate::Label::ApplyTemplate( this, mPc );

        EncoderRelocLabel* label = nullptr;
        if( mRelocLabelMap->TryGetReference( labelOffset,&label ) )
        {TRACE_IT(46234);
            label->labelSeen = true;
            label->pc = mPc;
        }
        else
        {TRACE_IT(46235);
            EncoderRelocLabel newLabel( mPc );
            mRelocLabelMap->AddNew( labelOffset, newLabel );
        }
        // Check - this should not be needed as we add to the map in Relocs , but the bytecodeoffset is off by 1 in relocs , see if we can work around that
        ptrdiff_t offset = mPc - mEncodeBuffer;
        this->GetAsmJsFunctionInfo()->mbyteCodeTJMap->AddNew(mReader.GetCurrentOffset(), offset);
    }

    template <class T>
    void AsmJsEncoder::OP_LdUndef( const unaligned T* playout )
    {TRACE_IT(46236);
        AsmJsJitTemplate::LdUndef::ApplyTemplate( this, mPc, CalculateOffset<Var>(playout->R0) );
    }

    template <class T>
    void AsmJsEncoder::OP_Br( const unaligned T* playout )
    {TRACE_IT(46237);
        if( playout->RelativeJumpOffset )
        {TRACE_IT(46238);
            const int labelOffset = mReader.GetCurrentOffset() + playout->RelativeJumpOffset;
            Assert( playout->RelativeJumpOffset > 0 || mRelocLabelMap->ContainsKey( labelOffset ) );
            bool isBackEdge = false;
            if (playout->RelativeJumpOffset < 0)
            {TRACE_IT(46239);
                isBackEdge = true;
            }
            BYTE* relocAddr = nullptr;
            AsmJsJitTemplate::Br::ApplyTemplate(this, mPc, &relocAddr, isBackEdge);
            Assert( relocAddr );
            AddReloc( labelOffset, relocAddr );
        }
    }

    template <class T>
    void AsmJsEncoder::OP_BrTrue( const unaligned T* playout )
    {TRACE_IT(46240);
        if( playout->RelativeJumpOffset )
        {TRACE_IT(46241);
            const int labelOffset = mReader.GetCurrentOffset() + playout->RelativeJumpOffset;
            Assert( playout->RelativeJumpOffset > 0 || mRelocLabelMap->ContainsKey( labelOffset ) );
            bool isBackEdge = false;
            if (playout->RelativeJumpOffset < 0)
                isBackEdge = true;
            BYTE* relocAddr = nullptr;
            AsmJsJitTemplate::BrTrue::ApplyTemplate( this, mPc, CalculateOffset<int>( playout->I1 ), &relocAddr, isBackEdge );
            Assert( relocAddr );
            AddReloc( labelOffset, relocAddr );
        }
    }

    template <class T>
    void AsmJsEncoder::OP_BrEq( const unaligned T* playout )
    {TRACE_IT(46242);
        if( playout->RelativeJumpOffset )
        {TRACE_IT(46243);
            const int labelOffset = mReader.GetCurrentOffset() + playout->RelativeJumpOffset;
            Assert( playout->RelativeJumpOffset > 0 || mRelocLabelMap->ContainsKey( labelOffset ) );
            bool isBackEdge = false;
            if (playout->RelativeJumpOffset < 0)
                isBackEdge = true;
            BYTE* relocAddr = nullptr;
            AsmJsJitTemplate::BrEq::ApplyTemplate(this, mPc, CalculateOffset<int>(playout->I1), CalculateOffset<int>(playout->I2), &relocAddr, isBackEdge);
            Assert( relocAddr );
            AddReloc( labelOffset, relocAddr );
        }
    }

    template <class T>
    void AsmJsEncoder::OP_BrEqConst(const unaligned T* playout)
    {TRACE_IT(46244);
        if (playout->RelativeJumpOffset)
        {TRACE_IT(46245);
            const int labelOffset = mReader.GetCurrentOffset() + playout->RelativeJumpOffset;
            Assert(playout->RelativeJumpOffset > 0 || mRelocLabelMap->ContainsKey(labelOffset));
            bool isBackEdge = false;
            if (playout->RelativeJumpOffset < 0)
                isBackEdge = true;
            BYTE* relocAddr = nullptr;
            AsmJsJitTemplate::BrEq::ApplyTemplate(this, mPc, CalculateOffset<int>(playout->I1), playout->C1, &relocAddr, isBackEdge, true);
            Assert(relocAddr);
            AddReloc(labelOffset, relocAddr);
        }
    }

    template <class T>
    void Js::AsmJsEncoder::Op_LdConst_Int( const unaligned T* playout )
    {TRACE_IT(46246);
        AsmJsJitTemplate::LdConst_Int::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->I0), playout->C1 );
    }

    template <class T>
    void Js::AsmJsEncoder::OP_SetReturnInt( const unaligned T* playout )
    {TRACE_IT(46247);
        AsmJsJitTemplate::SetReturn_Int::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->I1) );
    }

    template <class T>
    void Js::AsmJsEncoder::OP_SetReturnDouble( const unaligned T* playout )
    {TRACE_IT(46248);
        AsmJsJitTemplate::SetReturn_Db::ApplyTemplate( this, mPc, CalculateOffset<double>(playout->D1) );
    }

    template <class T>
    void Js::AsmJsEncoder::OP_SetReturnFloat(const unaligned T* playout)
    {TRACE_IT(46249);
        AsmJsJitTemplate::SetReturn_Flt::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->F1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_SetFroundInt(const unaligned T* playout)
    {TRACE_IT(46250);
        AsmJsJitTemplate::SetFround_Int::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->F0), CalculateOffset<int>(playout->I1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_SetFroundDb(const unaligned T* playout)
    {TRACE_IT(46251);
        AsmJsJitTemplate::SetFround_Db::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->F0), CalculateOffset<double>(playout->D1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_SetFroundFlt(const unaligned T* playout)
    {TRACE_IT(46252);
        AsmJsJitTemplate::SetFround_Flt::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->F0), CalculateOffset<float>(playout->F1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_AsmJsLoopBody(const unaligned T* playout)
    {TRACE_IT(46253);
        AsmJsJitTemplate::AsmJsLoopBody::ApplyTemplate(this, mPc, (int)playout->C1);
    }

    template <class T>
    void Js::AsmJsEncoder::Op_Float_To_Int(const unaligned T* playout)
    {TRACE_IT(46254);
        AsmJsJitTemplate::Float_To_Int::ApplyTemplate(this, mPc, CalculateOffset<int>(playout->I0), CalculateOffset<float>(playout->F1));
    }

    template <class T>
    void Js::AsmJsEncoder::Op_Float_To_Db(const unaligned T* playout)
    {TRACE_IT(46255);
        AsmJsJitTemplate::Float_To_Db::ApplyTemplate(this, mPc, CalculateOffset<double>(playout->D0), CalculateOffset<float>(playout->F1));
    }

    template <class T>
    void Js::AsmJsEncoder::Op_UInt_To_Db( const unaligned T* playout )
    {TRACE_IT(46256);
        AsmJsJitTemplate::UInt_To_Db::ApplyTemplate( this, mPc, CalculateOffset<double>(playout->D0), CalculateOffset<int>(playout->I1) );
    }

    template <class T>
    void Js::AsmJsEncoder::Op_Int_To_Db( const unaligned T* playout )
    {TRACE_IT(46257);
        AsmJsJitTemplate::Int_To_Db::ApplyTemplate( this, mPc, CalculateOffset<double>(playout->D0), CalculateOffset<int>(playout->I1) );
    }

    template <class T>
    void Js::AsmJsEncoder::Op_Db_To_Int( const unaligned T* playout )
    {TRACE_IT(46258);
        AsmJsJitTemplate::Db_To_Int::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->I0), CalculateOffset<double>(playout->D1) );
    }

    template <class T>
    void Js::AsmJsEncoder::Op_StSlot_Int( const unaligned T* playout )
    {TRACE_IT(46259);
        AsmJsJitTemplate::StSlot_Int::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->Value), playout->SlotIndex );
    }

    template <class T>
    void Js::AsmJsEncoder::Op_StSlot_Db( const unaligned T* playout )
    {TRACE_IT(46260);
        AsmJsJitTemplate::StSlot_Db::ApplyTemplate( this, mPc, CalculateOffset<double>(playout->Value), playout->SlotIndex );
    }

    template <class T>
    void Js::AsmJsEncoder::Op_StSlot_Flt(const unaligned T* playout)
    {TRACE_IT(46261);
        AsmJsJitTemplate::StSlot_Flt::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->Value), playout->SlotIndex);
    }

    template <class T>
    void Js::AsmJsEncoder::Op_LdSlot_Int( const unaligned T* playout )
    {TRACE_IT(46262);
        AsmJsJitTemplate::LdSlot_Int::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->Value), playout->SlotIndex );
    }

    template <class T>
    void Js::AsmJsEncoder::Op_LdSlot_Db( const unaligned T* playout )
    {TRACE_IT(46263);
        AsmJsJitTemplate::LdSlot_Db::ApplyTemplate( this, mPc, CalculateOffset<double>(playout->Value), playout->SlotIndex );
    }

    template <class T>
    void Js::AsmJsEncoder::Op_LdSlot_Flt(const unaligned T* playout)
    {TRACE_IT(46264);
        AsmJsJitTemplate::LdSlot_Flt::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->Value), playout->SlotIndex);
    }

    template <class T>
    void Js::AsmJsEncoder::Op_LdAddr_Db( const unaligned T* playout )
    {TRACE_IT(46265);
        AsmJsJitTemplate::LdAddr_Db::ApplyTemplate( this, mPc, CalculateOffset<double>(playout->D0), playout->A1 );
    }

    template <class T> void Js::AsmJsEncoder::OP_LdSlot( const unaligned T* playout )
    {TRACE_IT(46266);
        AsmJsJitTemplate::LdSlot::ApplyTemplate( this, mPc, CalculateOffset<Var>(playout->Value), CalculateOffset<Var>(playout->Instance), playout->SlotIndex );
    }

    template <class T> void Js::AsmJsEncoder::OP_StartCall( const unaligned T* playout )
    {TRACE_IT(46267);
        AsmJsJitTemplate::StartCall::ApplyTemplate( this, mPc, playout->ArgCount);
    }

    template <class T> void Js::AsmJsEncoder::OP_Call( const unaligned T* playout )
    {TRACE_IT(46268);
        AsmJsJitTemplate::Call::ApplyTemplate( this, mPc, CalculateOffset<Var>(playout->Return), CalculateOffset<Var>(playout->Function), playout->ArgCount );
    }

    template <class T> void Js::AsmJsEncoder::OP_ArgOut_Db( const unaligned T* playout )
    {TRACE_IT(46269);
        AsmJsJitTemplate::ArgOut_Db::ApplyTemplate( this, mPc, playout->R0, CalculateOffset<double>(playout->D1));
    }

    template <class T> void Js::AsmJsEncoder::OP_ArgOut_Int( const unaligned T* playout )
    {TRACE_IT(46270);
        AsmJsJitTemplate::ArgOut_Int::ApplyTemplate( this, mPc, playout->R0, CalculateOffset<int>(playout->I1));
    }

    template <class T> void Js::AsmJsEncoder::OP_Conv_VTD( const unaligned T* playout )
    {TRACE_IT(46271);
        AsmJsJitTemplate::Conv_VTD::ApplyTemplate( this, mPc, CalculateOffset<double>(playout->D0), CalculateOffset<Var>(playout->R1));
    }

    template <class T> void Js::AsmJsEncoder::OP_Conv_VTF(const unaligned T* playout)
    {TRACE_IT(46272);
        AsmJsJitTemplate::Conv_VTF::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->F0), CalculateOffset<Var>(playout->R1));
    }

    template <class T> void Js::AsmJsEncoder::OP_Conv_VTI( const unaligned T* playout )
    {TRACE_IT(46273);
        AsmJsJitTemplate::Conv_VTI::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->I0), CalculateOffset<Var>(playout->R1));
    }

    template <class T> void Js::AsmJsEncoder::OP_I_StartCall( const unaligned T* playout )
    {TRACE_IT(46274);
        AsmJsJitTemplate::I_StartCall::ApplyTemplate( this, mPc, playout->ArgCount);
    }

    template <class T> void Js::AsmJsEncoder::OP_I_Call( const unaligned T* playout )
    {TRACE_IT(46275);
        AsmJsJitTemplate::I_Call::ApplyTemplate( this, mPc, CalculateOffset<Var>(playout->Return), CalculateOffset<Var>(playout->Function), playout->ArgCount, AsmJsRetType((AsmJsRetType::Which)playout->ReturnType) );
    }

    template <class T> void Js::AsmJsEncoder::OP_I_ArgOut_Db( const unaligned T* playout )
    {TRACE_IT(46276);
        AsmJsJitTemplate::I_ArgOut_Db::ApplyTemplate( this, mPc, playout->R0, CalculateOffset<double>(playout->D1));
    }

    template <class T> void Js::AsmJsEncoder::OP_I_ArgOut_Flt(const unaligned T* playout)
    {TRACE_IT(46277);
        AsmJsJitTemplate::I_ArgOut_Flt::ApplyTemplate(this, mPc, playout->R0, CalculateOffset<float>(playout->F1));
    }

    template <class T> void Js::AsmJsEncoder::OP_I_ArgOut_Int( const unaligned T* playout )
    {TRACE_IT(46278);
        AsmJsJitTemplate::I_ArgOut_Int::ApplyTemplate( this, mPc, playout->R0, CalculateOffset<int>(playout->I1));
    }

    template <class T> void Js::AsmJsEncoder::OP_I_Conv_VTD( const unaligned T* playout )
    {TRACE_IT(46279);
        AsmJsJitTemplate::I_Conv_VTD::ApplyTemplate(this, mPc, CalculateOffset<double>(playout->D0), CalculateOffset<double>(playout->D1));
    }

    template <class T> void Js::AsmJsEncoder::OP_I_Conv_VTF(const unaligned T* playout)
    {TRACE_IT(46280);
        AsmJsJitTemplate::I_Conv_VTF::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->F0), CalculateOffset<float>(playout->F1));
    }

    template <class T> void Js::AsmJsEncoder::OP_I_Conv_VTI( const unaligned T* playout )
    {TRACE_IT(46281);
        AsmJsJitTemplate::I_Conv_VTI::ApplyTemplate( this, mPc, CalculateOffset<int>(playout->I0), CalculateOffset<int>(playout->I1));
    }

    template <class T>
    void Js::AsmJsEncoder::Op_LdArr( const unaligned T* playout )
    {TRACE_IT(46282);
        if (playout->ViewType == ArrayBufferView::TYPE_FLOAT32)
        {TRACE_IT(46283);
            AsmJsJitTemplate::LdArrFlt::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->Value), CalculateOffset<int>(playout->SlotIndex), (ArrayBufferView::ViewType)playout->ViewType);
        }
        else if(playout->ViewType == ArrayBufferView::TYPE_FLOAT64)
        {TRACE_IT(46284);
            AsmJsJitTemplate::LdArrDb::ApplyTemplate( this, mPc, CalculateOffset<double>( playout->Value ), CalculateOffset<int>( playout->SlotIndex ), (ArrayBufferView::ViewType)playout->ViewType );
        }
        else
        {TRACE_IT(46285);
            AsmJsJitTemplate::LdArr::ApplyTemplate( this, mPc, CalculateOffset<int>( playout->Value ), CalculateOffset<int>( playout->SlotIndex ), (ArrayBufferView::ViewType)playout->ViewType );
        }
    }
    template <class T>
    void Js::AsmJsEncoder::Op_LdArrConst( const unaligned T* playout )
    {TRACE_IT(46286);
        if (playout->ViewType == ArrayBufferView::TYPE_FLOAT32)
        {TRACE_IT(46287);
            AsmJsJitTemplate::ConstLdArrFlt::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->Value), playout->SlotIndex, (ArrayBufferView::ViewType)playout->ViewType);
        }
        else if(playout->ViewType == ArrayBufferView::TYPE_FLOAT64 )
        {TRACE_IT(46288);
            AsmJsJitTemplate::ConstLdArrDb::ApplyTemplate( this, mPc, CalculateOffset<double>( playout->Value ), playout->SlotIndex, (ArrayBufferView::ViewType)playout->ViewType );
        }
        else
        {TRACE_IT(46289);
            AsmJsJitTemplate::ConstLdArr::ApplyTemplate( this, mPc, CalculateOffset<int>( playout->Value ), playout->SlotIndex, (ArrayBufferView::ViewType)playout->ViewType );
        }
    }
    template <class T>
    void Js::AsmJsEncoder::Op_StArr( const unaligned T* playout )
    {TRACE_IT(46290);
        if (playout->ViewType == ArrayBufferView::TYPE_FLOAT32 )
        {TRACE_IT(46291);
            //Value can be double
            AsmJsJitTemplate::StArrFlt::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->Value), CalculateOffset<int>(playout->SlotIndex), (ArrayBufferView::ViewType)playout->ViewType);

        }
        else if( playout->ViewType == ArrayBufferView::TYPE_FLOAT64 )
        {TRACE_IT(46292);
            AsmJsJitTemplate::StArrDb::ApplyTemplate( this, mPc, CalculateOffset<double>( playout->Value ), CalculateOffset<int>( playout->SlotIndex ), (ArrayBufferView::ViewType)playout->ViewType );
        }
        else
        {TRACE_IT(46293);
            AsmJsJitTemplate::StArr::ApplyTemplate( this, mPc, CalculateOffset<int>( playout->Value ), CalculateOffset<int>( playout->SlotIndex ), (ArrayBufferView::ViewType)playout->ViewType );
        }
    }
    template <class T>
    void Js::AsmJsEncoder::Op_StArrConst( const unaligned T* playout )
    {TRACE_IT(46294);
        if (playout->ViewType == ArrayBufferView::TYPE_FLOAT32 )
        {TRACE_IT(46295);
            AsmJsJitTemplate::ConstStArrFlt::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->Value), playout->SlotIndex, (ArrayBufferView::ViewType)playout->ViewType);
        }
        else if( playout->ViewType == ArrayBufferView::TYPE_FLOAT64 )
        {TRACE_IT(46296);
            AsmJsJitTemplate::ConstStArrDb::ApplyTemplate( this, mPc, CalculateOffset<double>( playout->Value ), playout->SlotIndex , (ArrayBufferView::ViewType)playout->ViewType );
        }
        else
        {TRACE_IT(46297);
            AsmJsJitTemplate::ConstStArr::ApplyTemplate( this, mPc, CalculateOffset<int>( playout->Value ), playout->SlotIndex, (ArrayBufferView::ViewType)playout->ViewType );
        }
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LdF4(const unaligned T* playout)
    {TRACE_IT(46298);
        AsmJsJitTemplate::Simd128_Ld_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LdI4(const unaligned T* playout)
    {TRACE_IT(46299);
        AsmJsJitTemplate::Simd128_Ld_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LdD2(const unaligned T* playout)
    {TRACE_IT(46300);
        AsmJsJitTemplate::Simd128_Ld_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LdSlotF4(const unaligned T* playout)
    {TRACE_IT(46301);
        AsmJsJitTemplate::Simd128_LdSlot_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->Value), playout->SlotIndex);
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LdSlotI4(const unaligned T* playout)
    {TRACE_IT(46302);
        AsmJsJitTemplate::Simd128_LdSlot_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->Value), playout->SlotIndex);
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LdSlotD2(const unaligned T* playout)
    {TRACE_IT(46303);
        AsmJsJitTemplate::Simd128_LdSlot_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->Value), playout->SlotIndex);
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_StSlotF4(const unaligned T* playout)
    {TRACE_IT(46304);
        AsmJsJitTemplate::Simd128_StSlot_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->Value), playout->SlotIndex);
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_StSlotI4(const unaligned T* playout)
    {TRACE_IT(46305);
        AsmJsJitTemplate::Simd128_StSlot_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->Value), playout->SlotIndex);
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_StSlotD2(const unaligned T* playout)
    {TRACE_IT(46306);
        AsmJsJitTemplate::Simd128_StSlot_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->Value), playout->SlotIndex);
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FloatsToF4(const unaligned T* playout)
    {TRACE_IT(46307);
        AsmJsJitTemplate::Simd128_FloatsToF4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), \
            CalculateOffset<float>(playout->F1), CalculateOffset<float>(playout->F2), CalculateOffset<float>(playout->F3), CalculateOffset<float>(playout->F4));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_IntsToI4(const unaligned T* playout)
    {TRACE_IT(46308);
        AsmJsJitTemplate::Simd128_IntsToI4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), \
            CalculateOffset<int>(playout->I1), CalculateOffset<int>(playout->I2), CalculateOffset<int>(playout->I3), CalculateOffset<int>(playout->I4));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_DoublesToD2(const unaligned T* playout)
    {TRACE_IT(46309);
        AsmJsJitTemplate::Simd128_DoublesToD2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), \
            CalculateOffset<double>(playout->D1), CalculateOffset<double>(playout->D2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_ReturnF4(const unaligned T* playout)
    {TRACE_IT(46310);
        AsmJsJitTemplate::Simd128_Return_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_ReturnI4(const unaligned T* playout)
    {TRACE_IT(46311);
        AsmJsJitTemplate::Simd128_Return_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_ReturnD2(const unaligned T* playout)
    {TRACE_IT(46312);
        AsmJsJitTemplate::Simd128_Return_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SplatF4(const unaligned T* playout)
    {TRACE_IT(46313);
        AsmJsJitTemplate::Simd128_Splat_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<float>(playout->F1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SplatI4(const unaligned T* playout)
    {TRACE_IT(46314);
        AsmJsJitTemplate::Simd128_Splat_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<int>(playout->I1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SplatD2(const unaligned T* playout)
    {TRACE_IT(46315);
        AsmJsJitTemplate::Simd128_Splat_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<double>(playout->D1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromFloat64x2F4(const unaligned T* playout)
    {TRACE_IT(46316);
        AsmJsJitTemplate::Simd128_FromFloat64x2_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromInt32x4F4(const unaligned T* playout)
    {TRACE_IT(46317);
        AsmJsJitTemplate::Simd128_FromInt32x4_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromFloat32x4I4(const unaligned T* playout)
    {TRACE_IT(46318);
        AsmJsJitTemplate::Simd128_FromFloat32x4_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromFloat64x2I4(const unaligned T* playout)
    {TRACE_IT(46319);
        AsmJsJitTemplate::Simd128_FromFloat64x2_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromFloat32x4D2(const unaligned T* playout)
    {TRACE_IT(46320);
        AsmJsJitTemplate::Simd128_FromFloat32x4_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromInt32x4D2(const unaligned T* playout)
    {TRACE_IT(46321);
        AsmJsJitTemplate::Simd128_FromInt32x4_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromFloat64x2BitsF4(const unaligned T* playout)
    {TRACE_IT(46322);
        AsmJsJitTemplate::Simd128_FromFloat64x2Bits_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromInt32x4BitsF4(const unaligned T* playout)
    {TRACE_IT(46323);
        AsmJsJitTemplate::Simd128_FromInt32x4Bits_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromFloat32x4BitsI4(const unaligned T* playout)
    {TRACE_IT(46324);
        AsmJsJitTemplate::Simd128_FromFloat32x4Bits_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromFloat64x2BitsI4(const unaligned T* playout)
    {TRACE_IT(46325);
        AsmJsJitTemplate::Simd128_FromFloat64x2Bits_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromFloat32x4BitsD2(const unaligned T* playout)
    {TRACE_IT(46326);
        AsmJsJitTemplate::Simd128_FromFloat32x4Bits_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_FromInt32x4BitsD2(const unaligned T* playout)
    {TRACE_IT(46327);
        AsmJsJitTemplate::Simd128_FromInt32x4Bits_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_AbsF4(const unaligned T* playout)
    {TRACE_IT(46328);
        AsmJsJitTemplate::Simd128_Abs_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_AbsD2(const unaligned T* playout)
    {TRACE_IT(46329);
        AsmJsJitTemplate::Simd128_Abs_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_NegF4(const unaligned T* playout)
    {TRACE_IT(46330);
        AsmJsJitTemplate::Simd128_Neg_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_NegI4(const unaligned T* playout)
    {TRACE_IT(46331);
        AsmJsJitTemplate::Simd128_Neg_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_NegD2(const unaligned T* playout)
    {TRACE_IT(46332);
        AsmJsJitTemplate::Simd128_Neg_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_RcpF4(const unaligned T* playout)
    {TRACE_IT(46333);
        AsmJsJitTemplate::Simd128_Rcp_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_RcpD2(const unaligned T* playout)
    {TRACE_IT(46334);
        AsmJsJitTemplate::Simd128_Rcp_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_RcpSqrtF4(const unaligned T* playout)
    {TRACE_IT(46335);
        AsmJsJitTemplate::Simd128_RcpSqrt_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_RcpSqrtD2(const unaligned T* playout)
    {TRACE_IT(46336);
        AsmJsJitTemplate::Simd128_RcpSqrt_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SqrtF4(const unaligned T* playout)
    {TRACE_IT(46337);
        AsmJsJitTemplate::Simd128_Sqrt_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SqrtD2(const unaligned T* playout)
    {TRACE_IT(46338);
        AsmJsJitTemplate::Simd128_Sqrt_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_NotF4(const unaligned T* playout)
    {TRACE_IT(46339);
        AsmJsJitTemplate::Simd128_Not_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_NotI4(const unaligned T* playout)
    {TRACE_IT(46340);
        AsmJsJitTemplate::Simd128_Not_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_AddF4(const unaligned T* playout)
    {TRACE_IT(46341);
        AsmJsJitTemplate::Simd128_Add_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_AddI4(const unaligned T* playout)
    {TRACE_IT(46342);
        AsmJsJitTemplate::Simd128_Add_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_AddD2(const unaligned T* playout)
    {TRACE_IT(46343);
        AsmJsJitTemplate::Simd128_Add_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SubF4(const unaligned T* playout)
    {TRACE_IT(46344);
        AsmJsJitTemplate::Simd128_Sub_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SubI4(const unaligned T* playout)
    {TRACE_IT(46345);
        AsmJsJitTemplate::Simd128_Sub_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SubD2(const unaligned T* playout)
    {TRACE_IT(46346);
        AsmJsJitTemplate::Simd128_Sub_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_MulF4(const unaligned T* playout)
    {TRACE_IT(46347);
        AsmJsJitTemplate::Simd128_Mul_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_MulI4(const unaligned T* playout)
    {TRACE_IT(46348);
        AsmJsJitTemplate::Simd128_Mul_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_MulD2(const unaligned T* playout)
    {TRACE_IT(46349);
        AsmJsJitTemplate::Simd128_Mul_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_DivF4(const unaligned T* playout)
    {TRACE_IT(46350);
        AsmJsJitTemplate::Simd128_Div_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_DivD2(const unaligned T* playout)
    {TRACE_IT(46351);
        AsmJsJitTemplate::Simd128_Div_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_MinF4(const unaligned T* playout)
    {TRACE_IT(46352);
        AsmJsJitTemplate::Simd128_Min_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_MinD2(const unaligned T* playout)
    {TRACE_IT(46353);
        AsmJsJitTemplate::Simd128_Min_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_MaxF4(const unaligned T* playout)
    {TRACE_IT(46354);
        AsmJsJitTemplate::Simd128_Max_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_MaxD2(const unaligned T* playout)
    {TRACE_IT(46355);
        AsmJsJitTemplate::Simd128_Max_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LtF4(const unaligned T* playout)
    {TRACE_IT(46356);
        AsmJsJitTemplate::Simd128_Lt_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->B4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LtI4(const unaligned T* playout)
    {TRACE_IT(46357);
        AsmJsJitTemplate::Simd128_Lt_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->B4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LtD2(const unaligned T* playout)
    {TRACE_IT(46358);
        AsmJsJitTemplate::Simd128_Lt_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_GtF4(const unaligned T* playout)
    {TRACE_IT(46359);
        AsmJsJitTemplate::Simd128_Gt_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->B4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_GtI4(const unaligned T* playout)
    {TRACE_IT(46360);
        AsmJsJitTemplate::Simd128_Gt_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->B4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_GtD2(const unaligned T* playout)
    {TRACE_IT(46361);
        AsmJsJitTemplate::Simd128_Gt_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LtEqF4(const unaligned T* playout)
    {TRACE_IT(46362);
        AsmJsJitTemplate::Simd128_LtEq_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->B4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_LtEqD2(const unaligned T* playout)
    {TRACE_IT(46363);
        AsmJsJitTemplate::Simd128_LtEq_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_GtEqF4(const unaligned T* playout)
    {TRACE_IT(46364);
        AsmJsJitTemplate::Simd128_GtEq_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->B4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_GtEqD2(const unaligned T* playout)
    {TRACE_IT(46365);
        AsmJsJitTemplate::Simd128_GtEq_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_EqF4(const unaligned T* playout)
    {TRACE_IT(46366);
        AsmJsJitTemplate::Simd128_Eq_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->B4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_EqI4(const unaligned T* playout)
    {TRACE_IT(46367);
        AsmJsJitTemplate::Simd128_Eq_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->B4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_EqD2(const unaligned T* playout)
    {TRACE_IT(46368);
        AsmJsJitTemplate::Simd128_Eq_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_NeqF4(const unaligned T* playout)
    {TRACE_IT(46369);
        AsmJsJitTemplate::Simd128_Neq_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->B4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_NeqD2(const unaligned T* playout)
    {TRACE_IT(46370);
        AsmJsJitTemplate::Simd128_Neq_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_AndF4(const unaligned T* playout)
    {TRACE_IT(46371);
        AsmJsJitTemplate::Simd128_And_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_AndI4(const unaligned T* playout)
    {TRACE_IT(46372);
        AsmJsJitTemplate::Simd128_And_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_OrF4(const unaligned T* playout)
    {TRACE_IT(46373);
        AsmJsJitTemplate::Simd128_Or_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_OrI4(const unaligned T* playout)
    {TRACE_IT(46374);
        AsmJsJitTemplate::Simd128_Or_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2));
    }

    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_XorF4(const unaligned T* playout)
    {TRACE_IT(46375);
        AsmJsJitTemplate::Simd128_Xor_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_XorI4(const unaligned T* playout)
    {TRACE_IT(46376);
        AsmJsJitTemplate::Simd128_Xor_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2));
    }


    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SelectF4(const unaligned T* playout)
    {TRACE_IT(46377);
        AsmJsJitTemplate::Simd128_Select_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->B4_1), CalculateOffset<AsmJsSIMDValue>(playout->F4_2), CalculateOffset<AsmJsSIMDValue>(playout->F4_3));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SelectI4(const unaligned T* playout)
    {TRACE_IT(46378);
        AsmJsJitTemplate::Simd128_Select_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->B4_1), CalculateOffset<AsmJsSIMDValue>(playout->I4_2), CalculateOffset<AsmJsSIMDValue>(playout->I4_3));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_SelectD2(const unaligned T* playout)
    {TRACE_IT(46379);
        AsmJsJitTemplate::Simd128_Select_D2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<AsmJsSIMDValue>(playout->D2_2), CalculateOffset<AsmJsSIMDValue>(playout->D2_3));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_ExtractLaneI4(const unaligned T* playout)
    {TRACE_IT(46380);
        AsmJsJitTemplate::Simd128_ExtractLane_I4::ApplyTemplate(this, mPc, CalculateOffset<int>(playout->I0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<int>(playout->I2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_ExtractLaneF4(const unaligned T* playout)
    {TRACE_IT(46381);
        AsmJsJitTemplate::Simd128_ExtractLane_F4::ApplyTemplate(this, mPc, CalculateOffset<float>(playout->F0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<int>(playout->I2));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_ReplaceLaneI4(const unaligned T* playout)
    {TRACE_IT(46382);
        AsmJsJitTemplate::Simd128_ReplaceLane_I4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1), CalculateOffset<int>(playout->I2), CalculateOffset<int>(playout->I3));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_ReplaceLaneF4(const unaligned T* playout)
    {TRACE_IT(46383);
        AsmJsJitTemplate::Simd128_ReplaceLane_F4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1), CalculateOffset<int>(playout->I2), CalculateOffset<float>(playout->F3));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_I_ArgOutF4(const unaligned T* playout)
    {TRACE_IT(46384);
        AsmJsJitTemplate::Simd128_I_ArgOut_F4::ApplyTemplate(this, mPc, playout->R0, CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_I_ArgOutI4(const unaligned T* playout)
    {TRACE_IT(46385);
        AsmJsJitTemplate::Simd128_I_ArgOut_I4::ApplyTemplate(this, mPc, playout->R0, CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }
    template <class T>
    void Js::AsmJsEncoder::OP_Simd128_I_ArgOutD2(const unaligned T* playout)
    {TRACE_IT(46386);
        AsmJsJitTemplate::Simd128_I_ArgOut_D2::ApplyTemplate(this, mPc, playout->R0, CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }

    template <class T> void Js::AsmJsEncoder::OP_Simd128_I_Conv_VTF4(const unaligned T* playout)
    {TRACE_IT(46387);
        AsmJsJitTemplate::Simd128_I_Conv_VTF4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->F4_0), CalculateOffset<AsmJsSIMDValue>(playout->F4_1));
    }
    template <class T> void Js::AsmJsEncoder::OP_Simd128_I_Conv_VTI4(const unaligned T* playout)
    {TRACE_IT(46388);
        AsmJsJitTemplate::Simd128_I_Conv_VTI4::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->I4_0), CalculateOffset<AsmJsSIMDValue>(playout->I4_1));
    }
    template <class T> void Js::AsmJsEncoder::OP_Simd128_I_Conv_VTD2(const unaligned T* playout)
    {TRACE_IT(46389);
        AsmJsJitTemplate::Simd128_I_Conv_VTD2::ApplyTemplate(this, mPc, CalculateOffset<AsmJsSIMDValue>(playout->D2_0), CalculateOffset<AsmJsSIMDValue>(playout->D2_1));
    }
}
