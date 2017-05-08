//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

typedef BYTE ubyte;
typedef uint16 uhalf;
typedef uint32 uword;
CompileAssert(sizeof(ubyte) == 1);
CompileAssert(sizeof(uhalf) == 2);
CompileAssert(sizeof(uword) == 4);

BYTE* EmitLEB128(BYTE* pc, unsigned value);
BYTE* EmitLEB128(BYTE* pc, int value);
ubyte GetDwarfRegNum(ubyte regNum);

template <class T>
class LEB128Wrapper
{
private:
    T value;

public:
    LEB128Wrapper(T value): value(value)
    {TRACE_IT(1681);}

    BYTE* Write(BYTE* pc) const
    {TRACE_IT(1682);
        return EmitLEB128(pc, value);
    }
};

typedef LEB128Wrapper<unsigned> ULEB128;
typedef LEB128Wrapper<int> LEB128;

//
// EhFrame emits .eh_frame unwind data for our JIT code. We emit only one CIE
// followed by one FDE for each JIT function.
//
class EhFrame
{
    // Simple buffer writer. Must operate on a buffer of sufficient size.
    class Writer
    {
    private:
        BYTE* buffer;   // original buffer head
        BYTE* cur;      // current output position
        const size_t size;  // original size of buffer, for debug only

    public:
        Writer(BYTE* buffer, size_t size) : buffer(buffer), cur(buffer), size(size)
        {TRACE_IT(1683);}

        // Write a value, and advance cur position
        template <class T>
        void Write(T value)
        {TRACE_IT(1684);
            *reinterpret_cast<T*>(cur) = value;
            cur += sizeof(value);
            Assert(Count() <= size);
        }

        // Write a ULEB128 or LEB128 value, and advance cur position
        template <class T>
        void Write(const LEB128Wrapper<T>& leb128)
        {TRACE_IT(1685);
            cur = leb128.Write(cur);
            Assert(Count() <= size);
        }

        // Write a value at an absolute position
        template <class T>
        void Write(size_t offset, T value)
        {TRACE_IT(1686);
            Assert(offset + sizeof(value) <= size);
            *reinterpret_cast<T*>(buffer + offset) = value;
        }

        // Get original buffer head
        BYTE* Buffer() const
        {TRACE_IT(1687);
            return buffer;
        }

        // Get count of written bytes (== offset of cur position)
        size_t Count() const
        {TRACE_IT(1688);
            return cur - buffer;
        }
    };

    // Base class for CIE and FDE
    class Entry
    {
    protected:
        Writer* writer;
        size_t  beginOffset;    // where we'll update "length" record

        // To limit supported value types
        void Emit(ubyte value) {TRACE_IT(1689); writer->Write(value); }
        void Emit(uhalf value) {TRACE_IT(1690); writer->Write(value); }
        void Emit(uword value) {TRACE_IT(1691); writer->Write(value); }
        void Emit(const void* absptr) {TRACE_IT(1692); writer->Write(absptr); }
        void Emit(LEB128 value) {TRACE_IT(1693); writer->Write(value); }
        void Emit(ULEB128 value) {TRACE_IT(1694); writer->Write(value); }

        template <class T1>
        void Emit(ubyte op, T1 arg1)
        {TRACE_IT(1695);
            Emit(op);
            Emit(arg1);
        }

        template <class T1, class T2>
        void Emit(ubyte op, T1 arg1, T2 arg2)
        {
            Emit(op, arg1);
            Emit(arg2);
        }

    public:
        Entry(Writer* writer) : writer(writer), beginOffset(-1)
        {TRACE_IT(1696);}

        void Begin();
        void End();

#define ENTRY(name, op) \
    void cfi_##name() \
    {TRACE_IT(1697); Emit(static_cast<ubyte>(op)); }

#define ENTRY1(name, op, arg1_type) \
    void cfi_##name(arg1_type arg1) \
    { Emit(op, arg1); }

#define ENTRY2(name, op, arg1_type, arg2_type) \
    void cfi_##name(arg1_type arg1, arg2_type arg2) \
    { Emit(op, arg1, arg2); }

#define ENTRY_SM1(name, op, arg1_type) \
    void cfi_##name(arg1_type arg1) \
    {TRACE_IT(1698); Assert((arg1) <= 0x3F); Emit(static_cast<ubyte>((op) | arg1)); }

#define ENTRY_SM2(name, op, arg1_type, arg2_type) \
    void cfi_##name(arg1_type arg1, arg2_type arg2) \
    {TRACE_IT(1699); Assert((arg1) <= 0x3F); Emit((op) | arg1, arg2); }

#include "EhFrameCFI.inc"

        void cfi_advance(uword advance);
    };

    // Common Information Entry
    class CIE : public Entry
    {
    public:
        CIE(Writer* writer) : Entry(writer)
        {TRACE_IT(1700);}

        void Begin();
    };

    // Frame Description Entry
    class FDE: public Entry
    {
    private:
        size_t pcBeginOffset;

    public:
        FDE(Writer* writer) : Entry(writer)
        {TRACE_IT(1701);}

        void Begin();
        void UpdateAddressRange(const void* pcBegin, size_t pcRange);
    };

private:
    Writer writer;
    FDE fde;

public:
    EhFrame(BYTE* buffer, size_t size);

    Writer* GetWriter()
    {TRACE_IT(1702);
        return &writer;
    }

    FDE* GetFDE()
    {TRACE_IT(1703);
        return &fde;
    }

    void End();

    BYTE* Buffer() const
    {TRACE_IT(1704);
        return writer.Buffer();
    }

    size_t Count() const
    {TRACE_IT(1705);
        return writer.Count();
    }
};
