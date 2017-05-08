//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

// xplat-todo: Need to figure out equivalent method for allocation tracing
// on platforms other than Windows
#ifdef ETW_MEMORY_TRACKING
#include "microsoft-scripting-jscript9.internalevents.h"

enum ArenaType: unsigned short
{
    ArenaTypeRecycler = 1,
    ArenaTypeArena = 2
};

class EtwMemoryEvents
{
public:
    _NOINLINE static void ReportAllocation(void *arena, void *address, size_t size)
    {TRACE_IT(23212);
        allocCount++;
        if (allocCount == 7500)
        {TRACE_IT(23213);
            allocCount = 0;
            ::Sleep(350);
        }

        EventWriteJSCRIPT_INTERNAL_ALLOCATOR_ALLOC(arena, address, size);
    }

    _NOINLINE static void ReportFree(void *arena, void *address, size_t size)
    {
        EventWriteJSCRIPT_INTERNAL_ALLOCATOR_FREE(arena, address, size);
    }

    _NOINLINE static void ReportReallocation(void *arena, void *address, size_t existingSize, size_t newSize)
    {
        EventWriteJSCRIPT_INTERNAL_ALLOCATOR_FREE(arena, address, existingSize);
        EventWriteJSCRIPT_INTERNAL_ALLOCATOR_ALLOC(arena, address, newSize);
    }

    _NOINLINE static void ReportArenaCreated(void *arena, ArenaType arenaType)
    {
        EventWriteJSCRIPT_INTERNAL_ALLOCATOR_CREATE(arena, arenaType);
    }

    _NOINLINE static void ReportArenaDestroyed(void *arena)
    {TRACE_IT(23214);
        EventWriteJSCRIPT_INTERNAL_ALLOCATOR_DESTROY(arena);
    }

    _NOINLINE static void ReportFreeAll(void *arena, ArenaType arenaType)
    {TRACE_IT(23215);
        EventWriteJSCRIPT_INTERNAL_ALLOCATOR_DESTROY(arena);
        EventWriteJSCRIPT_INTERNAL_ALLOCATOR_CREATE(arena, arenaType);
    }

private:
    static int allocCount;
};

int EtwMemoryEvents::allocCount = 0;

// Workaround to stop the linker from collapsing the Arena/recycler methods to
// a single implementation so that we have nicer stacks
ArenaType g_arena;
#define DISTINGUISH_FUNCTION(arenaType) g_arena = ArenaType##arenaType

void ArenaMemoryTracking::Activate()
{TRACE_IT(23216);
}

_NOINLINE void ArenaMemoryTracking::ArenaCreated(Allocator *arena, __in LPCWSTR name)
{TRACE_IT(23217);
    DISTINGUISH_FUNCTION(Arena);
    EtwMemoryEvents::ReportArenaCreated(arena, ArenaTypeArena);
}

_NOINLINE void ArenaMemoryTracking::ArenaDestroyed(Allocator *arena)
{TRACE_IT(23218);
    DISTINGUISH_FUNCTION(Arena);
    EtwMemoryEvents::ReportArenaDestroyed(arena);
}

_NOINLINE void ArenaMemoryTracking::ReportAllocation(Allocator *arena, void *address, size_t size)
{TRACE_IT(23219);
    DISTINGUISH_FUNCTION(Arena);
    EtwMemoryEvents::ReportAllocation(arena, address, size);
}

_NOINLINE void ArenaMemoryTracking::ReportReallocation(Allocator *arena, void *address, size_t existingSize, size_t newSize)
{TRACE_IT(23220);
    DISTINGUISH_FUNCTION(Arena);
    EtwMemoryEvents::ReportReallocation(arena, address, existingSize, newSize);
}

_NOINLINE void ArenaMemoryTracking::ReportFree(Allocator *arena, void *address, size_t size)
{TRACE_IT(23221);
    DISTINGUISH_FUNCTION(Arena);
    EtwMemoryEvents::ReportFree(arena, address, size);
}

_NOINLINE void ArenaMemoryTracking::ReportFreeAll(Allocator *arena)
{TRACE_IT(23222);
    DISTINGUISH_FUNCTION(Arena);
    EtwMemoryEvents::ReportFreeAll(arena, ArenaTypeArena);
}

// Recycler tracking

void RecyclerMemoryTracking::Activate()
{TRACE_IT(23223);
}

bool RecyclerMemoryTracking::IsActive()
{TRACE_IT(23224);
    return true;
}

// The external reporting for the recycler uses the MemspectMemoryTracker
_NOINLINE void RecyclerMemoryTracking::ReportRecyclerCreate(Recycler * recycler)
{TRACE_IT(23225);
    DISTINGUISH_FUNCTION(Recycler);
    EtwMemoryEvents::ReportArenaCreated(recycler, ArenaTypeRecycler);
}

_NOINLINE void RecyclerMemoryTracking::ReportRecyclerDestroy(Recycler * recycler)
{TRACE_IT(23226);
    DISTINGUISH_FUNCTION(Recycler);
    EtwMemoryEvents::ReportArenaDestroyed(recycler);
}

_NOINLINE void RecyclerMemoryTracking::ReportAllocation(Recycler * recycler, __in void *address, size_t size)
{TRACE_IT(23227);
    DISTINGUISH_FUNCTION(Recycler);
    EtwMemoryEvents::ReportAllocation(recycler, address, size);
}

_NOINLINE void RecyclerMemoryTracking::ReportFree(Recycler * recycler, __in void *address, size_t size)
{TRACE_IT(23228);
    DISTINGUISH_FUNCTION(Recycler);
    EtwMemoryEvents::ReportFree(recycler, address, size);
}

void RecyclerMemoryTracking::ReportUnallocated(Recycler * recycler, __in void* address, __in void *endAddress, size_t sizeCat)
{TRACE_IT(23229);
    byte * byteAddress = (byte *) address;

    while (byteAddress + sizeCat <= endAddress)
    {TRACE_IT(23230);
        EtwMemoryEvents::ReportAllocation(recycler, byteAddress, sizeCat);
        byteAddress += sizeCat;
    }
}

#endif
