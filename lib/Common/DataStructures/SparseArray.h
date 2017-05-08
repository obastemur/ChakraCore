//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template<class T>
class SAChunk
{
public:
    SAChunk<T> *        next;
    uint32              startIndex;
    T *                 data[];
};


template<class T>
class SparseArray
{
private:
    ArenaAllocator *    alloc;
    uint32              chunkSize;
    SAChunk<T> *        firstChunk;

public:
    static SparseArray<T> * New(ArenaAllocator *allocator, uint32 chunkSize)
    {TRACE_IT(22193);
        SparseArray<T> * array;

        if (!Math::IsPow2(chunkSize))
        {TRACE_IT(22194);
            chunkSize = Math::NextPowerOf2(chunkSize);
        }

        // Throw early if this overflows, since chunkSize never changes, subsequent operations will be safe
        UInt32Math::MulAdd<sizeof(T*), sizeof(SAChunk<T>)>(chunkSize);
        array = Anew(allocator, SparseArray<T>);
        array->alloc = allocator;
        array->chunkSize = chunkSize;
        array->firstChunk = NULL;

        return array;
    }

    void Set(uint32 index, T *element)
    {TRACE_IT(22195);
        SAChunk<T> * chunk, **pPrev = &(this->firstChunk);
        uint32 indexInChunk = (index % this->chunkSize);

        for (chunk = this->firstChunk; chunk; chunk = chunk->next)
        {TRACE_IT(22196);
            if (index < chunk->startIndex)
            {TRACE_IT(22197);
                // Need a new chunk...
                chunk = NULL;
                break;
            }
            if (index < chunk->startIndex + this->chunkSize)
            {TRACE_IT(22198);
                break;
            }
            pPrev = &(chunk->next);
        }

        if (chunk == NULL)
        {TRACE_IT(22199);
            chunk = (SAChunk<T> *)this->alloc->AllocZero(sizeof(SAChunk<T>) + (chunkSize * sizeof(T *)));
            chunk->startIndex = index - indexInChunk;
            // Since startIndex and chunkSize don't change, check now if this overflows.
            // Cache the result or save memory ?
            UInt32Math::Add(chunk->startIndex, chunkSize);
            chunk->next = *pPrev;
            *pPrev = chunk;
        }
        chunk->data[indexInChunk] = element;
    }

    T * Get(uint32 index)
    {TRACE_IT(22200);
        SAChunk<T> * chunk;
        uint32 indexInChunk = (index % this->chunkSize);

        for (chunk = this->firstChunk; chunk; chunk = chunk->next)
        {TRACE_IT(22201);
            if (index < chunk->startIndex)
            {TRACE_IT(22202);
                return NULL;
            }
            if (index < chunk->startIndex + this->chunkSize)
            {TRACE_IT(22203);
                return chunk->data[indexInChunk];
            }
        }

        return NULL;
    }

    SparseArray<T> * Copy()
    {TRACE_IT(22204);
        SparseArray<T> * newSA = SparseArray<T>::New(this->alloc, this->chunkSize);
        SAChunk<T> * chunk, *pred = NULL;

        for (chunk = this->firstChunk; chunk; chunk = chunk->next)
        {TRACE_IT(22205);
            SAChunk<T> *newChunk = (SAChunk<T> *)this->alloc->Alloc(sizeof(SAChunk<T>) + (sizeof(T *) * this->chunkSize));

            newChunk->startIndex = chunk->startIndex;
            js_memcpy_s(newChunk->data, sizeof(T *) * this->chunkSize, chunk->data, sizeof(T *) * this->chunkSize);
            if (pred)
            {TRACE_IT(22206);
                pred->next = newChunk;
            }
            else
            {TRACE_IT(22207);
                newSA->firstChunk = newChunk;
            }
            pred = newChunk;
        }

        if (pred)
        {TRACE_IT(22208);
            pred->next = NULL;
        }
        else
        {TRACE_IT(22209);
            newSA->firstChunk = NULL;
        }
        return newSA;
    }

    void And(SparseArray<T> *this2)
    {TRACE_IT(22210);
        SAChunk<T> * chunk, *pred = NULL;
        SAChunk<T> * chunk2;

        AssertMsg(this->chunkSize == this2->chunkSize, "Anding incompatible arrays");
        chunk2 = this2->firstChunk;
        for (chunk = this->firstChunk; chunk; chunk = chunk->next)
        {TRACE_IT(22211);
            while (chunk2 && chunk->startIndex > chunk2->startIndex)
            {TRACE_IT(22212);
                chunk2 = chunk2->next;
            }

            if (chunk2 == NULL || chunk->startIndex < chunk2->startIndex)
            {TRACE_IT(22213);
                if (pred)
                {TRACE_IT(22214);
                    pred->next = chunk->next;
                }
                else
                {TRACE_IT(22215);
                    this->firstChunk = chunk->next;
                }
                continue;
            }
            AssertMsg(chunk->startIndex == chunk2->startIndex, "Huh??");

            for (int i = 0; i < this->chunkSize; i++)
            {TRACE_IT(22216);
                if (chunk->data[i])
                {TRACE_IT(22217);
                    if (chunk2->data[i])
                    {TRACE_IT(22218);
                        if (*(chunk->data[i]) == *(chunk2->data[i]))
                        {TRACE_IT(22219);
                            continue;
                        }
                    }
                    chunk->data[i] = NULL;
                }
            }
            chunk2 = chunk2->next;
            pred = chunk;
       }
    }

    void Clear()
    {TRACE_IT(22220);
        this->firstChunk = NULL;
    }

#if DBG_DUMP

    void Dump()
    {TRACE_IT(22221);
        for (SAChunk<T> *chunk = this->firstChunk; chunk; chunk = chunk->next)
        {TRACE_IT(22222);
            for (int index = chunk->startIndex; index < this->chunkSize; index++)
            {TRACE_IT(22223);
                if (chunk->data[index])
                {TRACE_IT(22224);
                    Output::Print(_u("Index %4d  =>  "), index);
                    chunk->data[index]->Dump();
                    Output::Print(_u("\n"));
                }
            }
        }
    }

#endif
};
