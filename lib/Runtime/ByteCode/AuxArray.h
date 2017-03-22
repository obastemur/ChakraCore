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
        {LOGMEIN("AuxArray.h] 15\n");
        }

        static size_t OffsetOfElements() {LOGMEIN("AuxArray.h] 18\n"); return offsetof(AuxArray<T>, elements); }
        void SetCount(uint count) {LOGMEIN("AuxArray.h] 19\n"); this->count = count; }
        size_t GetDataSize() const {LOGMEIN("AuxArray.h] 20\n"); return sizeof(AuxArray) + sizeof(T) * count; }
    };
    typedef AuxArray<Var> VarArray;

    struct FuncInfoEntry
    {
        uint nestedIndex;
        uint scopeSlot;
    };
    typedef AuxArray<FuncInfoEntry> FuncInfoArray;
}
