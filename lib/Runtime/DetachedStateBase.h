//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class DetachedStateBase
    {
    protected:
        TypeId typeId;
        bool hasBeenClaimed;

    public:
        DetachedStateBase(TypeId typeId)
            : typeId(typeId),
            hasBeenClaimed(false)
        {LOGMEIN("DetachedStateBase.h] 18\n");
        }

        virtual ~DetachedStateBase()
        {LOGMEIN("DetachedStateBase.h] 22\n");
        }

        TypeId GetTypeId() {LOGMEIN("DetachedStateBase.h] 25\n"); return typeId; }

        bool HasBeenClaimed() {LOGMEIN("DetachedStateBase.h] 27\n"); return hasBeenClaimed; }

        void MarkAsClaimed() {LOGMEIN("DetachedStateBase.h] 29\n"); hasBeenClaimed = true; }

        void CleanUp()
        {LOGMEIN("DetachedStateBase.h] 32\n");
            if (!hasBeenClaimed)
            {LOGMEIN("DetachedStateBase.h] 34\n");
                DiscardState();
            }
            ClearSelfOnly();
        }

        virtual void ClearSelfOnly() = 0;
        virtual void DiscardState() = 0;
        virtual void Discard() = 0;
    };

    typedef enum ArrayBufferAllocationType
    {
        Heap = 0x0,
        CoTask = 0x1,
        MemAlloc = 0x02
    } ArrayBufferAllocationType;

    class ArrayBufferDetachedStateBase : public DetachedStateBase
    {
    public:
        BYTE* buffer;
        uint32 bufferLength;
        ArrayBufferAllocationType allocationType;

        ArrayBufferDetachedStateBase(TypeId typeId, BYTE* buffer, uint32 bufferLength, ArrayBufferAllocationType allocationType)
            : DetachedStateBase(typeId),
            buffer(buffer),
            bufferLength(bufferLength),
            allocationType(allocationType)
        {LOGMEIN("DetachedStateBase.h] 64\n");}

    };
}
