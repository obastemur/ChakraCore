//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// This file contains debug dumpers which can be called from the debugger.
//
//----------------------------------------------------------------------------

#include "Backend.h"

#if DBG_DUMP

Func *CurrentFunc;

///----------------------------------------------------------------------------
///
///     Dump the given instr
///
///----------------------------------------------------------------------------

void dump(IR::Instr *instr)
{TRACE_IT(1656);
    instr->Dump();
}

///----------------------------------------------------------------------------
///
///     Dump the given instr, window/2 instrs before, and window/2 instrs after
///
///----------------------------------------------------------------------------

void dump(IR::Instr *instr, int window)
{TRACE_IT(1657);
    instr->Dump(window);
}

///----------------------------------------------------------------------------
///
///     Dump the current function being compiled
///
///----------------------------------------------------------------------------

void dump()
{TRACE_IT(1658);
    CurrentFunc->Dump();
}

#endif   // DBG_DUMP
