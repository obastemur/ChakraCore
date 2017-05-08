//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class StackProber
{
public:
    void Initialize();
    size_t GetScriptStackLimit() const {TRACE_IT(36888); return stackLimit; }
#if DBG
    void AdjustKnownStackLimit(size_t sp, size_t size)
    {TRACE_IT(36889);
        if (knownStackLimit == 0) knownStackLimit = sp - size;
        knownStackLimit = ((sp - size) < knownStackLimit) ? (sp - size) : knownStackLimit;
    }
#endif

private:
    size_t stackLimit;
#if DBG
    size_t knownStackLimit;
#endif
};
