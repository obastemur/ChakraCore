//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#if ENABLE_OOP_NATIVE_CODEGEN
#include "JITServer/JITServer.h"

ServerThreadContext::ServerThreadContext(ThreadContextDataIDL * data) :
    m_autoProcessHandle((HANDLE)data->processHandle),
    m_threadContextData(*data),
    m_refCount(0),
    m_numericPropertyBV(nullptr),
    m_preReservedSectionAllocator((HANDLE)data->processHandle),
    m_sectionAllocator((HANDLE)data->processHandle),
    m_thunkPageAllocators(nullptr, /* allocXData */ false, &m_sectionAllocator, nullptr, (HANDLE)data->processHandle),
    m_codePageAllocators(nullptr, ALLOC_XDATA, &m_sectionAllocator, &m_preReservedSectionAllocator, (HANDLE)data->processHandle),
    m_codeGenAlloc(nullptr, nullptr, &m_codePageAllocators, (HANDLE)data->processHandle),
    m_pageAlloc(nullptr, Js::Configuration::Global.flags, PageAllocatorType_BGJIT,
        AutoSystemInfo::Data.IsLowMemoryProcess() ?
        PageAllocator::DefaultLowMaxFreePageCount :
        PageAllocator::DefaultMaxFreePageCount
    )
{TRACE_IT(15376);
    ucrtC99MathApis.Ensure();
    m_pid = GetProcessId((HANDLE)data->processHandle);

#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
    m_codeGenAlloc.canCreatePreReservedSegment = data->allowPrereserveAlloc != FALSE;
#endif
    m_numericPropertyBV = HeapNew(BVSparse<HeapAllocator>, &HeapAllocator::Instance);
}

ServerThreadContext::~ServerThreadContext()
{TRACE_IT(15377);
    if (this->m_numericPropertyBV != nullptr)
    {TRACE_IT(15378);
        HeapDelete(m_numericPropertyBV);
        this->m_numericPropertyBV = nullptr;
    }

}

PreReservedSectionAllocWrapper *
ServerThreadContext::GetPreReservedSectionAllocator()
{TRACE_IT(15379);
    return &m_preReservedSectionAllocator;
}

intptr_t
ServerThreadContext::GetBailOutRegisterSaveSpaceAddr() const
{TRACE_IT(15380);
    return static_cast<intptr_t>(m_threadContextData.bailOutRegisterSaveSpaceAddr);
}

ptrdiff_t
ServerThreadContext::GetChakraBaseAddressDifference() const
{TRACE_IT(15381);
    return GetRuntimeChakraBaseAddress() - (intptr_t)AutoSystemInfo::Data.GetChakraBaseAddr();
}

ptrdiff_t
ServerThreadContext::GetCRTBaseAddressDifference() const
{TRACE_IT(15382);
    return GetRuntimeCRTBaseAddress() - GetJITCRTBaseAddress();
}

intptr_t
ServerThreadContext::GetDisableImplicitFlagsAddr() const
{TRACE_IT(15383);
    return static_cast<intptr_t>(m_threadContextData.disableImplicitFlagsAddr);
}

intptr_t
ServerThreadContext::GetImplicitCallFlagsAddr() const
{TRACE_IT(15384);
    return static_cast<intptr_t>(m_threadContextData.implicitCallFlagsAddr);
}

#if defined(ENABLE_SIMDJS) && (defined(_M_IX86) || defined(_M_X64))
intptr_t
ServerThreadContext::GetSimdTempAreaAddr(uint8 tempIndex) const
{TRACE_IT(15385);
    Assert(tempIndex < SIMD_TEMP_SIZE);
    return m_threadContextData.simdTempAreaBaseAddr + tempIndex * sizeof(_x86_SIMDValue);
}
#endif

intptr_t
ServerThreadContext::GetThreadStackLimitAddr() const
{TRACE_IT(15386);
    return static_cast<intptr_t>(m_threadContextData.threadStackLimitAddr);
}

size_t
ServerThreadContext::GetScriptStackLimit() const
{TRACE_IT(15387);
    return static_cast<size_t>(m_threadContextData.scriptStackLimit);
}

bool
ServerThreadContext::IsThreadBound() const
{TRACE_IT(15388);
    return m_threadContextData.isThreadBound != FALSE;
}

HANDLE
ServerThreadContext::GetProcessHandle() const
{TRACE_IT(15389);
    return reinterpret_cast<HANDLE>(m_threadContextData.processHandle);
}

CustomHeap::OOPCodePageAllocators *
ServerThreadContext::GetThunkPageAllocators()
{TRACE_IT(15390);
    return &m_thunkPageAllocators;
}

CustomHeap::OOPCodePageAllocators *
ServerThreadContext::GetCodePageAllocators()
{TRACE_IT(15391);
    return &m_codePageAllocators;
}

SectionAllocWrapper *
ServerThreadContext::GetSectionAllocator()
{TRACE_IT(15392);
    return &m_sectionAllocator;
}

OOPCodeGenAllocators *
ServerThreadContext::GetCodeGenAllocators()
{TRACE_IT(15393);
    return &m_codeGenAlloc;
}

intptr_t
ServerThreadContext::GetRuntimeChakraBaseAddress() const
{TRACE_IT(15394);
    return static_cast<intptr_t>(m_threadContextData.chakraBaseAddress);
}

intptr_t
ServerThreadContext::GetRuntimeCRTBaseAddress() const
{TRACE_IT(15395);
    return static_cast<intptr_t>(m_threadContextData.crtBaseAddress);
}

intptr_t
ServerThreadContext::GetJITCRTBaseAddress() const
{TRACE_IT(15396);
    return (intptr_t)ucrtC99MathApis.GetHandle();
}

PageAllocator *
ServerThreadContext::GetForegroundPageAllocator()
{TRACE_IT(15397);
    return &m_pageAlloc;
}

bool
ServerThreadContext::IsNumericProperty(Js::PropertyId propertyId)
{TRACE_IT(15398);
    if (propertyId >= 0 && Js::IsInternalPropertyId(propertyId))
    {TRACE_IT(15399);
        return Js::InternalPropertyRecords::GetInternalPropertyName(propertyId)->IsNumeric();
    }

    bool found = false;
    {
        AutoCriticalSection lock(&m_cs);
        found = m_numericPropertyBV->Test(propertyId) != FALSE;
    }

    return found;
}

void
ServerThreadContext::UpdateNumericPropertyBV(BVSparseNode * newProps)
{TRACE_IT(15400);
    AutoCriticalSection lock(&m_cs);
    m_numericPropertyBV->CopyFromNode(newProps);
}

void ServerThreadContext::AddRef()
{
    InterlockedExchangeAdd(&m_refCount, (uint)1);
}
void ServerThreadContext::Release()
{
    InterlockedExchangeSubtract(&m_refCount, (uint)1);
    if (m_isClosed && m_refCount == 0)
    {TRACE_IT(15401);
        HeapDelete(this);
    }
}
void ServerThreadContext::Close()
{TRACE_IT(15402);
    this->m_isClosed = true;
#ifdef STACK_BACK_TRACE
    ServerContextManager::RecordCloseContext(this);
#endif
}
#endif
