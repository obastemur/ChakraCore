//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#ifdef PROFILE_RECYCLER_ALLOC
// Initialization order
//  AB AutoSystemInfo
//  AD PerfCounter
//  AE PerfCounterSet
//  AM Output/Configuration
//  AN MemProtectHeap
//  AP DbgHelpSymbolManager
//  AQ CFGLogger
//  AR LeakReport
//  AS JavascriptDispatch/RecyclerObjectDumper
//  AT HeapAllocator/RecyclerHeuristic
//  AU RecyclerWriteBarrierManager
#pragma warning(disable:4075)       // initializers put in unrecognized initialization area on purpose
#pragma init_seg(".CRT$XCAS")

RecyclerObjectDumper::DumpFunctionMap * RecyclerObjectDumper::dumpFunctionMap = nullptr;
RecyclerObjectDumper RecyclerObjectDumper::Instance;

RecyclerObjectDumper::~RecyclerObjectDumper()
{TRACE_IT(26347);
    if (dumpFunctionMap)
    {TRACE_IT(26348);
        NoCheckHeapDelete(dumpFunctionMap);
    }
}

BOOL
RecyclerObjectDumper::EnsureDumpFunctionMap()
{TRACE_IT(26349);
    if (dumpFunctionMap == nullptr)
    {TRACE_IT(26350);
        dumpFunctionMap = NoCheckHeapNew(DumpFunctionMap, &NoCheckHeapAllocator::Instance);
    }
    return (dumpFunctionMap != nullptr);
}

void
RecyclerObjectDumper::RegisterDumper(type_info const * typeinfo, DumpFunction dumperFunction)
{TRACE_IT(26351);
    if (EnsureDumpFunctionMap())
    {TRACE_IT(26352);
        Assert(!dumpFunctionMap->ContainsKey(typeinfo));
        dumpFunctionMap->Add(typeinfo, dumperFunction);
    }
}

void
RecyclerObjectDumper::DumpObject(type_info const * typeinfo, bool isArray, void * objectAddress)
{TRACE_IT(26353);
    if (typeinfo == nullptr)
    {TRACE_IT(26354);
        Output::Print(_u("Address %p"), objectAddress);
    }
    else
    {TRACE_IT(26355);
        DumpFunction dumpFunction;
        if (dumpFunctionMap == nullptr || !dumpFunctionMap->TryGetValue(typeinfo, &dumpFunction) || !dumpFunction(typeinfo, isArray, objectAddress))
        {TRACE_IT(26356);
            Output::Print(isArray? _u("%S[] %p") : _u("%S %p"), typeinfo->name(), objectAddress);
        }
    }
}
#endif
