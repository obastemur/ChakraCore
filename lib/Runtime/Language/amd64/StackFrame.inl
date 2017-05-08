//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

inline void Js::Amd64StackFrame::EnsureFunctionEntry()
{TRACE_IT(53343);
    if (!functionEntry)
    {TRACE_IT(53344);
        functionEntry = RtlLookupFunctionEntry(currentContext->Rip, &imageBase, nullptr);
    }
}

inline bool Js::Amd64StackFrame::EnsureCallerContext(bool isCurrentContextNative)
{TRACE_IT(53345);
    if (!hasCallerContext)
    {TRACE_IT(53346);
        *callerContext = *currentContext;
        if (isCurrentContextNative)
        {TRACE_IT(53347);
            if (NextFromNativeAddress(callerContext))
            {TRACE_IT(53348);
                hasCallerContext = true;
                return true;
            }
            return false;
        }
        EnsureFunctionEntry();
        if (Next(callerContext, imageBase, functionEntry))
        {TRACE_IT(53349);
            hasCallerContext = true;
            return true;
        }
        else
        {TRACE_IT(53350);
            return false;
        }
    }

    return true;
}

inline void Js::Amd64StackFrame::OnCurrentContextUpdated()
{TRACE_IT(53351);
    imageBase = 0;
    functionEntry = nullptr;
    hasCallerContext = false;
}
