//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Memory
{
class Recycler;

typedef JsUtil::SynchronizedDictionary<void *, void *, NoCheckHeapAllocator, PrimeSizePolicy, RecyclerPointerComparer, JsUtil::SimpleDictionaryEntry, Js::DefaultContainerLockPolicy, CriticalSection> MarkMap;

class MarkContext
{
private:
    struct MarkCandidate
    {
        void ** obj;
        size_t byteCount;
    };

public:
    static const int MarkCandidateSize = sizeof(MarkCandidate);

    MarkContext(Recycler * recycler, PagePool * pagePool);
    ~MarkContext();

    void Init(uint reservedPageCount);
    void Clear();

    Recycler * GetRecycler() {TRACE_IT(24682); return this->recycler; }

    bool AddMarkedObject(void * obj, size_t byteCount);
#if ENABLE_CONCURRENT_GC
    bool AddTrackedObject(FinalizableObject * obj);
#endif

    template <bool parallel, bool interior, bool doSpecialMark>
    void Mark(void * candidate, void * parentReference);
    template <bool parallel>
    void MarkInterior(void * candidate);
    template <bool parallel, bool interior>
    void ScanObject(void ** obj, size_t byteCount);
    template <bool parallel, bool interior, bool doSpecialMark>
    void ScanMemory(void ** obj, size_t byteCount);
    template <bool parallel, bool interior>
    void ProcessMark();

    void MarkTrackedObject(FinalizableObject * obj);
    void ProcessTracked();

    uint Split(uint targetCount, __in_ecount(targetCount) MarkContext ** targetContexts);

    void Abort();
    void Release();

    bool HasPendingMarkObjects() const {TRACE_IT(24683); return !markStack.IsEmpty(); }
    bool HasPendingTrackObjects() const {TRACE_IT(24684); return !trackStack.IsEmpty(); }
    bool HasPendingObjects() const {TRACE_IT(24685); return HasPendingMarkObjects() || HasPendingTrackObjects(); }

    PageAllocator * GetPageAllocator() {TRACE_IT(24686); return this->pagePool->GetPageAllocator(); }

    bool IsEmpty()
    {TRACE_IT(24687);
        if (HasPendingObjects())
        {TRACE_IT(24688);
            return false;
        }

        Assert(pagePool->IsEmpty());
        Assert(!GetPageAllocator()->DisableAllocationOutOfMemory());
        return true;
    }

#if DBG
    void VerifyPostMarkState()
    {TRACE_IT(24689);
        Assert(this->markStack.HasChunk());
    }
#endif

    void Cleanup()
    {TRACE_IT(24690);
        Assert(!HasPendingObjects());
        Assert(!GetPageAllocator()->DisableAllocationOutOfMemory());
        this->pagePool->ReleaseFreePages();
    }

    void DecommitPages() {TRACE_IT(24691); this->pagePool->Decommit(); }


#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void SetMaxPageCount(size_t maxPageCount) {TRACE_IT(24692); markStack.SetMaxPageCount(maxPageCount); trackStack.SetMaxPageCount(maxPageCount); }
#endif

#ifdef RECYCLER_MARK_TRACK
    void SetMarkMap(MarkMap* markMap)
    {TRACE_IT(24693);
        this->markMap = markMap;
    }
#endif

private:
    Recycler * recycler;
    PagePool * pagePool;
    PageStack<MarkCandidate> markStack;
    PageStack<FinalizableObject *> trackStack;

#ifdef RECYCLER_MARK_TRACK
    MarkMap* markMap;

    void OnObjectMarked(void* object, void* parent);
#endif

#if DBG && GLOBAL_ENABLE_WRITE_BARRIER
public:
    void* parentRef;
#endif
};


}
