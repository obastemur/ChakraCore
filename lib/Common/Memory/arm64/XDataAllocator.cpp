//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// This one works only for ARM64
#include "CommonMemoryPch.h"
#if !defined(_M_ARM64)
CompileAssert(false)
#endif

#include "XDataAllocator.h"
#include "Core/DelayLoadLibrary.h"

XDataAllocator::XDataAllocator(BYTE* address, uint size)
{TRACE_IT(27221);
    __debugbreak();
}


void XDataAllocator::Delete()
{TRACE_IT(27222);
    __debugbreak();
}

bool XDataAllocator::Initialize(void* segmentStart, void* segmentEnd)
{TRACE_IT(27223);
    __debugbreak();
    return true;
}

bool XDataAllocator::Alloc(ULONG_PTR functionStart, DWORD functionSize, ushort pdataCount, ushort xdataSize, SecondaryAllocation* allocation)
{TRACE_IT(27224);
    __debugbreak();
    return false;
}


void XDataAllocator::Release(const SecondaryAllocation& allocation)
{TRACE_IT(27225);
    __debugbreak();
}

/* static */
void XDataAllocator::Register(XDataAllocation * xdataInfo, intptr_t functionStart, DWORD functionSize)
{TRACE_IT(27226);
    __debugbreak();
}

/* static */
void XDataAllocator::Unregister(XDataAllocation * xdataInfo)
{TRACE_IT(27227);
    __debugbreak();
}

bool XDataAllocator::CanAllocate()
{TRACE_IT(27228);
    __debugbreak();
    return true;
}
