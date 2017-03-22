//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

namespace Js
{
    namespace AsmJsJitTemplate
    {
#define IsPowerOfTwo(N) ((N >= 1) & !(N & (N - 1)))
#define Is64BitsReg(reg) (reg >= FIRST_FLOAT_REG)
#define Is8BitsReg(reg) (reg <= RegEBX)
#define Is64BitsOper() (sizeof(OperationSize) == 8)

#define Is128BitsOper() (sizeof(OperationSize) == 16)
#define Is128BitsReg(reg) Is64BitsReg(reg)

        const BYTE MOD0 = 0x0;
        const BYTE MOD1 = 0x40;
        const BYTE MOD2 = 0x80;
        const BYTE MOD3 = 0xC0;

        enum InstructionFlags
        {
            NoFlag = 0,
            AffectOp1 = 1 << 0,
        };

        struct AddressDefinition
        {
            AddressDefinition( RegNum _regEffAddr, int _offset ) :
                regEffAddr( _regEffAddr ),
                regEffAddr2( RegNOREG ),
                multiplier( 1 ),
                offset( _offset )
            {LOGMEIN("AsmJsInstructionTemplate.h] 35\n");
            }
            AddressDefinition( RegNum _regEffAddr, RegNum _regEffAddr2, int _multplier, int _offset ) :
                regEffAddr( _regEffAddr ),
                regEffAddr2( _regEffAddr2 ),
                multiplier( _multplier ),
                offset( _offset )
            {LOGMEIN("AsmJsInstructionTemplate.h] 42\n");
            }
            RegNum regEffAddr;
            RegNum regEffAddr2;
            int multiplier;
            int offset;
#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 50\n");
                if( regEffAddr2 == RegNOREG )
                {LOGMEIN("AsmJsInstructionTemplate.h] 52\n");
                    if (offset < 0)
                        Output::Print(_u("[%s-0x%X]"), RegNamesW[regEffAddr], offset * -1);
                    else
                        Output::Print( _u("[%s+0x%X]"), RegNamesW[regEffAddr], offset );
                }
                else
                {
                    if (offset < 0)
                        Output::Print(_u("[%s+%s*%d+0x%X]"), RegNamesW[regEffAddr], RegNamesW[regEffAddr2], multiplier, offset);
                    else
                        Output::Print( _u("[%s+%s*%d+0x%X]"), RegNamesW[regEffAddr], RegNamesW[regEffAddr2], multiplier, offset );
                }
            }
#endif
        };

        //  X1 : Unary instruction
        //  X1_X2 : X1 <--- X2
        //  X1_X2_X3 : X1 <--- X2 op X3
        enum FormatType
        {
            EMPTY,
            REG,
            ADDR,
            PTR,
            IMM,
            REG_PTR,
            REG_REG,
            REG_ADDR,
            ADDR_REG,
            REG_IMM,
            ADDR_IMM,
            REG_REG_IMM,
            REG_ADDR_IMM,
        };

        struct InstrParamsReg
        {
            InstrParamsReg(RegNum _reg ) :reg(_reg){LOGMEIN("AsmJsInstructionTemplate.h] 91\n");}
            RegNum reg;
            static const FormatType FORMAT_TYPE = REG;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 97\n");
                Output::Print( _u("%s"), RegNamesW[reg] );
            }
#endif
        };

        template<typename T>
        struct InstrParamsImm
        {
            InstrParamsImm(T _imm ) : imm(_imm){LOGMEIN("AsmJsInstructionTemplate.h] 106\n");}
            T imm;
            static const FormatType FORMAT_TYPE = IMM;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 112\n");
                Output::Print( _u("0x%X"), imm );
            }
#endif
        };
        struct InstrParamsEmpty
        {
            InstrParamsEmpty() {LOGMEIN("AsmJsInstructionTemplate.h] 119\n");}
            static const FormatType FORMAT_TYPE = EMPTY;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 124\n");
            }
#endif
        };

        struct InstrParamsPtr
        {
            InstrParamsPtr(const void* _addr ) : addr(_addr){LOGMEIN("AsmJsInstructionTemplate.h] 131\n");}
            const void* addr;
            static const FormatType FORMAT_TYPE = PTR;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 137\n");
                Output::Print( _u("ptr:0x%X"), (int)addr );
            }
#endif
        };

        struct InstrParamsRegPtr
        {
            InstrParamsRegPtr(RegNum _reg, const void* _addr ) : reg(_reg),addr(_addr){LOGMEIN("AsmJsInstructionTemplate.h] 145\n");}
            RegNum reg;
            const void* addr;
            static const FormatType FORMAT_TYPE = REG_PTR;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 152\n");
                Output::Print( _u("%s, ptr:0x%X"), RegNamesW[reg], (int)addr);
            }
#endif
        };


        struct InstrParamsAddr
        {
            InstrParamsAddr(RegNum _regEffAddr, int _offset ) :addr(_regEffAddr, _offset){LOGMEIN("AsmJsInstructionTemplate.h] 161\n");}
            AddressDefinition addr;
            static const FormatType FORMAT_TYPE = ADDR;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 167\n");
                addr.dump();
            }
#endif
        };

        struct InstrParams2Reg
        {
            InstrParams2Reg(RegNum _reg, RegNum _reg2) :reg(_reg), reg2(_reg2){LOGMEIN("AsmJsInstructionTemplate.h] 175\n");}
            RegNum reg, reg2;
            static const FormatType FORMAT_TYPE = REG_REG;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 181\n");
                Output::Print( _u("%s, %s"), RegNamesW[reg], RegNamesW[reg2] );
            }
#endif
        };

        // op   reg, [regEffAddr1 + regEffAddr2*multiplier + offset]
        struct InstrParamsRegAddr
        {
            InstrParamsRegAddr( RegNum _reg, RegNum _regEffAddr, int _offset ) :
                reg( _reg ),
                addr( _regEffAddr, _offset )
            {LOGMEIN("AsmJsInstructionTemplate.h] 193\n");
            }
            InstrParamsRegAddr( RegNum _reg, RegNum _regEffAddr, RegNum _regEffAddr2, int _multplier, int _offset ) :
                reg( _reg ),
                addr( _regEffAddr , _regEffAddr2 , _multplier , _offset )
            {LOGMEIN("AsmJsInstructionTemplate.h] 198\n");
            }
            RegNum reg;
            AddressDefinition addr;
            static const FormatType FORMAT_TYPE = REG_ADDR;
#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 205\n");
                Output::Print( _u("%s, "), RegNamesW[reg] );
                addr.dump();
            }
#endif
        };

        struct InstrParamsAddrReg
        {
            InstrParamsAddrReg( RegNum _regEffAddr, int _offset, RegNum _reg ) :
                reg( _reg ),
                addr( _regEffAddr, _offset )
            {LOGMEIN("AsmJsInstructionTemplate.h] 217\n");
            }
            InstrParamsAddrReg( RegNum _regEffAddr, RegNum _regEffAddr2, int _multplier, int _offset, RegNum _reg ) :
                reg( _reg ),
                addr( _regEffAddr , _regEffAddr2 , _multplier , _offset )
            {LOGMEIN("AsmJsInstructionTemplate.h] 222\n");
            }
            RegNum reg;
            AddressDefinition addr;
            static const FormatType FORMAT_TYPE = ADDR_REG;
#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 229\n");
                addr.dump();
                Output::Print( _u(" , %s"), RegNamesW[reg] );
            }
#endif
        };

        template<typename ImmType>
        struct InstrParamsRegImm
        {
            InstrParamsRegImm(RegNum _reg, ImmType _imm) :reg(_reg), imm(_imm){LOGMEIN("AsmJsInstructionTemplate.h] 239\n");}
            RegNum reg;
            ImmType imm;
            static const FormatType FORMAT_TYPE = REG_IMM;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 246\n");
                if (imm < 0)
                    Output::Print(_u("%s, -0x%X"), RegNamesW[reg], imm * -1);
                else
                    Output::Print(_u("%s, 0x%X"), RegNamesW[reg], imm);
            }
#endif
        };

        template<typename ImmType>
        struct InstrParamsAddrImm
        {
            InstrParamsAddrImm(RegNum _regEffAddr, int _offset, ImmType _imm) :
                addr(_regEffAddr, _offset),
                imm(_imm)
            {LOGMEIN("AsmJsInstructionTemplate.h] 261\n");}
            InstrParamsAddrImm(RegNum _regEffAddr, RegNum _regEffAddr2, int _multplier, int _offset, ImmType _imm) :
                addr( _regEffAddr , _regEffAddr2 , _multplier , _offset ),
                imm(_imm)
            {LOGMEIN("AsmJsInstructionTemplate.h] 265\n");}
            AddressDefinition addr;
            ImmType imm;
            static const FormatType FORMAT_TYPE = ADDR_IMM;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 272\n");
                addr.dump();
                if (imm < 0)
                    Output::Print(_u(", -0x%X"), imm * -1);
                else
                    Output::Print( _u(", 0x%X"), imm );
            }
#endif
        };

        // op   reg, [regEffAddr1 + regEffAddr2*multiplier + offset], imm8
        template<typename ImmType>
        struct InstrParamsRegAddrImm
        {
            CompileAssert(sizeof(ImmType) == 1);
            InstrParamsRegAddrImm(RegNum _reg, RegNum _regEffAddr, int _offset, ImmType imm) :
            reg(_reg),
            addr(_regEffAddr, _offset),
            imm(imm)
            {LOGMEIN("AsmJsInstructionTemplate.h] 291\n");
            }
            InstrParamsRegAddrImm(RegNum _reg, RegNum _regEffAddr, RegNum _regEffAddr2, int _multplier, int _offset, ImmType imm) :
                reg(_reg),
                addr(_regEffAddr, _regEffAddr2, _multplier, _offset),
                imm(imm)
            {LOGMEIN("AsmJsInstructionTemplate.h] 297\n");
            }
            RegNum reg;
            AddressDefinition addr;
            ImmType imm;
            static const FormatType FORMAT_TYPE = REG_ADDR_IMM;
#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 305\n");
                Output::Print(_u("%s, "), RegNamesW[reg]);
                addr.dump();
                if (imm < 0)
                    Output::Print(_u(", -0x%X"), imm * -1);
                else
                    Output::Print(_u(", 0x%X"), imm);
            }
#endif
        };

        // op reg, reg, imm8
        template<typename ImmType>
        struct InstrParams2RegImm
        {
            CompileAssert(sizeof(ImmType) == 1);
            InstrParams2RegImm(RegNum _reg, RegNum _reg2, ImmType imm) :reg(_reg), reg2(_reg2), imm(imm){LOGMEIN("AsmJsInstructionTemplate.h] 321\n");}
            RegNum reg, reg2;
            ImmType imm;
            static const FormatType FORMAT_TYPE = REG_REG_IMM;

#if DBG_DUMP
            void dump() const
            {LOGMEIN("AsmJsInstructionTemplate.h] 328\n");
                Output::Print(_u("%s, %s"), RegNamesW[reg], RegNamesW[reg2]);
                if (imm < 0)
                    Output::Print(_u(", -0x%X"), imm * -1);
                else
                    Output::Print(_u(", 0x%X"), imm);
            }
#endif
        };

        bool FitsInByte(size_t value)
        {LOGMEIN("AsmJsInstructionTemplate.h] 339\n");
            return ((size_t)(signed char)(value & 0xFF) == value);
        }

        bool FitsInByteUnsigned(size_t value)
        {LOGMEIN("AsmJsInstructionTemplate.h] 344\n");
            return ((size_t)(value & 0xFF) == value);
        }

        template <typename FormatType>
        int EncodeModRM_2Reg(BYTE*& buffer, const FormatType& params)
        {LOGMEIN("AsmJsInstructionTemplate.h] 350\n");
            *buffer++ = MOD3 | ( RegEncode[params.reg] << 3 ) | RegEncode[params.reg2];
            return 1;
        }

        template<BYTE b, typename FormatType>
        int EncodeModRM_ByteReg( BYTE*& buffer, const FormatType& params )
        {LOGMEIN("AsmJsInstructionTemplate.h] 357\n");
            *buffer++ = MOD3 | ( b << 3 ) | RegEncode[params.reg];
            return 1;
        }

        int EncodeModRM_Min( BYTE*& buffer, BYTE regByte, RegNum regEffAddr, int offset )
        {LOGMEIN("AsmJsInstructionTemplate.h] 363\n");
            // [offset]
            if( regEffAddr == RegNOREG )
            {LOGMEIN("AsmJsInstructionTemplate.h] 366\n");
                *buffer++ = MOD0 | ( regByte << 3 ) | 0x05;
                for( int i = 0; i < 4; i++ )
                {LOGMEIN("AsmJsInstructionTemplate.h] 369\n");
                    *buffer++ = (BYTE)offset & 0xFF;
                    offset >>= 8;
                }
                return 5;
            }

            // [reg+offset] or [ebp]
            if( offset || regEffAddr == RegEBP )
            {LOGMEIN("AsmJsInstructionTemplate.h] 378\n");
                // [reg + byte]
                if( FitsInByte( offset ) )
                {LOGMEIN("AsmJsInstructionTemplate.h] 381\n");
                    *buffer++ = MOD1 | ( regByte << 3 ) | RegEncode[regEffAddr];
                    // special case for esp
                    if( regEffAddr == RegESP )
                    {LOGMEIN("AsmJsInstructionTemplate.h] 385\n");
                        *buffer++ = 0x24; // SIB byte to esp scaled index none
                    }
                    *buffer++ = (BYTE)offset;
                    return 2;
                }

                // [reg + int]
                *buffer++ = MOD2 | ( regByte << 3 ) | RegEncode[regEffAddr];
                // special case for esp
                if( regEffAddr == RegESP )
                {LOGMEIN("AsmJsInstructionTemplate.h] 396\n");
                    *buffer++ = 0x24; // SIB byte to esp scaled index none
                }
                for( int i = 0; i < 4; i++ )
                {LOGMEIN("AsmJsInstructionTemplate.h] 400\n");
                    *buffer++ = (BYTE)offset & 0xFF;
                    offset >>= 8;
                }
                return 5;
            }

            // [reg]
            Assert( regEffAddr != RegEBP );
            if( regEffAddr == RegESP )
            {LOGMEIN("AsmJsInstructionTemplate.h] 410\n");
                // special case  [esp]
                *buffer++ = MOD0 | ( regByte << 3 ) | 0x04;
                *buffer++ = 0x24;
                return 2;
            }

            *buffer++ = MOD0 | ( regByte << 3 ) | RegEncode[regEffAddr];
            return 1;
        }

        int EncodeModRM( BYTE*& buffer, BYTE regByte, RegNum regEffAddr, RegNum regEffAddr2, int multiplier, int offset )
        {LOGMEIN("AsmJsInstructionTemplate.h] 422\n");
            Assert( !Is64BitsReg( regEffAddr ) );
            Assert( !Is64BitsReg( regEffAddr2 ) );
            AssertMsg( regEffAddr2 != RegESP, "Invalid encoding" );
            // Cannot have a multiplier with no register for second regAddr
            Assert( !(( regEffAddr2 == RegNOREG ) && ( multiplier != 1 )) );
            if( regEffAddr2 == RegNOREG )
            {LOGMEIN("AsmJsInstructionTemplate.h] 429\n");
                return EncodeModRM_Min( buffer, regByte, regEffAddr, offset );
            }
            // encode modr/m byte
            const bool offsetFitsInByte = FitsInByte( offset );
            BYTE mod = 0;
            // 0 = noEncoding, 1 = encode 1 byte, 2 = encode 4 bytes
            int offsetEncoding = 0;
            if( offset == 0 || regEffAddr == RegNOREG )
            {LOGMEIN("AsmJsInstructionTemplate.h] 438\n");
                mod = MOD0;
                if( regEffAddr == RegNOREG )
                {LOGMEIN("AsmJsInstructionTemplate.h] 441\n");
                    // encode 4 bytes even if offset is 0
                    offsetEncoding = 2;
                }
            }
            else if( offsetFitsInByte )
            {LOGMEIN("AsmJsInstructionTemplate.h] 447\n");
                mod = MOD1;
                offsetEncoding = 1;
            }
            else
            {
                mod = MOD2;
                offsetEncoding = 2;
            }
            *buffer++ = mod | ( regByte << 3 ) | 0x04;


            BYTE ss = 0;
            // encode SIB byte
            switch( multiplier )
            {LOGMEIN("AsmJsInstructionTemplate.h] 462\n");
            case 1:
                ss = MOD0;
                break;
            case 2:
                ss = MOD1;
                break;
            case 4:
                ss = MOD2;
                break;
            case 8:
                ss = MOD3;
                break;
            default:
                Assume( false );
            }
            BYTE sibReg = RegEncode[regEffAddr];
            if( regEffAddr == RegNOREG )
            {LOGMEIN("AsmJsInstructionTemplate.h] 480\n");
                sibReg = 0x05;
            }
            *buffer++ = ss | ( RegEncode[regEffAddr2] << 3 ) | sibReg;

            // encode offset
            if( offsetEncoding & 1 )
            {LOGMEIN("AsmJsInstructionTemplate.h] 487\n");
                *buffer++ = (BYTE)offset & 0xFF;
                return 3;
            }
            else if( offsetEncoding & 2 )
            {LOGMEIN("AsmJsInstructionTemplate.h] 492\n");
                for( int i = 0; i < 4; i++ )
                {LOGMEIN("AsmJsInstructionTemplate.h] 494\n");
                    *buffer++ = (BYTE)offset & 0xFF;
                    offset >>= 8;
                }
                return 6;
            }
            return 2;
        }

        template <typename FormatType>
        int EncodeModRM_RegRM(BYTE*& buffer, const FormatType& params)
        {LOGMEIN("AsmJsInstructionTemplate.h] 505\n");
            Assert( params.reg != RegNOREG );
            return EncodeModRM( buffer, RegEncode[params.reg], params.addr.regEffAddr, params.addr.regEffAddr2, params.addr.multiplier, params.addr.offset );
        }

        int EncodeModRM_RegPtr( BYTE*& buffer, const InstrParamsRegPtr& params )
        {LOGMEIN("AsmJsInstructionTemplate.h] 511\n");
            *buffer++ = MOD0 | RegEncode[params.reg] << 3 | 0x05;
            int addr = (int)params.addr;
            for( int i = 0; i < 4; i++ )
            {LOGMEIN("AsmJsInstructionTemplate.h] 515\n");
                *buffer++ = (BYTE)addr & 0xFF;
                addr >>= 8;
            }
            return 5;
        }

        template<BYTE b, typename FormatType>
        int EncodeModRM_ByteRM( BYTE*& buffer, const FormatType& params )
        {LOGMEIN("AsmJsInstructionTemplate.h] 524\n");
            return EncodeModRM( buffer, b, params.addr.regEffAddr, params.addr.regEffAddr2, params.addr.multiplier, params.addr.offset );
        }

        // encodes the opcode + register
        template<BYTE op, typename FormatType>
        int EncodeOpReg( BYTE*& buffer, const FormatType& params )
        {LOGMEIN("AsmJsInstructionTemplate.h] 531\n");
            *buffer++ = op | RegEncode[params.reg];
            return 1;
        }

        template<typename ImmType>
        int Encode_Immutable( BYTE*& buffer, ImmType imm )
        {LOGMEIN("AsmJsInstructionTemplate.h] 538\n");
            for( int i = 0; i < sizeof(ImmType); i++ )
            {LOGMEIN("AsmJsInstructionTemplate.h] 540\n");
                *buffer++ = imm & 0xFF;
                imm >>= 8;
            }
            return sizeof(ImmType);
        }

        int EncodeFarAddress( BYTE*& buffer, const InstrParamsPtr& params )
        {
            AssertMsg( false, "Todo:: need more work for encoding far addresses" );

            *(int32*)buffer = (int32)params.addr;
            buffer += 4;

            int16 a;
            __asm{
                mov a,cs
            };
            *(int16*)buffer = a;
            buffer += 2;
            return 6;
        }

        template<typename T>
        int Encode_Empty( BYTE*& buffer, const T& params )
        {LOGMEIN("AsmJsInstructionTemplate.h] 565\n");
            return 0;
        }

        //////////////////////////////////////////////////////////////////////////
        /// OpCode encoding functions

#define OpFuncSignature(name) template<int instrSize, typename ImmType> \
        int name##_OpFunc( BYTE*& buffer, FormatType formatType, void* params )

        template<int instrSize, typename ImmType, int opReg, int opAddr, int opImm>
        int GenericBinary_OpFunc( BYTE*& buffer, FormatType formatType )
        {LOGMEIN("AsmJsInstructionTemplate.h] 577\n");
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 579\n");
            case Js::AsmJsJitTemplate::REG_REG:
            case Js::AsmJsJitTemplate::REG_ADDR:
                *buffer++ = opReg | (int)( instrSize != 1 );
                break;
            case Js::AsmJsJitTemplate::ADDR_REG:
                *buffer++ = opAddr | (int)( instrSize != 1 );
                break;
            case Js::AsmJsJitTemplate::REG_IMM:
            case Js::AsmJsJitTemplate::ADDR_IMM:
                if( instrSize == sizeof( ImmType ) )
                {LOGMEIN("AsmJsInstructionTemplate.h] 590\n");
                    *buffer++ = opImm | (int)( instrSize != 1 );
                }
                else if( sizeof( ImmType ) == 1 )
                {LOGMEIN("AsmJsInstructionTemplate.h] 594\n");
                    *buffer++ = 0x83;
                }
                else
                {
                    AssertMsg( false, "Invalid format" );
                }
                break;
                break;
            default:
                Assume( false );
            }
            return 1;
        }

        OpFuncSignature( ADD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 610\n");
            return GenericBinary_OpFunc<instrSize, ImmType, 0x02, 0x00, 0x80>( buffer, formatType );
        }

        OpFuncSignature( ADDSD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 615\n");
            *buffer++ = 0xF2;
            *buffer++ = 0x0F;
            *buffer++ = 0x58;
            return 3;
        }

        OpFuncSignature(ADDSS)
        {LOGMEIN("AsmJsInstructionTemplate.h] 623\n");
            *buffer++ = 0xF3;
            *buffer++ = 0x0F;
            *buffer++ = 0x58;
            return 3;
        }

        OpFuncSignature(MULSS)
        {LOGMEIN("AsmJsInstructionTemplate.h] 631\n");
            *buffer++ = 0xF3;
            *buffer++ = 0x0F;
            *buffer++ = 0x59;
            return 3;
        }

        OpFuncSignature(DIVSS)
        {LOGMEIN("AsmJsInstructionTemplate.h] 639\n");
            *buffer++ = 0xF3;
            *buffer++ = 0x0F;
            *buffer++ = 0x5E;
            return 3;
        }

        OpFuncSignature( AND )
        {LOGMEIN("AsmJsInstructionTemplate.h] 647\n");
            return GenericBinary_OpFunc<instrSize, ImmType, 0x22, 0x20, 0x80>( buffer, formatType );
        }

        OpFuncSignature( BSR )
        {LOGMEIN("AsmJsInstructionTemplate.h] 652\n");
            *buffer++ = 0x0F;
            *buffer++ = 0xBD;
            return 2;
        }

        OpFuncSignature( CALL )
        {LOGMEIN("AsmJsInstructionTemplate.h] 659\n");
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 661\n");
            case Js::AsmJsJitTemplate::REG:
            case Js::AsmJsJitTemplate::ADDR:
                *buffer++ = 0xFF;
                break;
            case Js::AsmJsJitTemplate::PTR:
                *buffer++ = 0x9A;
                break;
            default:
                Assume( false );
            }
            return 1;
        }

        OpFuncSignature( CDQ )
        {LOGMEIN("AsmJsInstructionTemplate.h] 676\n");
            *buffer++ = 0x99;
            return 1;
        }

        OpFuncSignature( CMP )
        {LOGMEIN("AsmJsInstructionTemplate.h] 682\n");
            return GenericBinary_OpFunc<instrSize, ImmType, 0x3A, 0x38, 0x80>( buffer, formatType );
        }

        OpFuncSignature( COMISD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 687\n");
            *buffer++ = 0x66;
            *buffer++ = 0x0F;
            *buffer++ = 0x2F;
            return 3;
        }

        OpFuncSignature(COMISS)
        {LOGMEIN("AsmJsInstructionTemplate.h] 695\n");
            *buffer++ = 0x0F;
            *buffer++ = 0x2F;
            return 2;
        }

        OpFuncSignature( CVTDQ2PD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 702\n");
            *buffer++ = 0xF3;
            *buffer++ = 0x0F;
            *buffer++ = 0xE6;
            return 3;
        }

        OpFuncSignature( CVTPS2PD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 710\n");
            *buffer++ = 0x0F;
            *buffer++ = 0x5A;
            return 2;
        }

        OpFuncSignature( CVTSI2SD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 717\n");
            *buffer++ = 0xF2;
            *buffer++ = 0x0F;
            *buffer++ = 0x2A;
            return 3;
        }

        OpFuncSignature( CVTTSD2SI )
        {LOGMEIN("AsmJsInstructionTemplate.h] 725\n");
            *buffer++ = 0xF2;
            *buffer++ = 0x0F;
            *buffer++ = 0x2C;
            return 3;
        }

        OpFuncSignature( CVTSS2SD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 733\n");
            *buffer++ = 0xF3;
            *buffer++ = 0x0F;
            *buffer++ = 0x5A;
            return 3;
        }

        OpFuncSignature(CVTTSS2SI)
        {LOGMEIN("AsmJsInstructionTemplate.h] 741\n");
            *buffer++ = 0xF3;
            *buffer++ = 0x0F;
            *buffer++ = 0x2C;
            return 3;
        }

        OpFuncSignature( CVTSD2SS )
        {LOGMEIN("AsmJsInstructionTemplate.h] 749\n");
            *buffer++ = 0xF2;
            *buffer++ = 0x0F;
            *buffer++ = 0x5A;
            return 3;
        }

        OpFuncSignature(CVTSI2SS)
        {LOGMEIN("AsmJsInstructionTemplate.h] 757\n");
            *buffer++ = 0xF3;
            *buffer++ = 0x0F;
            *buffer++ = 0x2A;
            return 3;
        }

        OpFuncSignature( DIV )
        {LOGMEIN("AsmJsInstructionTemplate.h] 765\n");
            *buffer++ = 0xF6 | (int)( instrSize != 1 );
            return 1;
        }

        OpFuncSignature(IDIV)
        {LOGMEIN("AsmJsInstructionTemplate.h] 771\n");
            *buffer++ = 0xF6 | (int)(instrSize != 1);
            return 1;
        }
        OpFuncSignature( DIVSD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 776\n");
            *buffer++ = 0xF2;
            *buffer++ = 0x0F;
            *buffer++ = 0x5E;
            return 3;
        }

        OpFuncSignature( FLD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 784\n");
            // Add 4 if 64 bits
            *buffer++ = 0xD9 | ( ( instrSize == 8 ) << 2 );
            return 1;
        }

        OpFuncSignature( FSTP )
        {LOGMEIN("AsmJsInstructionTemplate.h] 791\n");
            // Add 4 if 64 bits
            *buffer++ = 0xD9 | ( ( instrSize == 8 ) << 2 );
            return 1;
        }

        OpFuncSignature( IMUL )
        {LOGMEIN("AsmJsInstructionTemplate.h] 798\n");
            *buffer++ = 0x0F;
            *buffer++ = 0xAF;
            return 2;
        }

        OpFuncSignature( INC )
        {LOGMEIN("AsmJsInstructionTemplate.h] 805\n");
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 807\n");
            case Js::AsmJsJitTemplate::REG:
                return 0; // encode nothing for op
            case Js::AsmJsJitTemplate::ADDR:
                *buffer++ = 0xFE | (int)( instrSize != 1 );
                return 1;
            default:
                Assume( false );
                break;
            }
            return 0;
        }

        OpFuncSignature( JMP )
        {LOGMEIN("AsmJsInstructionTemplate.h] 821\n");
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 823\n");
            case Js::AsmJsJitTemplate::REG:
            case Js::AsmJsJitTemplate::ADDR:
                *buffer++ = 0xFF;
                break;
            case Js::AsmJsJitTemplate::IMM:
                // add 2 if in 8bits
                *buffer++ = 0xE9 | ( ( sizeof(ImmType) == 1 ) << 1 );
                break;
            default:
                Assume( false );
                break;
            }
            return 1;
        }

        OpFuncSignature( LAHF )
        {LOGMEIN("AsmJsInstructionTemplate.h] 840\n");
            *buffer++ = 0x9F;
            return 1;
        }

        OpFuncSignature( MOV )
        {LOGMEIN("AsmJsInstructionTemplate.h] 846\n");
            int size = 1;
            if( instrSize == 2 )
            {LOGMEIN("AsmJsInstructionTemplate.h] 849\n");
                *buffer++ = 0x66;
                ++size;
            }
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 854\n");
            case Js::AsmJsJitTemplate::REG_REG:
            case Js::AsmJsJitTemplate::REG_ADDR:
                *buffer++ = 0x8A | (int)( instrSize != 1 );
                break;
            case Js::AsmJsJitTemplate::ADDR_REG:
                *buffer++ = 0x88 | (int)( instrSize != 1 );
                break;
            case Js::AsmJsJitTemplate::REG_IMM:
            case Js::AsmJsJitTemplate::ADDR_IMM:
                *buffer++ = 0xC6 | (int)( instrSize != 1 );
                break;
            default:
                Assume( false );
            }
            return size;
        }

        OpFuncSignature( MOVD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 873\n");
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 875\n");
            case Js::AsmJsJitTemplate::REG_REG:{LOGMEIN("AsmJsInstructionTemplate.h] 876\n");
                InstrParams2Reg* fparams = (InstrParams2Reg*)params;
                if( Is64BitsReg( fparams->reg ) )
                {LOGMEIN("AsmJsInstructionTemplate.h] 879\n");
                    Assert( !Is64BitsReg( fparams->reg2 ) );
                    Assert( instrSize == 8 || instrSize == 4); // Remove == 8 ? we are copying double-word.
                    *buffer++ = 0x66;
                    *buffer++ = 0x0F;
                    *buffer++ = 0x6E;
                }
                else
                {
                    Assert( Is64BitsReg( fparams->reg2 ) );
                    Assert( instrSize == 4 );
                    *buffer++ = 0x66;
                    *buffer++ = 0x0F;
                    *buffer++ = 0x7E;
                }
                return 3;
            }
            case Js::AsmJsJitTemplate::REG_ADDR:{LOGMEIN("AsmJsInstructionTemplate.h] 896\n");
                InstrParamsRegAddr* fparams = (InstrParamsRegAddr*)params;
                Assert( Is64BitsReg( fparams->reg ) );
                Assert( instrSize == 8 );
                *buffer++ = 0x66;
                *buffer++ = 0x0F;
                *buffer++ = 0x6E;
                return 3;
            }
            case Js::AsmJsJitTemplate::ADDR_REG:{LOGMEIN("AsmJsInstructionTemplate.h] 905\n");
                InstrParamsAddrReg* fparams = (InstrParamsAddrReg*)params;
                Assert( Is64BitsReg( fparams->reg ) );
                Assert( instrSize == 4 );
                *buffer++ = 0x66;
                *buffer++ = 0x0F;
                *buffer++ = 0x7E;
                return 3;
            }
            default:
                Assume( false );
            }
            return 0;
        }

        OpFuncSignature( MOVSD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 921\n");
            *buffer++ = 0xF2;
            *buffer++ = 0x0F;
            *buffer++ = 0x10 | (int)( formatType == ADDR_REG );
            return 3;
        }

        OpFuncSignature( MOVSS )
        {LOGMEIN("AsmJsInstructionTemplate.h] 929\n");
            *buffer++ = 0xF3;
            *buffer++ = 0x0F;
            *buffer++ = 0x10 | (int)( formatType == ADDR_REG );
            return 3;
        }

        OpFuncSignature( MOVSX )
        {LOGMEIN("AsmJsInstructionTemplate.h] 937\n");
            *buffer++ = 0x0F;
            *buffer++ = 0xBE | (int)( instrSize != 1 );
            return 2;
        }

        OpFuncSignature( MOVZX )
        {LOGMEIN("AsmJsInstructionTemplate.h] 944\n");
            *buffer++ = 0x0F;
            *buffer++ = 0xB6 | (int)( instrSize != 1 );
            return 2;
        }

        OpFuncSignature( MUL )
        {LOGMEIN("AsmJsInstructionTemplate.h] 951\n");
            *buffer++ = 0xF6 | (int)( instrSize != 1 );
            return 1;
        }

        OpFuncSignature( MULSD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 957\n");
            *buffer++ = 0xF2;
            *buffer++ = 0x0F;
            *buffer++ = 0x59;
            return 3;
        }

        OpFuncSignature( NEG )
        {LOGMEIN("AsmJsInstructionTemplate.h] 965\n");
            *buffer++ = 0xF6 | (int)( instrSize != 1 );
            return 1;
        }

        OpFuncSignature( NOT )
        {LOGMEIN("AsmJsInstructionTemplate.h] 971\n");
            *buffer++ = 0xF6 | (int)( instrSize != 1 );
            return 1;
        }

        OpFuncSignature( OR )
        {LOGMEIN("AsmJsInstructionTemplate.h] 977\n");
            return GenericBinary_OpFunc<instrSize, ImmType, 0x0A, 0x08, 0x80>( buffer, formatType );
        }

        OpFuncSignature( POP )
        {LOGMEIN("AsmJsInstructionTemplate.h] 982\n");
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 984\n");
            case Js::AsmJsJitTemplate::REG:
            {LOGMEIN("AsmJsInstructionTemplate.h] 986\n");
                InstrParamsReg* p = (InstrParamsReg*)params;
                *buffer++ = 0x58 | RegEncode[p->reg];
            }
                break;
            case Js::AsmJsJitTemplate::ADDR:
                *buffer++ = 0x8F;
                break;
            default:
                Assume( false );
                break;
            }
            return 1;
        }

        OpFuncSignature( PUSH )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1002\n");
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 1004\n");
            case Js::AsmJsJitTemplate::REG:
            case Js::AsmJsJitTemplate::ADDR:
                *buffer++ = 0xFF;
                break;
            case Js::AsmJsJitTemplate::IMM:
                // add 2 if in 8bits
                *buffer++ = 0x68 | ( ( sizeof(ImmType) == 1 ) << 1 );
                break;
            default:
                Assume( false );
                break;
            }
            return 1;
        }

        OpFuncSignature( RET )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1021\n");
            *buffer++ = 0xC2;
            return 1;
        }

#define ShiftInstruction(name)\
        OpFuncSignature( name )\
        {LOGMEIN("AsmJsInstructionTemplate.h] 1028\n");\
            switch( formatType )\
            {LOGMEIN("AsmJsInstructionTemplate.h] 1030\n");\
            case Js::AsmJsJitTemplate::REG:\
            case Js::AsmJsJitTemplate::ADDR:\
                *buffer++ = 0xD0 | (BYTE)(instrSize!= 1);\
                break;\
            case Js::AsmJsJitTemplate::REG_REG:\
                Assert( ((InstrParams2Reg*)params)->reg2 == RegECX );\
                *buffer++ = 0xD2 | (BYTE)(instrSize!= 1);\
                break;\
            case Js::AsmJsJitTemplate::ADDR_REG:\
                Assert( ((InstrParamsAddrReg*)params)->reg == RegECX );\
                *buffer++ = 0xD2 | (BYTE)(instrSize!= 1);\
                break;\
            case Js::AsmJsJitTemplate::REG_IMM:\
            case Js::AsmJsJitTemplate::ADDR_IMM:\
                Assert( sizeof( ImmType ) == 1 );\
                *buffer++ = 0xC0 | (BYTE)(instrSize!= 1);\
                break;\
            default:\
                Assume( false );\
                break;\
            }\
            return 1;\
        }

        ShiftInstruction(SAL)
        ShiftInstruction(SAR)
        ShiftInstruction(SHL)
        ShiftInstruction(SHR)
#undef ShiftInstruction

        OpFuncSignature( SUB )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1062\n");
            return GenericBinary_OpFunc<instrSize, ImmType, 0x2A, 0x28, 0x80>( buffer, formatType );
        }

        OpFuncSignature( SUBSD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1067\n");
            *buffer++ = 0xF2;
            *buffer++ = 0x0F;
            *buffer++ = 0x5C;
            return 3;
        }

        OpFuncSignature(SUBSS)
        {LOGMEIN("AsmJsInstructionTemplate.h] 1075\n");
            *buffer++ = 0xF3;
            *buffer++ = 0x0F;
            *buffer++ = 0x5C;
            return 3;
        }

        OpFuncSignature( TEST )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1083\n");
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 1085\n");
            case Js::AsmJsJitTemplate::REG_REG:
            case Js::AsmJsJitTemplate::ADDR_REG:
                *buffer++ = 0x84 | (int)( instrSize != 1 );
                break;
            case Js::AsmJsJitTemplate::REG_IMM:
            case Js::AsmJsJitTemplate::ADDR_IMM:
                *buffer++ = 0xF6 | (int)( instrSize != 1 );
                break;
            default:
                Assume( false );
            }
            return 1;
        }

        OpFuncSignature( UCOMISD )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1101\n");
            *buffer++ = 0x66;
            *buffer++ = 0x0F;
            *buffer++ = 0x2E;
            return 3;
        }

        OpFuncSignature(UCOMISS)
        {LOGMEIN("AsmJsInstructionTemplate.h] 1109\n");
            *buffer++ = 0x0F;
            *buffer++ = 0x2E;
            return 2;
        }
        OpFuncSignature( XOR )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1115\n");
            switch( formatType )
            {LOGMEIN("AsmJsInstructionTemplate.h] 1117\n");
            case Js::AsmJsJitTemplate::REG_REG:
            case Js::AsmJsJitTemplate::REG_ADDR:
                *buffer++ = 0x32 | (int)( instrSize != 1 );
                break;
            case Js::AsmJsJitTemplate::ADDR_REG:
                *buffer++ = 0x30 | (int)( instrSize != 1 );
                break;
            case Js::AsmJsJitTemplate::REG_IMM:
            case Js::AsmJsJitTemplate::ADDR_IMM:
                *buffer++ = 0x80 | (int)( instrSize != 1 );
                break;
            default:
                Assume( false );
            }
            return 1;
        }

        OpFuncSignature( XORPS )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1136\n");
            *buffer++ = 0x0F;
            *buffer++ = 0x57;
            return 2;
        }

        template<typename ImmType, int op>
        int JmpGeneric_OpFunc( BYTE*& buffer, FormatType formatType )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1144\n");
            if( sizeof(ImmType) != 1 )
            {LOGMEIN("AsmJsInstructionTemplate.h] 1146\n");
                *buffer++ = 0x0F;
                *buffer++ = op ^ 0xF0;
                return 2;
            }
            *buffer++ = op;
            return 1;
        }

#define Jcc(name,op) \
    OpFuncSignature(name){LOGMEIN("AsmJsInstructionTemplate.h] 1156\n");return JmpGeneric_OpFunc<ImmType, op>( buffer, formatType );}
        Jcc(JA  , 0x77 )
        Jcc(JAE , 0x73 )
        Jcc(JB  , 0x72 )
        Jcc(JBE , 0x76 )
        Jcc(JC  , 0x72 )
        Jcc(JE  , 0x74 )
        Jcc(JG  , 0x7F )
        Jcc(JGE , 0x7D )
        Jcc(JL  , 0x7C )
        Jcc(JLE , 0x7E )
        Jcc(JNA , 0x75 )
        Jcc(JNAE, 0x72 )
        Jcc(JNB , 0x73 )
        Jcc(JNBE, 0x77 )
        Jcc(JNC , 0x73 )
        Jcc(JNE , 0x75 )
        Jcc(JNG , 0x7E )
        Jcc(JNGE, 0x7C )
        Jcc(JNL , 0x7D )
        Jcc(JNLE, 0x7F )
        Jcc(JNO , 0x71 )
        Jcc(JNP , 0x7B )
        Jcc(JNS , 0x79 )
        Jcc(JNZ , 0x75 )
        Jcc(JO  , 0x70 )
        Jcc(JP  , 0x7A )
        Jcc(JPE , 0x7A )
        Jcc(JPO , 0x7B )
        Jcc(JS  , 0x78 )
        Jcc(JZ  , 0x74 )
#undef Jcc

        template<int op>
        int SetFlagGeneric_OpFunc( BYTE*& buffer, FormatType formatType )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1191\n");
            *buffer++ = op;
            return 1;
        }

#define SETFLAG(name,op) \
    OpFuncSignature(name){LOGMEIN("AsmJsInstructionTemplate.h] 1197\n");\
        *buffer++ = 0x0F;\
        *buffer++ = op;\
        return 1;\
    }

    SETFLAG(SETA  ,0x97)
    SETFLAG(SETAE ,0x93)
    SETFLAG(SETB  ,0x92)
    SETFLAG(SETBE ,0x96)
    SETFLAG(SETC  ,0x92)
    SETFLAG(SETE  ,0x94)
    SETFLAG(SETG  ,0x9F)
    SETFLAG(SETGE ,0x9D)
    SETFLAG(SETL  ,0x9C)
    SETFLAG(SETLE ,0x9E)
    SETFLAG(SETNA ,0x96)
    SETFLAG(SETNAE,0x92)
    SETFLAG(SETNB ,0x93)
    SETFLAG(SETNBE,0x97)
    SETFLAG(SETNC ,0x93)
    SETFLAG(SETNE ,0x95)
    SETFLAG(SETNG ,0x9E)
    SETFLAG(SETNGE,0x9C)
    SETFLAG(SETNL ,0x9D)
    SETFLAG(SETNLE,0x9F)
    SETFLAG(SETNO ,0x91)
    SETFLAG(SETNP ,0x9B)
    SETFLAG(SETNS ,0x99)
    SETFLAG(SETNZ ,0x95)
    SETFLAG(SETO  ,0x90)
    SETFLAG(SETP  ,0x9A)
    SETFLAG(SETPE ,0x9A)
    SETFLAG(SETPO ,0x9B)
    SETFLAG(SETS  ,0x98)
    SETFLAG(SETZ  ,0x94)
#undef SETFLAG

#define CMOV(name,op) \
    OpFuncSignature(name){LOGMEIN("AsmJsInstructionTemplate.h] 1236\n");\
        *buffer++ = 0x0F;\
        *buffer++ = op;\
        return 1;\
    }

    CMOV(CMOVA  , 0x47)
    CMOV(CMOVAE , 0x43)
    CMOV(CMOVB  , 0x42)
    CMOV(CMOVBE , 0x46)
    CMOV(CMOVC  , 0x42)
    CMOV(CMOVE  , 0x44)
    CMOV(CMOVG  , 0x4F)
    CMOV(CMOVGE , 0x4D)
    CMOV(CMOVL  , 0x4C)
    CMOV(CMOVLE , 0x4E)
    CMOV(CMOVNA , 0x46)
    CMOV(CMOVNAE, 0x42)
    CMOV(CMOVNB , 0x43)
    CMOV(CMOVNBE, 0x47)
    CMOV(CMOVNC , 0x43)
    CMOV(CMOVNE , 0x45)
    CMOV(CMOVNG , 0x4E)
    CMOV(CMOVNGE, 0x4C)
    CMOV(CMOVNL , 0x4D)
    CMOV(CMOVNLE, 0x4F)
    CMOV(CMOVNO , 0x41)
    CMOV(CMOVNP , 0x4B)
    CMOV(CMOVNS , 0x49)
    CMOV(CMOVNZ , 0x45)
    CMOV(CMOVO  , 0x40)
    CMOV(CMOVP  , 0x4A)
    CMOV(CMOVPE , 0x4A)
    CMOV(CMOVPO , 0x4B)
    CMOV(CMOVS  , 0x48)
    CMOV(CMOVZ  , 0x44)
#undef CMOV

    // SSE2 instructions
    OpFuncSignature( MOVUPS ){LOGMEIN("AsmJsInstructionTemplate.h] 1275\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x10 | (int)(formatType == ADDR_REG);
        return 2;
    }

    OpFuncSignature(MOVAPS){LOGMEIN("AsmJsInstructionTemplate.h] 1282\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x28 | (int)(formatType == ADDR_REG);
        return 2;
    }

    OpFuncSignature(MOVHPD){LOGMEIN("AsmJsInstructionTemplate.h] 1289\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x16 | (int)(formatType == ADDR_REG);
        return 3;
    }

    OpFuncSignature(MOVHLPS){LOGMEIN("AsmJsInstructionTemplate.h] 1297\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x12;
        return 2;
    }

    OpFuncSignature(MOVLHPS){LOGMEIN("AsmJsInstructionTemplate.h] 1304\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x16;
        return 2;
    }

    OpFuncSignature( SHUFPS ){LOGMEIN("AsmJsInstructionTemplate.h] 1311\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0xC6;
        return 2;
    }

    OpFuncSignature(SHUFPD){LOGMEIN("AsmJsInstructionTemplate.h] 1318\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xC6;
        return 3;
    }

    OpFuncSignature(PSHUFD){LOGMEIN("AsmJsInstructionTemplate.h] 1326\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x70;
        return 3;
    }

    OpFuncSignature(CVTPD2PS){LOGMEIN("AsmJsInstructionTemplate.h] 1334\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x5A;
        return 3;
    }

    OpFuncSignature(CVTDQ2PS){LOGMEIN("AsmJsInstructionTemplate.h] 1342\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x5B;
        return 2;
    }

    OpFuncSignature(CVTTPS2DQ)
    {LOGMEIN("AsmJsInstructionTemplate.h] 1350\n");
        *buffer++ = 0xF3;
        *buffer++ = 0x0F;
        *buffer++ = 0x5B;
        return 3;
    }

    OpFuncSignature(CVTTPD2DQ)
    {LOGMEIN("AsmJsInstructionTemplate.h] 1358\n");
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xE6;
        return 3;
    }

    OpFuncSignature(ANDPD){LOGMEIN("AsmJsInstructionTemplate.h] 1365\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x54;
        return 3;
    }

    OpFuncSignature(ANDNPS){LOGMEIN("AsmJsInstructionTemplate.h] 1373\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x55;
        return 2;
    }

    OpFuncSignature(ANDNPD){LOGMEIN("AsmJsInstructionTemplate.h] 1380\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x55;
        return 3;
    }

    OpFuncSignature(PXOR){LOGMEIN("AsmJsInstructionTemplate.h] 1388\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xEF;
        return 3;
    }

    OpFuncSignature(DIVPS){LOGMEIN("AsmJsInstructionTemplate.h] 1396\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x5E;
        return 2;
    }

    OpFuncSignature(DIVPD){LOGMEIN("AsmJsInstructionTemplate.h] 1403\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x5E;
        return 3;
    }

    OpFuncSignature(SQRTPS){LOGMEIN("AsmJsInstructionTemplate.h] 1411\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x51;
        return 2;
    }

    OpFuncSignature(SQRTPD){LOGMEIN("AsmJsInstructionTemplate.h] 1418\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x51;
        return 3;
    }

    OpFuncSignature(ADDPS){LOGMEIN("AsmJsInstructionTemplate.h] 1426\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x58;
        return 2;
    }

    OpFuncSignature(ADDPD){LOGMEIN("AsmJsInstructionTemplate.h] 1433\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x58;
        return 3;
    }

    OpFuncSignature(PADDD){LOGMEIN("AsmJsInstructionTemplate.h] 1441\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xFE;
        return 3;
    }

    OpFuncSignature(PADDB) {LOGMEIN("AsmJsInstructionTemplate.h] 1449\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xFC;
    return 3;
    }

    OpFuncSignature(SUBPS){LOGMEIN("AsmJsInstructionTemplate.h] 1457\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x5C;
        return 2;
    }

    OpFuncSignature(SUBPD){LOGMEIN("AsmJsInstructionTemplate.h] 1464\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x5C;
        return 3;
    }

    OpFuncSignature(PSUBD){LOGMEIN("AsmJsInstructionTemplate.h] 1472\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xFA;
        return 3;
    }

    OpFuncSignature(PSUBB) {LOGMEIN("AsmJsInstructionTemplate.h] 1480\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xF8;
        return 3;
    }

    OpFuncSignature(MULPS){LOGMEIN("AsmJsInstructionTemplate.h] 1488\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x59;
        return 2;
    }

    OpFuncSignature(MULPD){LOGMEIN("AsmJsInstructionTemplate.h] 1495\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x59;
        return 3;
    }

    OpFuncSignature(PMULUDQ){LOGMEIN("AsmJsInstructionTemplate.h] 1503\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xF4;
        return 3;
    }

    OpFuncSignature(PSRLDQ){LOGMEIN("AsmJsInstructionTemplate.h] 1511\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x73;
        return 3;
    }

    OpFuncSignature(PUNPCKLDQ){LOGMEIN("AsmJsInstructionTemplate.h] 1519\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x62;
        return 3;
    }

    OpFuncSignature(MINPS){LOGMEIN("AsmJsInstructionTemplate.h] 1527\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x5D;
        return 3;
    }

    OpFuncSignature(MAXPS){LOGMEIN("AsmJsInstructionTemplate.h] 1534\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x5F;
        return 3;
    }

    OpFuncSignature(MINPD){LOGMEIN("AsmJsInstructionTemplate.h] 1541\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x5D;
        return 3;
    }

    OpFuncSignature(MAXPD){LOGMEIN("AsmJsInstructionTemplate.h] 1549\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x5F;
        return 3;
    }

    OpFuncSignature(CMPPS){LOGMEIN("AsmJsInstructionTemplate.h] 1557\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0xC2;
        return 2;
    }

    OpFuncSignature(CMPPD){LOGMEIN("AsmJsInstructionTemplate.h] 1564\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xC2;
        return 3;
    }

    OpFuncSignature(PCMPGTD){LOGMEIN("AsmJsInstructionTemplate.h] 1572\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x66;
        return 3;
    }

    OpFuncSignature(PCMPGTB) {LOGMEIN("AsmJsInstructionTemplate.h] 1580\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x64;
        return 3;
    }


    OpFuncSignature(PCMPEQD){LOGMEIN("AsmJsInstructionTemplate.h] 1589\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x76;
        return 3;
    }

    OpFuncSignature(PCMPEQB) {LOGMEIN("AsmJsInstructionTemplate.h] 1597\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x74;
        return 3;
    }

    OpFuncSignature(ANDPS){LOGMEIN("AsmJsInstructionTemplate.h] 1605\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x54;
        return 2;
    }

    OpFuncSignature(PAND){LOGMEIN("AsmJsInstructionTemplate.h] 1612\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xDB;
        return 2;
    }

    OpFuncSignature(ORPS){LOGMEIN("AsmJsInstructionTemplate.h] 1620\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x56;
        return 2;
    }

    OpFuncSignature(POR){LOGMEIN("AsmJsInstructionTemplate.h] 1627\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0xEB;
        return 3;
    }

    OpFuncSignature(MOVMSKPS){LOGMEIN("AsmJsInstructionTemplate.h] 1635\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x0F;
        *buffer++ = 0x50;
        return 2;
    }

    OpFuncSignature(MOVMSKPD){LOGMEIN("AsmJsInstructionTemplate.h] 1642\n");
        CompileAssert(instrSize == sizeof(AsmJsSIMDValue));
        *buffer++ = 0x66;
        *buffer++ = 0x0F;
        *buffer++ = 0x50;
        return 3;
    }

    struct EncodingInfo
    {
        int opSize, operandSize, immSize;
        void Fill( int _opSize, int _operandSize, int _immSize )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1654\n");
            opSize = _opSize;
            operandSize = _operandSize;
            immSize = _immSize;
        }
        int GetSizeBeforeImm()    const {LOGMEIN("AsmJsInstructionTemplate.h] 1659\n");return opSize + operandSize;}
        int GetSizeBeforeOperand()const {LOGMEIN("AsmJsInstructionTemplate.h] 1660\n");return opSize;}
        int GetSizeBeforeOpCOde() const {LOGMEIN("AsmJsInstructionTemplate.h] 1661\n");return 0;}
    };

        // Dump generated bytes
#define DUMP_ASM_CODE_NB_BYTES 5
#define DUMP_ASM_CODE_PADDING(size) ((DUMP_ASM_CODE_NB_BYTES-size%DUMP_ASM_CODE_NB_BYTES)%DUMP_ASM_CODE_NB_BYTES)*5+1

    template<typename T>
    void DumpAsmCode( const BYTE* buffer, const int size, const char16* instructionName, T* params )
    {LOGMEIN("AsmJsInstructionTemplate.h] 1670\n");
#if DBG_DUMP
        if( PHASE_TRACE( AsmjsEncoderPhase, AsmJsJitTemplate::Globals::CurrentEncodingFunction ) )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1673\n");
            int j = 0;
            for( int i = size; i > 0; --i, ++j )
            {LOGMEIN("AsmJsInstructionTemplate.h] 1676\n");
                if( j == DUMP_ASM_CODE_NB_BYTES )
                {LOGMEIN("AsmJsInstructionTemplate.h] 1678\n");
                    Output::Print( _u("\n") ); j = 0;
                }
                Output::Print( _u("0x%02X "), buffer[-i] );
            }
            Output::Print( _u("%*c  %s "), DUMP_ASM_CODE_PADDING( size ), ' ', instructionName );
            if( params )
            {LOGMEIN("AsmJsInstructionTemplate.h] 1685\n");
                params->dump();
            }
            Output::Print( _u(" (size: %d)\n"), size );
        }
#endif
    }

#if DBG_DUMP
#define InstructionMembers(name, supInstrSize, flags) \
    static const int SupportedInstrSize = supInstrSize;\
    static const char16* InstructionName;\
    static const int Flags = flags;\
    static const char16* GetInstructionName() {LOGMEIN("AsmJsInstructionTemplate.h] 1698\n"); return InstructionName; }

#define InstructionMemberInit(name)\
    const char16* name::InstructionName = _u(#name);
#else
#define InstructionMembers(name, supInstrSize, flags) \
    static const int SupportedInstrSize = supInstrSize;\
    static const int Flags = flags;\
    static const char16* GetInstructionName() {LOGMEIN("AsmJsInstructionTemplate.h] 1706\n"); return _u(""); }
#define InstructionMemberInit(name)
#endif


#define InstructionStart(name, supInstrSize, maxSize, flags) \
    struct name {\
        InstructionMembers(name, supInstrSize, flags)\
    private:\
        template<int instrSize, typename ImmType> \
        static int EncodeOpFunc( BYTE*& buffer, FormatType formatType, void* params )\
        {LOGMEIN("AsmJsInstructionTemplate.h] 1717\n");\
            return name##_OpFunc<instrSize,ImmType>(buffer,formatType,params);\
        }\
    public:

#define InstructionEnd(name) \
    };\
    InstructionMemberInit(name);

// Structure for instructions
#define InstructionEmpty() \
    template<typename OperationSize> static int EncodeInstruction( BYTE*& buffer, EncodingInfo* info = nullptr )\
    {LOGMEIN("AsmJsInstructionTemplate.h] 1729\n");\
        CompileAssert((sizeof(OperationSize)&(SupportedInstrSize)));\
        CompileAssert(IsPowerOfTwo(sizeof(OperationSize)));\
        const int size = EncodeOpFunc<sizeof(OperationSize),int>(buffer,EMPTY,nullptr);\
        if(info) info->Fill(size,0,0); \
        DumpAsmCode<InstrParamsEmpty>(buffer,size,GetInstructionName(),nullptr);\
        return size;\
    }

#define InstructionFormat(check,Format,encodingfunc) \
    template<typename OperationSize> static int EncodeInstruction( BYTE*& buffer, const Format& params, EncodingInfo* info = nullptr )\
    {LOGMEIN("AsmJsInstructionTemplate.h] 1740\n");\
        CompileAssert((sizeof(OperationSize)&(SupportedInstrSize))); \
        CompileAssert(IsPowerOfTwo(sizeof(OperationSize))); \
        Assert(check);\
        const int opsize = EncodeOpFunc<sizeof(OperationSize),int>(buffer,Format::FORMAT_TYPE,(void*)&params);\
        const int operandSize = encodingfunc(buffer,params);\
        const int size = opsize+operandSize;\
        if(info) info->Fill(opsize,operandSize,0); \
        DumpAsmCode(buffer,size,GetInstructionName(),&params);\
        return size;\
    }

        // Structure for instructions with a constant value
#define InstructionFormat_Imm(check,Format,encodingfunc) \
    template<typename OperationSize, typename ImmType> static int EncodeInstruction( BYTE*& buffer, const Format<ImmType>& params, EncodingInfo* info = nullptr )\
    {LOGMEIN("AsmJsInstructionTemplate.h] 1755\n");\
        CompileAssert((sizeof(OperationSize)&(SupportedInstrSize)));\
        CompileAssert(IsPowerOfTwo(sizeof(OperationSize)));\
        Assert(check);\
        const int opsize = EncodeOpFunc<sizeof(OperationSize),ImmType>(buffer,Format<ImmType>::FORMAT_TYPE, (void*)&params) ;\
        const int operandSize = encodingfunc(buffer,params);\
        const int immSize = Encode_Immutable<ImmType>(buffer,params.imm);\
        const int size = opsize+operandSize+immSize;\
        if(info) info->Fill(opsize,operandSize,immSize); \
        DumpAsmCode(buffer,size,GetInstructionName(),&params);\
        return size;\
    }


#define FormatEmpty() \
    InstructionEmpty()

#define FormatUnaryPtr(encodingfunc) \
    InstructionFormat(\
        (true)\
        ,InstrParamsPtr\
        ,encodingfunc\
    )

#define FormatRegPtr(encodingfunc) \
    InstructionFormat(\
        (!Is64BitsOper() || Is64BitsReg(params.reg))\
        ,InstrParamsRegPtr\
        ,encodingfunc\
    )


#define FormatUnaryRegCustomCheck(encodingfunc,check) \
    InstructionFormat(\
        (check)\
        ,InstrParamsReg\
        ,encodingfunc\
    )

#define FormatUnaryReg(encodingfunc) \
    FormatUnaryRegCustomCheck(\
        encodingfunc,\
        (!(Is64BitsOper()^Is64BitsReg(params.reg)))\
    )

// Support only al,cl,dl,bl
#define FormatUnaryReg8Bits(encodingfunc) \
    FormatUnaryRegCustomCheck(\
        encodingfunc,\
        (!Is64BitsOper() && Is8BitsReg(params.reg))\
    )


#define FormatUnaryAddr(encodingfunc) \
    InstructionFormat(\
        (true)\
        ,InstrParamsAddr\
        ,encodingfunc\
    )

#define Format2RegCustomCheck(encodingfunc, check) \
    InstructionFormat(\
        (check)\
        ,InstrParams2Reg\
        ,encodingfunc\
    )

// Left register must be 64bits and right register must be 32 bits
// op xmm,r32
#define Format2Reg64_32(encodingfunc) \
    Format2RegCustomCheck(\
        encodingfunc,\
        Is64BitsReg(params.reg) && !Is64BitsReg(params.reg2)\
    )

// Left register must be 32 bits and right register must be 64 bits
// op r32,xmm
#define Format2Reg32_64(encodingfunc) \
    Format2RegCustomCheck(\
        encodingfunc,\
        !Is64BitsReg(params.reg) && Is64BitsReg(params.reg2)\
    )

#define Format2Reg(encodingfunc) \
    Format2RegCustomCheck(\
        encodingfunc,\
        ((!Is64BitsOper() || Is64BitsReg(params.reg)) && (!Is64BitsOper() || Is64BitsReg(params.reg2)))\
    )

#define FormatRegAddrCustomCheck(encodingfunc,check) \
    InstructionFormat(\
        (check)\
        ,InstrParamsRegAddr\
        ,encodingfunc\
    )

#define FormatRegAddr(encodingfunc) \
    FormatRegAddrCustomCheck(\
        encodingfunc,\
        (!Is64BitsOper() || Is64BitsReg(params.reg))\
    )

#define FormatAddrRegCustomCheck(encodingfunc,check) \
     InstructionFormat(\
        (check)\
        ,InstrParamsAddrReg\
        ,encodingfunc\
    )

#define FormatAddrReg(encodingfunc) \
     FormatAddrRegCustomCheck(\
        encodingfunc,\
        (!Is64BitsOper() || Is64BitsReg(params.reg))\
    )

#define FormatRegImm(encodingfunc) \
    InstructionFormat_Imm(\
        ( !Is64BitsOper() && !Is64BitsReg(params.reg) )\
        ,InstrParamsRegImm\
        ,encodingfunc\
    )

#define FormatAddrImm(encodingfunc) \
    InstructionFormat_Imm(\
        ( !Is64BitsOper() )\
        ,InstrParamsAddrImm\
        ,encodingfunc\
    )

#define FormatUnaryImm(encodingfunc) \
    InstructionFormat_Imm(\
        (!(Is64BitsOper()))\
        ,InstrParamsImm\
        ,encodingfunc\
    )

#define Format2RegImm8(encodingfunc) \
    InstructionFormat_Imm(\
    (Is128BitsOper() && Is128BitsReg(params.reg) && Is128BitsReg(params.reg2) && FitsInByteUnsigned(params.imm))\
    , InstrParams2RegImm\
    , encodingfunc\
    )

#define FormatRegAddrImm8(encodingfunc) \
    InstructionFormat_Imm(\
    (Is128BitsOper() && Is128BitsReg(params.reg) && FitsInByte(params.imm))\
    , InstrParamsRegAddrImm\
    , encodingfunc\
    )

#define FormatRegImm8(encodingfunc) \
    InstructionFormat_Imm(\
    (Is128BitsOper() && Is128BitsReg(params.reg) && FitsInByte(params.imm))\
    , InstrParamsRegImm\
    , encodingfunc\
    )

    #include "AsmJsInstructionTemplate.inl"
// cleanup macros
#undef InstructionStart

#undef Format2Reg
#undef FormatRegAddr
#undef FormatAddrReg
#undef FormatRegImm
#undef FormatAddrImm

#undef InstructionEnd

#undef InstructionMembers
#undef InstructionMemberInit
#undef InstructionFormat_Imm
#undef InstructionFormat

        int MovHigh8Bits( BYTE*& buffer, RegNum reg, int8 imm )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1930\n");
            Assert( reg <= RegEBX );
            BYTE* opDst = buffer;
            int size = MOV::EncodeInstruction<int8>( buffer, InstrParamsRegImm<int8>(reg, imm) );
            *opDst |= 4;
            return size;
        }
        enum High8BitsRegType
        {
            LOW_HIGH = 0x04,
            HIGH_LOW = 0x20,
            HIGH_HIGH = 0x24,
        };
        int MovHigh8Bits( BYTE*& buffer, RegNum reg, RegNum reg2, High8BitsRegType high8BitsRegType )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1944\n");
            Assert( reg <= RegEBX );
            Assert( reg2 <= RegEBX );
            BYTE* opDst = buffer;
            int size = MOV::EncodeInstruction<int8>( buffer, InstrParams2Reg(reg, reg2) );
            opDst[1] |= high8BitsRegType;
            return size;
        }
        int MovHigh8Bits( BYTE*& buffer, RegNum reg, RegNum regEffAddr, int offset )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1953\n");
            Assert( reg <= RegEBX );
            BYTE* opDst = buffer;
            int size = MOV::EncodeInstruction<int8>( buffer, InstrParamsRegAddr(reg, regEffAddr, offset) );
            opDst[1] |= 0x20;
            return size;
        }
        int MovHigh8Bits( BYTE*& buffer, RegNum regEffAddr, int offset, RegNum reg )
        {LOGMEIN("AsmJsInstructionTemplate.h] 1961\n");
            Assert( reg <= RegEBX );
            BYTE* opDst = buffer;
            int size = MOV::EncodeInstruction<int8>( buffer, InstrParamsAddrReg(regEffAddr, offset, reg) );
            opDst[1] |= 0x20;
            return size;
        }

        int ApplyCustomTemplate( BYTE*& buffer, const BYTE* src, const int size )
        {
            memcpy_s( buffer, size, src, size );
            buffer += size;
            DumpAsmCode<InstrParamsEmpty>( buffer, size, _u("Custom"),nullptr);
            return size;
        }
    };
}
