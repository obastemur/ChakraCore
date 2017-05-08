//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef STACK_BACK_TRACE

#ifndef _WIN32
#include <execinfo.h>
#define CaptureStackBackTrace(FramesToSkip, FramesToCapture, buffer, hash) \
    backtrace(buffer, FramesToCapture)
#endif  // !_WIN32

class StackBackTrace
{
public:
    static const ULONG DefaultFramesToCapture = 30;
    static StackBackTrace * Capture(char* buffer, size_t bufSize, ULONG framesToSkip = 0);
    template <typename TAllocator> _NOINLINE
        static StackBackTrace * Capture(TAllocator * alloc, ULONG framesToSkip = 0, ULONG framesToCapture = DefaultFramesToCapture);

    template <typename TAllocator> _NOINLINE
        static StackBackTrace * Create(TAllocator * alloc, ULONG framesToCaptureLater = DefaultFramesToCapture);
    size_t Print();
    template<typename Fn>void Map(Fn fn);   // The Fn is expected to be: void Fn(void*).
    ULONG Capture(ULONG framesToSkip);
    ULONG GetRequestedFrameCount() {TRACE_IT(20517); return this->requestedFramesToCapture; }
    template <typename TAllocator>
    void Delete(TAllocator * alloc);
private:
    // We want to skip at lease the StackBackTrace::Capture and the constructor frames
    static const ULONG BaseFramesToSkip = 2;

    _NOINLINE StackBackTrace(ULONG framesToSkip, ULONG framesToCapture);
    _NOINLINE StackBackTrace(ULONG framesToCapture);

    ULONG requestedFramesToCapture;
    ULONG framesCount;
    PVOID stackBackTrace[];
};

template <typename TAllocator>
StackBackTrace *
StackBackTrace::Capture(TAllocator * alloc, ULONG framesToSkip, ULONG framesToCapture)
{TRACE_IT(20518);
    return AllocatorNewPlusZ(TAllocator, alloc, sizeof(PVOID) * framesToCapture, StackBackTrace, framesToSkip, framesToCapture);
}

template <typename TAllocator>
StackBackTrace* StackBackTrace::Create(TAllocator * alloc, ULONG framesToCaptureLater)
{TRACE_IT(20519);
    return AllocatorNewPlusZ(TAllocator, alloc, sizeof(PVOID)* framesToCaptureLater, StackBackTrace, framesToCaptureLater);
}

template <typename TAllocator>
void StackBackTrace::Delete(TAllocator * alloc)
{
    AllocatorDeletePlus(TAllocator, alloc, sizeof(PVOID)* requestedFramesToCapture, this);
}

template <typename Fn>
void StackBackTrace::Map(Fn fn)
{TRACE_IT(20520);
    for (ULONG i = 0; i < this->framesCount; ++i)
    {TRACE_IT(20521);
        fn(this->stackBackTrace[i]);
    }
}

class StackBackTraceNode
{
public:
    template <typename TAllocator>
    static void Prepend(TAllocator * allocator, StackBackTraceNode *& head, StackBackTrace * stackBackTrace)
    {TRACE_IT(20522);
        head = AllocatorNew(TAllocator, allocator, StackBackTraceNode, stackBackTrace, head);
    }

    template <typename TAllocator>
    static void DeleteAll(TAllocator * allocator, StackBackTraceNode *& head)
    {TRACE_IT(20523);
        StackBackTraceNode * curr = head;
        while (curr != nullptr)
        {TRACE_IT(20524);
            StackBackTraceNode * next = curr->next;
            curr->stackBackTrace->Delete(allocator);
            AllocatorDelete(TAllocator, allocator, curr);
            curr = next;
        }
        head = nullptr;
    }

    static void PrintAll(StackBackTraceNode * head)
    {TRACE_IT(20525);
        // We want to print them tail first because that is the first stack trace we added

        // Reverse the list
        StackBackTraceNode * curr = head;
        StackBackTraceNode * prev = nullptr;
        while (curr != nullptr)
        {TRACE_IT(20526);
            StackBackTraceNode * next = curr->next;
            curr->next = prev;
            prev = curr;
            curr = next;
        }

        // print and reverse again.
        curr = prev;
        prev = nullptr;
        while (curr != nullptr)
        {TRACE_IT(20527);
            curr->stackBackTrace->Print();
            StackBackTraceNode * next = curr->next;
            curr->next = prev;
            prev = curr;
            curr = next;
        }

        Assert(prev == head);
    }
private:
    StackBackTraceNode(StackBackTrace * stackBackTrace, StackBackTraceNode * next) : stackBackTrace(stackBackTrace), next(next) {TRACE_IT(20528);};
    StackBackTrace * stackBackTrace;
    StackBackTraceNode * next;
};


//
// In memory TraceRing
//

uint _trace_ring_next_id();

//
// A buffer of size "T[count]", dynamically allocated (!useStatic) or
// statically embedded (useStatic).
//
template <class T, LONG count, bool useStatic>
struct _TraceRingBuffer
{
    T* buf;
    _TraceRingBuffer() {TRACE_IT(20529); buf = HeapNewArray(T, count); }
    ~_TraceRingBuffer() { HeapDeleteArray(count, buf); }
};
template <class T, LONG count>
struct _TraceRingBuffer<T, count, true>
{
    T buf[count];
};

//
// A trace ring frame, consisting of id, header, and optionally stack
//
template <class Header, uint STACK_FRAMES>
struct _TraceRingFrame
{
    uint id;  // unique id in order
    Header header;
    void* stack[STACK_FRAMES];
};

//
// Trace code execution using an in-memory ring buffer. Capture each trace
// point with a frame, which contains of custom data and optional stack trace.
// Useful for instrumenting source code to track execution.
//
//  Header:     Custom header data type to capture in each frame. Used to
//              capture execution state at a trace point.
//  COUNT:      Number of frames to keep in the ring buffer. When the buffer
//              is filled up, capture will start over from the beginning.
//  STACK_FRAMES:
//              Number of stack frames to capture for each trace frame.
//              This can be 0, only captures header data without stack trace.
//  USE_STATIC_BUFFER:
//              Use embedded buffer instead of dynamically allocate. This may
//              be helpful to avoid static data initialization problem.
//  SKIP_TOP_FRAMES:
//              Top stack frames to skip for each capture (only on _WIN32).
//
//  Usage: Following captures the last 100 stacks that changes
//  scriptContext->debuggerMode:
//          struct DebugModeChange {
//              ScriptContext* scriptContext;
//              DebuggerMode debuggerMode;
//          };
//          static TraceRing<DebugModeChange, 100> s_ev;
//      Call at every debuggerMode change point:
//          DebugModeChange e = { scriptContext, debuggerMode };
//          s_ev.Capture(e);
//
//  Examine trace frame i with its call stack in debugger:
//  gdb:
//      p s_ev.buf[i]
//  windbg:
//      ?? &s_ev.buf[i]
//      dds/dqs [above address]
//
template <class Header, LONG COUNT,
          uint STACK_FRAMES = 30,
          bool USE_STATIC_BUFFER = false,
          uint SKIP_TOP_FRAMES = 1>
class TraceRing:
    protected _TraceRingBuffer<_TraceRingFrame<Header, STACK_FRAMES>, COUNT, USE_STATIC_BUFFER>
{
protected:
    LONG cur;

public:
    TraceRing()
    {TRACE_IT(20530);
        cur = (uint)-1;
    }

    template <class HeaderFunc>
    void Capture(const HeaderFunc& writeHeader)
    {TRACE_IT(20531);
        LONG i = InterlockedIncrement(&cur);
        if (i >= COUNT)
        {
            InterlockedCompareExchange(&cur, i % COUNT, i);
            i %= COUNT;
        }

        auto* frame = &this->buf[i];
        *frame = {};
        frame->id = _trace_ring_next_id();
        writeHeader(&frame->header);
        if (STACK_FRAMES > 0)
        {
            CaptureStackBackTrace(SKIP_TOP_FRAMES, STACK_FRAMES, frame->stack, nullptr);
        }
    }

    void Capture(const Header& header)
    {TRACE_IT(20532);
        Capture([&](Header* h)
        {
            *h = header;
        });
    }

    // Capture a trace (no header data, stack only)
    void Capture()
    {TRACE_IT(20533);
        Capture([&](Header* h) { });
    }
};

#endif  // STACK_BACK_TRACE
