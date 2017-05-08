//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template<typename T>
    struct AuxArray
    {
        uint32 count;
        T elements[];

        AuxArray(uint32 count) : count(count)
        {TRACE_IT(38312);
        }

        static size_t OffsetOfElements() {TRACE_IT(38313); return offsetof(AuxArray<T>, elements); }
        void SetCount(uint count) {TRACE_IT(38314); this->count = count; }
        size_t GetDataSize() const {TRACE_IT(38315); return sizeof(AuxArray) + sizeof(T) * count; }
    };
    typedef AuxArray<Var> VarArray;

    struct FuncInfoEntry
    {
        uint nestedIndex;
        uint scopeSlot;
    };
    typedef AuxArray<FuncInfoEntry> FuncInfoArray;
}
