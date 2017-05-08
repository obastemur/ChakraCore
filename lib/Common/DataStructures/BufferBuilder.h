//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Buffer builder is used to layout out binary content which contains offsets
// from one part of the content to another.
// It works in two-pass fashion:
// - Pass one fixes the real offset of each element (BufferBuilder) of the
//   content.
// - Pass two writes the actual content including any relative offset values.
//----------------------------------------------------------------------------

#pragma once

#if _M_IX86
#define serialization_alignment
#elif _M_X64 || defined(_M_ARM32_OR_ARM64)
#define serialization_alignment __unaligned
#else
#error Must define alignment capabilities for processor
#endif

struct _SIMDValue;
typedef _SIMDValue SIMDValue;

namespace Js
{
    // The base buffer builder class
    class BufferBuilder
    {
    protected:
        LPCWSTR clue;
        BufferBuilder(LPCWSTR clue)
            : clue(clue), offset(0xffffffff) {TRACE_IT(20872); }
    public:
        uint32 offset;
        virtual uint32 FixOffset(uint32 offset) = 0;
        virtual void Write(__in_bcount(bufferSize) byte * buffer, __in uint32 bufferSize) const = 0;
#if DBG
    protected:
        void TraceOutput(byte * buffer, uint32 size) const;
#endif
    };

#if VARIABLE_INT_ENCODING
#define VARIABLE_INT_TAGBIT_COUNT (1)
#define VARIABLE_INT_BYTE_SHIFT (8 - VARIABLE_INT_TAGBIT_COUNT)
#define VARIABLE_INT_BYTE_MAX (1 << VARIABLE_INT_BYTE_SHIFT)
#define VARIABLE_INT_BYTE_MASK ~((byte) VARIABLE_INT_BYTE_MAX)
#define SENTINEL_BYTE_COUNT 1

#define ONE_BYTE_MAX ((byte) 0xfd)
#define TWO_BYTE_MAX ((uint16) 0xffff)

#define TWO_BYTE_SENTINEL   ONE_BYTE_MAX + 1
#define FOUR_BYTE_SENTINEL  ONE_BYTE_MAX + 2

#define MIN_SENTINEL TWO_BYTE_SENTINEL
#endif

#if INSTRUMENT_BUFFER_INTS
    static uint Counts[] = { 0, 0, 0, 0 };
#endif

    // Templatized buffer builder for writing fixed-size content
    template<typename T, bool useVariableIntEncoding>
    struct BufferBuilderOf : BufferBuilder
    {
        typedef serialization_alignment T value_type;
        value_type value;

        BufferBuilderOf(LPCWSTR clue, const T & value)
            : BufferBuilder(clue), value(value)
        {TRACE_IT(20873); }

        //  Assume that the value is 0- for negative values of value, we'll just use the default encoding
        bool UseOneByte() const
        {TRACE_IT(20874);
            return value >= 0 && value <= ONE_BYTE_MAX;
        }

        bool UseTwoBytes() const
        {TRACE_IT(20875);
            return value > ONE_BYTE_MAX && value <= TWO_BYTE_MAX;
        }

        uint32 FixOffset(uint32 offset) override
        {
            this->offset = offset;

            if (useVariableIntEncoding)
            {TRACE_IT(20876);
                if (UseOneByte())
                {TRACE_IT(20877);
                    return this->offset + sizeof(serialization_alignment byte);
                }
                else if (UseTwoBytes())
                {TRACE_IT(20878);
                    return this->offset + sizeof(serialization_alignment uint16) + SENTINEL_BYTE_COUNT;
                }

                return this->offset + sizeof(serialization_alignment T) + SENTINEL_BYTE_COUNT;
            }
            else
            {TRACE_IT(20879);
                return this->offset + sizeof(serialization_alignment T);
            }
        }

        void Write(__in_bcount(bufferSize) byte * buffer, __in uint32 bufferSize) const
        {TRACE_IT(20880);
            DebugOnly(uint32 size = sizeof(T));

#if INSTRUMENT_BUFFER_INTS
            if (value < ((1 << 8)))
            {TRACE_IT(20881);
                Counts[0]++;
            }
            else if (value < ((1 << 16)))
            {TRACE_IT(20882);
                Counts[1]++;
            }
            else if (value < ((1 << 24)))
            {TRACE_IT(20883);
                Counts[2]++;
            }
            else
            {TRACE_IT(20884);
                Counts[3]++;
            }
#endif

            if (useVariableIntEncoding)
            {TRACE_IT(20885);
                if (UseOneByte())
                {TRACE_IT(20886);
                    if (bufferSize - this->offset<sizeof(serialization_alignment byte))
                    {TRACE_IT(20887);
                        Throw::FatalInternalError();
                    }
                    DebugOnly(size = sizeof(byte));
                    *(serialization_alignment byte*)(buffer + this->offset) = (byte) value;
                }
                else if (UseTwoBytes())
                {TRACE_IT(20888);
                    if (bufferSize - this->offset<sizeof(serialization_alignment uint16))
                    {TRACE_IT(20889);
                        Throw::FatalInternalError();
                    }
                    DebugOnly(size = sizeof(uint16) + 1);
                    *(serialization_alignment byte*)(buffer + this->offset) = TWO_BYTE_SENTINEL;
                    *(serialization_alignment uint16*)(buffer + this->offset + SENTINEL_BYTE_COUNT) = (uint16) this->value;
                }
                else
                {TRACE_IT(20890);
                    if (bufferSize - this->offset<sizeof(serialization_alignment T))
                    {TRACE_IT(20891);
                        Throw::FatalInternalError();
                    }
                    *(serialization_alignment byte*)(buffer + this->offset) = FOUR_BYTE_SENTINEL;
                    *(serialization_alignment T*)(buffer + this->offset + SENTINEL_BYTE_COUNT) = this->value;
                    DebugOnly(size = sizeof(T) + 1);
#if INSTRUMENT_BUFFER_INTS
                    printf("[BCGENSTATS] %d, %d\n", value, sizeof(T));
#endif
                }
            }
            else
            {TRACE_IT(20892);
                if (bufferSize - this->offset<sizeof(serialization_alignment T))
                {TRACE_IT(20893);
                    Throw::FatalInternalError();
                }
                *(serialization_alignment T*)(buffer + this->offset) = value;
            }
            DebugOnly(TraceOutput(buffer, size));
        }
    };

    template<typename T>
    struct BufferBuilderOf<T, false>: BufferBuilder
    {
        typedef serialization_alignment T value_type;
        value_type value;

        BufferBuilderOf(LPCWSTR clue, const T & value)
            : BufferBuilder(clue), value(value)
        {TRACE_IT(20894); }

        uint32 FixOffset(uint32 offset) override
        {
            this->offset = offset;

            return this->offset + sizeof(serialization_alignment T);
        }

        void Write(__in_bcount(bufferSize) byte * buffer, __in uint32 bufferSize) const
        {TRACE_IT(20895);
            if (bufferSize - this->offset<sizeof(serialization_alignment T))
            {TRACE_IT(20896);
                Throw::FatalInternalError();
            }

            *(serialization_alignment T*)(buffer + this->offset) = value;
            DebugOnly(TraceOutput(buffer, sizeof(T)));
        }
    };


    template <typename T>
    struct ConstantSizedBufferBuilderOf : BufferBuilderOf<T, false>
    {
        ConstantSizedBufferBuilderOf(LPCWSTR clue, const T & value)
        : BufferBuilderOf<T, false>(clue, value)
        {TRACE_IT(20897); }
    };

#if VARIABLE_INT_ENCODING
    typedef BufferBuilderOf<int16, true> BufferBuilderInt16;
    typedef BufferBuilderOf<int, true> BufferBuilderInt32;
    typedef ConstantSizedBufferBuilderOf<byte> BufferBuilderByte;
    typedef ConstantSizedBufferBuilderOf<float> BufferBuilderFloat;
    typedef ConstantSizedBufferBuilderOf<double> BufferBuilderDouble;
    typedef ConstantSizedBufferBuilderOf<SIMDValue> BufferBuilderSIMD;
#else
    typedef ConstantSizedBufferBuilderOf<int16> BufferBuilderInt16;
    typedef ConstantSizedBufferBuilderOf<int> BufferBuilderInt32;
    typedef ConstantSizedBufferBuilderOf<byte> BufferBuilderByte;
    typedef ConstantSizedBufferBuilderOf<float> BufferBuilderFloat;
    typedef ConstantSizedBufferBuilderOf<double> BufferBuilderDouble;
    typedef ConstantSizedBufferBuilderOf<SIMDValue> BufferBuilderSIMD;
#endif

    // A buffer builder which contains a list of buffer builders
    struct BufferBuilderList : BufferBuilder
    {
        regex::ImmutableList<BufferBuilder*> * list;
        BufferBuilderList(LPCWSTR clue)
            : BufferBuilder(clue), list(nullptr)
        {TRACE_IT(20898); }

        uint32 FixOffset(uint32 offset) override
        {
            this->offset = offset;
            return list->Accumulate(offset,[](uint32 size, BufferBuilder * builder)->uint32 {
                return builder->FixOffset(size);
            });
        }

        void Write(__in_bcount(bufferSize) byte * buffer, __in uint32 bufferSize) const
        {TRACE_IT(20899);
            return list->Iterate([&](BufferBuilder * builder) {
                builder->Write(buffer, bufferSize);
            });
        }

    };

    // A buffer builder which points to another buffer builder.
    // At write time, it will write the offset from the start of the raw buffer to
    // the pointed-to location.
    struct BufferBuilderRelativeOffset : BufferBuilder
    {
        BufferBuilder * pointsTo;
        uint32 additionalOffset;
        BufferBuilderRelativeOffset(LPCWSTR clue, BufferBuilder * pointsTo, uint32 additionalOffset)
            : BufferBuilder(clue), pointsTo(pointsTo), additionalOffset(additionalOffset)
        {TRACE_IT(20900); }
        BufferBuilderRelativeOffset(LPCWSTR clue, BufferBuilder * pointsTo)
            : BufferBuilder(clue), pointsTo(pointsTo), additionalOffset(0)
        {TRACE_IT(20901); }

        uint32 FixOffset(uint32 offset) override
        {
            this->offset = offset;
            return this->offset + sizeof(int);
        }

        void Write(__in_bcount(bufferSize) byte * buffer, __in uint32 bufferSize) const
        {TRACE_IT(20902);
            if (bufferSize - this->offset<sizeof(int))
            {TRACE_IT(20903);
                Throw::FatalInternalError();
            }

            int offsetOfPointedTo = pointsTo->offset;
            *(int*)(buffer + this->offset) = offsetOfPointedTo + additionalOffset;
            DebugOnly(TraceOutput(buffer, sizeof(int)));
        }
    };

    // A buffer builder which holds a raw byte buffer
    struct BufferBuilderRaw : BufferBuilder
    {
        uint32 size;
        const byte * raw;
        BufferBuilderRaw(LPCWSTR clue, __in uint32 size, __in_bcount(size) const byte * raw)
            : BufferBuilder(clue), size(size), raw(raw)
        {TRACE_IT(20904); }

        uint32 FixOffset(uint32 offset) override
        {
            this->offset = offset;
            return this->offset + size;
        }

        void Write(__in_bcount(bufferSize) byte * buffer, __in uint32 bufferSize) const
        {TRACE_IT(20905);
            if (bufferSize - this->offset<size)
            {TRACE_IT(20906);
                Throw::FatalInternalError();
            }

            js_memcpy_s(buffer + this->offset, bufferSize-this->offset, raw, size);
            DebugOnly(TraceOutput(buffer, size));
        }
    };

    // A buffer builder which aligns its contents to the specified alignment
    struct BufferBuilderAligned : BufferBuilder
    {
        BufferBuilder * content;
        uint32 alignment;
        uint32 padding;

        BufferBuilderAligned(LPCWSTR clue, BufferBuilder * content, uint32 alignment)
            : BufferBuilder(clue), content(content), alignment(alignment), padding(0)
        {TRACE_IT(20907); }

        uint32 FixOffset(uint32 offset) override
        {
            this->offset = offset;

            // Calculate padding
            offset = ::Math::Align(this->offset, this->alignment);
            this->padding = offset - this->offset;
            Assert(this->padding < this->alignment);

            return content->FixOffset(offset);
        }

        void Write(__in_bcount(bufferSize) byte * buffer, __in uint32 bufferSize) const
        {TRACE_IT(20908);
            if (bufferSize - this->offset < this->padding)
            {TRACE_IT(20909);
                Throw::FatalInternalError();
            }

            for (uint32 i = 0; i < this->padding; i++)
            {TRACE_IT(20910);
                buffer[this->offset + i] = 0;
            }

            this->content->Write(buffer, bufferSize);
        }
    };

}
