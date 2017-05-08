//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeBasePch.h"

#if ENABLE_NATIVE_CODEGEN
#include "CodeGenAllocators.h"
#include "ServerThreadContext.h"
#endif

ThreadContextInfo::ThreadContextInfo() :
    m_isAllJITCodeInPreReservedRegion(true),
    wellKnownHostTypeHTMLAllCollectionTypeId(Js::TypeIds_Undefined),
    m_isClosed(false)
{TRACE_IT(37717);
}

#if ENABLE_NATIVE_CODEGEN
intptr_t
ThreadContextInfo::GetNullFrameDisplayAddr() const
{TRACE_IT(37718);
    return SHIFT_ADDR(this, &Js::NullFrameDisplay);
}

intptr_t
ThreadContextInfo::GetStrictNullFrameDisplayAddr() const
{TRACE_IT(37719);
    return SHIFT_ADDR(this, &Js::StrictNullFrameDisplay);
}

intptr_t
ThreadContextInfo::GetAbsDoubleCstAddr() const
{TRACE_IT(37720);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::AbsDoubleCst);
}

intptr_t
ThreadContextInfo::GetAbsFloatCstAddr() const
{TRACE_IT(37721);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::AbsFloatCst);
}

intptr_t ThreadContextInfo::GetSgnFloatBitCst() const
{TRACE_IT(37722);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::SgnFloatBitCst);
}

intptr_t ThreadContextInfo::GetSgnDoubleBitCst() const
{TRACE_IT(37723);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::SgnDoubleBitCst);
}

intptr_t
ThreadContextInfo::GetMaskNegFloatAddr() const
{TRACE_IT(37724);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::MaskNegFloat);
}

intptr_t
ThreadContextInfo::GetMaskNegDoubleAddr() const
{TRACE_IT(37725);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::MaskNegDouble);
}

intptr_t
ThreadContextInfo::GetUIntConvertConstAddr() const
{TRACE_IT(37726);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::UIntConvertConst);
}

intptr_t
ThreadContextInfo::GetUInt64ConvertConstAddr() const
{TRACE_IT(37727);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::UInt64ConvertConst);
}

intptr_t
ThreadContextInfo::GetUint8ClampedArraySetItemAddr() const
{TRACE_IT(37728);
    return SHIFT_ADDR(this, (BOOL(*)(Js::Uint8ClampedArray * arr, uint32 index, Js::Var value))&Js::Uint8ClampedArray::DirectSetItem);
}

intptr_t
ThreadContextInfo::GetConstructorCacheDefaultInstanceAddr() const
{TRACE_IT(37729);
    return SHIFT_ADDR(this, &Js::ConstructorCache::DefaultInstance);
}

intptr_t
ThreadContextInfo::GetJavascriptObjectNewInstanceAddr() const
{TRACE_IT(37730);
    return SHIFT_ADDR(this, &Js::JavascriptObject::EntryInfo::NewInstance);
}

intptr_t
ThreadContextInfo::GetJavascriptArrayNewInstanceAddr() const
{TRACE_IT(37731);
    return SHIFT_ADDR(this, &Js::JavascriptArray::EntryInfo::NewInstance);
}

intptr_t
ThreadContextInfo::GetDoubleOnePointZeroAddr() const
{TRACE_IT(37732);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::ONE_POINT_ZERO);
}

intptr_t
ThreadContextInfo::GetDoublePointFiveAddr() const
{TRACE_IT(37733);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_PointFive);
}

intptr_t
ThreadContextInfo::GetFloatPointFiveAddr() const
{TRACE_IT(37734);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_Float32PointFive);
}

intptr_t
ThreadContextInfo::GetDoubleNegPointFiveAddr() const
{TRACE_IT(37735);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_NegPointFive);
}

intptr_t
ThreadContextInfo::GetFloatNegPointFiveAddr() const
{TRACE_IT(37736);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_Float32NegPointFive);
}

intptr_t
ThreadContextInfo::GetDoubleNegOneAddr() const
{TRACE_IT(37737);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_NegOne);
}

intptr_t
ThreadContextInfo::GetDoubleTwoToFractionAddr() const
{TRACE_IT(37738);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_TwoToFraction);
}

intptr_t
ThreadContextInfo::GetFloatTwoToFractionAddr() const
{TRACE_IT(37739);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_Float32TwoToFraction);
}

intptr_t
ThreadContextInfo::GetDoubleNegTwoToFractionAddr() const
{TRACE_IT(37740);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_NegTwoToFraction);
}

intptr_t
ThreadContextInfo::GetDoubleNaNAddr() const
{TRACE_IT(37741);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_Nan);
}

intptr_t
ThreadContextInfo::GetDoubleUintMaxPlusOneAddr() const
{TRACE_IT(37742);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_UintMaxPlusOne);
}

intptr_t
ThreadContextInfo::GetDoubleIntMaxPlusOneAddr() const
{TRACE_IT(37743);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_IntMaxPlusOne);
}

intptr_t
ThreadContextInfo::GetDoubleIntMinMinusOneAddr() const
{TRACE_IT(37744);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_MinIntMinusOne);
}

intptr_t
ThreadContextInfo::GetFloatNaNAddr() const
{TRACE_IT(37745);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_Nan32);
}

intptr_t
ThreadContextInfo::GetFloatNegTwoToFractionAddr() const
{TRACE_IT(37746);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_Float32NegTwoToFraction);
}

intptr_t
ThreadContextInfo::GetDoubleZeroAddr() const
{TRACE_IT(37747);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_Zero);
}

intptr_t
ThreadContextInfo::GetFloatZeroAddr() const
{TRACE_IT(37748);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::k_Float32Zero);
}

intptr_t
ThreadContextInfo::GetNativeFloatArrayMissingItemAddr() const
{TRACE_IT(37749);
    return SHIFT_ADDR(this, &Js::JavascriptNativeFloatArray::MissingItem);
}

intptr_t
ThreadContextInfo::GetExponentMaskAddr() const
{TRACE_IT(37750);
    return SHIFT_ADDR(this, &Js::Constants::ExponentMask);
}

intptr_t
ThreadContextInfo::GetMantissaMaskAddr() const
{TRACE_IT(37751);
    return SHIFT_ADDR(this, &Js::Constants::MantissaMask);
}

#if _M_IX86 || _M_AMD64

intptr_t
ThreadContextInfo::GetX86AbsMaskF4Addr() const
{TRACE_IT(37752);
    return SHIFT_ADDR(this, &X86_ABS_MASK_F4);
}

intptr_t
ThreadContextInfo::GetX86AbsMaskD2Addr() const
{TRACE_IT(37753);
    return SHIFT_ADDR(this, &X86_ABS_MASK_D2);
}

intptr_t
ThreadContextInfo::GetX86NegMaskF4Addr() const
{TRACE_IT(37754);
    return SHIFT_ADDR(this, &X86_NEG_MASK_F4);
}

intptr_t
ThreadContextInfo::GetX86NegMaskD2Addr() const
{TRACE_IT(37755);
    return SHIFT_ADDR(this, &X86_NEG_MASK_D2);
}

intptr_t
ThreadContextInfo::GetX86AllNegOnesAddr() const
{TRACE_IT(37756);
    return SHIFT_ADDR(this, &X86_ALL_NEG_ONES);
}

intptr_t
ThreadContextInfo::GetX86AllNegOnesF4Addr() const
{TRACE_IT(37757);
    return SHIFT_ADDR(this, &X86_ALL_NEG_ONES_F4);
}

intptr_t
ThreadContextInfo::GetX86AllZerosAddr() const
{TRACE_IT(37758);
    return SHIFT_ADDR(this, &X86_ALL_ZEROS);
}

intptr_t
ThreadContextInfo::GetX86AllOnesF4Addr() const
{TRACE_IT(37759);
    return SHIFT_ADDR(this, &X86_ALL_ONES_F4);
}

intptr_t
ThreadContextInfo::GetX86LowBytesMaskAddr() const
{TRACE_IT(37760);
    return SHIFT_ADDR(this, &X86_LOWBYTES_MASK);
}

intptr_t
ThreadContextInfo::GetX86HighBytesMaskAddr() const
{TRACE_IT(37761);
    return SHIFT_ADDR(this, &X86_HIGHBYTES_MASK);
}

intptr_t
ThreadContextInfo::GetX86DoubleWordSignBitsAddr() const
{TRACE_IT(37762);
    return SHIFT_ADDR(this, &X86_DWORD_SIGNBITS);
}

intptr_t
ThreadContextInfo::GetX86WordSignBitsAddr() const
{TRACE_IT(37763);
    return SHIFT_ADDR(this, &X86_WORD_SIGNBITS);
}

intptr_t
ThreadContextInfo::GetX86ByteSignBitsAddr() const
{TRACE_IT(37764);
    return SHIFT_ADDR(this, &X86_BYTE_SIGNBITS);
}

intptr_t
ThreadContextInfo::GetX86TwoPower32F4Addr() const
{TRACE_IT(37765);
    return SHIFT_ADDR(this, &X86_TWO_32_F4);
}

intptr_t
ThreadContextInfo::GetX86TwoPower31F4Addr() const
{TRACE_IT(37766);
    return SHIFT_ADDR(this, &X86_TWO_31_F4);
}

intptr_t
ThreadContextInfo::GetX86TwoPower31I4Addr() const
{TRACE_IT(37767);
    return SHIFT_ADDR(this, &X86_TWO_31_I4);
}

intptr_t
ThreadContextInfo::GetX86NegTwoPower31F4Addr() const
{TRACE_IT(37768);
    return SHIFT_ADDR(this, &X86_NEG_TWO_31_F4);
}

intptr_t
ThreadContextInfo::GetX86FourLanesMaskAddr(uint8 minorityLane) const
{TRACE_IT(37769);
    return SHIFT_ADDR(this, &X86_4LANES_MASKS[minorityLane]);
}

intptr_t
ThreadContextInfo::GetDoubleIntMinAddr() const
{TRACE_IT(37770);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::DOUBLE_INT_MIN);
}

intptr_t
ThreadContextInfo::GetDoubleTwoTo31Addr() const
{TRACE_IT(37771);
    return SHIFT_ADDR(this, &Js::JavascriptNumber::DOUBLE_TWO_TO_31);
}
#endif

intptr_t
ThreadContextInfo::GetStringReplaceNameAddr() const
{TRACE_IT(37772);
    return SHIFT_ADDR(this, Js::Constants::StringReplace);
}

intptr_t
ThreadContextInfo::GetStringMatchNameAddr() const
{TRACE_IT(37773);
    return SHIFT_ADDR(this, Js::Constants::StringMatch);
}
#endif

bool
ThreadContextInfo::IsAllJITCodeInPreReservedRegion() const
{TRACE_IT(37774);
    return m_isAllJITCodeInPreReservedRegion;
}

void
ThreadContextInfo::ResetIsAllJITCodeInPreReservedRegion()
{TRACE_IT(37775);
    m_isAllJITCodeInPreReservedRegion = false;
}

#ifdef ENABLE_GLOBALIZATION

#if defined(_CONTROL_FLOW_GUARD)
Js::DelayLoadWinCoreProcessThreads *
ThreadContextInfo::GetWinCoreProcessThreads()
{TRACE_IT(37776);
    m_delayLoadWinCoreProcessThreads.EnsureFromSystemDirOnly();
    return &m_delayLoadWinCoreProcessThreads;
}

Js::DelayLoadWinCoreMemory *
ThreadContextInfo::GetWinCoreMemoryLibrary()
{TRACE_IT(37777);
    m_delayLoadWinCoreMemoryLibrary.EnsureFromSystemDirOnly();
    return &m_delayLoadWinCoreMemoryLibrary;
}
#endif

bool
ThreadContextInfo::IsCFGEnabled()
{TRACE_IT(37778);
#if defined(_CONTROL_FLOW_GUARD)
    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY CfgPolicy;
    m_delayLoadWinCoreProcessThreads.EnsureFromSystemDirOnly();
    BOOL isGetMitigationPolicySucceeded = m_delayLoadWinCoreProcessThreads.GetMitigationPolicyForProcess(
        this->GetProcessHandle(),
        ProcessControlFlowGuardPolicy,
        &CfgPolicy,
        sizeof(CfgPolicy));
    Assert(isGetMitigationPolicySucceeded || !AutoSystemInfo::Data.IsCFGEnabled());
    return CfgPolicy.EnableControlFlowGuard && AutoSystemInfo::Data.IsCFGEnabled();
#else
    return false;
#endif // _CONTROL_FLOW_GUARD
}
#endif // ENABLE_GLOBALIZATION

//Masking bits according to AutoSystemInfo::PageSize
#define PAGE_START_ADDR(address) ((size_t)(address) & ~(size_t)(AutoSystemInfo::PageSize - 1))
#define IS_16BYTE_ALIGNED(address) (((size_t)(address) & 0xF) == 0)
#define OFFSET_ADDR_WITHIN_PAGE(address) ((size_t)(address) & (AutoSystemInfo::PageSize - 1))

void
ThreadContextInfo::SetValidCallTargetForCFG(PVOID callTargetAddress, bool isSetValid)
{TRACE_IT(37779);
#ifdef _CONTROL_FLOW_GUARD
    if (IsCFGEnabled())
    {
        AssertMsg(IS_16BYTE_ALIGNED(callTargetAddress), "callTargetAddress is not 16-byte page aligned?");

        PVOID startAddressOfPage = (PVOID)(PAGE_START_ADDR(callTargetAddress));
        size_t codeOffset = OFFSET_ADDR_WITHIN_PAGE(callTargetAddress);

        CFG_CALL_TARGET_INFO callTargetInfo[1];

        callTargetInfo[0].Offset = codeOffset;
        callTargetInfo[0].Flags = (isSetValid ? CFG_CALL_TARGET_VALID : 0);

        AssertMsg((size_t)callTargetAddress - (size_t)startAddressOfPage <= AutoSystemInfo::PageSize - 1, "Only last bits corresponding to PageSize should be masked");
        AssertMsg((size_t)startAddressOfPage + (size_t)codeOffset == (size_t)callTargetAddress, "Wrong masking of address?");

        BOOL isCallTargetRegistrationSucceed = GetWinCoreMemoryLibrary()->SetProcessCallTargets(GetProcessHandle(), startAddressOfPage, AutoSystemInfo::PageSize, 1, callTargetInfo);

        if (!isCallTargetRegistrationSucceed)
        {TRACE_IT(37780);
            if (GetLastError() == ERROR_COMMITMENT_LIMIT)
            {TRACE_IT(37781);
                //Throw OOM, if there is not enough virtual memory for paging (required for CFG BitMap)
                Js::Throw::OutOfMemory();
            }
            else
            {TRACE_IT(37782);
                Js::Throw::InternalError();
            }
        }
#if DBG
        if (isSetValid && !JITManager::GetJITManager()->IsOOPJITEnabled())
        {TRACE_IT(37783);
            _guard_check_icall((uintptr_t)callTargetAddress);
        }

        if (PHASE_TRACE1(Js::CFGPhase))
        {TRACE_IT(37784);
            if (!isSetValid)
            {TRACE_IT(37785);
                Output::Print(_u("DEREGISTER:"));
            }
            Output::Print(_u("CFGRegistration: StartAddr: 0x%p , Offset: 0x%x, TargetAddr: 0x%x \n"), (char*)startAddressOfPage, callTargetInfo[0].Offset, ((size_t)startAddressOfPage + (size_t)callTargetInfo[0].Offset));
            Output::Flush();
        }
#endif
    }
#endif // _CONTROL_FLOW_GUARD
}

bool
ThreadContextInfo::IsClosed()
{TRACE_IT(37786);
    return m_isClosed;
}

intptr_t SHIFT_ADDR(const ThreadContextInfo*const context, intptr_t address)
{TRACE_IT(37787);
#if ENABLE_OOP_NATIVE_CODEGEN
    Assert(AutoSystemInfo::Data.IsJscriptModulePointer((void*)address));
    ptrdiff_t diff = 0;
    if (JITManager::GetJITManager()->IsJITServer())
    {TRACE_IT(37788);
        diff = ((ServerThreadContext*)context)->GetChakraBaseAddressDifference();
    }
    else
    {TRACE_IT(37789);
        diff = ((ThreadContext*)context)->GetChakraBaseAddressDifference();
    }
    return (intptr_t)address + diff;
#else
    return address;
#endif

}

intptr_t SHIFT_CRT_ADDR(const ThreadContextInfo*const context, intptr_t address)
{TRACE_IT(37790);
#if ENABLE_OOP_NATIVE_CODEGEN
    if (AutoSystemInfo::Data.IsJscriptModulePointer((void*)address))
    {TRACE_IT(37791);
        // the function is compiled to chakra.dll, or statically linked to crt
        return SHIFT_ADDR(context, address);
    }
    ptrdiff_t diff = 0;
    if (JITManager::GetJITManager()->IsJITServer())
    {TRACE_IT(37792);
        diff = ((ServerThreadContext*)context)->GetCRTBaseAddressDifference();
    }
    else
    {TRACE_IT(37793);
        diff = ((ThreadContext*)context)->GetCRTBaseAddressDifference();
    }
    return (intptr_t)address + diff;
#else
    return address;
#endif
}

