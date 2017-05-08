//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// PagePool caches freed pages in a pool for reuse, and more importantly,
// defers freeing them until ReleaseFreePages is called.
// This allows us to free the pages when we know it is multi-thread safe to do so,
// e.g. after all parallel marking is completed.

namespace Memory
{
class PagePoolPage
{
private:
    PageAllocator * pageAllocator;
    PageSegment * pageSegment;
    bool isReserved;

public:
    static PagePoolPage * New(PageAllocator * pageAllocator, bool isReserved = false)
    {TRACE_IT(25171);
        PageSegment * pageSegment;
        PagePoolPage * newPage = (PagePoolPage *)pageAllocator->AllocPages(1, &pageSegment);
        if (newPage == nullptr)
        {TRACE_IT(25172);
            return nullptr;
        }

        newPage->pageAllocator = pageAllocator;
        newPage->pageSegment = pageSegment;
        newPage->isReserved = isReserved;

        return newPage;
    }

    void Free()
    {TRACE_IT(25173);
        this->pageAllocator->ReleasePages(this, 1, this->pageSegment);
    }

    bool IsReserved()
    {TRACE_IT(25174);
        return isReserved;
    }
};

class PagePool
{
private:
    class PagePoolFreePage : public PagePoolPage
    {
    public:
        PagePoolFreePage * nextFreePage;
    };

    PageAllocator pageAllocator;
    PagePoolFreePage * freePageList;

    // List of pre-allocated pages that are
    // freed only when the page pool is destroyed
    PagePoolFreePage * reservedPageList;

public:
    PagePool(Js::ConfigFlagsTable& flagsTable) :
        pageAllocator(NULL, flagsTable, PageAllocatorType_GCThread,
            PageAllocator::DefaultMaxFreePageCount, false,
#if ENABLE_BACKGROUND_PAGE_ZEROING
            nullptr,
#endif
            PageAllocator::DefaultMaxAllocPageCount, 0, true),
        freePageList(nullptr),
        reservedPageList(nullptr)
    {TRACE_IT(25175);
    }

    void ReservePages(uint reservedPageCount)
    {TRACE_IT(25176);
        for (uint i = 0; i < reservedPageCount; i++)
        {TRACE_IT(25177);
            PagePoolPage* page = PagePoolPage::New(&pageAllocator, true);

            if (page == nullptr)
            {TRACE_IT(25178);
                Js::Throw::OutOfMemory();
            }
            FreeReservedPage(page);
        }
    }

    ~PagePool()
    {TRACE_IT(25179);
        Assert(freePageList == nullptr);

        if (reservedPageList != nullptr)
        {TRACE_IT(25180);
            while (reservedPageList != nullptr)
            {TRACE_IT(25181);
                PagePoolFreePage * page = reservedPageList;
                Assert(page->IsReserved());
                reservedPageList = reservedPageList->nextFreePage;
                page->Free();
            }
        }
    }

    PageAllocator * GetPageAllocator() {TRACE_IT(25182); return &this->pageAllocator; }

    PagePoolPage * GetPage(bool useReservedPages = false)
    {TRACE_IT(25183);
        if (freePageList != nullptr)
        {TRACE_IT(25184);
            PagePoolPage * page = freePageList;
            freePageList = freePageList->nextFreePage;
            Assert(!page->IsReserved());
            return page;
        }

        if (useReservedPages && reservedPageList != nullptr)
        {TRACE_IT(25185);
            PagePoolPage * page = reservedPageList;
            reservedPageList = reservedPageList->nextFreePage;
            Assert(page->IsReserved());
            return page;
        }

        return PagePoolPage::New(&pageAllocator);
    }

    void FreePage(PagePoolPage * page)
    {TRACE_IT(25186);
        PagePoolFreePage * freePage = (PagePoolFreePage *)page;
        if (freePage->IsReserved())
        {TRACE_IT(25187);
            return FreeReservedPage(page);

        }

        freePage->nextFreePage = freePageList;
        freePageList = freePage;
    }

    void ReleaseFreePages()
    {TRACE_IT(25188);
        while (freePageList != nullptr)
        {TRACE_IT(25189);
            PagePoolFreePage * page = freePageList;
            Assert(!page->IsReserved());
            freePageList = freePageList->nextFreePage;
            page->Free();
        }
    }

    void Decommit()
    {TRACE_IT(25190);
        pageAllocator.DecommitNow();
    }

#if DBG
    bool IsEmpty() const {TRACE_IT(25191); return (freePageList == nullptr); }
#endif

private:
    void FreeReservedPage(PagePoolPage * page)
    {TRACE_IT(25192);
        Assert(page->IsReserved());
        PagePoolFreePage * freePage = (PagePoolFreePage *)page;

        freePage->nextFreePage = reservedPageList;
        reservedPageList = freePage;
    }
 };
}
