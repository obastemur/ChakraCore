//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
    template <bool isGuestArena>
    TempArenaAllocatorWrapper<isGuestArena>* TempArenaAllocatorWrapper<isGuestArena>::Create(ThreadContext * threadContext)
    {TRACE_IT(36890);
        Recycler * recycler = threadContext->GetRecycler();
        TempArenaAllocatorWrapper<isGuestArena> * wrapper = RecyclerNewFinalizedLeaf(recycler, Js::TempArenaAllocatorWrapper<isGuestArena>,
            _u("temp"), threadContext->GetPageAllocator(), Js::Throw::OutOfMemory);
        if (isGuestArena)
        {TRACE_IT(36891);
            wrapper->recycler = recycler;
            wrapper->AdviseInUse();
        }
        return wrapper;
    }

    template <bool isGuestArena>
    TempArenaAllocatorWrapper<isGuestArena>::TempArenaAllocatorWrapper(__in LPCWSTR name, PageAllocator * pageAllocator, void (*outOfMemoryFunc)()) :
        allocator(name, pageAllocator, outOfMemoryFunc), recycler(nullptr), externalGuestArenaRef(nullptr)
    {TRACE_IT(36892);
    }

    template <bool isGuestArena>
    void TempArenaAllocatorWrapper<isGuestArena>::Dispose(bool isShutdown)
    {TRACE_IT(36893);
        allocator.Clear();
        if (isGuestArena && externalGuestArenaRef != nullptr)
        {TRACE_IT(36894);
            this->recycler->UnregisterExternalGuestArena(externalGuestArenaRef);
            externalGuestArenaRef = nullptr;
        }

        Assert(allocator.AllocatedSize() == 0);
    }

    template <bool isGuestArena>
    void TempArenaAllocatorWrapper<isGuestArena>::AdviseInUse()
    {TRACE_IT(36895);
        if (isGuestArena)
        {TRACE_IT(36896);
            if (externalGuestArenaRef == nullptr)
            {TRACE_IT(36897);
                externalGuestArenaRef = this->recycler->RegisterExternalGuestArena(this->GetAllocator());
                if (externalGuestArenaRef == nullptr)
                {TRACE_IT(36898);
                    Js::Throw::OutOfMemory();
                }
            }
        }
    }

    template <bool isGuestArena>
    void TempArenaAllocatorWrapper<isGuestArena>::AdviseNotInUse()
    {TRACE_IT(36899);
        this->allocator.Reset();

        if (isGuestArena)
        {TRACE_IT(36900);
            Assert(externalGuestArenaRef != nullptr);
            this->recycler->UnregisterExternalGuestArena(externalGuestArenaRef);
            externalGuestArenaRef = nullptr;
        }
    }

    // Explicit instantiation
    template class TempArenaAllocatorWrapper<true>;
    template class TempArenaAllocatorWrapper<false>;

} // namespace Js
