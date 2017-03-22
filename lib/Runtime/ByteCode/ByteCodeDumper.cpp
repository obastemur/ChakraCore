//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"
#if DBG_DUMP

#if DBG
// Parser Includes
#include "RegexCommon.h"
#include "DebugWriter.h"
#include "RegexPattern.h"
#endif

namespace Js
{
    // Pre-order recursive dump, head first, then children.
    void ByteCodeDumper::DumpRecursively(FunctionBody* dumpFunction)
    {LOGMEIN("ByteCodeDumper.cpp] 18\n");
        dumpFunction->EnsureDeserialized();
        ByteCodeDumper::Dump(dumpFunction);
        for (uint i = 0; i < dumpFunction->GetNestedCount(); i ++)
        {LOGMEIN("ByteCodeDumper.cpp] 22\n");
            dumpFunction->GetNestedFunctionForExecution(i);
            ByteCodeDumper::DumpRecursively(dumpFunction->GetNestedFunc(i)->GetFunctionBody());
        }
    }

    void ByteCodeDumper::Dump(FunctionBody* dumpFunction)
    {LOGMEIN("ByteCodeDumper.cpp] 29\n");
        ByteCodeReader reader;
        reader.Create(dumpFunction);
        StatementReader<FunctionBody::StatementMapList> statementReader;
        statementReader.Create(dumpFunction);
        dumpFunction->DumpFullFunctionName();
        Output::Print(_u(" ("));
        ArgSlot inParamCount = dumpFunction->GetInParamsCount();
        for (ArgSlot paramIndex = 0; paramIndex < inParamCount; paramIndex++)
        {LOGMEIN("ByteCodeDumper.cpp] 38\n");
            if (paramIndex > 0)
            {LOGMEIN("ByteCodeDumper.cpp] 40\n");
                Output::Print(_u(", "));
            }
            Output::Print(_u("In%hu"), paramIndex);
        }
        Output::Print(_u(") "));
        Output::Print(_u("(size: %d [%d])\n"), dumpFunction->GetByteCodeCount(), dumpFunction->GetByteCodeWithoutLDACount());
#if defined(DBG) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
        if (dumpFunction->IsInDebugMode())
        {LOGMEIN("ByteCodeDumper.cpp] 49\n");
            Output::Print(_u("[Bytecode was generated for debug mode]\n"));
        }
#endif
#if DBG
        if (dumpFunction->IsReparsed())
        {LOGMEIN("ByteCodeDumper.cpp] 55\n");
            Output::Print(_u("[A reparse is being done]\n"));
        }
#endif
        Output::Print(
            _u("      %u locals (%u temps from R%u), %u inline cache\n"),
            dumpFunction->GetLocalsCount(),
            dumpFunction->GetTempCount(),
            dumpFunction->GetFirstTmpReg(),
            dumpFunction->GetInlineCacheCount());
        uint32 statementIndex = 0;
        ByteCodeDumper::DumpConstantTable(dumpFunction);
        ByteCodeDumper::DumpImplicitArgIns(dumpFunction);
        while (true)
        {LOGMEIN("ByteCodeDumper.cpp] 69\n");
            while (statementReader.AtStatementBoundary(&reader))
            {LOGMEIN("ByteCodeDumper.cpp] 71\n");
                dumpFunction->PrintStatementSourceLine(statementIndex);
                statementIndex = statementReader.MoveNextStatementBoundary();
            }
            uint byteOffset = reader.GetCurrentOffset();
            LayoutSize layoutSize;
            OpCode op = reader.ReadOp(layoutSize);
            if (op == OpCode::EndOfBlock)
            {LOGMEIN("ByteCodeDumper.cpp] 79\n");
                Assert(reader.GetCurrentOffset() == dumpFunction->GetByteCode()->GetLength());
                break;
            }
            Output::Print(_u("    %04x %2s"), byteOffset, layoutSize == LargeLayout? _u("L-") : layoutSize == MediumLayout? _u("M-") : _u(""));
            DumpOp(op, layoutSize, reader, dumpFunction);
            if (Js::Configuration::Global.flags.Verbose)
            {LOGMEIN("ByteCodeDumper.cpp] 86\n");
                int layoutStart = byteOffset + 2; // Account fo the prefix op
                int endByteOffset = reader.GetCurrentOffset();
                Output::SkipToColumn(70);
                if (layoutSize == LargeLayout)
                {LOGMEIN("ByteCodeDumper.cpp] 91\n");
                    Output::Print(_u("%02X "),
                        op > Js::OpCode::MaxByteSizedOpcodes?
                            Js::OpCode::ExtendedLargeLayoutPrefix : Js::OpCode::LargeLayoutPrefix);
                }
                else if (layoutSize == MediumLayout)
                {LOGMEIN("ByteCodeDumper.cpp] 97\n");
                    Output::Print(_u("%02X "),
                        op > Js::OpCode::MaxByteSizedOpcodes?
                            Js::OpCode::ExtendedMediumLayoutPrefix : Js::OpCode::MediumLayoutPrefix);
                }
                else
                {
                    Assert(layoutSize == SmallLayout);
                    if (op > Js::OpCode::MaxByteSizedOpcodes)
                    {LOGMEIN("ByteCodeDumper.cpp] 106\n");
                        Output::Print(_u("%02X "), Js::OpCode::ExtendedOpcodePrefix);
                    }
                    else
                    {
                        Output::Print(_u("   "));
                        layoutStart--; // don't have a prefix
                    }
                }

                Output::Print(_u("%02x"), (byte)op);
                for (int i = layoutStart; i < endByteOffset; i++)
                {LOGMEIN("ByteCodeDumper.cpp] 118\n");
                    Output::Print(_u(" %02x"), reader.GetRawByte(i));
                }
            }
            Output::Print(_u("\n"));
        }
        if (statementReader.AtStatementBoundary(&reader))
        {LOGMEIN("ByteCodeDumper.cpp] 125\n");
            dumpFunction->PrintStatementSourceLine(statementIndex);
            statementIndex = statementReader.MoveNextStatementBoundary();
        }
        Output::Print(_u("\n"));
        Output::Flush();
    }

    void ByteCodeDumper::DumpConstantTable(FunctionBody *dumpFunction)
    {LOGMEIN("ByteCodeDumper.cpp] 134\n");
        Output::Print(_u("    Constant Table:\n    ======== =====\n    "));
        uint count = dumpFunction->GetConstantCount();
        for (RegSlot reg = FunctionBody::FirstRegSlot; reg < count; reg++)
        {LOGMEIN("ByteCodeDumper.cpp] 138\n");
            DumpReg(reg);
            Var varConst = dumpFunction->GetConstantVar(reg);
            Assert(varConst != nullptr);
            if (TaggedInt::Is(varConst))
            {LOGMEIN("ByteCodeDumper.cpp] 143\n");
#if ENABLE_NATIVE_CODEGEN
                Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::LdC_A_I4));
#else
                Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::Ld_A));
#endif
                DumpI4(TaggedInt::ToInt32(varConst));
            }
            else if (varConst == (Js::Var)&Js::NullFrameDisplay)
            {LOGMEIN("ByteCodeDumper.cpp] 152\n");
#if ENABLE_NATIVE_CODEGEN
                Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::LdNullDisplay));
#else
                Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::Ld_A));
                Output::Print(_u(" (NullDisplay)"));
#endif
            }
            else if (varConst == (Js::Var)&Js::StrictNullFrameDisplay)
            {LOGMEIN("ByteCodeDumper.cpp] 161\n");
#if ENABLE_NATIVE_CODEGEN
                Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::LdStrictNullDisplay));
#else
                Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::Ld_A));
                Output::Print(_u(" (StrictNullDisplay)"));
#endif
            }
            else
            {
                switch (JavascriptOperators::GetTypeId(varConst))
                {LOGMEIN("ByteCodeDumper.cpp] 172\n");
                case Js::TypeIds_Undefined:
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::Ld_A));
                    Output::Print(_u(" (undefined)"));
                    break;
                case Js::TypeIds_Null:
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::Ld_A));
                    Output::Print(_u(" (null)"));
                    break;
                case Js::TypeIds_Boolean:
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(
                        JavascriptBoolean::FromVar(varConst)->GetValue() ? OpCode::LdTrue : OpCode::LdFalse));
                    break;
                case Js::TypeIds_Number:
#if ENABLE_NATIVE_CODEGEN
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::LdC_A_R8));
#else
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::Ld_A));
#endif
                    Output::Print(_u("%G"), JavascriptNumber::GetValue(varConst));
                    break;
                case Js::TypeIds_String:
#if ENABLE_NATIVE_CODEGEN
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::LdStr));
#else
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::Ld_A));
#endif
                    Output::Print(_u(" (\"%s\")"), JavascriptString::FromVar(varConst)->GetSz());
                    break;
                case Js::TypeIds_GlobalObject:
#if ENABLE_NATIVE_CODEGEN
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::LdRoot));
#else
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::Ld_A));
#endif
                    break;
                case Js::TypeIds_ModuleRoot:
#if ENABLE_NATIVE_CODEGEN
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::LdModuleRoot));
#else
                    Output::Print(_u("%-10s"), OpCodeUtil::GetOpCodeName(OpCode::Ld_A));
#endif
                    DumpI4(dumpFunction->GetModuleID());
                    break;
                case Js::TypeIds_ES5Array:
                    // ES5Array objects in the constant table are always string template callsite objects.
                    // If we later put other ES5Array objects in the constant table, we'll need another way
                    // to decide the constant type.
                    Output::Print(_u("%-10s"), _u("LdStringTemplate"));
                    Output::Print(_u(" (\"%s\")"), dumpFunction->GetScriptContext()->GetLibrary()->GetStringTemplateCallsiteObjectKey(varConst));
                    break;
                default:
                    AssertMsg(UNREACHED, "Unexpected object type in DumpConstantTable");
                    break;
                }
            }
            Output::Print(_u("\n    "));
        }
        Output::Print(_u("\n"));
    }

    void ByteCodeDumper::DumpImplicitArgIns(FunctionBody * dumpFunction)
    {LOGMEIN("ByteCodeDumper.cpp] 234\n");
        if (dumpFunction->GetInParamsCount() <= 1 || !dumpFunction->GetHasImplicitArgIns())
        {LOGMEIN("ByteCodeDumper.cpp] 236\n");
            return;
        }
        Output::Print(_u("    Implicit Arg Ins:\n    ======== === ===\n    "));
        for (RegSlot reg = 1;
            reg < dumpFunction->GetInParamsCount(); reg++)
        {LOGMEIN("ByteCodeDumper.cpp] 242\n");
            DumpReg((RegSlot)(reg + dumpFunction->GetConstantCount() - 1));
            // DisableJIT-TODO: Should this entire function be ifdefed?
#if ENABLE_NATIVE_CODEGEN
            Output::Print(_u("%-11s"), OpCodeUtil::GetOpCodeName(Js::OpCode::ArgIn_A));
#endif
            Output::Print(_u("In%d\n    "), reg);
        }
        if (dumpFunction->GetHasRestParameter())
        {LOGMEIN("ByteCodeDumper.cpp] 251\n");
            DumpReg(dumpFunction->GetRestParamRegSlot());
#if ENABLE_NATIVE_CODEGEN
            Output::Print(_u("%-11s"), OpCodeUtil::GetOpCodeName(Js::OpCode::ArgIn_Rest));
#endif
            Output::Print(_u("In%d\n    "), dumpFunction->GetInParamsCount());
        }
        Output::Print(_u("\n"));
    }

    void ByteCodeDumper::DumpU4(uint32 value)
    {LOGMEIN("ByteCodeDumper.cpp] 262\n");
        Output::Print(_u(" uint:%u "), value);
    }

    void ByteCodeDumper::DumpI4(int value)
    {LOGMEIN("ByteCodeDumper.cpp] 267\n");
        Output::Print(_u(" int:%d "), value);
    }

    void ByteCodeDumper::DumpI8(int64 value)
    {LOGMEIN("ByteCodeDumper.cpp] 272\n");
        Output::Print(_u(" int64:%lld "), value);
    }

    void ByteCodeDumper::DumpU2(ushort value)
    {LOGMEIN("ByteCodeDumper.cpp] 277\n");
        Output::Print(_u(" ushort:%d "), value);
    }

    void ByteCodeDumper::DumpOffset(int byteOffset, ByteCodeReader const& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 282\n");
        Output::Print(_u(" x:%04x (%4d) "), reader.GetCurrentOffset() + byteOffset, byteOffset);
    }

    void ByteCodeDumper::DumpAddr(void* addr)
    {LOGMEIN("ByteCodeDumper.cpp] 287\n");
        Output::Print(_u(" addr:%04x "), addr);
    }

    void ByteCodeDumper::DumpR4(float value)
    {LOGMEIN("ByteCodeDumper.cpp] 292\n");
        Output::Print(_u(" float:%g "), value);
    }

    void ByteCodeDumper::DumpR8(double value)
    {LOGMEIN("ByteCodeDumper.cpp] 297\n");
        Output::Print(_u(" double:%g "), value);
    }

    void ByteCodeDumper::DumpReg(RegSlot registerID)
    {LOGMEIN("ByteCodeDumper.cpp] 302\n");
        Output::Print(_u(" R%d "), (int) registerID);
    }

    void ByteCodeDumper::DumpReg(RegSlot_TwoByte registerID)
    {LOGMEIN("ByteCodeDumper.cpp] 307\n");
        Output::Print(_u(" R%d "), (int) registerID);
    }

    void ByteCodeDumper::DumpReg(RegSlot_OneByte registerID)
    {LOGMEIN("ByteCodeDumper.cpp] 312\n");
        Output::Print(_u(" R%d "), (int) registerID);
    }

    void ByteCodeDumper::DumpProfileId(uint id)
    {LOGMEIN("ByteCodeDumper.cpp] 317\n");
        Output::Print(_u(" <%d> "), id);
    }

    void ByteCodeDumper::DumpEmpty(OpCode op, const unaligned OpLayoutEmpty * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 322\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 324\n");
            case OpCode::CommitScope:
            {LOGMEIN("ByteCodeDumper.cpp] 326\n");
                const Js::PropertyIdArray *propIds = dumpFunction->GetFormalsPropIdArray();
                ScriptContext* scriptContext = dumpFunction->GetScriptContext();
                Output::Print(_u(" %d ["), propIds->count);
                for (uint i = 0; i < propIds->count && i < 3; i++)
                {LOGMEIN("ByteCodeDumper.cpp] 331\n");
                    PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(propIds->elements[i]);
                    if (i != 0)
                    {LOGMEIN("ByteCodeDumper.cpp] 334\n");
                        Output::Print(_u(", "));
                    }
                    Output::Print(_u("%s"), pPropertyName->GetBuffer());
                }
                Output::Print(_u("]"));
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpCallI(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 347\n");
        if (data->Return != Constants::NoRegister)
        {LOGMEIN("ByteCodeDumper.cpp] 349\n");
            DumpReg((RegSlot)data->Return);
            Output::Print(_u("="));
        }
        Output::Print(_u(" R%d(ArgCount: %d)"), data->Function, data->ArgCount);
    }

    template <class T>
    void ByteCodeDumper::DumpCallIExtended(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpCallI(op, data, dumpFunction, reader);
        if (data->Options & Js::CallIExtended_SpreadArgs)
        {LOGMEIN("ByteCodeDumper.cpp] 361\n");
            const Js::AuxArray<uint32> *arr = reader.ReadAuxArray<uint32>(data->SpreadAuxOffset, dumpFunction);
            Output::Print(_u(" spreadArgs ["), arr->count);
            for (uint i = 0; i < arr->count; i++)
            {LOGMEIN("ByteCodeDumper.cpp] 365\n");
                if (i > 10)
                {LOGMEIN("ByteCodeDumper.cpp] 367\n");
                    Output::Print(_u(", ..."));
                    break;
                }
                if (i != 0)
                {LOGMEIN("ByteCodeDumper.cpp] 372\n");
                    Output::Print(_u(", "));
                }
                Output::Print(_u("%u"), arr->elements[i]);
            }
            Output::Print(_u("]"));
        }
    }

    template <class T>
    void ByteCodeDumper::DumpCallIFlags(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpCallI(op, data, dumpFunction, reader);
        Output::Print(_u(" <%04x> "), data->callFlags);
    }

    template <class T>
    void ByteCodeDumper::DumpCallIExtendedFlags(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpCallIFlags(op, data, dumpFunction, reader);
        if (data->Options & Js::CallIExtended_SpreadArgs)
        {LOGMEIN("ByteCodeDumper.cpp] 393\n");
            const Js::AuxArray<uint32> *arr = reader.ReadAuxArray<uint32>(data->SpreadAuxOffset, dumpFunction);
            Output::Print(_u(" spreadArgs ["), arr->count);
            for (uint i = 0; i < arr->count; i++)
            {LOGMEIN("ByteCodeDumper.cpp] 397\n");
                if (i > 10)
                {LOGMEIN("ByteCodeDumper.cpp] 399\n");
                    Output::Print(_u(", ..."));
                    break;
                }
                if (i != 0)
                {LOGMEIN("ByteCodeDumper.cpp] 404\n");
                    Output::Print(_u(", "));
                }
                Output::Print(_u("%u"), arr->elements[i]);
            }
            Output::Print(_u("]"));
        }
    }

    template <class T>
    void ByteCodeDumper::DumpCallIExtendedFlagsWithICIndex(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpCallIFlags(op, data, dumpFunction, reader);
        DumpCallIWithICIndex(op, data, dumpFunction, reader);
        if (data->Options & Js::CallIExtended_SpreadArgs)
        {LOGMEIN("ByteCodeDumper.cpp] 419\n");
            const Js::AuxArray<uint32> *arr = reader.ReadAuxArray<uint32>(data->SpreadAuxOffset, dumpFunction);
            Output::Print(_u(" spreadArgs ["), arr->count);
            for (uint i = 0; i < arr->count; i++)
            {LOGMEIN("ByteCodeDumper.cpp] 423\n");
                if (i > 10)
                {LOGMEIN("ByteCodeDumper.cpp] 425\n");
                    Output::Print(_u(", ..."));
                    break;
                }
                if (i != 0)
                {LOGMEIN("ByteCodeDumper.cpp] 430\n");
                    Output::Print(_u(", "));
                }
                Output::Print(_u("%u"), arr->elements[i]);
            }
            Output::Print(_u("]"));
        }
    }

    template <class T>
    void ByteCodeDumper::DumpCallIWithICIndex(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpCallI(op, data, dumpFunction, reader);
        Output::Print(_u(" <%d> "), data->inlineCacheIndex);
    }

    template <class T>
    void ByteCodeDumper::DumpCallIFlagsWithICIndex(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpCallI(op, data, dumpFunction, reader);
        Output::Print(_u(" <%d> "), data->inlineCacheIndex);
        Output::Print(_u(" <%d> "), data->callFlags);
    }

    template <class T>
    void ByteCodeDumper::DumpCallIExtendedWithICIndex(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpCallIWithICIndex(op, data, dumpFunction, reader);
        if (data->Options & Js::CallIExtended_SpreadArgs)
        {LOGMEIN("ByteCodeDumper.cpp] 459\n");
            const Js::AuxArray<uint32> *arr = reader.ReadAuxArray<uint32>(data->SpreadAuxOffset, dumpFunction);
            Output::Print(_u(" spreadArgs ["), arr->count);
            for (uint i=0; i < arr->count; i++)
            {LOGMEIN("ByteCodeDumper.cpp] 463\n");
                if (i > 10)
                {LOGMEIN("ByteCodeDumper.cpp] 465\n");
                    Output::Print(_u(", ..."));
                    break;
                }
                if (i != 0)
                {LOGMEIN("ByteCodeDumper.cpp] 470\n");
                    Output::Print(_u(", "));
                }
                Output::Print(_u("%u"), arr->elements[i]);
            }
            Output::Print(_u("]"));
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementI(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 481\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 483\n");
            case OpCode::ProfiledLdElemI_A:
            case OpCode::LdElemI_A:
            case OpCode::LdMethodElem:
            case OpCode::TypeofElem:
            {LOGMEIN("ByteCodeDumper.cpp] 488\n");
                Output::Print(_u(" R%d = R%d[R%d]"), data->Value, data->Instance, data->Element);
                break;
            }
            case OpCode::ProfiledStElemI_A:
            case OpCode::ProfiledStElemI_A_Strict:
            case OpCode::StElemI_A:
            case OpCode::StElemI_A_Strict:
            case OpCode::InitSetElemI:
            case OpCode::InitGetElemI:
            case OpCode::InitComputedProperty:
            case OpCode::InitClassMemberComputedName:
            case OpCode::InitClassMemberGetComputedName:
            case OpCode::InitClassMemberSetComputedName:
            {LOGMEIN("ByteCodeDumper.cpp] 502\n");
                Output::Print(_u(" R%d[R%d] = R%d"), data->Instance, data->Element, data->Value);
                break;
            }
            case OpCode::DeleteElemI_A:
            case OpCode::DeleteElemIStrict_A:
            {LOGMEIN("ByteCodeDumper.cpp] 508\n");
                Output::Print(_u(" R%d[R%d]"), data->Instance, data->Element);
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementI");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpReg2Int1(OpCode op, const unaligned T* data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 522\n");
         switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 524\n");
            case OpCode::LdThis:
            case OpCode::ProfiledLdThis:
                Output::Print(_u(" R%d = R%d, %d"), data->R0, data->R1, data->C1);
                break;
            case OpCode::LdIndexedFrameDisplay:
                Output::Print(_u(" R%d = [%d], R%d "), data->R0, data->C1, data->R1);
                break;
            case OpCode::GetCachedFunc:
                DumpReg(data->R0);
                Output::Print(_u("= func("));
                DumpReg(data->R1);
                Output::Print(_u(","));
                DumpI4(data->C1);
                Output::Print(_u(")"));
                break;
            default:
                AssertMsg(false, "Unknown OpCode for OpLayoutReg2Int1");
                break;
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementScopedU(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 548\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 553\n");
            case OpCode::LdElemUndefScoped:
            {LOGMEIN("ByteCodeDumper.cpp] 555\n");
                Output::Print(_u(" %s = undefined, R%d"), pPropertyName->GetBuffer(), Js::FunctionBody::RootObjectRegSlot);
                break;
            }
            case OpCode::InitUndeclConsoleLetFld:
            case OpCode::InitUndeclConsoleConstFld:
            {LOGMEIN("ByteCodeDumper.cpp] 561\n");
                Output::Print(_u(" %s = undefined"), pPropertyName->GetBuffer());
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for ElementScopedU");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementU(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 575\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 580\n");
            case OpCode::LdElemUndef:
            {LOGMEIN("ByteCodeDumper.cpp] 582\n");
                Output::Print(_u(" R%d.%s = undefined"), data->Instance, pPropertyName->GetBuffer());
                break;
            }
            // TODO: Change InitUndeclLetFld and InitUndeclConstFld to ElementU layout
            // case OpCode::InitUndeclLetFld:
            // case OpCode::InitUndeclConstFld:
            // {
            //     PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(data->PropertyIndex);
            //     Output::Print(_u(" R%d.%s"), data->Instance, pPropertyName->GetBuffer());
            //     break;
            // }
            case OpCode::ClearAttributes:
            {LOGMEIN("ByteCodeDumper.cpp] 595\n");
                Output::Print(_u(" R%d.%s.writable/enumerable/configurable = 0"), data->Instance, pPropertyName->GetBuffer());
                break;
            }

            case OpCode::DeleteLocalFld:
                Output::Print(_u(" R%d = %s "), data->Instance, pPropertyName->GetBuffer());
                break;

            case OpCode::StLocalFuncExpr:
                Output::Print(_u(" %s = R%d"), pPropertyName->GetBuffer(), data->Instance);
                break;

            default:
            {
                AssertMsg(false, "Unknown OpCode for ElementU");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementRootU(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 618\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 623\n");
            case OpCode::InitUndeclRootLetFld:
            case OpCode::InitUndeclRootConstFld:
            case OpCode::EnsureNoRootFld:
            case OpCode::EnsureNoRootRedeclFld:
            {LOGMEIN("ByteCodeDumper.cpp] 628\n");
                Output::Print(_u(" root.%s"), pPropertyName->GetBuffer());
                break;
            }
            case OpCode::LdLocalElemUndef:
            {LOGMEIN("ByteCodeDumper.cpp] 633\n");
                Output::Print(_u(" %s = undefined"), pPropertyName->GetBuffer());
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for ElementRootU");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementScopedC(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 647\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 652\n");
            case OpCode::ScopedEnsureNoRedeclFld:
            case OpCode::ScopedDeleteFld:
            case OpCode::ScopedDeleteFldStrict:
            {LOGMEIN("ByteCodeDumper.cpp] 656\n");
                Output::Print(_u(" %s, R%d"), pPropertyName->GetBuffer(), data->Value);
                break;
            }
            case OpCode::ScopedInitFunc:
            {LOGMEIN("ByteCodeDumper.cpp] 661\n");
                Output::Print(_u(" %s = R%d, R%d"), pPropertyName->GetBuffer(), data->Value,
                    Js::FunctionBody::RootObjectRegSlot);
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementScopedC");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementC(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 676\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 681\n");
            case OpCode::DeleteFld:
            case OpCode::DeleteRootFld:
            case OpCode::DeleteFldStrict:
            case OpCode::DeleteRootFldStrict:
            {LOGMEIN("ByteCodeDumper.cpp] 686\n");
                Output::Print(_u(" R%d.%s"), data->Instance, pPropertyName->GetBuffer());
                break;
            }
            case OpCode::InitSetFld:
            case OpCode::InitGetFld:
            case OpCode::InitClassMemberGet:
            case OpCode::InitClassMemberSet:
            {LOGMEIN("ByteCodeDumper.cpp] 694\n");
                Output::Print(_u(" R%d.%s = (Set/Get) R%d"), data->Instance, pPropertyName->GetBuffer(),
                        data->Value);
                break;
            }
            case OpCode::StFuncExpr:
            case OpCode::InitProto:
            {LOGMEIN("ByteCodeDumper.cpp] 701\n");
                Output::Print(_u(" R%d.%s = R%d"), data->Instance, pPropertyName->GetBuffer(),
                        data->Value);
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementC");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementScopedC2(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 716\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 721\n");
            case OpCode::ScopedLdInst:
            {LOGMEIN("ByteCodeDumper.cpp] 723\n");
                Output::Print(_u(" R%d, R%d = %s"), data->Value, data->Value2, pPropertyName->GetBuffer());
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementScopedC2");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementC2(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 737\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 742\n");
            case OpCode::LdSuperFld:
            {LOGMEIN("ByteCodeDumper.cpp] 744\n");
                Output::Print(_u(" R%d = R%d(this=R%d).%s #%d"), data->Value, data->Instance, data->Value2,
                        pPropertyName->GetBuffer(), data->PropertyIdIndex);
                break;
            }
            case OpCode::ProfiledLdSuperFld:
            {LOGMEIN("ByteCodeDumper.cpp] 750\n");
                Output::Print(_u(" R%d = R%d(this=R%d).%s #%d"), data->Value, data->Instance, data->Value2,
                        pPropertyName->GetBuffer(), data->PropertyIdIndex);
                DumpProfileId(data->PropertyIdIndex);
                break;
            }
            case OpCode::StSuperFld:
            {LOGMEIN("ByteCodeDumper.cpp] 757\n");
                Output::Print(_u(" R%d.%s(this=R%d) = R%d #%d"), data->Instance, pPropertyName->GetBuffer(),
                    data->Value2, data->Value, data->PropertyIdIndex);
                break;
            }
            case OpCode::ProfiledStSuperFld:
            {LOGMEIN("ByteCodeDumper.cpp] 763\n");
                Output::Print(_u(" R%d.%s(this=R%d) = R%d #%d"), data->Instance, pPropertyName->GetBuffer(),
                    data->Value2, data->Value, data->PropertyIdIndex);
                DumpProfileId(data->PropertyIdIndex);
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementC2");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpReg1Unsigned1(OpCode op, const unaligned T* data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 779\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 781\n");
            case OpCode::InvalCachedScope:
#if ENABLE_NATIVE_CODEGEN
            case OpCode::NewScopeSlots:
#endif
                Output::Print(_u(" R%u[%u]"), data->R0, data->C1);
                break;
            case OpCode::NewRegEx:
            {LOGMEIN("ByteCodeDumper.cpp] 789\n");
                DumpReg(data->R0);
#if DBG
                Output::Print(_u("="));
                UnifiedRegex::DebugWriter w;
                dumpFunction->GetLiteralRegex(data->C1)->Print(&w);
#else
                Output::Print(_u("=<regex>"));
#endif
                break;
            }
            case OpCode::InitForInEnumerator:
            {LOGMEIN("ByteCodeDumper.cpp] 801\n");
                DumpReg(data->R0);
                DumpU4(data->C1);
                break;
            }
            default:
                DumpReg(data->R0);
                Output::Print(_u("="));
                DumpU4(data->C1);
                break;
        };
    }

    template <class T>
    void ByteCodeDumper::DumpElementSlot(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 816\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 818\n");
            case OpCode::NewInnerStackScFunc:
            case OpCode::NewInnerScFunc:
            case OpCode::NewInnerScGenFunc:
            {LOGMEIN("ByteCodeDumper.cpp] 822\n");
                FunctionProxy* pfuncActual = dumpFunction->GetNestedFunctionProxy((uint)data->SlotIndex);
                Output::Print(_u(" R%d = env:R%d, %s()"), data->Value, data->Instance,
                        pfuncActual->EnsureDeserialized()->GetDisplayName());
                break;
            }
#if ENABLE_NATIVE_CODEGEN
            case OpCode::StSlot:
            case OpCode::StSlotChkUndecl:
#endif
            case OpCode::StObjSlot:
            case OpCode::StObjSlotChkUndecl:
                Output::Print(_u(" R%d[%d] = R%d "),data->Instance,data->SlotIndex,data->Value);
                break;
            case OpCode::LdSlot:
#if ENABLE_NATIVE_CODEGEN
            case OpCode::LdSlotArr:
#endif
            case OpCode::LdObjSlot:
                Output::Print(_u(" R%d = R%d[%d] "),data->Value,data->Instance,data->SlotIndex);
                break;
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementSlot");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementSlotI1(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 853\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 855\n");
            case OpCode::StLocalSlot:
            case OpCode::StLocalObjSlot:
            case OpCode::StLocalSlotChkUndecl:
            case OpCode::StLocalObjSlotChkUndecl:
                Output::Print(_u(" [%d] = R%d "),data->SlotIndex, data->Value);
                break;
            case OpCode::LdLocalSlot:
            case OpCode::LdParamSlot:
            case OpCode::LdEnvObj:
            case OpCode::LdLocalObjSlot:
            case OpCode::LdParamObjSlot:
                Output::Print(_u(" R%d = [%d] "),data->Value, data->SlotIndex);
                break;
            case OpCode::NewScFunc:
            case OpCode::NewStackScFunc:
            case OpCode::NewScGenFunc:
            {LOGMEIN("ByteCodeDumper.cpp] 872\n");
                FunctionProxy* pfuncActual = dumpFunction->GetNestedFunctionProxy((uint)data->SlotIndex);
                Output::Print(_u(" R%d = %s()"), data->Value,
                        pfuncActual->EnsureDeserialized()->GetDisplayName());
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementSlotI1");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementSlotI2(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 888\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 890\n");
            case OpCode::StInnerSlot:
            case OpCode::StInnerSlotChkUndecl:
            case OpCode::StInnerObjSlot:
            case OpCode::StInnerObjSlotChkUndecl:
            case OpCode::StEnvSlot:
            case OpCode::StEnvObjSlot:
            case OpCode::StEnvSlotChkUndecl:
            case OpCode::StEnvObjSlotChkUndecl:
            case OpCode::StModuleSlot:
                Output::Print(_u(" [%d][%d] = R%d "),data->SlotIndex1, data->SlotIndex2, data->Value);
                break;
            case OpCode::LdInnerSlot:
            case OpCode::LdInnerObjSlot:
            case OpCode::LdEnvSlot:
            case OpCode::LdEnvObjSlot:
            case OpCode::LdModuleSlot:
                Output::Print(_u(" R%d = [%d][%d] "),data->Value, data->SlotIndex1, data->SlotIndex2);
                break;
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementSlotI2");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementP(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 919\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyId propertyId = dumpFunction->GetPropertyIdFromCacheId(data->inlineCacheIndex);
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(propertyId);
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 924\n");
            case OpCode::ScopedLdFldForTypeOf:
            case OpCode::ScopedLdFld:
                Output::Print(_u(" R%d = %s, R%d #%d"), data->Value, pPropertyName->GetBuffer(),
                    Js::FunctionBody::RootObjectRegSlot, data->inlineCacheIndex);
                break;

            case OpCode::ScopedStFld:
            case OpCode::ConsoleScopedStFld:
            case OpCode::ScopedStFldStrict:
                Output::Print(_u(" %s = R%d, R%d #%d"), pPropertyName->GetBuffer(), data->Value,
                    Js::FunctionBody::RootObjectRegSlot, data->inlineCacheIndex);
                break;

            case OpCode::LdLocalFld:
                Output::Print(_u(" R%d = %s #%d"), data->Value, pPropertyName->GetBuffer(), data->inlineCacheIndex);
                break;

            case OpCode::ProfiledLdLocalFld:
                Output::Print(_u(" R%d = %s #%d"), data->Value, pPropertyName->GetBuffer(), data->inlineCacheIndex);
                DumpProfileId(data->inlineCacheIndex);
                break;

            case OpCode::StLocalFld:
            case OpCode::InitLocalFld:
            case OpCode::InitLocalLetFld:
            case OpCode::InitUndeclLocalLetFld:
            case OpCode::InitUndeclLocalConstFld:
                Output::Print(_u(" %s = R%d #%d"), pPropertyName->GetBuffer(), data->Value, data->inlineCacheIndex);
                break;

            case OpCode::ProfiledStLocalFld:
            case OpCode::ProfiledInitLocalFld:
                Output::Print(_u(" %s = R%d #%d"), pPropertyName->GetBuffer(), data->Value, data->inlineCacheIndex);
                DumpProfileId(data->inlineCacheIndex);
                break;

            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementP");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementPIndexed(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 971\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyId propertyId = dumpFunction->GetPropertyIdFromCacheId(data->inlineCacheIndex);
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(propertyId);
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 976\n");
            case OpCode::InitInnerFld:
            case OpCode::InitInnerLetFld:
            case OpCode::InitUndeclLetFld:
            case OpCode::InitUndeclConstFld:
                Output::Print(_u(" [%d].%s = R%d #%d"), data->scopeIndex, pPropertyName->GetBuffer(), data->Value, data->inlineCacheIndex);
                break;

            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementPIndexed");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementCP(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 994\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyId propertyId = dumpFunction->GetPropertyIdFromCacheId(data->inlineCacheIndex);
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(propertyId);
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 999\n");
            case OpCode::LdFldForTypeOf:
            case OpCode::LdFld:
            case OpCode::LdFldForCallApplyTarget:
            case OpCode::LdMethodFld:
            case OpCode::ScopedLdMethodFld:
            {LOGMEIN("ByteCodeDumper.cpp] 1005\n");
                Output::Print(_u(" R%d = R%d.%s #%d"), data->Value, data->Instance,
                        pPropertyName->GetBuffer(), data->inlineCacheIndex);
                break;
            }
            case OpCode::InitFld:
            case OpCode::InitLetFld:
            case OpCode::InitConstFld:
            case OpCode::StFld:
            case OpCode::StFldStrict:
            case OpCode::InitClassMember:
            {LOGMEIN("ByteCodeDumper.cpp] 1016\n");
                Output::Print(_u(" R%d.%s = R%d #%d"), data->Instance, pPropertyName->GetBuffer(),
                        data->Value, data->inlineCacheIndex);
                break;
            }
            case OpCode::ProfiledLdFldForTypeOf:
            case OpCode::ProfiledLdFld:
            case OpCode::ProfiledLdFldForCallApplyTarget:
            case OpCode::ProfiledLdMethodFld:
            {LOGMEIN("ByteCodeDumper.cpp] 1025\n");
                Output::Print(_u(" R%d = R%d.%s #%d"), data->Value, data->Instance,
                        pPropertyName->GetBuffer(), data->inlineCacheIndex);
                DumpProfileId(data->inlineCacheIndex);
                break;
            }
            case OpCode::ProfiledInitFld:
            case OpCode::ProfiledStFld:
            case OpCode::ProfiledStFldStrict:
            {LOGMEIN("ByteCodeDumper.cpp] 1034\n");
                Output::Print(_u(" R%d.%s = R%d #%d"), data->Instance, pPropertyName->GetBuffer(),
                        data->Value, data->inlineCacheIndex);
                DumpProfileId(data->inlineCacheIndex);
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementCP");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementRootCP(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1050\n");
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyId propertyId = dumpFunction->GetPropertyIdFromCacheId(data->inlineCacheIndex);
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(propertyId);
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1055\n");
            case OpCode::LdRootFld:
            case OpCode::LdRootMethodFld:
            case OpCode::LdRootFldForTypeOf:
            {LOGMEIN("ByteCodeDumper.cpp] 1059\n");
                Output::Print(_u(" R%d = root.%s #%d"), data->Value,
                        pPropertyName->GetBuffer(), data->inlineCacheIndex);
                break;
            }
            case OpCode::InitRootFld:
            case OpCode::InitRootLetFld:
            case OpCode::InitRootConstFld:
            case OpCode::StRootFld:
            case OpCode::StRootFldStrict:
            {LOGMEIN("ByteCodeDumper.cpp] 1069\n");
                Output::Print(_u(" root.%s = R%d #%d"), pPropertyName->GetBuffer(),
                        data->Value, data->inlineCacheIndex);
                break;
            }
            case OpCode::ProfiledLdRootFld:
            case OpCode::ProfiledLdRootFldForTypeOf:
            case OpCode::ProfiledLdRootMethodFld:
            {LOGMEIN("ByteCodeDumper.cpp] 1077\n");
                Output::Print(_u(" R%d = root.%s #%d"), data->Value,
                        pPropertyName->GetBuffer(), data->inlineCacheIndex);
                DumpProfileId(data->inlineCacheIndex);
                break;
            }
            case OpCode::ProfiledInitRootFld:
            case OpCode::ProfiledStRootFld:
            case OpCode::ProfiledStRootFldStrict:
            {LOGMEIN("ByteCodeDumper.cpp] 1086\n");
                Output::Print(_u(" root.%s = R%d #%d"), pPropertyName->GetBuffer(),
                        data->Value, data->inlineCacheIndex);
                DumpProfileId(data->inlineCacheIndex);
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutElementRootCP");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpElementUnsigned1(OpCode op, const unaligned T * data, Js::FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1102\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1104\n");
            case OpCode::StArrItemC_CI4:
            case OpCode::StArrItemI_CI4:
            case OpCode::StArrSegItem_CI4:
            case OpCode::StArrInlineItem_CI4:
                Output::Print(_u(" R%d["), data->Instance);
                DumpI4(data->Element);
                Output::Print(_u("] = R%d"), data->Value);
                break;
            default:
                AssertMsg(false, "Unknown OpCode for OpLayoutElementUnsigned1");
                break;
        }
    }

    template <class T>
    void ByteCodeDumper::DumpArg(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1121\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1123\n");
            case OpCode::ProfiledArgOut_A:
            case OpCode::ArgOut_A:
            case OpCode::ArgOut_ANonVar:
            {LOGMEIN("ByteCodeDumper.cpp] 1127\n");
                Output::Print(_u(" Out%d ="), (int) data->Arg);
                DumpReg(data->Reg);
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutArg");
                break;
            }
        }
    }

    template <class T>
    void ByteCodeDumper::DumpArgNoSrc(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1142\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1144\n");
            case Js::OpCode::ArgOut_Env:
            {LOGMEIN("ByteCodeDumper.cpp] 1146\n");
                Output::Print(_u(" Out%d "), (int) data->Arg);
                break;
            }
            default:
            {
                AssertMsg(false, "Unknown OpCode for OpLayoutArgNoSrc");
                break;
            }
        }
    }

    void
    ByteCodeDumper::DumpStartCall(OpCode op, const unaligned OpLayoutStartCall * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1160\n");
        Assert(op == OpCode::StartCall );
        Output::Print(_u(" ArgCount: %d"), data->ArgCount);
    }

    template <class T> void
    ByteCodeDumper::DumpUnsigned1(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1167\n");
        DumpU4(data->C1);
    }

    template <class T> void
    ByteCodeDumper::DumpReg1(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1173\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1175\n");
        case OpCode::ObjectFreeze:
            Output::Print(_u(" R%d.freeze()"), data->R0);
            break;
        default:
            DumpReg(data->R0);
            break;
        }
    }

    template <class T> void
    ByteCodeDumper::DumpReg2(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1187\n");
        DumpReg(data->R0);
        DumpReg(data->R1);
    }

    template <class T> void
    ByteCodeDumper::DumpReg2WithICIndex(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg2(op, data, dumpFunction, reader);
        Output::Print(_u(" <%d> "), data->inlineCacheIndex);
    }

    template <class T> void
    ByteCodeDumper::DumpReg3(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1201\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1203\n");
        case OpCode::NewInnerScopeSlots:
            Output::Print(_u(" [%d], %d, %d "), data->R0, data->R1, data->R2);
            break;

        default:
            DumpReg(data->R0);
            DumpReg(data->R1);
            DumpReg(data->R2);
            break;
        }
    }

    template <class T> void
    ByteCodeDumper::DumpReg3C(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1218\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1220\n");
        case OpCode::IsInst:
            Output::Print(_u("R%d = R%d instanceof R%d #%d"),
                data->R0, data->R1, data->R2, data->inlineCacheIndex);
            break;
        default:
            AssertMsg(false, "Unknown OpCode for OpLayoutReg3C");
        }
    }

    template <class T> void
    ByteCodeDumper::DumpReg4(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1232\n");
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpReg(data->R2);
        DumpReg(data->R3);
    }

    template <class T> void
    ByteCodeDumper::DumpReg2B1(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1241\n");
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpI4(data->B2);
    }

    template <class T> void
    ByteCodeDumper::DumpReg3B1(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1249\n");
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpReg(data->R2);
        DumpI4(data->B3);
    }

    template <class T> void
    ByteCodeDumper::DumpReg5(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1258\n");
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpReg(data->R2);
        DumpReg(data->R3);
        DumpReg(data->R4);
    }

    void
    ByteCodeDumper::DumpW1(OpCode op, const unaligned OpLayoutW1 * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1268\n");
        DumpU2(data->C1);
    }

    void
    ByteCodeDumper::DumpReg1Int2(OpCode op, const unaligned OpLayoutReg1Int2 * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1274\n");
        DumpReg(data->R0);
        Output::Print(_u("="));
        DumpI4(data->C1);
        DumpI4(data->C2);
    }

    void
    ByteCodeDumper::DumpAuxNoReg(OpCode op, const unaligned OpLayoutAuxNoReg * playout, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1283\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1285\n");
            case Js::OpCode::InitCachedFuncs:
            {LOGMEIN("ByteCodeDumper.cpp] 1287\n");
                const Js::FuncInfoArray *arr = reader.ReadAuxArray<FuncInfoEntry>(playout->Offset, dumpFunction);
                Output::Print(_u(" %d ["), arr->count);
                for (uint i = 0; i < arr->count && i < 3; i++)
                {LOGMEIN("ByteCodeDumper.cpp] 1291\n");
                    Js::ParseableFunctionInfo *info = dumpFunction->GetNestedFunctionForExecution(arr->elements[i].nestedIndex);
                    if (i != 0)
                    {LOGMEIN("ByteCodeDumper.cpp] 1294\n");
                        Output::Print(_u(", "));
                    }
                    Output::Print(_u("%s"), info->GetDisplayName());
                }
                Output::Print(_u("]"));
                break;
            }
            default:
                AssertMsg(false, "Unknown OpCode for OpLayoutType::AuxNoReg");
                break;
        }
    }

    void
    ByteCodeDumper::DumpAuxiliary(OpCode op, const unaligned OpLayoutAuxiliary * playout, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1310\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1312\n");
            case OpCode::NewScObjectLiteral:
            case OpCode::LdPropIds:
            {LOGMEIN("ByteCodeDumper.cpp] 1315\n");
                const Js::PropertyIdArray *propIds = reader.ReadPropertyIdArray(playout->Offset, dumpFunction);
                ScriptContext* scriptContext = dumpFunction->GetScriptContext();
                DumpReg(playout->R0);
                Output::Print(_u("= %d ["), propIds->count);
                for (uint i=0; i< propIds->count && i < 3; i++)
                {LOGMEIN("ByteCodeDumper.cpp] 1321\n");
                    PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(propIds->elements[i]);
                    if (i != 0)
                    {LOGMEIN("ByteCodeDumper.cpp] 1324\n");
                        Output::Print(_u(", "));
                    }
                    Output::Print(_u("%s"), pPropertyName->GetBuffer());
                }
                if (propIds->count >= 3)
                {LOGMEIN("ByteCodeDumper.cpp] 1330\n");
                    Output::Print(_u(", ..."));
                }
                Output::Print(_u("], LiteralId %d"), playout->C1);
                break;
            }
            case OpCode::StArrSegItem_A:
            {LOGMEIN("ByteCodeDumper.cpp] 1337\n");
                const Js::VarArray *vars = reader.ReadAuxArray<Var>(playout->Offset, dumpFunction);
                DumpReg(playout->R0);
                Output::Print(_u("= %d ["), vars->count);
                uint i=0;
                for (; i<vars->count && i < 3; i++)
                {LOGMEIN("ByteCodeDumper.cpp] 1343\n");
                    if (i != 0)
                    {LOGMEIN("ByteCodeDumper.cpp] 1345\n");
                        Output::Print(_u(", "));
                    }
                    Output::Print(_u("%d"), vars->elements[i]);
                }
                if (i != vars->count)
                {LOGMEIN("ByteCodeDumper.cpp] 1351\n");
                    Output::Print(_u(", ..."));
                }
                Output::Print(_u("]"));
                break;
            }
            case OpCode::NewScIntArray:
            {LOGMEIN("ByteCodeDumper.cpp] 1358\n");
                const Js::AuxArray<int32> *intArray = reader.ReadAuxArray<int32>(playout->Offset, dumpFunction);
                Output::Print(_u(" R%d = %d ["), playout->R0, intArray->count);
                uint i;
                for (i = 0; i<intArray->count && i < 3; i++)
                {LOGMEIN("ByteCodeDumper.cpp] 1363\n");
                    if (i != 0)
                    {LOGMEIN("ByteCodeDumper.cpp] 1365\n");
                        Output::Print(_u(", "));
                    }
                    Output::Print(_u("%d"), intArray->elements[i]);
                }
                if (i != intArray->count)
                {LOGMEIN("ByteCodeDumper.cpp] 1371\n");
                    Output::Print(_u(", ..."));
                }
                Output::Print(_u("]"));
                break;
            }
            case OpCode::NewScFltArray:
            {LOGMEIN("ByteCodeDumper.cpp] 1378\n");
                const Js::AuxArray<double> *dblArray = reader.ReadAuxArray<double>(playout->Offset, dumpFunction);
                Output::Print(_u(" R%d = %d ["), playout->R0, dblArray->count);
                uint i;
                for (i = 0; i<dblArray->count && i < 3; i++)
                {LOGMEIN("ByteCodeDumper.cpp] 1383\n");
                    if (i != 0)
                    {LOGMEIN("ByteCodeDumper.cpp] 1385\n");
                        Output::Print(_u(", "));
                    }
                    Output::Print(_u("%f"), dblArray->elements[i]);
                }
                if (i != dblArray->count)
                {LOGMEIN("ByteCodeDumper.cpp] 1391\n");
                    Output::Print(_u(", ..."));
                }
                Output::Print(_u("]"));
                break;
            }
            case OpCode::NewScObject_A:
            {LOGMEIN("ByteCodeDumper.cpp] 1398\n");
                const Js::VarArrayVarCount *vars = reader.ReadVarArrayVarCount(playout->Offset, dumpFunction);
                DumpReg(playout->R0);
                int count = Js::TaggedInt::ToInt32(vars->count);
                Output::Print(_u("= %d ["), count);
                int i=0;
                for (; i<count && i < 3; i++)
                {LOGMEIN("ByteCodeDumper.cpp] 1405\n");
                    if (i != 0)
                    {LOGMEIN("ByteCodeDumper.cpp] 1407\n");
                        Output::Print(_u(", "));
                    }
                    if (TaggedInt::Is(vars->elements[i]))
                    {LOGMEIN("ByteCodeDumper.cpp] 1411\n");
                        Output::Print(_u("%d"), TaggedInt::ToInt32(vars->elements[i]));
                    }
                    else if (JavascriptNumber::Is(vars->elements[i]))
                    {LOGMEIN("ByteCodeDumper.cpp] 1415\n");
                        Output::Print(_u("%g"), JavascriptNumber::GetValue(vars->elements[i]));
                    }
                    else
                    {
                        Assert(false);
                    }
                }
                if (i != count)
                {LOGMEIN("ByteCodeDumper.cpp] 1424\n");
                    Output::Print(_u(", ..."));
                }
                Output::Print(_u("]"));
                break;
            }
            default:
                AssertMsg(false, "Unknown OpCode for OpLayoutType::Auxiliary");
                break;
        }
    }

    void
    ByteCodeDumper::DumpReg2Aux(OpCode op, const unaligned OpLayoutReg2Aux * playout, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1438\n");
        switch (op)
        {LOGMEIN("ByteCodeDumper.cpp] 1440\n");
        case Js::OpCode::SpreadArrayLiteral:
        {LOGMEIN("ByteCodeDumper.cpp] 1442\n");
            const Js::AuxArray<uint32> *arr = reader.ReadAuxArray<uint32>(playout->Offset, dumpFunction);
            Output::Print(_u(" R%u <- R%u, %u spreadArgs ["), playout->R0, playout->R1, arr->count);
            for (uint i = 0; i < arr->count; i++)
            {LOGMEIN("ByteCodeDumper.cpp] 1446\n");
                if (i > 10)
                {LOGMEIN("ByteCodeDumper.cpp] 1448\n");
                    Output::Print(_u(", ..."));
                    break;
                }
                if (i != 0)
                {LOGMEIN("ByteCodeDumper.cpp] 1453\n");
                    Output::Print(_u(", "));
                }
                Output::Print(_u("%u"), arr->elements[i]);
            }
            Output::Print(_u("]"));
            break;
        }
        default:
            AssertMsg(false, "Unknown OpCode for OpLayoutType::Reg2Aux");
            break;
        }
    }

    template <class T>
    void ByteCodeDumper::DumpClass(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1469\n");
        DumpReg(data->Constructor);
        if (data->Extends != Js::Constants::NoRegister)
        {LOGMEIN("ByteCodeDumper.cpp] 1472\n");
            Output::Print(_u("extends"));
            DumpReg((RegSlot)data->Extends);
        }
    }

#ifdef BYTECODE_BRANCH_ISLAND
    void ByteCodeDumper::DumpBrLong(OpCode op, const unaligned OpLayoutBrLong* data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1480\n");
        DumpOffset(data->RelativeJumpOffset, reader);
    }
#endif

    void ByteCodeDumper::DumpBr(OpCode op, const unaligned OpLayoutBr * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1486\n");
        DumpOffset(data->RelativeJumpOffset, reader);
    }

    void ByteCodeDumper::DumpBrS(OpCode op, const unaligned OpLayoutBrS * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1491\n");
        DumpOffset(data->RelativeJumpOffset, reader);
        DumpI4(data->val);
    }

    template <class T>
    void ByteCodeDumper::DumpBrReg1(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1498\n");
        DumpOffset(data->RelativeJumpOffset, reader);
        DumpReg(data->R1);
    }

    template <class T>
    void ByteCodeDumper::DumpBrReg1Unsigned1(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1505\n");
        DumpOffset(data->RelativeJumpOffset, reader);
        DumpReg(data->R1);
        DumpU4(data->C2);
    }

    template <class T>
    void ByteCodeDumper::DumpBrReg2(OpCode op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1513\n");
        DumpOffset(data->RelativeJumpOffset, reader);
        DumpReg(data->R1);
        DumpReg(data->R2);
    }

    void ByteCodeDumper::DumpBrProperty(OpCode op, const unaligned OpLayoutBrProperty * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1520\n");
        DumpOffset(data->RelativeJumpOffset, reader);
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        Output::Print(_u("R%d.%s"), data->Instance, pPropertyName->GetBuffer());
    }

    void ByteCodeDumper::DumpBrLocalProperty(OpCode op, const unaligned OpLayoutBrLocalProperty * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1529\n");
        DumpOffset(data->RelativeJumpOffset, reader);
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        Output::Print(_u("%s"), pPropertyName->GetBuffer());
    }

    void ByteCodeDumper::DumpBrEnvProperty(OpCode op, const unaligned OpLayoutBrEnvProperty * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {LOGMEIN("ByteCodeDumper.cpp] 1538\n");
        DumpOffset(data->RelativeJumpOffset, reader);
        ScriptContext* scriptContext = dumpFunction->GetScriptContext();
        PropertyRecord const * pPropertyName = scriptContext->GetPropertyName(
            dumpFunction->GetReferencedPropertyId(data->PropertyIdIndex));
        Output::Print(_u("[%d].%s"), data->SlotIndex, pPropertyName->GetBuffer());
    }

    void ByteCodeDumper::DumpOp(OpCode op, LayoutSize layoutSize, ByteCodeReader& reader, FunctionBody* dumpFunction)
    {LOGMEIN("ByteCodeDumper.cpp] 1547\n");
        Output::Print(_u("%-20s"), OpCodeUtil::GetOpCodeName(op));
        OpLayoutType nType = OpCodeUtil::GetOpCodeLayout(op);
        switch (layoutSize * OpLayoutType::Count + nType)
        {LOGMEIN("ByteCodeDumper.cpp] 1551\n");
#define LAYOUT_TYPE(layout) \
            case OpLayoutType::layout: \
                Assert(layoutSize == SmallLayout); \
                Dump##layout(op, reader.layout(), dumpFunction, reader); \
                break;

#define LAYOUT_SCHEMA(type, layout) \
            case type##Layout * OpLayoutType::Count + OpLayoutType::layout: \
                Dump##layout(op, reader.layout##_##type(), dumpFunction, reader); \
                break

#define LAYOUT_TYPE_WMS(layout) \
            LAYOUT_SCHEMA(Small,  layout); \
            LAYOUT_SCHEMA(Medium, layout); \
            LAYOUT_SCHEMA(Large,  layout);

#define LAYOUT_TYPE_PROFILED_WMS(layout) \
            LAYOUT_TYPE_WMS(Profiled##layout) \
            LAYOUT_TYPE_WMS(layout)

#include "LayoutTypes.h"

            default:
            {
                AssertMsg(false, "Unknown OpLayout");
                break;
            }
        }
    }
} // namespace Js
#endif
