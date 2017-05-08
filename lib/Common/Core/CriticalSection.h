//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class CriticalSection
{
public:
    CriticalSection(DWORD spincount = 0)
    {TRACE_IT(19935);
#pragma prefast(suppress:6031, "InitializeCriticalSectionAndSpinCount always succeed since Vista. No need to check return value");
        ::InitializeCriticalSectionAndSpinCount(&cs, spincount);
    }
    ~CriticalSection() {TRACE_IT(19936); ::DeleteCriticalSection(&cs); }
    BOOL TryEnter() {TRACE_IT(19937); return ::TryEnterCriticalSection(&cs); }
    void Enter() {TRACE_IT(19938); ::EnterCriticalSection(&cs); }
    void Leave() {TRACE_IT(19939); ::LeaveCriticalSection(&cs); }
#if DBG
    bool IsLocked() const {TRACE_IT(19940); return cs.OwningThread == (HANDLE)::GetCurrentThreadId(); }
    bool IsLockedByAnyThread() const {TRACE_IT(19941); return (InterlockedExchangeAdd((volatile LONG*)&cs.LockCount, 0L) & 1/*CS_LOCK_BIT*/) == 0; }
#endif
private:
    CRITICAL_SECTION cs;
};

//FakeCriticalSection mimics CriticalSection apis
class FakeCriticalSection
{
public:
    FakeCriticalSection(DWORD spincount = 0) {TRACE_IT(19942); /*do nothing*/spincount++; }
    ~FakeCriticalSection() {TRACE_IT(19943);}
    BOOL TryEnter() {TRACE_IT(19944); return true; }
    void Enter() {TRACE_IT(19945);}
    void Leave() {TRACE_IT(19946);}
#if DBG
    bool IsLocked() const {TRACE_IT(19947); return true; }
#endif
};

class AutoCriticalSection
{
public:
    AutoCriticalSection(CriticalSection * cs) : cs(cs) {TRACE_IT(19948); cs->Enter(); }
    ~AutoCriticalSection() {TRACE_IT(19949); cs->Leave(); }
private:
    CriticalSection * cs;
};

class AutoOptionalCriticalSection
{
public:
    AutoOptionalCriticalSection(CriticalSection * cs) : cs(cs)
    {TRACE_IT(19950);
        if (cs)
        {TRACE_IT(19951);
            cs->Enter();
        }
    }

    ~AutoOptionalCriticalSection()
    {TRACE_IT(19952);
        if (cs)
        {TRACE_IT(19953);
            cs->Leave();
        }
    }

private:
    CriticalSection * cs;
};

template <class SyncObject = FakeCriticalSection >
class AutoRealOrFakeCriticalSection
{
public:
    AutoRealOrFakeCriticalSection(SyncObject * cs) : cs(cs) {TRACE_IT(19954); cs->Enter(); }
    ~AutoRealOrFakeCriticalSection() {TRACE_IT(19955); cs->Leave(); }
private:
    SyncObject * cs;
};

template <class SyncObject = FakeCriticalSection >
class AutoOptionalRealOrFakeCriticalSection
{
public:
    AutoOptionalRealOrFakeCriticalSection(SyncObject * cs) : cs(cs)
    {TRACE_IT(19956);
        if (cs)
        {TRACE_IT(19957);
            cs->Enter();
        }
    }

    ~AutoOptionalRealOrFakeCriticalSection()
    {TRACE_IT(19958);
        if (cs)
        {TRACE_IT(19959);
            cs->Leave();
        }
    }

private:
    SyncObject * cs;
};

