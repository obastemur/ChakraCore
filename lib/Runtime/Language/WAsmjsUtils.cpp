//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#if defined(ASMJS_PLAT) || defined(ENABLE_WASM)

namespace WAsmJs
{

template<> Types RegisterSpace::GetRegisterSpaceType<int32>(){LOGMEIN("WAsmjsUtils.cpp] 12\n");return WAsmJs::INT32;}
template<> Types RegisterSpace::GetRegisterSpaceType<int64>(){LOGMEIN("WAsmjsUtils.cpp] 13\n");return WAsmJs::INT64;}
template<> Types RegisterSpace::GetRegisterSpaceType<float>(){LOGMEIN("WAsmjsUtils.cpp] 14\n");return WAsmJs::FLOAT32;}
template<> Types RegisterSpace::GetRegisterSpaceType<double>(){LOGMEIN("WAsmjsUtils.cpp] 15\n");return WAsmJs::FLOAT64;}
template<> Types RegisterSpace::GetRegisterSpaceType<AsmJsSIMDValue>(){LOGMEIN("WAsmjsUtils.cpp] 16\n");return WAsmJs::SIMD;}

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void TraceAsmJsArgsIn(Js::Var function, int n, ...)
    {
        Assert(Js::AsmJsScriptFunction::Is(function));
        Js::AsmJsScriptFunction* asmFunction = (Js::AsmJsScriptFunction*)function;
        va_list argptr;
        va_start(argptr, n);

        Output::Print(_u("Executing function %s("), asmFunction->GetFunctionBody()->GetDisplayName());
        for (int i = 0; i < n ; i++)
        {LOGMEIN("WAsmjsUtils.cpp] 28\n");
            IRType type = (IRType)va_arg(argptr, int32);
            switch (type)
            {LOGMEIN("WAsmjsUtils.cpp] 31\n");
            case TyInt32:
            case TyUint32:
                Output::Print(_u("%d, "), va_arg(argptr, int32));
                break;
            case TyInt64:
            case TyUint64:
                Output::Print(_u("%lld, "), va_arg(argptr, int64));
                break;
            case TyFloat32:
            {LOGMEIN("WAsmjsUtils.cpp] 41\n");
                int v = va_arg(argptr, int);
                Output::Print(_u("%.2f, "), *(float*)v);
                break;
            }
            case TyFloat64:
                Output::Print(_u("%.2f, "), va_arg(argptr, double));
                break;
            default:
                break;
            }
        }
        Output::Print(_u("){\n"));
    }
#endif

    uint32 ConvertOffset(uint32 ptr, uint32 fromSize, uint32 toSize)
    {LOGMEIN("WAsmjsUtils.cpp] 58\n");
        if (fromSize == toSize)
        {LOGMEIN("WAsmjsUtils.cpp] 60\n");
            return ptr;
        }
        uint64 tmp = ptr * fromSize;
        tmp = Math::Align<uint64>(tmp, toSize);
        tmp /= toSize;
        if (tmp > (uint64)UINT32_MAX)
        {LOGMEIN("WAsmjsUtils.cpp] 67\n");
            Math::DefaultOverflowPolicy();
        }
        return (uint32)tmp;
    }

    uint32 GetTypeByteSize(Types type)
    {LOGMEIN("WAsmjsUtils.cpp] 74\n");
        // Since this needs to be done manually for each type, this assert will make sure to not forget to update this if a new type is added
        CompileAssert(WAsmJs::LIMIT == 5);
        switch (type)
        {LOGMEIN("WAsmjsUtils.cpp] 78\n");
        case INT32  : return sizeof(int32);
        case INT64  : return sizeof(int64);
        case FLOAT32: return sizeof(float);
        case FLOAT64: return sizeof(double);
        case SIMD   : return sizeof(AsmJsSIMDValue);
        default     : break;
        }
        Js::Throw::InternalError();
    }

    WAsmJs::Types FromIRType(IRType irType)
    {LOGMEIN("WAsmjsUtils.cpp] 90\n");
        switch(irType)
        {LOGMEIN("WAsmjsUtils.cpp] 92\n");
        case TyInt8:
        case TyInt16:
        case TyInt32:
        case TyUint8:
        case TyUint16:
        case TyUint32:
            return WAsmJs::INT32;
        case TyInt64:
        case TyUint64:
            return WAsmJs::INT64;
        case TyFloat32:
            return WAsmJs::FLOAT32;
        case TyFloat64:
            return WAsmJs::FLOAT64;
        case TySimd128F4:
        case TySimd128I4:
        case TySimd128I8:
        case TySimd128I16:
        case TySimd128U4:
        case TySimd128U8:
        case TySimd128U16:
        case TySimd128B4:
        case TySimd128B8:
        case TySimd128B16:
        case TySimd128D2:
            return WAsmJs::SIMD;
        }
        return WAsmJs::LIMIT;
    }

#if DBG_DUMP
    void RegisterSpace::GetTypeDebugName(Types type, char16* buf, uint bufsize, bool shortName)
    {LOGMEIN("WAsmjsUtils.cpp] 125\n");
        // Since this needs to be done manually for each type, this assert will make sure to not forget to update this if a new type is added
        CompileAssert(LIMIT == 5);

        switch (type)
        {LOGMEIN("WAsmjsUtils.cpp] 130\n");
        case INT32: wcscpy_s(buf, bufsize  , shortName ? _u("I"): _u("INT32")); break;
        case INT64: wcscpy_s(buf, bufsize  , shortName ? _u("L"): _u("INT64")); break;
        case FLOAT32: wcscpy_s(buf, bufsize, shortName ? _u("F"): _u("FLOAT32")); break;
        case FLOAT64: wcscpy_s(buf, bufsize, shortName ? _u("D"): _u("FLOAT64")); break;
        case SIMD: wcscpy_s(buf, bufsize   , _u("SIMD")); break;
        default: wcscpy_s(buf, bufsize     , _u("UNKNOWN")); break;
        }
    }

    void RegisterSpace::PrintTmpRegisterAllocation(RegSlot loc, bool deallocation)
    {LOGMEIN("WAsmjsUtils.cpp] 141\n");
        if (PHASE_TRACE1(Js::AsmjsTmpRegisterAllocationPhase))
        {LOGMEIN("WAsmjsUtils.cpp] 143\n");
            char16 buf[16];
            GetTypeDebugName(mType, buf, 16, true);
            Output::Print(_u("%s%s %d\n"), deallocation ? _u("-") : _u("+"), buf, loc);
        }
    }
#endif

    TypedRegisterAllocator::TypedRegisterAllocator(ArenaAllocator* allocator, AllocateRegisterSpaceFunc allocateFunc, uint32 excludedMask/* = 0*/)
    {LOGMEIN("WAsmjsUtils.cpp] 152\n");
        Assert(allocateFunc);
        AssertMsg(excludedMask >> WAsmJs::LIMIT == 0, "Invalid type in the excluded mask");
        mExcludedMask = excludedMask;

        for (int i = 0; i < WAsmJs::LIMIT; ++i)
        {LOGMEIN("WAsmjsUtils.cpp] 158\n");
            Types type = (Types)i;
            mTypeSpaces[i] = nullptr;
            if (!IsTypeExcluded(type))
            {LOGMEIN("WAsmjsUtils.cpp] 162\n");
                mTypeSpaces[i] = allocateFunc(allocator, type);
#if DBG_DUMP
                mTypeSpaces[i]->mType = type;
#endif
            }
        }
    }

    void TypedRegisterAllocator::CommitToFunctionInfo(Js::AsmJsFunctionInfo* funcInfo, Js::FunctionBody* body) const
    {LOGMEIN("WAsmjsUtils.cpp] 172\n");
        uint32 offset = Js::AsmJsFunctionMemory::RequiredVarConstants * sizeof(Js::Var);
        WAsmJs::TypedConstSourcesInfo constSourcesInfo = GetConstSourceInfos();

#if DBG_DUMP
        if (PHASE_TRACE(Js::AsmjsInterpreterStackPhase, body))
        {LOGMEIN("WAsmjsUtils.cpp] 178\n");
            Output::Print(_u("ASMFunctionInfo Stack Data for %s\n"), body->GetDisplayName());
            Output::Print(_u("==========================\n"));
            Output::Print(_u("RequiredVarConstants:%d\n"), Js::AsmJsFunctionMemory::RequiredVarConstants);
        }
#endif

        for (int i = 0; i < WAsmJs::LIMIT; ++i)
        {LOGMEIN("WAsmjsUtils.cpp] 186\n");
            Types type = (Types)i;

            TypedSlotInfo& slotInfo = *funcInfo->GetTypedSlotInfo(type);
            // Check if we don't want to commit this type
            if (!IsTypeExcluded(type))
            {LOGMEIN("WAsmjsUtils.cpp] 192\n");
                RegisterSpace* registerSpace = GetRegisterSpace(type);
                slotInfo.constCount = registerSpace->GetConstCount();
                slotInfo.varCount = registerSpace->GetVarCount();
                slotInfo.tmpCount = registerSpace->GetTmpCount();
                slotInfo.constSrcByteOffset = constSourcesInfo.srcByteOffsets[i];

                offset = Math::AlignOverflowCheck(offset, GetTypeByteSize(type));
                slotInfo.byteOffset = offset;

                // Update offset for next type
                uint32 totalTypeCount = 0;
                totalTypeCount = UInt32Math::Add(totalTypeCount, slotInfo.constCount);
                totalTypeCount = UInt32Math::Add(totalTypeCount, slotInfo.varCount);
                totalTypeCount = UInt32Math::Add(totalTypeCount, slotInfo.tmpCount);

                offset = UInt32Math::Add(offset, UInt32Math::Mul(totalTypeCount, GetTypeByteSize(type)));
#if DBG_DUMP
                if (PHASE_TRACE(Js::AsmjsInterpreterStackPhase, body))
                {LOGMEIN("WAsmjsUtils.cpp] 211\n");
                    char16 buf[16];
                    RegisterSpace::GetTypeDebugName(type, buf, 16);
                    Output::Print(_u("%s Offset:%d  ConstCount:%d  VarCount:%d  TmpCount:%d = %d * %d = 0x%x bytes\n"),
                                  buf,
                                  slotInfo.byteOffset,
                                  slotInfo.constCount,
                                  slotInfo.varCount,
                                  slotInfo.tmpCount,
                                  totalTypeCount,
                                  GetTypeByteSize(type),
                                  totalTypeCount * GetTypeByteSize(type));
                }
#endif
            }
        }

        // These bytes offset already calculated the alignment, used them to determine how many Js::Var we need to do the allocation
        uint32 stackByteSize = offset;
        uint32 bytesUsedForConst = constSourcesInfo.bytesUsed;
        uint32 jsVarUsedForConstsTable = ConvertToJsVarOffset<byte>(bytesUsedForConst);
        uint32 totalVarsNeeded = ConvertToJsVarOffset<byte>(stackByteSize);

        uint32 jsVarNeededForVars = totalVarsNeeded - jsVarUsedForConstsTable;
        if (totalVarsNeeded < jsVarUsedForConstsTable)
        {LOGMEIN("WAsmjsUtils.cpp] 236\n");
            // If for some reason we allocated more space in the const table than what we need, just don't allocate anymore vars
            jsVarNeededForVars = 0;
        }

        Assert((jsVarUsedForConstsTable + jsVarNeededForVars) >= totalVarsNeeded);
        body->CheckAndSetVarCount(jsVarNeededForVars);

#if DBG_DUMP
        if (PHASE_TRACE(Js::AsmjsInterpreterStackPhase, body))
        {LOGMEIN("WAsmjsUtils.cpp] 246\n");
            Output::Print(_u("Total memory required: (%u consts + %u vars) * sizeof(Js::Var) >= %u * sizeof(Js::Var) = 0x%x bytes\n"),
                          jsVarUsedForConstsTable,
                          jsVarNeededForVars,
                          totalVarsNeeded,
                          stackByteSize);
        }
#endif
    }

    void TypedRegisterAllocator::CommitToFunctionBody(Js::FunctionBody* body)
    {LOGMEIN("WAsmjsUtils.cpp] 257\n");
        // this value is the number of Var slots needed to allocate all the const
        uint32 bytesUsedForConst = GetConstSourceInfos().bytesUsed;
        // Add the registers not included in the const table
        uint32 nbConst = ConvertToJsVarOffset<byte>(bytesUsedForConst) + Js::FunctionBody::FirstRegSlot;
        body->CheckAndSetConstantCount(nbConst);
    }

    WAsmJs::TypedConstSourcesInfo TypedRegisterAllocator::GetConstSourceInfos() const
    {LOGMEIN("WAsmjsUtils.cpp] 266\n");
        WAsmJs::TypedConstSourcesInfo infos;
        // The const table doesn't contain the first reg slots (aka return register)
        uint32 offset = ConvertOffset<Js::Var, byte>(Js::AsmJsFunctionMemory::RequiredVarConstants - Js::FunctionBody::FirstRegSlot);
        for (int i = 0; i < WAsmJs::LIMIT; ++i)
        {LOGMEIN("WAsmjsUtils.cpp] 271\n");
            Types type = (Types)i;
            if (!IsTypeExcluded(type))
            {LOGMEIN("WAsmjsUtils.cpp] 274\n");
                infos.srcByteOffsets[i] = offset;
                uint32 typeSize = GetTypeByteSize(type);
                uint32 constCount = GetRegisterSpace(type)->GetConstCount();
                uint32 typeBytesUsage = ConvertOffset<byte>(constCount, typeSize);
                offset = Math::AlignOverflowCheck(offset, typeSize);
                offset = UInt32Math::Add(offset, typeBytesUsage);
            }
            else
            {
                infos.srcByteOffsets[i] = (uint32)Js::Constants::InvalidOffset;
            }
        }
        infos.bytesUsed = offset;
        return infos;
    }

    bool TypedRegisterAllocator::IsValidType(Types type) const
    {LOGMEIN("WAsmjsUtils.cpp] 292\n");
        return (uint)type < WAsmJs::LIMIT;
    }

    bool TypedRegisterAllocator::IsTypeExcluded(Types type) const
    {LOGMEIN("WAsmjsUtils.cpp] 297\n");
        return !IsValidType(type) || (mExcludedMask & (1 << type)) != 0;
    }

#if DBG_DUMP
    void TypedRegisterAllocator::DumpLocalsInfo() const
    {LOGMEIN("WAsmjsUtils.cpp] 303\n");
        for (int i = 0; i < WAsmJs::LIMIT; ++i)
        {LOGMEIN("WAsmjsUtils.cpp] 305\n");
            Types type = (Types)i;
            if (!IsTypeExcluded(type))
            {LOGMEIN("WAsmjsUtils.cpp] 308\n");
                char16 typeName[16];
                char16 shortTypeName[16];
                RegisterSpace::GetTypeDebugName(type, typeName, 16);
                RegisterSpace::GetTypeDebugName(type, shortTypeName, 16, true);
                RegisterSpace* registerSpace = GetRegisterSpace(type);
                Output::Print(
                    _u("     %-10s : %u locals (%u temps from %s%u)\n"),
                    typeName,
                    registerSpace->GetVarCount(),
                    registerSpace->GetTmpCount(),
                    shortTypeName,
                    registerSpace->GetFirstTmpRegister());
            }
        }
    }

    void TypedRegisterAllocator::GetArgumentStartIndex(uint32* indexes) const
    {LOGMEIN("WAsmjsUtils.cpp] 326\n");
        for (int i = 0; i < WAsmJs::LIMIT; ++i)
        {LOGMEIN("WAsmjsUtils.cpp] 328\n");
            Types type = (Types)i;
            if (!IsTypeExcluded(type))
            {LOGMEIN("WAsmjsUtils.cpp] 331\n");
                // Arguments starts right after the consts
                indexes[i] = GetRegisterSpace(type)->GetConstCount();
            }
        }
    }
#endif

    RegisterSpace* TypedRegisterAllocator::GetRegisterSpace(Types type) const
    {LOGMEIN("WAsmjsUtils.cpp] 340\n");
        if (!IsValidType(type))
        {LOGMEIN("WAsmjsUtils.cpp] 342\n");
            Assert("Invalid type for RegisterSpace in TypedMemoryStructure");
            Js::Throw::InternalError();
        }
        Assert(!IsTypeExcluded(type));
        return mTypeSpaces[type];
    }
};

#endif
