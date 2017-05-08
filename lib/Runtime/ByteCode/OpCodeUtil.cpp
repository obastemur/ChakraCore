//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    bool OpCodeUtil::IsPrefixOpcode(OpCode op)
    {TRACE_IT(41704);
        return op <= OpCode::ExtendedLargeLayoutPrefix && op != OpCode::EndOfBlock;
    }

    void OpCodeUtil::ConvertOpToNonProfiled(OpCode& op)
    {TRACE_IT(41705);
        if (IsProfiledCallOp(op) || IsProfiledCallOpWithICIndex(op))
        {TRACE_IT(41706);
            op = ConvertProfiledCallOpToNonProfiled(op);
        }
        else if (IsProfiledReturnTypeCallOp(op))
        {TRACE_IT(41707);
            op = ConvertProfiledReturnTypeCallOpToNonProfiled(op);
        }
        else
        {TRACE_IT(41708);
            ConvertNonCallOpToNonProfiled(op);
        }
    }
    void OpCodeUtil::ConvertNonCallOpToProfiled(OpCode& op)
    {TRACE_IT(41709);
        Assert(OpCodeAttr::HasProfiledOp(op));
        op += 1;
        Assert(OpCodeAttr::IsProfiledOp(op));
    }

    void OpCodeUtil::ConvertNonCallOpToProfiledWithICIndex(OpCode& op)
    {TRACE_IT(41710);
        Assert(OpCodeAttr::HasProfiledOp(op) && OpCodeAttr::HasProfiledOpWithICIndex(op));
        op += 2;
        Assert(OpCodeAttr::IsProfiledOpWithICIndex(op));
    }

    void OpCodeUtil::ConvertNonCallOpToNonProfiled(OpCode& op)
    {TRACE_IT(41711);
        if (OpCodeAttr::IsProfiledOp(op))
        {TRACE_IT(41712);
            op -= 1;
            Assert(OpCodeAttr::HasProfiledOp(op));
        }
        else if (OpCodeAttr::IsProfiledOpWithICIndex(op))
        {TRACE_IT(41713);
            op -= 2;
            Assert(OpCodeAttr::HasProfiledOpWithICIndex(op));
        }
        else
        {TRACE_IT(41714);
            Assert(false);
        }
    }

    bool OpCodeUtil::IsCallOp(OpCode op)
    {TRACE_IT(41715);
        return op >= Js::OpCode::CallI && op <= Js::OpCode::CallIExtendedFlags;
    }

    bool OpCodeUtil::IsProfiledCallOp(OpCode op)
    {TRACE_IT(41716);
        return op >= Js::OpCode::ProfiledCallI && op <= Js::OpCode::ProfiledCallIExtendedFlags;
    }

    bool OpCodeUtil::IsProfiledCallOpWithICIndex(OpCode op)
    {TRACE_IT(41717);
        return op >= Js::OpCode::ProfiledCallIWithICIndex && op <= Js::OpCode::ProfiledCallIExtendedFlagsWithICIndex;
    }

    bool OpCodeUtil::IsProfiledConstructorCall(OpCode op)
    {TRACE_IT(41718);
        return ((op >= Js::OpCode::NewScObject && op <= Js::OpCode::ProfiledNewScObjArraySpread) || op == Js::OpCode::ProfiledNewScObjectSpread) && (OpCodeAttr::IsProfiledOp(op) || OpCodeAttr::IsProfiledOpWithICIndex(op));
    }

    bool OpCodeUtil::IsProfiledReturnTypeCallOp(OpCode op)
    {TRACE_IT(41719);
        return op >= Js::OpCode::ProfiledReturnTypeCallI && op <= Js::OpCode::ProfiledReturnTypeCallIExtendedFlags;
    }

#if DBG
    OpCode OpCodeUtil::DebugConvertProfiledCallToNonProfiled(OpCode op)
    {TRACE_IT(41720);
        switch (op)
        {
        case Js::OpCode::ProfiledCallI:
        case Js::OpCode::ProfiledCallIWithICIndex:
            return Js::OpCode::CallI;
        case Js::OpCode::ProfiledCallIFlags:
        case Js::OpCode::ProfiledCallIFlagsWithICIndex:
            return Js::OpCode::CallIFlags;
        case Js::OpCode::ProfiledCallIExtendedFlags:
        case Js::OpCode::ProfiledCallIExtendedFlagsWithICIndex:
            return Js::OpCode::CallIExtendedFlags;
        case Js::OpCode::ProfiledCallIExtended:
        case Js::OpCode::ProfiledCallIExtendedWithICIndex:
            return Js::OpCode::CallIExtended;
        default:
            Assert(false);
        };
        return Js::OpCode::Nop;
    }

    OpCode OpCodeUtil::DebugConvertProfiledReturnTypeCallToNonProfiled(OpCode op)
    {TRACE_IT(41721);
        switch (op)
        {
        case Js::OpCode::ProfiledReturnTypeCallI:
            return Js::OpCode::CallI;
        case Js::OpCode::ProfiledReturnTypeCallIFlags:
            return Js::OpCode::CallIFlags;
        case Js::OpCode::ProfiledReturnTypeCallIExtendedFlags:
            return Js::OpCode::CallIExtendedFlags;
        case Js::OpCode::ProfiledReturnTypeCallIExtended:
            return Js::OpCode::CallIExtended;
        default:
            Assert(false);
        };

        return Js::OpCode::Nop;
    }
#endif

    OpCode OpCodeUtil::ConvertProfiledCallOpToNonProfiled(OpCode op)
    {TRACE_IT(41722);
        OpCode newOpcode;
        if (IsProfiledCallOp(op))
        {TRACE_IT(41723);
            newOpcode = (OpCode)(op - Js::OpCode::ProfiledCallI + Js::OpCode::CallI);
        }
        else if (IsProfiledCallOpWithICIndex(op))
        {TRACE_IT(41724);
            newOpcode = (OpCode)(op - Js::OpCode::ProfiledCallIWithICIndex + Js::OpCode::CallI);
        }
        else
        {TRACE_IT(41725);
            Assert(false);
            __assume(false);
        }
        Assert(DebugConvertProfiledCallToNonProfiled(op) == newOpcode);
        return newOpcode;
    }

    OpCode OpCodeUtil::ConvertProfiledReturnTypeCallOpToNonProfiled(OpCode op)
    {TRACE_IT(41726);
        OpCode newOpcode;
        if (IsProfiledReturnTypeCallOp(op))
        {TRACE_IT(41727);
            newOpcode = (OpCode)(op - Js::OpCode::ProfiledReturnTypeCallI + Js::OpCode::CallI);
        }
        else
        {TRACE_IT(41728);
            Assert(false);
            __assume(false);
        }

        Assert(DebugConvertProfiledReturnTypeCallToNonProfiled(op) == newOpcode);
        return newOpcode;
    }

    OpCode OpCodeUtil::ConvertCallOpToProfiled(OpCode op, bool withICIndex)
    {TRACE_IT(41729);
        return (!withICIndex) ?
            (OpCode)(op - OpCode::CallI + OpCode::ProfiledCallI) :
            (OpCode)(op - OpCode::CallI + OpCode::ProfiledCallIWithICIndex);
    }

    OpCode OpCodeUtil::ConvertCallOpToProfiledReturnType(OpCode op)
    {TRACE_IT(41730);
        return (OpCode)(op - OpCode::CallI + OpCode::ProfiledReturnTypeCallI);
    }

    CompileAssert(((int)Js::OpCode::CallIExtendedFlags - (int)Js::OpCode::CallI) == ((int)Js::OpCode::ProfiledCallIExtendedFlags - (int)Js::OpCode::ProfiledCallI));
    CompileAssert(((int)Js::OpCode::CallIExtendedFlags - (int)Js::OpCode::CallI) == ((int)Js::OpCode::ProfiledReturnTypeCallIExtendedFlags - (int)Js::OpCode::ProfiledReturnTypeCallI));
    CompileAssert(((int)Js::OpCode::CallIExtendedFlags - (int)Js::OpCode::CallI) == ((int)Js::OpCode::ProfiledCallIExtendedFlagsWithICIndex - (int)Js::OpCode::ProfiledCallIWithICIndex));

    // Only include the opcode name on debug and test build
#if DBG_DUMP || ENABLE_DEBUG_CONFIG_OPTIONS

    char16 const * const OpCodeUtil::OpCodeNames[] =
    {
#define DEF_OP(x, y, ...) _u("") STRINGIZEW(x) _u(""),
#include "OpCodeList.h"
#undef DEF_OP
    };

    char16 const * const OpCodeUtil::ExtendedOpCodeNames[] =
    {
#define DEF_OP(x, y, ...) _u("") STRINGIZEW(x) _u(""),
#include "ExtendedOpCodeList.h"
#undef DEF_OP
    };

    char16 const * const OpCodeUtil::BackendOpCodeNames[] =
    {
#define DEF_OP(x, y, ...) _u("") STRINGIZEW(x) _u(""),
#include "BackendOpCodeList.h"
#undef DEF_OP
    };

    char16 const * OpCodeUtil::GetOpCodeName(OpCode op)
    {TRACE_IT(41731);
        if (op <= Js::OpCode::MaxByteSizedOpcodes)
        {TRACE_IT(41732);
            Assert((uint)op < _countof(OpCodeNames));
            __analysis_assume((uint)op < _countof(OpCodeNames));
            return OpCodeNames[(uint)op];
        }
        else if (op < Js::OpCode::ByteCodeLast)
        {TRACE_IT(41733);
            uint opIndex = op - (Js::OpCode::MaxByteSizedOpcodes + 1);
            Assert(opIndex < _countof(ExtendedOpCodeNames));
            __analysis_assume(opIndex < _countof(ExtendedOpCodeNames));
            return ExtendedOpCodeNames[opIndex];
        }
        uint opIndex = op - (Js::OpCode::ByteCodeLast + 1);
        Assert(opIndex < _countof(BackendOpCodeNames));
        __analysis_assume(opIndex < _countof(BackendOpCodeNames));
        return BackendOpCodeNames[opIndex];
    }

#else
    wchar const * OpCodeUtil::GetOpCodeName(OpCode op)
    {TRACE_IT(41734);
        return _u("<NotAvail>");
    }
#endif

    OpLayoutType const OpCodeUtil::OpCodeLayouts[] =
    {
#define DEF_OP(x, y, ...) OpLayoutType::y,
#include "OpCodeList.h"
    };

    OpLayoutType const OpCodeUtil::ExtendedOpCodeLayouts[] =
    {
#define DEF_OP(x, y, ...) OpLayoutType::y,
#include "ExtendedOpCodeList.h"
    };
    OpLayoutType const OpCodeUtil::BackendOpCodeLayouts[] =
    {
#define DEF_OP(x, y, ...) OpLayoutType::y,
#include "BackendOpCodeList.h"
    };

    OpLayoutType OpCodeUtil::GetOpCodeLayout(OpCode op)
    {TRACE_IT(41735);
        if ((uint)op <= (uint)Js::OpCode::MaxByteSizedOpcodes)
        {TRACE_IT(41736);
            AnalysisAssert((uint)op < _countof(OpCodeLayouts));
            return OpCodeLayouts[(uint)op];
        }
        else if (op < Js::OpCode::ByteCodeLast)
        {TRACE_IT(41737);
            uint opIndex = op - (Js::OpCode::MaxByteSizedOpcodes + 1);
            AnalysisAssert(opIndex < _countof(ExtendedOpCodeLayouts));
            return ExtendedOpCodeLayouts[opIndex];
        }
        uint opIndex = op - (Js::OpCode::ByteCodeLast + 1);
        AnalysisAssert(opIndex < _countof(BackendOpCodeLayouts));
        return BackendOpCodeLayouts[opIndex];
    }

    bool OpCodeUtil::IsValidByteCodeOpcode(OpCode op)
    {TRACE_IT(41738);
        CompileAssert((int)Js::OpCode::MaxByteSizedOpcodes + 1 + _countof(OpCodeUtil::ExtendedOpCodeLayouts) == (int)Js::OpCode::ByteCodeLast);
        return (uint)op < _countof(OpCodeLayouts)
            || (op > Js::OpCode::MaxByteSizedOpcodes && op < Js::OpCode::ByteCodeLast);
    }

    bool OpCodeUtil::IsValidOpcode(OpCode op)
    {TRACE_IT(41739);
        return IsValidByteCodeOpcode(op)
            || (op > Js::OpCode::ByteCodeLast && op < Js::OpCode::Count);
    }
};
