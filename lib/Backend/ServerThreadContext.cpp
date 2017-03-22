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
{LOGMEIN("ServerThreadContext.cpp] 25\n");
    ucrtC99MathApis.Ensure();
    m_pid = GetProcessId((HANDLE)data->processHandle);

#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
    m_codeGenAlloc.canCreatePreReservedSegment = data->allowPrereserveAlloc != FALSE;
#endif
    m_numericPropertyBV = HeapNew(BVSparse<HeapAllocator>, &HeapAllocator::Instance);
}

ServerThreadContext::~ServerThreadContext()
{LOGMEIN("ServerThreadContext.cpp] 36\n");
    if (this->m_numericPropertyBV != nullptr)
    {LOGMEIN("ServerThreadContext.cpp] 38\n");
        HeapDelete(m_numericPropertyBV);
        this->m_numericPropertyBV = nullptr;
    }

}

PreReservedSectionAllocWrapper *
ServerThreadContext::GetPreReservedSectionAllocator()
{LOGMEIN("ServerThreadContext.cpp] 47\n");
    return &m_preReservedSectionAllocator;
}

intptr_t
ServerThreadContext::GetBailOutRegisterSaveSpaceAddr() const
{LOGMEIN("ServerThreadContext.cpp] 53\n");
    return static_cast<intptr_t>(m_threadContextData.bailOutRegisterSaveSpaceAddr);
}

ptrdiff_t
ServerThreadContext::GetChakraBaseAddressDifference() const
{LOGMEIN("ServerThreadContext.cpp] 59\n");
    return GetRuntimeChakraBaseAddress() - (intptr_t)AutoSystemInfo::Data.GetChakraBaseAddr();
}

ptrdiff_t
ServerThreadContext::GetCRTBaseAddressDifference() const
{LOGMEIN("ServerThreadContext.cpp] 65\n");
    return GetRuntimeCRTBaseAddress() - GetJITCRTBaseAddress();
}

intptr_t
ServerThreadContext::GetDisableImplicitFlagsAddr() const
{LOGMEIN("ServerThreadContext.cpp] 71\n");
    return static_cast<intptr_t>(m_threadContextData.disableImplicitFlagsAddr);
}

intptr_t
ServerThreadContext::GetImplicitCallFlagsAddr() const
{LOGMEIN("ServerThreadContext.cpp] 77\n");
    return static_cast<intptr_t>(m_threadContextData.implicitCallFlagsAddr);
}

#if defined(ENABLE_SIMDJS) && (defined(_M_IX86) || defined(_M_X64))
intptr_t
ServerThreadContext::GetSimdTempAreaAddr(uint8 tempIndex) const
{LOGMEIN("ServerThreadContext.cpp] 84\n");
    Assert(tempIndex < SIMD_TEMP_SIZE);
    return m_threadContextData.simdTempAreaBaseAddr + tempIndex * sizeof(_x86_SIMDValue);
}
#endif

intptr_t
ServerThreadContext::GetThreadStackLimitAddr() const
{LOGMEIN("ServerThreadContext.cpp] 92\n");
    return static_cast<intptr_t>(m_threadContextData.threadStackLimitAddr);
}

size_t
ServerThreadContext::GetScriptStackLimit() const
{LOGMEIN("ServerThreadContext.cpp] 98\n");
    return static_cast<size_t>(m_threadContextData.scriptStackLimit);
}

bool
ServerThreadContext::IsThreadBound() const
{LOGMEIN("ServerThreadContext.cpp] 104\n");
    return m_threadContextData.isThreadBound != FALSE;
}

HANDLE
ServerThreadContext::GetProcessHandle() const
{LOGMEIN("ServerThreadContext.cpp] 110\n");
    return reinterpret_cast<HANDLE>(m_threadContextData.processHandle);
}

CustomHeap::OOPCodePageAllocators *
ServerThreadContext::GetThunkPageAllocators()
{LOGMEIN("ServerThreadContext.cpp] 116\n");
    return &m_thunkPageAllocators;
}

CustomHeap::OOPCodePageAllocators *
ServerThreadContext::GetCodePageAllocators()
{LOGMEIN("ServerThreadContext.cpp] 122\n");
    return &m_codePageAllocators;
}

SectionAllocWrapper *
ServerThreadContext::GetSectionAllocator()
{LOGMEIN("ServerThreadContext.cpp] 128\n");
    return &m_sectionAllocator;
}

OOPCodeGenAllocators *
ServerThreadContext::GetCodeGenAllocators()
{LOGMEIN("ServerThreadContext.cpp] 134\n");
    return &m_codeGenAlloc;
}

intptr_t
ServerThreadContext::GetRuntimeChakraBaseAddress() const
{LOGMEIN("ServerThreadContext.cpp] 140\n");
    return static_cast<intptr_t>(m_threadContextData.chakraBaseAddress);
}

intptr_t
ServerThreadContext::GetRuntimeCRTBaseAddress() const
{LOGMEIN("ServerThreadContext.cpp] 146\n");
    return static_cast<intptr_t>(m_threadContextData.crtBaseAddress);
}

intptr_t
ServerThreadContext::GetJITCRTBaseAddress() const
{LOGMEIN("ServerThreadContext.cpp] 152\n");
    return (intptr_t)ucrtC99MathApis.GetHandle();
}

PageAllocator *
ServerThreadContext::GetForegroundPageAllocator()
{LOGMEIN("ServerThreadContext.cpp] 158\n");
    return &m_pageAlloc;
}

bool
ServerThreadContext::IsNumericProperty(Js::PropertyId propertyId)
{LOGMEIN("ServerThreadContext.cpp] 164\n");
    if (propertyId >= 0 && Js::IsInternalPropertyId(propertyId))
    {LOGMEIN("ServerThreadContext.cpp] 166\n");
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
{LOGMEIN("ServerThreadContext.cpp] 181\n");
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
    {LOGMEIN("ServerThreadContext.cpp] 194\n");
        HeapDelete(this);
    }
}
void ServerThreadContext::Close()
{LOGMEIN("ServerThreadContext.cpp] 199\n");
    this->m_isClosed = true;
#ifdef STACK_BACK_TRACE
    ServerContextManager::RecordCloseContext(this);
#endif
}
#endif
