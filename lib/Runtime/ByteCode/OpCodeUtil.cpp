//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    bool OpCodeUtil::IsPrefixOpcode(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 9\n");
        return op <= OpCode::ExtendedLargeLayoutPrefix && op != OpCode::EndOfBlock;
    }

    void OpCodeUtil::ConvertOpToNonProfiled(OpCode& op)
    {LOGMEIN("OpCodeUtil.cpp] 14\n");
        if (IsProfiledCallOp(op) || IsProfiledCallOpWithICIndex(op))
        {LOGMEIN("OpCodeUtil.cpp] 16\n");
            op = ConvertProfiledCallOpToNonProfiled(op);
        }
        else if (IsProfiledReturnTypeCallOp(op))
        {LOGMEIN("OpCodeUtil.cpp] 20\n");
            op = ConvertProfiledReturnTypeCallOpToNonProfiled(op);
        }
        else
        {
            ConvertNonCallOpToNonProfiled(op);
        }
    }
    void OpCodeUtil::ConvertNonCallOpToProfiled(OpCode& op)
    {LOGMEIN("OpCodeUtil.cpp] 29\n");
        Assert(OpCodeAttr::HasProfiledOp(op));
        op += 1;
        Assert(OpCodeAttr::IsProfiledOp(op));
    }

    void OpCodeUtil::ConvertNonCallOpToProfiledWithICIndex(OpCode& op)
    {LOGMEIN("OpCodeUtil.cpp] 36\n");
        Assert(OpCodeAttr::HasProfiledOp(op) && OpCodeAttr::HasProfiledOpWithICIndex(op));
        op += 2;
        Assert(OpCodeAttr::IsProfiledOpWithICIndex(op));
    }

    void OpCodeUtil::ConvertNonCallOpToNonProfiled(OpCode& op)
    {LOGMEIN("OpCodeUtil.cpp] 43\n");
        if (OpCodeAttr::IsProfiledOp(op))
        {LOGMEIN("OpCodeUtil.cpp] 45\n");
            op -= 1;
            Assert(OpCodeAttr::HasProfiledOp(op));
        }
        else if (OpCodeAttr::IsProfiledOpWithICIndex(op))
        {LOGMEIN("OpCodeUtil.cpp] 50\n");
            op -= 2;
            Assert(OpCodeAttr::HasProfiledOpWithICIndex(op));
        }
        else
        {
            Assert(false);
        }
    }

    bool OpCodeUtil::IsCallOp(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 61\n");
        return op >= Js::OpCode::CallI && op <= Js::OpCode::CallIExtendedFlags;
    }

    bool OpCodeUtil::IsProfiledCallOp(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 66\n");
        return op >= Js::OpCode::ProfiledCallI && op <= Js::OpCode::ProfiledCallIExtendedFlags;
    }

    bool OpCodeUtil::IsProfiledCallOpWithICIndex(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 71\n");
        return op >= Js::OpCode::ProfiledCallIWithICIndex && op <= Js::OpCode::ProfiledCallIExtendedFlagsWithICIndex;
    }

    bool OpCodeUtil::IsProfiledConstructorCall(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 76\n");
        return ((op >= Js::OpCode::NewScObject && op <= Js::OpCode::ProfiledNewScObjArraySpread) || op == Js::OpCode::ProfiledNewScObjectSpread) && (OpCodeAttr::IsProfiledOp(op) || OpCodeAttr::IsProfiledOpWithICIndex(op));
    }

    bool OpCodeUtil::IsProfiledReturnTypeCallOp(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 81\n");
        return op >= Js::OpCode::ProfiledReturnTypeCallI && op <= Js::OpCode::ProfiledReturnTypeCallIExtendedFlags;
    }

#if DBG
    OpCode OpCodeUtil::DebugConvertProfiledCallToNonProfiled(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 87\n");
        switch (op)
        {LOGMEIN("OpCodeUtil.cpp] 89\n");
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
    {LOGMEIN("OpCodeUtil.cpp] 109\n");
        switch (op)
        {LOGMEIN("OpCodeUtil.cpp] 111\n");
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
    {LOGMEIN("OpCodeUtil.cpp] 129\n");
        OpCode newOpcode;
        if (IsProfiledCallOp(op))
        {LOGMEIN("OpCodeUtil.cpp] 132\n");
            newOpcode = (OpCode)(op - Js::OpCode::ProfiledCallI + Js::OpCode::CallI);
        }
        else if (IsProfiledCallOpWithICIndex(op))
        {LOGMEIN("OpCodeUtil.cpp] 136\n");
            newOpcode = (OpCode)(op - Js::OpCode::ProfiledCallIWithICIndex + Js::OpCode::CallI);
        }
        else
        {
            Assert(false);
            __assume(false);
        }
        Assert(DebugConvertProfiledCallToNonProfiled(op) == newOpcode);
        return newOpcode;
    }

    OpCode OpCodeUtil::ConvertProfiledReturnTypeCallOpToNonProfiled(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 149\n");
        OpCode newOpcode;
        if (IsProfiledReturnTypeCallOp(op))
        {LOGMEIN("OpCodeUtil.cpp] 152\n");
            newOpcode = (OpCode)(op - Js::OpCode::ProfiledReturnTypeCallI + Js::OpCode::CallI);
        }
        else
        {
            Assert(false);
            __assume(false);
        }

        Assert(DebugConvertProfiledReturnTypeCallToNonProfiled(op) == newOpcode);
        return newOpcode;
    }

    OpCode OpCodeUtil::ConvertCallOpToProfiled(OpCode op, bool withICIndex)
    {LOGMEIN("OpCodeUtil.cpp] 166\n");
        return (!withICIndex) ?
            (OpCode)(op - OpCode::CallI + OpCode::ProfiledCallI) :
            (OpCode)(op - OpCode::CallI + OpCode::ProfiledCallIWithICIndex);
    }

    OpCode OpCodeUtil::ConvertCallOpToProfiledReturnType(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 173\n");
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
    {LOGMEIN("OpCodeUtil.cpp] 206\n");
        if (op <= Js::OpCode::MaxByteSizedOpcodes)
        {LOGMEIN("OpCodeUtil.cpp] 208\n");
            Assert((uint)op < _countof(OpCodeNames));
            __analysis_assume((uint)op < _countof(OpCodeNames));
            return OpCodeNames[(uint)op];
        }
        else if (op < Js::OpCode::ByteCodeLast)
        {LOGMEIN("OpCodeUtil.cpp] 214\n");
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
    {LOGMEIN("OpCodeUtil.cpp] 228\n");
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
    {LOGMEIN("OpCodeUtil.cpp] 251\n");
        if ((uint)op <= (uint)Js::OpCode::MaxByteSizedOpcodes)
        {LOGMEIN("OpCodeUtil.cpp] 253\n");
            AnalysisAssert((uint)op < _countof(OpCodeLayouts));
            return OpCodeLayouts[(uint)op];
        }
        else if (op < Js::OpCode::ByteCodeLast)
        {LOGMEIN("OpCodeUtil.cpp] 258\n");
            uint opIndex = op - (Js::OpCode::MaxByteSizedOpcodes + 1);
            AnalysisAssert(opIndex < _countof(ExtendedOpCodeLayouts));
            return ExtendedOpCodeLayouts[opIndex];
        }
        uint opIndex = op - (Js::OpCode::ByteCodeLast + 1);
        AnalysisAssert(opIndex < _countof(BackendOpCodeLayouts));
        return BackendOpCodeLayouts[opIndex];
    }

    bool OpCodeUtil::IsValidByteCodeOpcode(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 269\n");
        CompileAssert((int)Js::OpCode::MaxByteSizedOpcodes + 1 + _countof(OpCodeUtil::ExtendedOpCodeLayouts) == (int)Js::OpCode::ByteCodeLast);
        return (uint)op < _countof(OpCodeLayouts)
            || (op > Js::OpCode::MaxByteSizedOpcodes && op < Js::OpCode::ByteCodeLast);
    }

    bool OpCodeUtil::IsValidOpcode(OpCode op)
    {LOGMEIN("OpCodeUtil.cpp] 276\n");
        return IsValidByteCodeOpcode(op)
            || (op > Js::OpCode::ByteCodeLast && op < Js::OpCode::Count);
    }
};
