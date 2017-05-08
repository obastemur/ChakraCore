//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename TAllocator>
    class StringBuilder
    {
    private:
        struct Data
        {
        public:
            union {
                struct st_Single
                {
                    char16 buffer[];
                } single;

                struct st_Chained
                {
                    charcount_t length;
                    Data *next;
                    char16 buffer[];
                } chained;
            }u;
        };

    private:
        static const charcount_t MaxLength = INT_MAX - 1;

        const static charcount_t MaxRealloc = 64;
        TAllocator* alloc;
        // First chunk is just a buffer, and which can be detached without copying.
        Data *firstChunk;
        // Second chunk is a chained list of chunks.  UnChain() needs to be called to copy the first chunk
        // and the list of chained chunks to a single buffer on calls to GetBuffer().
        Data *secondChunk;
        Data *lastChunk;
        char16 * appendPtr;
        charcount_t length; // Total capacity (allocated number of elements - 1), in all chunks. Note that we keep one allocated element which is not accounted in length for terminating '\0'.
        charcount_t count;  // Total number of elements, in all chunks.
        charcount_t firstChunkLength;
        charcount_t initialSize;

        bool IsChained() {TRACE_IT(22383); return this->secondChunk != NULL; }

        Data *NewChainedChunk(charcount_t bufLengthRequested)
        {TRACE_IT(22384);
            CompileAssert(sizeof(charcount_t) == sizeof(uint32));

            // allocation = (bufLengthRequested * sizeof(char16) + sizeof(Data)
            charcount_t alloc32 = UInt32Math::MulAdd<sizeof(char16), sizeof(Data)>(bufLengthRequested);
            size_t allocation = TAllocator::GetAlignedSize(alloc32);
            size_t size_t_length = (allocation - sizeof(Data)) / sizeof(char16);
            charcount_t bufLength = (charcount_t)size_t_length;
            Assert(bufLength == size_t_length);

            Data *newChunk = AllocatorNewStructPlus(TAllocator, this->alloc, allocation, Data);

            newChunk->u.chained.length = bufLength;
            newChunk->u.chained.next = NULL;

            // Recycler gives zeroed memory, so rely on that instead of memsetting the tail
#if 0
            // Align memset to machine register size for perf
            bufLengthRequested &= ~(sizeof(size_t) - 1);
            memset(newChunk->u.chained.buffer + bufLengthRequested, 0, (bufLength - bufLengthRequested) * sizeof(char16));
#endif
            return newChunk;
        }

        Data *NewSingleChunk(charcount_t *pBufLengthRequested)
        {TRACE_IT(22385);
            Assert(*pBufLengthRequested <= MaxLength);

            // Let's just grow the current chunk in place

            CompileAssert(sizeof(charcount_t) == sizeof(uint32));

            //// allocation = (bufLengthRequested+1) * sizeof(char16)
            charcount_t alloc32 = UInt32Math::AddMul< 1, sizeof(char16) >(*pBufLengthRequested);

            size_t allocation = HeapInfo::GetAlignedSize(alloc32);
            size_t size_t_newLength  = allocation / sizeof(char16) - 1;
            charcount_t newLength = (charcount_t)size_t_newLength;
            Assert(newLength == size_t_newLength);
            Assert(newLength <= MaxLength + 1);
            if (newLength == MaxLength + 1)
            {TRACE_IT(22386);
                // newLength could be MaxLength + 1 because of alignment.
                // In this case alloc size is 2 elements more than newLength (normally 1 elements more for NULL), that's fine.
                newLength = MaxLength;
            }
            Assert(newLength <= MaxLength);

            Data* newChunk = AllocatorNewStructPlus(TAllocator, this->alloc, allocation, Data);
            newChunk->u.single.buffer[newLength] = _u('\0');

            *pBufLengthRequested = newLength;
            return newChunk;
        }

        _NOINLINE void ExtendBuffer(charcount_t newLength)
        {TRACE_IT(22387);
            Data *newChunk;

            // To maintain this->length under MaxLength, check it here/throw, this is the only place we grow the buffer.
            if (newLength > MaxLength)
            {TRACE_IT(22388);
                Throw::OutOfMemory();
            }

            Assert(this->length <= MaxLength);

            charcount_t newLengthTryGrowPolicy = newLength + (this->length*2/3); // Note: this would never result in uint32 overflow.
            if (newLengthTryGrowPolicy <= MaxLength)
            {TRACE_IT(22389);
                newLength = newLengthTryGrowPolicy;
            }
            Assert(newLength <= MaxLength);

            // We already have linked chunks
            if (this->IsChained() || (this->firstChunk != NULL && newLength - this->length > MaxRealloc))
            {TRACE_IT(22390);
                newChunk = this->NewChainedChunk(newLength - this->count);

                if (this->IsChained())
                {TRACE_IT(22391);
                    this->lastChunk->u.chained.next = newChunk;

                    // We're not going to use the extra space in the current chunk...
                    Assert(this->lastChunk->u.chained.length > this->length - this->count);
                    this->lastChunk->u.chained.length -= (this->length - this->count);
                }
                else
                {TRACE_IT(22392);
                    // Time to add our first linked chunk
                    Assert(this->secondChunk == NULL);
                    this->secondChunk = newChunk;

                    // We're not going to use the extra space in the current chunk...
                    this->firstChunkLength = this->count;
                }

                this->length = this->count + newChunk->u.chained.length;
                this->lastChunk = newChunk;
                this->appendPtr = newChunk->u.chained.buffer;
            }
            else
            {TRACE_IT(22393);
                if (this->initialSize < MaxLength)
                {TRACE_IT(22394);
                    newLength = max(newLength, this->initialSize + 1);
                }
                else
                {TRACE_IT(22395);
                    newLength = MaxLength;
                }
                Assert(newLength <= MaxLength);

                // Let's just grow the current chunk in place
                newChunk = this->NewSingleChunk(&newLength);

                if (this->count)
                {TRACE_IT(22396);
                    js_memcpy_s(newChunk->u.single.buffer, newLength * sizeof(char16), this->firstChunk->u.single.buffer, sizeof(char16) * this->count);
                }

                this->firstChunk = this->lastChunk = newChunk;
                this->firstChunkLength = newLength;
                this->length = newLength;
                this->appendPtr = newChunk->u.single.buffer + this->count;
            }
        }

        void EnsureBuffer(charcount_t countNeeded)
        {TRACE_IT(22397);
            if(countNeeded == 0) return;

            if (countNeeded >= this->length - this->count)
            {TRACE_IT(22398);
                if (countNeeded > MaxLength)
                {TRACE_IT(22399);
                    // Check upfront to prevent potential uint32 overflow caused by (this->count + countNeeded + 1).
                    Throw::OutOfMemory();
                }
                ExtendBuffer(this->count + countNeeded + 1);
            }
        }

    public:

        static StringBuilder<TAllocator> *
            New(TAllocator* alloc, charcount_t initialSize)
        {TRACE_IT(22400);
            if (initialSize > MaxLength)
            {TRACE_IT(22401);
                Throw::OutOfMemory();
            }
            return AllocatorNew(TAllocator, alloc, StringBuilder<TAllocator>, alloc, initialSize);
        }

        StringBuilder(TAllocator* alloc)
        {TRACE_IT(22402);
            new (this) StringBuilder(alloc, 0);
        }

        StringBuilder(TAllocator* alloc, charcount_t initialSize) : alloc(alloc), length(0), count(0), firstChunk(NULL),
            secondChunk(NULL), appendPtr(NULL), initialSize(initialSize)
        {TRACE_IT(22403);
            if (initialSize > MaxLength)
            {TRACE_IT(22404);
                Throw::OutOfMemory();
            }
        }

        void UnChain(__out __ecount(bufLen) char16 *pBuf, charcount_t bufLen)
        {TRACE_IT(22405);
            charcount_t lastChunkCount = this->count;

            Assert(this->IsChained());

            Assert(bufLen >= this->count);
            char16 *pSrcBuf = this->firstChunk->u.single.buffer;
            Data *next = this->secondChunk;
            charcount_t srcLength = this->firstChunkLength;

            for (Data *chunk = this->firstChunk; chunk != this->lastChunk; next = chunk->u.chained.next)
            {TRACE_IT(22406);
                if (bufLen < srcLength)
                {TRACE_IT(22407);
                    Throw::FatalInternalError();
                }
                js_memcpy_s(pBuf, bufLen * sizeof(char16), pSrcBuf, sizeof(char16) * srcLength);
                bufLen -= srcLength;
                pBuf += srcLength;
                lastChunkCount -= srcLength;

                chunk = next;
                pSrcBuf = chunk->u.chained.buffer;
                srcLength = chunk->u.chained.length;
            }

            if (bufLen < lastChunkCount)
            {TRACE_IT(22408);
                Throw::FatalInternalError();
            }
            js_memcpy_s(pBuf, bufLen * sizeof(char16), this->lastChunk->u.chained.buffer, sizeof(char16) * lastChunkCount);
        }

        void UnChain()
        {TRACE_IT(22409);
            Assert(this->IsChained());

            charcount_t newLength = this->count;

            Data *newChunk = this->NewSingleChunk(&newLength);

            this->length = newLength;

            this->UnChain(newChunk->u.single.buffer, newLength);

            this->firstChunk = this->lastChunk = newChunk;
            this->secondChunk = NULL;
            this->appendPtr = newChunk->u.single.buffer + this->count;
        }

        void Copy(__out __ecount(bufLen) char16 *pBuf, charcount_t bufLen)
        {TRACE_IT(22410);
            if (this->IsChained())
            {TRACE_IT(22411);
                this->UnChain(pBuf, bufLen);
            }
            else
            {TRACE_IT(22412);
                if (bufLen < this->count)
                {TRACE_IT(22413);
                    Throw::FatalInternalError();
                }
                js_memcpy_s(pBuf, bufLen * sizeof(char16), this->firstChunk->u.single.buffer, this->count * sizeof(char16));
            }
        }

        inline char16* Buffer()
        {TRACE_IT(22414);
            if (this->IsChained())
            {TRACE_IT(22415);
                this->UnChain();
            }

            if (this->firstChunk)
            {TRACE_IT(22416);
                this->firstChunk->u.single.buffer[this->count] = _u('\0');

                return this->firstChunk->u.single.buffer;
            }
            else
            {TRACE_IT(22417);
                return _u("");
            }
        }

        inline charcount_t Count() {TRACE_IT(22418); return this->count; }

        void Append(char16 c)
        {TRACE_IT(22419);
            if (this->count == this->length)
            {TRACE_IT(22420);
                ExtendBuffer(this->length+1);
            }
            *(this->appendPtr++) = c;
            this->count++;
        }

        void AppendSz(const char16 * str)
        {TRACE_IT(22421);
            // WARNING!!
            // Do not use this to append JavascriptStrings.  They can have embedded
            // nulls which obviously won't be handled correctly here.  Instead use
            // Append with a length, which will use memcpy and correctly include any
            // embedded null characters.
            // WARNING!!

            while (*str != _u('\0'))
            {TRACE_IT(22422);
                Append(*str++);
            }
        }

        void Append(const char16 * str, charcount_t countNeeded)
        {TRACE_IT(22423);
            EnsureBuffer(countNeeded);

            char16 *dst = this->appendPtr;

            JavascriptString::CopyHelper(dst, str, countNeeded);

            this->appendPtr += countNeeded;
            this->count += countNeeded;
        }

        template <size_t N>
        void AppendCppLiteral(const char16(&str)[N])
        {TRACE_IT(22424);
            // Need to account for the terminating null character in C++ string literals, hence N > 2 and N - 1 below
            static_assert(N > 2, "Use Append(char16) for appending literal single characters and do not append empty string literal");
            Append(str, N - 1);
        }

        // If we expect str to be large - we should just use this version that uses memcpy directly instead of Append
        void AppendLarge(const char16 * str, charcount_t countNeeded)
        {TRACE_IT(22425);
            EnsureBuffer(countNeeded);

            char16 *dst = this->appendPtr;

            js_memcpy_s(dst, sizeof(WCHAR) * countNeeded, str, sizeof(WCHAR) * countNeeded);

            this->appendPtr += countNeeded;
            this->count += countNeeded;
        }


        errno_t AppendUint64(unsigned __int64 value)
        {TRACE_IT(22426);
            const int max_length = 20; // maximum length of 64-bit value converted to base 10 string
            const int radix = 10;
            WCHAR buf[max_length+1];
            errno_t result = _ui64tow_s(value, buf, max_length+1, radix);
            AssertMsg(result==0, "Failed to translate value to string");
            if (result == 0)
            {TRACE_IT(22427);
                AppendSz(buf);
            }
            return result;
        }

        char16 *AllocBufferSpace(charcount_t countNeeded)
        {TRACE_IT(22428);
            EnsureBuffer(countNeeded);

            return this->appendPtr;
        }

        void IncreaseCount(charcount_t countInc)
        {TRACE_IT(22429);
            if(countInc == 0) return;

            this->count += countInc;
            this->appendPtr += countInc;
            Assert(this->count < this->length);
        }

        char16* Detach()
        {TRACE_IT(22430);
            // NULL terminate the string
            Append(_u('\0'));

            // if there is a chain we need to account for that also, so that the new buffer will have the NULL at the end.
            if (this->IsChained())
            {TRACE_IT(22431);
                this->UnChain();
            }
            // Now decrement the count to adjust according to number of chars
            this->count--;

            char16* result = this->firstChunk->u.single.buffer;
            this->firstChunk = this->lastChunk = NULL;
            return result;
        }

        void TrimTrailingNULL()
        {TRACE_IT(22432);
            Assert(this->count);
            if (this->IsChained())
            {TRACE_IT(22433);
                Assert(this->lastChunk->u.chained.buffer[this->count - (this->length - this->lastChunk->u.chained.length) - 1] == _u('\0'));
            }
            else
            {TRACE_IT(22434);
                Assert(this->lastChunk->u.single.buffer[this->count - 1] == _u('\0'));
            }
            this->appendPtr--;
            this->count--;
        }

        void Reset()
        {TRACE_IT(22435);
            this->length = 0;
            this->count = 0;
            this->firstChunk = NULL;
            this->secondChunk = NULL;
            this->lastChunk = NULL;
        }
    };
}
